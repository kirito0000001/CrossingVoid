
<#
.SYNOPSIS
    零境交错（CrossingVoid）打包脚本 —— chunk 包 / patch 包

.DESCRIPTION
    走 UAT BuildCookRun，产出：
      - 基础包：pakchunk0..N-<Platform>.pak（按 DefaultGame.ini 的 bGenerateChunks=True 分块）
      - 补丁包：pakchunkN-<Platform>_P.pak（Patch 模式，只含变化资产）

    默认不压缩 zip，产物就是目录 —— 直接映射成文件清单，符合 DreamChunkDownloader 的清单设计。

    关于 chunk 的生效链路（已对照 UE 5.8 源码核实）：
      DefaultGame.ini 的 bGenerateChunks=True
        → DeploymentContext 读它，设置 PlatformUsesChunkManifests
        → 但要命令行给 -manifests，cook 阶段才会真的产 chunk 清单
        → 才拆出 pakchunkN-*.pak
      注意：UE 5.8 里【已经没有 -nochunks 这个参数了】，别去找它。

.PARAMETER Mode
    Base    首次发布基线包（-createreleaseversion）
    Patch   补丁包，产 _P.pak（需要已存在的基线）
    Quick   不建基线版本，只验证 chunk 生不生效（快很多）

    三个模式都必须经过 stage —— pak 是从 staging 目录生成的，
    少了 -stage 会报 "Only staged builds can be paked"。
    Shipping 目标缺失时会自动补 -build（第一次打包必然如此）。
    全量 cook 很慢，零境实测约 43 分钟；cook 缓存会复用于后续打包。

.PARAMETER Target
    打包目标类型，三选一：
      Client  客户端（玩家跑的包）
      Server  专用服务器（不带客户端资源的独立服务器）
      Both    两者都打（各自一份产物，分目录）

    对应关系：
      Client → -clientconfig=Shipping -client
      Server → -serverconfig=Shipping -server
      Both   → 两次 BuildCookRun，分别产到 Client\ / Server\ 子目录

    服务器目标依赖 Source\CrossingVoidServer.Target.cs（Type=TargetType.Server），工程里已有。

.PARAMETER Platform
    Win64 / Android
    注意：Server 目标只在 Win64 与 Linux 上有意义；Android 不支持专用服务器。

.PARAMETER ReleaseVersion
    Base 模式必填（如 1.0）。基线会落在 <工程>\Releases\<版本>\<平台>\。
    Patch 模式也必填 —— 必须指明「基于哪个基线版本」算差异，引擎强制要求
    （只给 -generatepatch 不给 -basedonreleaseversion 会直接抛异常）。

.PARAMETER ArchiveDir
    产物输出根目录。默认 F:\DaBaoV\Crossingvoid

.PARAMETER ReleaseRoot
    基线（Releases）根目录。默认 F:\DaBaoV\Crossingvoid\Release
    UAT 的"建基线"(-createreleaseversionroot) 和"算补丁"(-basedonreleaseversionroot) 都用它，
    所以读和写必须是同一个值 —— 插件「运行设置（额外）→ 基线位置」每次都会把界面上的值传进来。

.PARAMETER Cook
    强制全量重新 cook。第一次打包或改了大量资产时用。

.PARAMETER SkipBuild
    允许在不需要编译时附加 -nocompile（省时间）。默认已是这个行为。

.PARAMETER ForceBuild
    强制本次编译 C++（即使 Shipping 目标已存在）。

.EXAMPLE
    # 第一次打客户端基线包（Windows）
    .\Pack-CrossingVoid.ps1 -Mode Base -ReleaseVersion 1.0

    # 打专用服务器包
    .\Pack-CrossingVoid.ps1 -Mode Base -ReleaseVersion 1.0 -Target Server

    # 客户端 + 服务器一起打
    .\Pack-CrossingVoid.ps1 -Mode Base -ReleaseVersion 1.0 -Target Both

    # 改一张贴图后打补丁
    .\Pack-CrossingVoid.ps1 -Mode Patch -ReleaseVersion 1.0

    # 只想快速看 chunk 生不生效
    .\Pack-CrossingVoid.ps1 -Mode Quick

    # Android
    .\Pack-CrossingVoid.ps1 -Mode Base -ReleaseVersion 1.0 -Platform Android
#>

[CmdletBinding()]
param(
    [ValidateSet('Base', 'Patch', 'Quick')]
    [string]$Mode = 'Base',

    [ValidateSet('Client', 'Server', 'Both')]
    [string]$Target = 'Client',

    [ValidateSet('Win64', 'Android')]
    [string]$Platform = 'Win64',

    [string]$ReleaseVersion = '',

    # 给玩家看的版本号（写进 Config\DefaultEngine.ini 的
    # AndroidRuntimeSettings.VersionDisplayName）。只影响 APK 上显示的版本；
    # StoreVersion（versionCode）刻意不动。
    [string]$PlayerVersion = '',

    [string]$ArchiveDir = 'F:\DaBaoV\Crossingvoid',

    # 基线（Releases）根目录；插件「运行设置（额外）→ 基线位置」写的就是它。
    # 建基线和算补丁都用这一个值，别只改一处。
    [string]$ReleaseRoot = 'F:\DaBaoV\Crossingvoid\Release',

    # 本次要 cook 的地图（逗号分隔，如 "/Game/MapS/LoginMap,/Game/MapS/MainMap"）。
    # 留空 = 用 DefaultGame.ini 里的 +MapsToCook 那四条。
    # 非空时会逐个拼成 UAT 的 -map= 参数。
    [string]$Maps = '',

    [switch]$Cook,

    [switch]$SkipBuild,

    [switch]$ForceBuild,

    # 默认每次打包前都会重新生成 Config\DefaultPakFileRules.ini（由 CrossingChunkRuleSet 翻译而来）
    [switch]$SkipRuleGen,

    # 只打印将要执行的命令行（含规则生成器、Zen 检查），不真的打包。
    # 用来核对参数、也方便页面上的「复制命令」。
    [switch]$DryRun,

    # 保留 pdb（符号文件）。默认【从源头就不生成】+ 归档目录里也兜底剔除。
    # Windows 在 Shipping 下默认会产 pdb（客户端 1.6 GB / 服务器 1.5 GB），
    # 要关掉必须 -NoDebugInfo 和 -NoLinkerDebugInfo 一起给，缺一个都不行。
    # 详见 Tools\打包说明.md 的「pdb 与符号文件」一节。
    [switch]$KeepSymbols,

    # 只清"能再生"的缓存，清完就退出（不打包）。页面上的《清除缓存》按钮走的就是这条。
    # 清的三样：Saved\StagedBuilds（UAT 组装产物）、Saved\Cooked（cooked 数据）、
    #            Saved\Shaders（shader 缓存）。
    # 绝不碰：Intermediate\Build（编译产物，重编很贵）、Content、归档目录、Releases 基线。
    # 配 -DryRun 只列不删。
    [switch]$ClearCache,

    # 只读查询：报告 / 产物 / 基线三件事，给页面上的状态行用。查完就退出。
    [switch]$Status,

    # 多进程 cook（MPCook，UE5.3+）：把 cook 拆成「1 个 director + (N-1) 个 cook worker」好几个进程并行跑。
    # 引擎里的开关就是 -CookProcessCount=N —— 出处是引擎自带注释（Engine\Config\BaseEditor.ini 的 [CookSettings]）：
    #   "CookProcessCount=(1 or less) is singleprocess. CookProcessCount=(N>1) is 1 director and N-1 cookworkers."
    # UAT 的 ProjectParams 里没有这个参数，所以只能靠 -AdditionalCookerOptions 透传给 cooker。
    # 默认 1 = 单进程（现状）。两条硬约束都来自 CookDirector 源码：
    #   · CPU 是硬切的：CoreLimit = 物理核数 / 进程数，所以进程数超过物理核只会互相拖慢；
    #   · 每个 worker 都是一整个 editor 进程。多进程下引擎会把"低内存就 GC"的保护关掉
    #     （只留 Critical 压力才 GC），内存不够是直接跟系统抢，不会优雅降级。
    [int]$CookProcessCount = 1,

    # 工程根。留空 = 从脚本所在位置自动推（见下面 Resolve-ProjectRoot）。
    # 插件面板每次都会显式传这个值，所以「界面上跑」和「命令行跑」必然是同一份工程。
    [string]$ProjectRoot = ''
)
$ErrorActionPreference = 'Stop'

# ============================ 固定路径 ============================
# 工程根不再写死。本脚本会被放在两处，都要能自己推出来：
#   <工程>\Tools\                              —— 旧布局（脚本挂在工程里）
#   <工程>\Plugins\CrossingChunkEditor\Tools\  —— 插件自带（本仓库的形态）
# 装在 Engine\Plugins\ 下时推不出工程根，那种情况必须显式传 -ProjectRoot。
# 推导方式：从脚本所在目录逐级向上，找第一个含 *.uproject 的目录。
function Resolve-ProjectRoot {
    param([string]$StartDir)
    $dir = $StartDir
    for ($i = 0; $i -lt 12 -and $dir; $i++) {
        if (Get-ChildItem -LiteralPath $dir -Filter '*.uproject' -File -ErrorAction SilentlyContinue |
            Select-Object -First 1) { return $dir }
        $parent = Split-Path -Parent $dir
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return ''
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Resolve-ProjectRoot -StartDir $PSScriptRoot
    if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
        throw "[检查失败] 推不出工程根：从 $PSScriptRoot 逐级向上都没找到 .uproject。`n  请显式传 -ProjectRoot <工程目录>。"
    }
}
$ProjectRoot = $ProjectRoot.TrimEnd('\', '/')
$ProjectFile = Join-Path $ProjectRoot 'CrossingVoid.uproject'
$EngineRoot  = 'D:\UnrealEngine-5.8.2'
$RunUAT      = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

# 基线（Releases）根目录来自 -ReleaseRoot 参数（默认 F:\DaBaoV\Crossingvoid\Release，一条基线约 1.5 GB）。
# UAT 的"写"(-createreleaseversionroot) 和"读"(-basedonreleaseversionroot) 必须同根 ——
# 插件每次打包/查状态都会把「运行设置（额外）→ 基线位置」里的值原样传进来，
# 所以两边永远是同一个值，不会出现"写 D 读 C 找不到基线"。

$PlatformDir = if ($Platform -eq 'Win64') { 'Windows' } else { 'Android' }

# ============================ 前置检查 ============================
function Assert-Path {
    param([string]$Path, [string]$What)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "[检查失败] 找不到${What}：`n  $Path"
    }
}

# ============================ 清归档目录 ============================
# 为什么需要：UAT 归档只复制、从不删（ArchiveCommand.Automation.cs:40-52 的
# ApplyArchiveManifest 里只有 CopyFileOrSymlink），所以上一版从工程里删掉的资源
# 会永远躺在归档目录里。服务器走 git 的话，这些"幽灵文件"会被一起提交、
# 也永远不会从线上消失（散件模式下引擎是按路径读 Content 的，旧资源甚至可能还被加载到）。
# 铁律：路径里任何一级叫 Saved 的一律不碰 —— 那是服务器的玩家数据（存档/配置）。
# 返回删了多少项、保留了几处 Saved，调用方打印用。
function Clear-ArchiveExceptSaved {
    param([Parameter(Mandatory)][string]$Path)

    $RootFull = [System.IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $RootFull)) {
        return [pscustomobject]@{ Removed = 0; KeptSaved = 0 }
    }

    $Sep = [System.IO.Path]::DirectorySeparatorChar
    function Test-IsUnderSaved {
        param([string]$FullName)
        $Rel = $FullName.Substring($RootFull.Length).TrimStart($Sep)
        if ([string]::IsNullOrEmpty($Rel)) { return $false }
        foreach ($Seg in ($Rel -split '[\\/]')) { if ($Seg -eq 'Saved') { return $true } }
        return $false
    }
    # 不跟 reparse point（junction/symlink）走，避免删到目录外面去
    $Normal = { param($i) -not ($i.Attributes -band [System.IO.FileAttributes]::ReparsePoint) }

    $Removed = 0
    $KeptSaved = 0

    # 1) 删文件（Saved 下面的一律跳过）
    foreach ($f in (Get-ChildItem -LiteralPath $RootFull -Recurse -Force -File -ErrorAction SilentlyContinue | Where-Object $Normal)) {
        if (Test-IsUnderSaved $f.FullName) { continue }
        Remove-Item -LiteralPath $f.FullName -Force -ErrorAction Stop
        $Removed++
    }

    # 2) 再删空目录。Saved 自己不能删；而它的祖先目录因为"里面还有 Saved"仍然非空，会被自动保住。
    $Dirs = Get-ChildItem -LiteralPath $RootFull -Recurse -Force -Directory -ErrorAction SilentlyContinue |
        Where-Object $Normal | Sort-Object { $_.FullName.Length } -Descending
    foreach ($d in $Dirs) {
        if (Test-IsUnderSaved $d.FullName) {
            if ((Split-Path -Leaf $d.FullName) -eq 'Saved') { $KeptSaved++ }
            continue
        }
        if (@(Get-ChildItem -LiteralPath $d.FullName -Force -ErrorAction SilentlyContinue).Count -eq 0) {
            Remove-Item -LiteralPath $d.FullName -Force -ErrorAction Stop
            $Removed++
        }
    }

    return [pscustomobject]@{ Removed = $Removed; KeptSaved = $KeptSaved }
}

# ============================ cook 缓存体检 ============================
# 缓存会脏的唯一原因：上次 cook 之后有资产被删掉/改名 —— 那样 Saved\Cooked 里
# 会留下"工程里已经没有了"的过期文件，而它们会被打进下一份基线。
# 所以只在《版本更新》（要建基线）时扫一遍：脏了自动全量（-clean），干净就照走增量。
# 扫的是文件列表，不开引擎、不联网；几千个文件几秒。
function Test-CookCacheDirty {
    param([string]$Root, [string]$CookPlatform, [string]$Name)

    $CookedContent = Join-Path $Root "Saved\Cooked\$CookPlatform\$Name\Content"
    $SourceContent = Join-Path $Root 'Content'
    $Result = @{ Dirty = $false; Cooked = 0; Orphans = @(); Reason = '' }

    if (-not (Test-Path -LiteralPath $CookedContent)) {
        $Result.Reason = '还没有已烘焙的内容（首次 cook 本来就是全量）'
        return $Result
    }

    # 工程里现有的源资产收成一个集合，避免上万次 Test-Path
    $Src = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    Get-ChildItem -LiteralPath $SourceContent -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in '.uasset', '.umap' } |
        ForEach-Object { $null = $Src.Add($_.FullName) }

    $Orphans = New-Object System.Collections.Generic.List[string]
    $Total = 0
    Get-ChildItem -LiteralPath $CookedContent -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in '.uasset', '.umap' } |
        ForEach-Object {
            $Total++
            $Rel = $_.FullName.Substring($CookedContent.Length).TrimStart('\')
            if (-not $Src.Contains((Join-Path $SourceContent $Rel))) { $Orphans.Add($Rel) }
        }

    $Result.Cooked = $Total
    $Result.Orphans = $Orphans.ToArray()
    $Result.Dirty = ($Orphans.Count -gt 0)
    if ($Total -eq 0) {
        # 扫到 0 个说明"Cooked 布局"的假设可能不对 —— 显式报出来，别静默当成干净
        $Result.Reason = "Cooked 里没扫到项目内容（路径 $CookedContent？）"
    }
    return $Result
}

# ============================ 平台设置快照 ============================
# 打包和平台设置"分得比较开"，容易打完才想起来没改。所以每次打包前把当时
# 真正生效的设置打印一遍并落成 json（报告里也会带上），以后查"这包是哪套设置打的"。
# 密码类字段一律不记录。
function Get-PlatformSettingsSnapshot {
    param([string]$Root)

    $EngineIni = Join-Path $Root 'Config\DefaultEngine.ini'
    $AndroidKeys = @(
        'PackageName', 'ApplicationDisplayName', 'VersionDisplayName', 'StoreVersion',
        'MinSDKVersion', 'TargetSDKVersion', 'bBuildForArm64', 'bBuildForX8664',
        'bPackageDataInsideApk', 'bEnableBundle', 'Orientation', 'KeyStore', 'KeyAlias'
    )

    $Android = [ordered]@{}
    if (Test-Path -LiteralPath $EngineIni) {
        $inSection = $false
        foreach ($line in (Get-Content -LiteralPath $EngineIni -Encoding UTF8)) {
            $t = $line.Trim()
            if ($t.StartsWith('[')) {
                $inSection = ($t -eq '[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]')
                continue
            }
            if (-not $inSection -or $t.StartsWith(';')) { continue }
            $kv = $t -split '=', 2
            if ($kv.Count -eq 2 -and ($AndroidKeys -contains $kv[0])) { $Android[$kv[0]] = $kv[1] }
        }
    }

    $WinIcon = Join-Path $Root 'Build\Windows\Application.ico'
    $WinIconExists = Test-Path -LiteralPath $WinIcon
    $AndroidRes = Join-Path $Root 'Build\Android\res'
    $AndroidIcons = @(Get-ChildItem -LiteralPath $AndroidRes -Recurse -Filter 'icon.png' -ErrorAction SilentlyContinue)
    $AndroidIconTime = if ($AndroidIcons.Count -gt 0) {
        ($AndroidIcons | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime.ToString('yyyy-MM-dd HH:mm')
    } else { '' }

    # 和引擎自带的默认图标逐个比 hash：一模一样 = 还没有换过（比看大小/靠感觉可靠）
    # 引擎默认位置：<Engine>\Build\Android\Java\res\drawable*/icon.png
    $EngineRes = Join-Path $EngineRoot 'Engine\Build\Android\Java\res'
    $IconCompared = 0
    $IconDefault = 0
    foreach ($icon in $AndroidIcons) {
        $rel = $icon.FullName.Substring($AndroidRes.Length).TrimStart('\')
        $engineIcon = Join-Path $EngineRes $rel
        if (-not (Test-Path -LiteralPath $engineIcon)) { continue }
        $IconCompared++
        if ((Get-FileHash -LiteralPath $icon.FullName -Algorithm MD5).Hash -eq
            (Get-FileHash -LiteralPath $engineIcon -Algorithm MD5).Hash) { $IconDefault++ }
    }

    return [ordered]@{
        android      = $Android
        windowsIcon  = [ordered]@{
            path      = $WinIcon
            exists    = $WinIconExists
            kb        = if ($WinIconExists) { [math]::Round((Get-Item -LiteralPath $WinIcon).Length / 1KB, 1) } else { 0 }
            lastWrite = if ($WinIconExists) { (Get-Item -LiteralPath $WinIcon).LastWriteTime.ToString('yyyy-MM-dd HH:mm') } else { '' }
            folder    = (Join-Path $Root 'Build\Windows')
        }
        androidIcons = [ordered]@{
            count        = $AndroidIcons.Count
            lastWrite    = $AndroidIconTime
            folder       = $AndroidRes
            compared     = $IconCompared
            defaultCount = $IconDefault
        }
    }
}

function Show-PlatformSettings {
    param($S)

    Write-Host '[平台设置]' -ForegroundColor Cyan
    $a = $S.android
    if ($a.Count -gt 0) {
        Write-Host ('  安卓   版本 {0} · 包名 {1}' -f $a['VersionDisplayName'], $a['PackageName']) -ForegroundColor Gray
        Write-Host ('         显示名 {0} · 商店版本号 {1}' -f $a['ApplicationDisplayName'], $a['StoreVersion']) -ForegroundColor Gray
        Write-Host ('         SDK {0}/{1} · ABI arm64={2} x86_64={3} · APK内打包数据={4}' -f `
                $a['MinSDKVersion'], $a['TargetSDKVersion'], $a['bBuildForArm64'], $a['bBuildForX8664'], $a['bPackageDataInsideApk']) -ForegroundColor Gray
        if ($S.androidIcons.compared -gt 0 -and $S.androidIcons.defaultCount -eq $S.androidIcons.compared) {
            Write-Host ('         [警告] 安卓图标 {0} 张全是引擎默认（还没换过）—— 去「项目设置 → 平台 → Android → 图标」换' -f $S.androidIcons.count) -ForegroundColor Yellow
        }
        elseif ($S.androidIcons.defaultCount -gt 0) {
            Write-Host ('         [警告] 安卓图标里有 {0}/{1} 张还是引擎默认 —— 去「项目设置 → 平台 → Android → 图标」换' -f $S.androidIcons.defaultCount, $S.androidIcons.compared) -ForegroundColor Yellow
        }
        else {
            Write-Host ('         安卓图标 {0} 张（最近改于 {1}）' -f $S.androidIcons.count, $S.androidIcons.lastWrite) -ForegroundColor Gray
        }
    }
    if ($S.windowsIcon.exists) {
        Write-Host ('  Windows  图标 {0} KB（{1}）' -f $S.windowsIcon.kb, $S.windowsIcon.lastWrite) -ForegroundColor Gray
    }
    else {
        Write-Host '  Windows  [警告] 没有 Build\Windows\Application.ico —— 现在用的是引擎默认图标（这个没有界面设置，把 .ico 放进 Build\Windows\ 就行）' -ForegroundColor Yellow
    }
}



Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' 零境交错 / CrossingVoid  打包' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ""

Assert-Path $ProjectFile '工程文件 .uproject'
Assert-Path $RunUAT      'RunUAT.bat（检查 EngineRoot 是否正确）'

# ============================ 清缓存模式 ============================
if ($ClearCache) {
    $CacheTargets = @(
        @{ Path = Join-Path $ProjectRoot 'Saved\StagedBuilds'; Hint = 'UAT 组装产物（下次打包重新生成）' },
        @{ Path = Join-Path $ProjectRoot 'Saved\Cooked';       Hint = 'cooked 数据（下次 cook 重新生成）' },
        @{ Path = Join-Path $ProjectRoot 'Saved\Shaders';      Hint = 'shader 缓存（下次重新编译）' }
    )

    Write-Host '[清缓存] 只删能再生的目录；Intermediate\Build、Content、归档目录一律不碰' -ForegroundColor Cyan
    Write-Host ''

    $TotalMB = 0.0
    foreach ($t in $CacheTargets) {
        if (-not (Test-Path -LiteralPath $t.Path)) {
            Write-Host ('  {0,-24} {1}' -f (Split-Path $t.Path -Leaf), '不存在，跳过') -ForegroundColor DarkGray
            continue
        }

        # 兜底：解析出来的真实路径必须还在工程内，否则不删
        $resolved = (Resolve-Path -LiteralPath $t.Path).Path
        if (-not $resolved.StartsWith($ProjectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Host ('  {0,-24} {1}' -f (Split-Path $t.Path -Leaf), "拒绝：路径不在工程内 -> $resolved") -ForegroundColor Red
            continue
        }

        $MB = [math]::Round(((Get-ChildItem -LiteralPath $t.Path -Recurse -File -ErrorAction SilentlyContinue |
                Measure-Object Length -Sum).Sum) / 1MB, 1)
        $TotalMB += $MB

        if ($DryRun) {
            Write-Host ('  {0,-24} {1,10} MB   （DryRun，未删）' -f (Split-Path $t.Path -Leaf), $MB) -ForegroundColor Yellow
            continue
        }

        Remove-Item -LiteralPath $t.Path -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $t.Path) {
            Write-Host ('  {0,-24} {1,10} MB   没删干净（可能有进程占用）' -f (Split-Path $t.Path -Leaf), $MB) -ForegroundColor Red
        }
        else {
            Write-Host ('  {0,-24} {1,10} MB   已删除 —— {2}' -f (Split-Path $t.Path -Leaf), $MB, $t.Hint) -ForegroundColor Green
        }
    }

    Write-Host ''
    if ($DryRun) {
        Write-Host ('[清缓存] DryRun：将释放约 {0} MB' -f [math]::Round($TotalMB, 1)) -ForegroundColor Yellow
    }
    else {
        Write-Host ('[清缓存] 共释放 {0} MB' -f [math]::Round($TotalMB, 1)) -ForegroundColor Green
    }
    return
}

# ============================ 状态查询模式 ============================
# 输出三行 STATIC key=value（值里用 | 分隔字段），页面按行解析。只读，不改任何东西。
if ($Status) {
    $ReleaseSub = if ($Target -eq 'Server') { "$PlatformDir-Server" } else { $PlatformDir }

    # ① 报告：最近一次打包留下的报告 json
    # 注意目录名用的是平台名（Win64 / Android），不是 Windows / Android 那个
    # —— 见 Build-ChunkReport.ps1 的 $ReportDir
    # 《快速验证》的报告是单独一份（见 Build-ChunkReport.ps1）
    $RepSuffix = if ($Mode -eq 'Quick') { '-Quick' } else { '' }
    $ReportPath = Join-Path $ProjectRoot "Saved\PackLogs\ChunkReports\$Platform-$Target$RepSuffix\latest.json"
    $RepTime = ''; $RepMode = ''
    if (Test-Path -LiteralPath $ReportPath) {
        $raw = Get-Content -Raw -Encoding UTF8 $ReportPath
        if ($raw -match '"generatedAt"\s*:\s*"([^"]+)"') { $RepTime = $Matches[1] }
        if ($raw -match '"mode"\s*:\s*"([^"]+)"')       { $RepMode = $Matches[1] }
    }
    Write-Output ("STATUS_REPORT={0}|{1}|{2}" -f $RepTime, $RepMode, (Test-Path -LiteralPath $ReportPath))

    # ② 产物：归档目录下对应平台的子目录（Windows / WindowsServer / Android）
    # 服务器散件的归档目录是 <ArchiveDir>\WindowsServer，跟客户端那份 <ArchiveDir>\Windows
    # 是两回事。原来用 "$PlatformDir*" 前缀匹配，Windows 会先命中，于是选服务器时
    # 那一格显示的是客户端产物（2026-09-20 修）。
    $ArtifactName = if ($Target -eq 'Server') { 'WindowsServer' } else { $PlatformDir }
    $ArtifactPath = Join-Path $ArchiveDir $ArtifactName
    if (Test-Path -LiteralPath $ArchiveDir) {
        $cand = Get-ChildItem -LiteralPath $ArchiveDir -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ieq $ArtifactName } | Select-Object -First 1
        if (-not $cand) {
            # 兜底：目录名带后缀（老归档、或以后改名）时按前缀再找一个
            $cand = Get-ChildItem -LiteralPath $ArchiveDir -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "$ArtifactName*" } | Select-Object -First 1
        }
        if ($cand) { $ArtifactPath = $cand.FullName }
    }
    Write-Output ("STATUS_ARTIFACT={0}|{1}" -f $ArtifactPath, (Test-Path -LiteralPath $ArtifactPath))

    # ③ 基线
    $BaselinePath = if ([string]::IsNullOrWhiteSpace($ReleaseVersion)) { '' } else { Join-Path $ReleaseRoot "$ReleaseVersion\$ReleaseSub" }
    Write-Output ("STATUS_BASELINE={0}|{1}|{2}" -f $BaselinePath, ($BaselinePath -and (Test-Path -LiteralPath $BaselinePath)), $ReleaseRoot)

    # 除了给命令行看的文本，再落一个小 json 给页面读。
    # 页面不去解析 stdout —— 那东西会随终端宽度换行，字段拼错一次就是"某一格空白"这种难查的毛病。
    # json 走的是和报告完全同一条路（UTF-8 无 BOM），页面用读报告那套读它。
    $StatusPath = Join-Path $ProjectRoot 'Saved\PackLogs\pack-status.json'
    $StatusObj = [ordered]@{
        reportTime      = $RepTime
        reportMode      = $RepMode
        reportExists    = [bool](Test-Path -LiteralPath $ReportPath)
        artifactPath    = $ArtifactPath
        artifactExists  = [bool](Test-Path -LiteralPath $ArtifactPath)
        baselineVersion = $ReleaseVersion
        baselinePath    = $BaselinePath
        baselineExists  = [bool]($BaselinePath -and (Test-Path -LiteralPath $BaselinePath))
        releaseRoot     = $ReleaseRoot
        platformSettings = Get-PlatformSettingsSnapshot -Root $ProjectRoot
    }
    $StatusDir = Split-Path $StatusPath -Parent
    if (-not (Test-Path -LiteralPath $StatusDir)) { $null = New-Item -ItemType Directory -Path $StatusDir -Force }
    [System.IO.File]::WriteAllText($StatusPath, ($StatusObj | ConvertTo-Json -Compress), (New-Object System.Text.UTF8Encoding($false)))
    Write-Output ("STATUS_FILE={0}" -f $StatusPath)
    Show-PlatformSettings -S $StatusObj.platformSettings
    return
}

# ============================ 前置 1：分包规则 ============================
# 把 Config\DefaultCrossingChunk.ini 的 CrossingChunkRuleSet 翻译成
# Config\DefaultPakFileRules.ini（引擎自带的按文件覆盖 chunk 归属的机制）。
# 服务器目标出散件、不分包，所以只有客户端目标才需要这份规则。
# 生成器和本脚本在同一个目录，所以用 $PSScriptRoot 而不是「工程根 + Tools」——
# 这样脚本从 <工程>\Tools\ 搬到插件里也不用改。
$RuleGenScript = Join-Path $PSScriptRoot 'Build-PakFileRules.ps1'
if (-not $SkipRuleGen -and $Target -ne 'Server') {
    Assert-Path $RuleGenScript '分包规则生成器 Build-PakFileRules.ps1'
    & $RuleGenScript -RuleIni (Join-Path $ProjectRoot 'Config\DefaultCrossingChunk.ini')
}

# ============================ 前置 2：Zen 存储服务器 ============================
# 分包走的是官方 oplog 链路（-RunChunkAssigner）：cook 把包写进 Zen 的 oplog，
# 之后 OplogChunkAssigner 再读回来做 chunk 分配。
# 坑：ZenServer 是跟着 cook 进程起的，cook 一结束它就退出，而分配器那一步在 cook 之后，
#     于是会直接 BUILD FAILED（Failed to send op attachment request to Zen）。
# 所以这里先确保有一个常驻的 ZenServer。
function Ensure-ZenServer {
    $listening = Get-NetTCPConnection -LocalPort 8558 -State Listen -ErrorAction SilentlyContinue
    if ($listening) {
        Write-Host '[Zen] 已有 ZenServer 在监听 8558，直接复用' -ForegroundColor DarkGray
        return $true
    }

    $zenInstall = Join-Path $env:LOCALAPPDATA 'UnrealEngine\Common\Zen\Install'
    $zenExe     = Join-Path $zenInstall 'zenserver.exe'
    $zenData    = Join-Path $env:LOCALAPPDATA 'UnrealEngine\Common\Zen\Data'
    if (-not (Test-Path -LiteralPath $zenExe)) {
        Write-Host "[Zen] 找不到 $zenExe，跳过（打包可能因为分配器读不到 oplog 而失败）" -ForegroundColor Yellow
        return $false
    }

    Write-Host '[Zen] 启动常驻 ZenServer（参数与引擎自动拉起的一致，去掉 --owner-pid）' -ForegroundColor DarkGray
    $zenArgs = @(
        '--port', '8558',
        '--data-dir', $zenData,
        '--http', 'asio',
        '--gc-cache-duration-seconds', '1209600',
        '--gc-interval-seconds', '21600',
        '--gc-low-diskspace-threshold', '2147483648',
        '--cache-bucket-limit-overwrites',
        '--quiet',
        '--restrict-content-types',
        '--allow-external-oidctoken-exe=false',
        '--plugins-config', (Join-Path $zenInstall 'zen_plugins_v1.json'),
        '--security-config-path', (Join-Path $zenInstall 'security-config.json')
    )
    Start-Process -FilePath $zenExe -ArgumentList $zenArgs -WindowStyle Hidden | Out-Null

    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Seconds 1
        if (Get-NetTCPConnection -LocalPort 8558 -State Listen -ErrorAction SilentlyContinue) {
            Write-Host '[Zen] ZenServer 就绪' -ForegroundColor DarkGray
            return $true
        }
    }
    Write-Host '[Zen] 等了 30 秒还没就绪，继续打包（失败的话看上面的 Zen 提示）' -ForegroundColor Yellow
    return $false
}

$null = Ensure-ZenServer

Write-Host "[环境]" -ForegroundColor Yellow
Write-Host "  工程     : $ProjectFile"
Write-Host "  引擎     : $EngineRoot"
Write-Host "  平台     : $Platform"
Write-Host "  目标     : $Target"
Write-Host "  模式     : $Mode"
Write-Host "  产物目录 : $ArchiveDir"
Write-Host ""

# Android 不支持专用服务器，提前拦住（比让 UAT 报一堆错友好）
if ($Platform -eq 'Android' -and $Target -ne 'Client') {
    throw "[参数冲突] Android 没有专用服务器目标，Target 只能是 Client。"
}

# ============================ 参数组装 ============================
# 注意：这里【绝对不加】 -nochunks / -chunks=0
#       ini 里 bGenerateChunks=True 要靠这里放行才会生效
function Build-UatArgs {
    param(
        [string]$PackTarget,      # Client 或 Server
        [string]$OutDir
    )

    $args = [System.Collections.Generic.List[string]]::new()
    $AutoClean = $false   # 《版本更新》里缓存体检发现脏了才会变 $true（见 Test-CookCacheDirty）

    $args.Add('BuildCookRun')
    $args.Add("-project=$ProjectFile")
    $args.Add('-noP4')
    $args.Add('-utf8output')
    $args.Add("-platform=$Platform")

    # UAT 靠这个参数认游戏 target。不传的话 -cook 会直接抛
    # "Game target not found. Game target is required with -cook or -cookonthefly"
    # （-client / -server 只说明要打哪一侧，不提供 target 名）。
    $args.Add("-target=$(if ($PackTarget -eq 'Server') { 'CrossingVoidServer' } else { 'CrossingVoid' })")

    # ---- 选定的地图（空则交给 ini 的 +MapsToCook）----
    if (-not [string]::IsNullOrWhiteSpace($Maps)) {
        foreach ($m in ($Maps -split '[,;]')) {
            $mapName = $m.Trim()
            if ($mapName -ne '') { $args.Add("-map=$mapName") }
        }
    }

    # ---- 客户端 / 服务器 的关键分歧点 ----
    if ($PackTarget -eq 'Server') {
        $args.Add('-serverconfig=Shipping')
        $args.Add('-server')
        # 服务器不需要客户端资源以外的很多东西，但 chunk 划分逻辑一致
        # （默认按 clientconfig 算，服务器侧沿用同一套 -manifests 规则）
    }
    else {
        $args.Add('-clientconfig=Shipping')
        # 这里**不要**加 -client：工程的 CrossingVoid.Target.cs 声明的是 Game 类型，
        # 而 -client 要求 Client 类型的目标，两者混用 UAT 会直接抛
        # "Cannot mix game and client targets"。
        # 对 Game 目标来说，-clientconfig=Shipping 就是客户端打包。
    }

    # ---- 检查 Shipping 目标是否存在 ----
    # stage 需要 <工程>\Binaries\Win64\CrossingVoid-Win64-Shipping.target
    # 服务器需要 CrossingVoidServer-Win64-Shipping.target
    $NeedBuild = $true
    if ($Platform -eq 'Win64') {
        $TargetName = if ($PackTarget -eq 'Server') { 'CrossingVoidServer' } else { 'CrossingVoid' }
        $ShippingTarget = Join-Path $ProjectRoot "Binaries\Win64\$TargetName-Win64-Shipping.target"
        if (Test-Path -LiteralPath $ShippingTarget) { $NeedBuild = $false }
    }
    else {
        # Android 的 Shipping 目标落在 Android 子目录下
        $AndroidBin = Join-Path $ProjectRoot 'Binaries\Android'
        if (Test-Path -LiteralPath $AndroidBin) {
            $found = Get-ChildItem -LiteralPath $AndroidBin -Filter '*Shipping*' -ErrorAction SilentlyContinue
            if ($found) { $NeedBuild = $false }
        }
    }

    $DoBuild = $false
    if ($Mode -eq 'Quick') {
        # Quick 默认不编译，但如果 Shipping 目标缺失就绕不过去，必须补上
        if ($NeedBuild -or $ForceBuild) { $DoBuild = $true }
    }
    else {
        $DoBuild = $true
    }

    # --- cook ---
    if ($CookProcessCount -gt 1) {
        $args.Add("-AdditionalCookerOptions=-CookProcessCount=$CookProcessCount")
        Write-Host ("[多进程 cook] CookProcessCount={0}（1 个 director + {1} 个 cook worker）" -f $CookProcessCount, ($CookProcessCount - 1)) -ForegroundColor Cyan
    }
    if ($Mode -eq 'Quick') {
        $args.Add('-cook')
        if ($PackTarget -eq 'Server') {
            # 服务器【出散件】：不打包、不分 chunk。
            # 服务端更新走 git（文件级差异比 chunk 细），打成 pak 反而没法用；
            # 所以这里既不加 -manifests / -pak，也不加 -RunChunkAssigner。
        }
        else {
            $args.Add('-manifests')
            # oplog 分包链路。不传这个参数 → OplogChunkAssigner 不运行 →
            # pak 阶段退回读磁盘上的旧 ChunkManifest（历史产物永远停在 0/2/3）
            $args.Add('-RunChunkAssigner')
        }
        if ($DoBuild) { $args.Add('-build') }
        $args.Add('-stage')
        # 客户端【不打 pak】—— 《快速验证》只是"看看分块归属对不对"，产物不该能拿去用。
        # 没有 pak 就没法分发（启动器/玩家要的是 pak），这是故意的：
        # 要发布请走《版本更新》（完整包 + 建基线）或《常规补丁》（_P.pak）。
        # 顺带省掉 UnrealPak + Oodle 压缩那一段（实测约 1 分钟）。
        $args.Add('-archive')
        $args.Add("-archivedirectory=$OutDir")
    }
    else {
        $args.Add('-cook')
        if ($Cook -or $AutoClean) { $args.Add('-clean') }
        if ($PackTarget -ne 'Server') {
            $args.Add('-manifests')
            $args.Add('-RunChunkAssigner')
        }
        $args.Add('-build')
        $args.Add('-stage')
        if ($PackTarget -ne 'Server') {
            $args.Add('-pak')
            $args.Add('-compressed')
        }
        # 安卓必须显式加 -package：APK 和 OBB 都是 UAT 的 "Package" 阶段生成的
        # （PackageCommand.Automation.cs:17 —— Project.Package 只在 -package 或 -deploy 时才跑）。
        # 不加的话流程是 stage → 直接 archive，archive 会去要 OBB 并报
        #   "ARCHIVE FAILED - <...>main.1.<包名>.obb was not found"（ExitCode=53）。
        # Windows 不需要这一步（产物就是 staged 文件 + pak），所以只有安卓会暴露这个问题。
        if ($Platform -eq 'Android') {
            $args.Add('-package')
        }
        $args.Add('-archive')
        $args.Add("-archivedirectory=$OutDir")

        if ($PackTarget -eq 'Server') {
            # 服务器目标：出散件、走 git，跟"基线 / 补丁 / chunk"那套无关。
            # 所以这里既不建基线版本，也不算补丁差异。
            Write-Host '[服务器] 散件模式：不分包、不建基线、不打补丁（更新走 git）' -ForegroundColor DarkGray
        }
        elseif ($Mode -eq 'Base') {
            if ([string]::IsNullOrWhiteSpace($ReleaseVersion)) {
                throw "[参数缺失] Base 模式必须指定 -ReleaseVersion，例如：-ReleaseVersion 1.0"
            }

            # 基线目录：<工程>\Releases\<版本>\<平台>\
            # 服务器与客户端各占一个子目录，避免互相覆盖
            $ReleaseSub = if ($PackTarget -eq 'Server') { "$PlatformDir-Server" } else { $PlatformDir }
            # 基线根目录可能在界面上被换成新位置 → 先确保它存在（UAT 只负责往里写版本子目录）
            if (-not (Test-Path -LiteralPath $ReleaseRoot)) {
                New-Item -ItemType Directory -Path $ReleaseRoot -Force | Out-Null
                Write-Host "[基线] 新建基线根目录：$ReleaseRoot" -ForegroundColor DarkGray
            }
            $ReleaseDir = Join-Path $ReleaseRoot "$ReleaseVersion\$ReleaseSub"
            if (Test-Path -LiteralPath $ReleaseDir) {
                # 《版本更新》的语义就是"更新 / 重建基线"：同一个版本号再跑一次 = 重建它，旧的直接删。
                # 删之前只做一层兜底：解析出来的路径必须还在基线根目录里，防止路径拼错删到别处。
                $ResolvedRelease = (Resolve-Path -LiteralPath $ReleaseDir).Path
                if (-not $ResolvedRelease.StartsWith($ReleaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                    throw "[安全检查] 基线路径不在 $ReleaseRoot 里，拒绝删除：$ResolvedRelease"
                }
                Write-Host ''
                Write-Host "[基线] $ReleaseVersion（$PackTarget）已有基线 —— 按《版本更新》的语义重建，旧的直接删除" -ForegroundColor Yellow
                Write-Host "       $ResolvedRelease" -ForegroundColor DarkGray
                Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
            }

            $args.Add("-createreleaseversion=$ReleaseVersion")
            # 基线落到我们指定的根目录（不写这句就会落在 <工程>\Releases\）
            $args.Add("-createreleaseversionroot=$ReleaseRoot")

            # ---- 基线必须干净：给 cook 缓存做一次体检（方案 C）----
            # 脏了就自动全量（-clean），干净就照走增量 —— 不用人再想"这次该用哪种"。
            $ProjName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile)
            $Cache = Test-CookCacheDirty -Root $ProjectRoot -CookPlatform $PlatformDir -Name $ProjName
            if ($Cache.Dirty) {
                $AutoClean = $true
                Write-Host ("[缓存] {0} 个已烘焙文件里有 {1} 个在工程里已经不存在 -> 这次自动全量重 cook" -f $Cache.Cooked, $Cache.Orphans.Count) -ForegroundColor Yellow
                $Cache.Orphans | Select-Object -First 8 | ForEach-Object { Write-Host "       过期: $_" -ForegroundColor DarkGray }
                if ($Cache.Orphans.Count -gt 8) {
                    Write-Host ("       ...（还有 {0} 个）" -f ($Cache.Orphans.Count - 8)) -ForegroundColor DarkGray
                }
            }
            else {
                $Why = if ($Cache.Reason) { $Cache.Reason } else { ('{0} 个已烘焙文件，没有过期项' -f $Cache.Cooked) }
                Write-Host ("[缓存] {0} -> 走增量，省时间" -f $Why) -ForegroundColor Green
            }
        }
        elseif ($Mode -eq 'Patch') {
            if ([string]::IsNullOrWhiteSpace($ReleaseVersion)) {
                throw "[参数缺失] Patch 模式必须指定 -ReleaseVersion，指明基于哪个基线版本"
            }

            $ReleaseSub = if ($PackTarget -eq 'Server') { "$PlatformDir-Server" } else { $PlatformDir }
            $ReleaseDir = Join-Path $ReleaseRoot "$ReleaseVersion\$ReleaseSub"
            if (-not (Test-Path -LiteralPath $ReleaseDir)) {
                throw @"
[检查失败] 找不到基线版本 $ReleaseVersion（$PackTarget），无法生成补丁。

  期望位置：$ReleaseDir

  先用 Base 模式建立基线：
    .\Pack-CrossingVoid.ps1 -Mode Base -ReleaseVersion $ReleaseVersion -Target $PackTarget
"@
            }

            $args.Add('-generatepatch')
            $args.Add("-basedonreleaseversion=$ReleaseVersion")
            # 去同一个根目录找基线
            $args.Add("-basedonreleaseversionroot=$ReleaseRoot")
        }
    }

    # ---- Windows 客户端的运行库安装器（官方开关：UAT -prereqs）----
    # 只有带 -prereqs 时，引擎才会把 Engine\Extras\Redist\en-us 下的
    # vc_redist.x64.exe / vc_redist.arm64.exe 一起 stage 进产物
    # （见 WinPlatform.Automation.cs 的 if (Params.Prereqs)）；
    # GameInputRedist.msi 还要求项目 Config/DefaultEngine.ini 里写
    #   [GameInput]
    #   IncludeRedistFiles=True
    # 包根启动器启动前检查的就是这两个：机器上缺 GameInput 时它会弹"要现在安装吗"，
    # 而它要装的那个 msi 必须就在这个位置，否则弹窗会卡死在没桌面的会话里
    # （2026-09-19 服务器上就是这么卡的：切完版本端口一直不监听）。
    # 原来这个参数只加在 Patch 分支，所以 Base / 正式打包出来的产物都没有它们。
    # 只给 PC 客户端加：服务器在部署时手动装，安卓用不到。
    if ($Platform -eq 'Win64' -and $PackTarget -eq 'Client') {
        $args.Add('-prereqs')
    }

    # ---- 编译一律交给外部（Rider / 你手动），打包默认【不编译】----
    # 原因：UAT 的 -build 会顺带把「编辑器目标」也编一遍
    # （一次调用里带 CrossingVoidEditor + UnrealPak + ShaderCompileWorker + 目标），
    # 而编辑器开着时 Live Coding 持有互斥体 Global\LiveCoding_...UnrealEditor.exe，
    # UBT 会直接拒绝 —— 报 "Unable to build while Live Coding is active"。
    # 打包本身不需要编译：编辑器目标已经在跑，Shipping 目标由你在 Rider 里编好即可。
    # 真想让打包顺带编译，显式加 -ForceBuild。
    if (-not $ForceBuild) {
        while ($args.Contains('-build')) { $args.Remove('-build') }
        if (-not $args.Contains('-nocompile')) { $args.Add('-nocompile') }
    }

    # 注意：这里【不】给 UAT 传 -ubtargs=-NoLinkerDebugInfo。
    # 原因：-ForceBuild 时 UAT 那一轮会把编辑器目标也带上，ubtargs 是整轮生效的，
    # 会把 Rider 调试用的编辑器 pdb 一起干掉。pdb 的事只在下面「单独编 Shipping 目标」
    # 那一步处理 —— 那里只编 Shipping，不会连累编辑器。
    return @{ Args = $args; DoBuild = $DoBuild }
}

# ---- 组装本次要跑的任务列表 ----
# Target=Both 时跑两轮；否则一轮。
$RunList = [System.Collections.Generic.List[hashtable]]::new()

if ($Target -eq 'Both') {
    $RunList.Add(@{
        Name    = 'Client'
        OutDir  = Join-Path $ArchiveDir 'Client'
    })
    $RunList.Add(@{
        Name    = 'Server'
        OutDir  = Join-Path $ArchiveDir 'Server'
    })
}
else {
    $RunList.Add(@{
        Name    = $Target
        OutDir  = $ArchiveDir
    })
}

# ============================ 记录日志 ============================
$Stamp  = Get-Date -Format 'yyyyMMdd-HHmmss'
$LogDir = Join-Path $ProjectRoot 'Saved\PackLogs'
if (-not (Test-Path -LiteralPath $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

# ============================ 逐个任务执行 ============================
# ---- 选定地图：临时改写 DefaultGame.ini 的 +MapsToCook ----
# 为什么必须改写：UAT 的 -map= 是「并进」MapsToCook 集合，不是覆盖 ——
# 实测只传 -map=/Game/MapS/ConnectMap 时，ini 里那 4 张照样被 cook，
# 结果还是 11247 个包（≈全量）。所以要让"只勾选的地图"真正生效，
# 只能在打包期间把 ini 里那份清单换成勾选的那几张，打完立刻还原。
$GameIniPath = Join-Path $ProjectRoot 'Config\DefaultGame.ini'
# 备份一份原件；下面「地图清单」和「服务器散件」两处改写共用这一份，跑完统一还原
$GameIniBackupPath = Join-Path $ProjectRoot 'Config\DefaultGame.ini.bak-tmp'
$bGameIniOverridden = $false

if (-not $DryRun -and -not [string]::IsNullOrWhiteSpace($Maps)) {
    $PickedMaps = @($Maps -split '[,;]' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
    if ($PickedMaps.Count -gt 0) {
        Copy-Item -LiteralPath $GameIniPath -Destination $GameIniBackupPath -Force
        $bGameIniOverridden = $true

        $NewLines = [System.Collections.Generic.List[string]]::new()
        $bWritten = $false
        foreach ($Line in (Get-Content -LiteralPath $GameIniPath -Encoding UTF8)) {
            if ($Line -match '^\s*\+MapsToCook=') {
                if (-not $bWritten) {
                    foreach ($Map in $PickedMaps) { $NewLines.Add("+MapsToCook=(FilePath=`"$Map`")") }
                    $bWritten = $true
                }
                continue
            }
            $NewLines.Add($Line)
        }
        if (-not $bWritten) {
            $NewLines.Add('[/Script/UnrealEd.ProjectPackagingSettings]')
            foreach ($Map in $PickedMaps) { $NewLines.Add("+MapsToCook=(FilePath=`"$Map`")") }
        }
        [System.IO.File]::WriteAllLines($GameIniPath, $NewLines, (New-Object System.Text.UTF8Encoding($true)))
        Write-Host "[地图] 本次只 cook：$($PickedMaps -join '、')（已临时改写 DefaultGame.ini，打完自动还原）" -ForegroundColor Yellow

        # 留个标记文件：Write-Host 进不了 pack 日志（那是 UAT 的输出），
        # 有了它，事后翻 Saved\PackLogs 就能确认这次到底有没有改写过地图清单。
        $MapMarkerPath = Join-Path $ProjectRoot 'Saved\PackLogs\maps-override.txt'
        @(
            "时间 : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
            "本次只 cook : $($PickedMaps -join ', ')",
            "参数 -Maps : $Maps"
        ) | Set-Content -LiteralPath $MapMarkerPath -Encoding UTF8
    }
}

# ---- 服务器：临时把打包方式切成「散件」（关 ZenStore / IoStore / Pak）----
# 为什么必须改：DefaultGame.ini 现在是 UsePakFile=True + bUseIoStore=True + bUseZenStore=True。
# 这套组合下 cook 的结果【不进盘】—— 全流进 Zen 的 oplog，只有 -pak 那一步才会把它
# 物化成 Content\Paks\*.utoc/.ucas。而服务器这条路从来不打 pak（要出散件给 git 用），
# 于是 stage 出来是 0 个内容文件：2026-09-19 实测包内只有 24 个文件、连 Content 都没有，
# 服务器一启动就崩在 ICUInternationalization.cpp:161「ICU data directory was not discovered」。
# 这三项都是 UAT / 引擎直接从工程 ini 读的（ZenUtils.cs:707 读 bUseZenStore；
# CopyBuildToStagingDirectory.Automation.cs:3018 用 ShouldTreatAsFileServer 决定要不要复制散件），
# 命令行没有覆盖开关 —— 所以只能临时改写 ini，和上面的地图改写共用同一份备份。
# 详见 Tools\打包说明.md 的「服务器为什么出散件」一节。
$bLooseServerPack = $false
$bAlsoClientRun   = $false
foreach ($Run in $RunList) {
    if ($Run.Name -eq 'Server') { $bLooseServerPack = $true }
    if ($Run.Name -eq 'Client') { $bAlsoClientRun  = $true }
}
if ($bLooseServerPack -and $bAlsoClientRun) {
    # 服务器要关 ZenStore 才出得来散件，而客户端的分包（-RunChunkAssigner）恰恰要靠 Zen 的 oplog，
    # 两者没法在同一次打包里共存 —— 一次只打一侧。
    throw '[服务器散件] -Target Both 不行：服务器要关 ZenStore 才能出散件，客户端分包必须开 ZenStore。请分两次打。'
}

if ($bLooseServerPack) {
    if ($DryRun) {
        Write-Host '[服务器散件] (DryRun) 本应临时关掉 UsePakFile / bUseIoStore / bUseZenStore，并清掉旧的 Saved\Cooked\WindowsServer' -ForegroundColor Yellow
    }
    else {
        if (-not $bGameIniOverridden) {
            Copy-Item -LiteralPath $GameIniPath -Destination $GameIniBackupPath -Force
            $bGameIniOverridden = $true
        }

        $LooseKeys = @('UsePakFile', 'bUseIoStore', 'bUseZenStore')
        $NewLines = [System.Collections.Generic.List[string]]::new()
        $Seen = @{}
        foreach ($Line in (Get-Content -LiteralPath $GameIniPath -Encoding UTF8)) {
            $t = $Line.Trim()
            $Hit = $null
            foreach ($k in $LooseKeys) {
                if ($t -match ('^' + [regex]::Escape($k) + '\s*=')) { $Hit = $k; break }
            }
            if ($Hit) {
                if (-not $Seen.ContainsKey($Hit)) { $NewLines.Add("$Hit=False"); $Seen[$Hit] = $true }
                continue
            }
            $NewLines.Add($Line)
        }
        # 找不到就不动手：宁可报错，也不要在 Zen 还开着的情况下打出一个空包
        foreach ($k in $LooseKeys) {
            if (-not $Seen.ContainsKey($k)) { throw "[服务器散件] DefaultGame.ini 里找不到 $k= 这一行，拒绝继续" }
        }
        [System.IO.File]::WriteAllLines($GameIniPath, $NewLines, (New-Object System.Text.UTF8Encoding($true)))
        Write-Host '[服务器散件] 临时关掉 UsePakFile / bUseIoStore / bUseZenStore（打完自动还原）' -ForegroundColor Yellow

        # 顺手清掉服务器自己那份 cooked：上次是 Zen 管线（盘上只有 Metadata，没有 .uasset），
        # 增量 cook 会以为"已经 cook 过"从而跳过，这次照样出不来散件。
        # 只删 WindowsServer 这一份，客人端的 Saved\Cooked\Windows 不动（客户端不用重 cook）。
        $ServerCookedRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved\Cooked'))
        $ServerCooked = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved\Cooked\WindowsServer'))
        if (-not $ServerCooked.StartsWith($ServerCookedRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "[服务器散件] 拒绝删除不在 Saved\Cooked 下的路径：$ServerCooked"
        }
        if (Test-Path -LiteralPath $ServerCooked) {
            Remove-Item -LiteralPath $ServerCooked -Recurse -Force
            Write-Host "[服务器散件] 已清掉 $ServerCooked（管线换了，这一份必须全量重 cook）" -ForegroundColor Yellow
        }
    }
}

# ============================ 安卓版本号 ============================
# 只写 DefaultEngine.ini 里的 AndroidRuntimeSettings.VersionDisplayName（给玩家看的版本）。
# StoreVersion（versionCode）刻意不动。
if (-not [string]::IsNullOrWhiteSpace($PlayerVersion)) {
    $EngineIni = Join-Path $ProjectRoot 'Config\DefaultEngine.ini'
    Assert-Path $EngineIni 'Config\DefaultEngine.ini'

    $Lines = [System.Collections.Generic.List[string]]@(Get-Content -Encoding UTF8 $EngineIni)
    $SectionName = '[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]'
    $SectionIdx = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i].Trim() -eq $SectionName) { $SectionIdx = $i; break }
    }
    if ($SectionIdx -lt 0) {
        throw "[安卓版本号] DefaultEngine.ini 里找不到 $SectionName 段，拒绝瞎写"
    }

    $NewLine = "VersionDisplayName=$PlayerVersion"
    $Done = $false
    $Changed = $false
    for ($i = $SectionIdx + 1; $i -lt $Lines.Count; $i++) {
        $t = $Lines[$i].Trim()
        if ($t.StartsWith('[')) { break }                       # 已经到下一段
        if ($t.StartsWith('VersionDisplayName=')) {
            $Done = $true
            if ($Lines[$i].Trim() -eq $NewLine) {
                Write-Host "[安卓版本号] VersionDisplayName 已经是 $PlayerVersion，不动" -ForegroundColor DarkGray
            }
            else {
                $Old = $Lines[$i].Trim()
                $Lines[$i] = $NewLine
                $Changed = $true
                Write-Host "[安卓版本号] $Old  ->  $NewLine" -ForegroundColor Green
            }
            break
        }
    }
    if (-not $Done) {
        $Lines.Insert($SectionIdx + 1, $NewLine)
        $Changed = $true
        Write-Host "[安卓版本号] 新增 $NewLine" -ForegroundColor Green
    }
    if ($Changed) {
        # 保持原文件风格：无 BOM + CRLF
        [System.IO.File]::WriteAllText($EngineIni, (($Lines -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
    }
}

# ============================ 平台设置快照（防忘）============================
# 打包和平台设置"分得比较开"，容易打完才想起来没改。这里在开工前过一遍目，
# 同时落盘给分包报告和页面用（报告里会带上，方便以后查"这包是哪套设置打的"）。
$PlatformSettings = Get-PlatformSettingsSnapshot -Root $ProjectRoot
Show-PlatformSettings -S $PlatformSettings
# （不另外落盘：报告的 settings 段已经记了同一批值 + 图标状态，避免造第二份真相）

# ============================ Shipping 目标「要不要重编」============================
# 之前只看收据在不在（Binaries\Win64\X-Win64-Shipping.target）。
# 2026-09-19 就是这么翻车的：收据是 9/18 编的，DreamChunkDownloaderSubsystem.cpp 9/19 16:56 改过，
# 脚本看到收据在 → 跳过编译 → 打出来的客户端里是【一天前的旧二进制】→ 双击直接崩。
# 所以改成比时间戳：源码比 Shipping 二进制新就当"要重编"。
# 只会多编一次（编完二进制就比源码新了），不会每次都重编。
function Get-NewestProjectSource {
    $newest = $null
    $newestPath = ''
    foreach ($root in @((Join-Path $ProjectRoot 'Source'), (Join-Path $ProjectRoot 'Plugins'))) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $files = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                # 只看人写的源码：排除中间产物 / 二进制 / 第三方编译输出
                ($_.Extension -in '.cpp', '.h', '.cs', '.uplugin') -and
                ($_.FullName -notmatch '\\Intermediate\\|\\Binaries\\|\\Saved\\')
            }
        foreach ($f in $files) {
            if (($null -eq $newest) -or ($f.LastWriteTime -gt $newest)) {
                $newest = $f.LastWriteTime
                $newestPath = $f.FullName
            }
        }
    }
    return [pscustomobject]@{ Time = $newest; Path = $newestPath }
}

$AllOk = $true

foreach ($Run in $RunList) {
    $PackTarget = $Run.Name
    $OutDir     = $Run.OutDir

    # ---- 目标 Shipping 二进制缺失时：先【单独】把它编出来 ----
    # 关键点：不要交给 UAT 的 -build。UAT 那一次 UBT 调用会同时带上
    # CrossingVoidEditor + UnrealPak + ShaderCompileWorker + 本目标，
    # 编辑器开着时 Live Coding 持有 Global\LiveCoding_...UnrealEditor.exe，
    # UBT 检查到编辑器目标就被整批拒绝（Unable to build while Live Coding is active）。
    # 单独编本目标时，UBT 只检查本目标的互斥体 —— 服务器目标的互斥体编辑器并不持有，
    # 所以能编过。这样用户不用关编辑器，打包也不碰 Live Coding。
    if ($Platform -eq 'Win64') {
        $ReceiptName = if ($PackTarget -eq 'Server') { 'CrossingVoidServer' } else { 'CrossingVoid' }
        $ReceiptPath = Join-Path $ProjectRoot "Binaries\Win64\$ReceiptName-Win64-Shipping.target"
        $ShippingExe = Join-Path $ProjectRoot "Binaries\Win64\$ReceiptName-Win64-Shipping.exe"

        # 判定：收据不存在 → 编；收据在但源码比二进制新 → 也编
        $NeedCompile = $false
        $WhyCompile  = ''
        if (-not (Test-Path -LiteralPath $ReceiptPath)) {
            $NeedCompile = $true
            $WhyCompile  = '收据不存在（这个目标从没编过）'
        }
        else {
            $NewestSrc = Get-NewestProjectSource
            $BinaryStamp = if (Test-Path -LiteralPath $ShippingExe) {
                (Get-Item -LiteralPath $ShippingExe).LastWriteTime
            }
            else {
                (Get-Item -LiteralPath $ReceiptPath).LastWriteTime
            }
            if ($NewestSrc.Time -and ($NewestSrc.Time -gt $BinaryStamp)) {
                $NeedCompile = $true
                $WhyCompile  = ('源码比 Shipping 二进制新：{0}（源码 {1:yyyy-MM-dd HH:mm:ss} / 二进制 {2:yyyy-MM-dd HH:mm:ss}）' -f (Split-Path $NewestSrc.Path -Leaf), $NewestSrc.Time, $BinaryStamp)
            }
        }
        if ($ForceBuild) {
            # -ForceBuild 时编译交给 UAT 那一轮（Build-UatArgs 会保留 -build），这里就不再单独编一遍
            $NeedCompile = $false
        }

        if ($NeedCompile -and $DryRun) {
            # DryRun 的语义是"只打印不真干"（见 Tools\打包说明.md），所以这里只报"本应编译"
            Write-Host "[编译] (DryRun) 本应编译：$WhyCompile" -ForegroundColor Yellow
        }
        elseif ($NeedCompile) {
            Write-Host ''
            Write-Host "[编译] $WhyCompile —— 先单独编译 $ReceiptName 的 Shipping 目标（只编这个目标，不会被 Live Coding 挡住）" -ForegroundColor Yellow
            $BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
            $BuildArgs = @($ReceiptName, 'Win64', 'Shipping', "-Project=$ProjectFile", '-WaitMutex')
            # ---- 从源头不生成 pdb（两个开关必须一起给）----
            # 引擎在 Windows 上是两段式，只看一处会误判：
            #   ① 编译期 UEBuildPlatform.cs:1367
            #        GlobalCompileEnvironment.bCreateDebugInfo =
            #            Target.DebugInfo != None && ShouldCreateDebugInfo(Target)
            #      Shipping 下这一项【默认是 true】（ShouldCreateDebugInfo 返回
            #      !bOmitPCDebugInfoInDevelopment，而那个开关默认 false）
            #   ② 链接期 UEBuildWindows.cs:2181
            #        if (!bNoLinkerDebugInfo) { GlobalLinkEnvironment.bCreateDebugInfo = true; }
            #      这句是"不再强制打开"，不会关掉 ① 已经打开的
            # 链接期的值一开始是从编译期抄过去的（UEBuildPlatform.cs:1369），
            # 所以只给 -NoLinkerDebugInfo 等于什么都没做 —— 实测：2026-09-19 14:26 那次
            # 照样产出了 1544.6 MB 的 CrossingVoidServer-Win64-Shipping.pdb。
            # 正确组合：-NoDebugInfo 让 ① 变 false，-NoLinkerDebugInfo 挡住 ② 再打开。
            # 代价：-NoDebugInfo 改了编译参数 → 该目标要全量重编一次（编完中间物也会变小）。
            # 加 -KeepSymbols 时两个都不给，回到"产出来留档"。详见 Tools\打包说明.md。
            if (-not $KeepSymbols) { $BuildArgs += @('-NoDebugInfo', '-NoLinkerDebugInfo') }
            & $BuildBat @BuildArgs
            if ($LASTEXITCODE -ne 0) {
                # 编译没过就别往下走了：Shipping 二进制是旧的/不存在的，cook 会白跑十几分钟，
                # 最后必然死在 stage「Missing receipt」上。这里直接跳过这一侧，脚本末尾的正常
                # 收尾（还原 DefaultGame.ini）照常执行，整体以失败退出。
                Write-Host "[编译] 失败，退出码 $LASTEXITCODE —— 跳过本目标的打包（先修编译错误）" -ForegroundColor Red
                $AllOk = $false
                continue
            }
            else {
                Write-Host "[编译] 完成" -ForegroundColor Green
            }
        }
        else {
            Write-Host ("[编译] {0} Shipping 目标是最新的，跳过编译" -f $ReceiptName) -ForegroundColor DarkGray
        }
    }
    else {
        # ---- 安卓：同理，Shipping 目标缺失时单独编它 ----
        # 为什么必须单独编：打包一律带 -nocompile，UAT 不会替我们编译；
        # 而第一次打安卓时 Binaries\Android 下根本没有产物，不先编出来必然失败。
        $ReceiptName = if ($PackTarget -eq 'Server') { 'CrossingVoidServer' } else { 'CrossingVoid' }
        $AndroidBin = Join-Path $ProjectRoot 'Binaries\Android'
        $HasAndroidShipping = $false
        if (Test-Path -LiteralPath $AndroidBin) {
            $HasAndroidShipping = [bool](Get-ChildItem -LiteralPath $AndroidBin -Filter "$ReceiptName-Android*Shipping*.target" -ErrorAction SilentlyContinue |
                Select-Object -First 1)
        }
        # 有的版本收据名不带平台后缀，兜一层
        if (-not $HasAndroidShipping -and (Test-Path -LiteralPath $AndroidBin)) {
            $HasAndroidShipping = [bool](Get-ChildItem -LiteralPath $AndroidBin -Filter '*Shipping*.target' -ErrorAction SilentlyContinue | Select-Object -First 1)
        }
        if (-not $HasAndroidShipping) {
            Write-Host ''
            Write-Host "[编译] $ReceiptName 的 Android Shipping 目标还没编过 —— 先单独编译它（第一次会比较久：arm64 + 引擎模块）" -ForegroundColor Yellow
            $BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
            & $BuildBat $ReceiptName Android Shipping "-Project=$ProjectFile" -WaitMutex
            if ($LASTEXITCODE -ne 0) {
                # 同上：编译没过就别去 cook 了，省十几分钟
                Write-Host "[编译] 失败，退出码 $LASTEXITCODE —— 跳过本目标的打包（先修编译错误）" -ForegroundColor Red
                $AllOk = $false
                continue
            }
            else {
                Write-Host "[编译] 完成" -ForegroundColor Green
            }
        }
    }

    # ---- 服务器：归档目录只留「这一版真正有的东西」----
    # UAT 归档只复制、从不删（见 Clear-ArchiveExceptSaved 的注释），所以删掉的资源会一直留着。
    # 放在【编译之后】是有意的：编译失败就别动上一版的发布目录。
    # 铁律：任何 Saved 一律不碰 —— 那是服务器的玩家数据。
    if ($Platform -eq 'Win64' -and $PackTarget -eq 'Server') {
        # UAT 会自己在归档根下加平台子目录（DeploymentContext.cs:515），别重复加
        $ServerArchiveDir = $Run.OutDir
        if ((Split-Path -Leaf $ServerArchiveDir) -ne 'WindowsServer') {
            $ServerArchiveDir = Join-Path $ServerArchiveDir 'WindowsServer'
        }
        if ($DryRun) {
            Write-Host "[服务器散件] (DryRun) 本应清空 $ServerArchiveDir（Saved 除外）" -ForegroundColor Yellow
        }
        else {
            $Cleaned = Clear-ArchiveExceptSaved -Path $ServerArchiveDir
            Write-Host ("[服务器散件] 归档目录已清：删 {0} 项，保留 {1} 处 Saved —— {2}" -f $Cleaned.Removed, $Cleaned.KeptSaved, $ServerArchiveDir) -ForegroundColor Yellow
        }
    }

    $built    = Build-UatArgs -PackTarget $PackTarget -OutDir $OutDir
    $uatArgs  = $built.Args
    $ArgString = ($uatArgs | ForEach-Object { "`"$_`"" }) -join ' '

    if ($DryRun) {
        Write-Host ''
        Write-Host "[DryRun] $PackTarget" -ForegroundColor Cyan
        Write-Host "  $RunUAT $ArgString" -ForegroundColor Green
        continue
    }

    $LogFile = Join-Path $LogDir "pack-$Mode-$Platform-$PackTarget-$Stamp.log"

    Write-Host ''
    Write-Host '----------------------------------------' -ForegroundColor Cyan
    Write-Host " [$PackTarget] 开始" -ForegroundColor Cyan
    Write-Host '----------------------------------------' -ForegroundColor Cyan
    Write-Host "[命令行]" -ForegroundColor Yellow
    Write-Host "  $RunUAT $ArgString" -ForegroundColor Green
    Write-Host "[日志] $LogFile" -ForegroundColor DarkGray
    Write-Host ''

    # 日志头 —— 以后查「到底哪个参数生效了」直接看这里
    @(
        "===== CrossingVoid Pack =====",
        "Time     : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
        "Mode     : $Mode",
        "Target   : $PackTarget",
        "Platform : $Platform",
        "Engine   : $EngineRoot",
        "Project  : $ProjectFile",
        "OutDir   : $OutDir",
        "",
        "===== Command Line =====",
        "`"$RunUAT`" $ArgString",
        "",
        "===== UAT Output ====="
    ) | Set-Content -LiteralPath $LogFile -Encoding UTF8

    $StartTime = Get-Date

    & $RunUAT @uatArgs 2>&1 | ForEach-Object {
        Write-Host $_
        Add-Content -LiteralPath $LogFile -Value $_ -Encoding UTF8
    }

    $ExitCode = $LASTEXITCODE
    $Elapsed  = (Get-Date) - $StartTime

    Write-Host ''
    if ($ExitCode -eq 0) {
        Write-Host " [$PackTarget] 打包成功   用时 $($Elapsed.ToString('mm\:ss'))" -ForegroundColor Green
    }
    else {
        Write-Host " [$PackTarget] 打包失败   退出码 $ExitCode" -ForegroundColor Red
        $AllOk = $false
    }

    # ============================ 产物盘点 ============================
    if ($ExitCode -eq 0) {
        # 服务器产物的目录名带 Server 后缀，客户端不带
        $SubName = if ($PackTarget -eq 'Server') { 'CrossingVoidServer' } else { 'CrossingVoid' }
        $PaksDir = Join-Path $OutDir "$PlatformDir\$SubName\Content\Paks"

        # ---- 《快速验证》留个显眼的标记 ----
        # 它不打 pak，产物是散件，本来就没法分发；再放一个文件名就写明白的 txt，
        # 免得几个月后（或别人）拿这份东西去发。
        if ($Mode -eq 'Quick' -and $PackTarget -ne 'Server') {
            $MarkPath = Join-Path $OutDir '_快速验证产物_不能用于发布.txt'
            $MarkText = @(
                '这是《快速验证》的产物。',
                '',
                '它只用来确认"分块归属对不对"，没有 pak 容器 —— 玩家和启动器都用不了，不能发布。',
                '要发布请用：',
                '  · 《版本更新》 → 完整包 + 建立基线',
                '  · 《常规补丁》 → 基于基线的 _P.pak',
                '',
                ('生成时间：{0}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')),
                ('本次：Mode={0} Platform={1} Target={2}' -f $Mode, $PlatformDir, $PackTarget)
            ) -join "`r`n"
            [System.IO.File]::WriteAllText($MarkPath, $MarkText + "`r`n", (New-Object System.Text.UTF8Encoding($true)))
            Write-Host ''
            Write-Host '  [快速验证] 本次不打 pak（只看归属），产物不能用于发布' -ForegroundColor Yellow
            Write-Host "             标记文件：$MarkPath" -ForegroundColor DarkGray
        }

        # ---- 剔除 PDB（客户端 + 服务器都做）----
        # 两个 pdb 都是 1.5 GB 量级：
        #   客户端：CrossingVoid-Win64-Shipping.pdb（约 1.6 GB）
        #   服务器：CrossingVoidServer-Win64-Shipping.pdb（约 1.5 GB）
        # 正常情况下已经由 -NoLinkerDebugInfo 从源头掐掉了，这里兜底的是
        # 「复用旧 exe / 旧 pdb」的情况（打包默认 -nocompile，不会重新链接）。
        # 之前只有服务器分支做了这一步，客户端归档目录里那个 pdb 会跟着包上 CDN。
        if (-not $KeepSymbols) {
            $PdbRoots = Get-ChildItem -LiteralPath $OutDir -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "$PlatformDir*" } | Select-Object -ExpandProperty FullName
            $LeftPdbs = @()
            foreach ($PdbRoot in $PdbRoots) {
                $LeftPdbs += Get-ChildItem -LiteralPath $PdbRoot -Recurse -File -Filter '*.pdb' -ErrorAction SilentlyContinue
            }
            if ($LeftPdbs.Count -gt 0) {
                $FreedMB = [math]::Round((($LeftPdbs | Measure-Object Length -Sum).Sum) / 1MB, 1)
                $LeftPdbs | Remove-Item -Force -ErrorAction SilentlyContinue
                Write-Host ('  {0,-14} {1,10} MB 已剔除（{2} 个 pdb）' -f '省下', $FreedMB, $LeftPdbs.Count) -ForegroundColor Green
            }
        }
        else {
            Write-Host '  （-KeepSymbols：保留 pdb，也不加 -NoLinkerDebugInfo）' -ForegroundColor DarkGray
        }

        # ---- PC 客户端：自检运行库安装器有没有被 stage 进来 ----
        # 这两份不是我们手工拷的，靠官方开关：
        #   · UAT 的 -prereqs（见上面拼参数那里）→ vc_redist.x64.exe / vc_redist.arm64.exe
        #   · 项目 Config/DefaultEngine.ini 里的 [GameInput] IncludeRedistFiles=True
        #     → GameInputRedist.msi
        # 包根启动器启动前检查的就是它们：机器上缺 GameInput 时会弹"要现在安装吗"，
        # 而要装的 msi 必须就在这个位置，否则弹窗会卡死在没桌面的会话里。
        # 这里只做体检：没 stage 进来就明确报出来，别等玩家/服务器上才发现。
        if ($Platform -eq 'Win64' -and $PackTarget -eq 'Client') {
            $RedistDst = Join-Path $OutDir "$PlatformDir\Engine\Extras\Redist\en-us"
            $RedistWant = @('GameInputRedist.msi', 'vc_redist.x64.exe')
            $RedistMiss = @($RedistWant | Where-Object { -not (Test-Path -LiteralPath (Join-Path $RedistDst $_)) })

            Write-Host ''
            if ($RedistMiss.Count -eq 0) {
                $RedistMB = [math]::Round((($RedistWant | ForEach-Object { (Get-Item -LiteralPath (Join-Path $RedistDst $_)).Length } | Measure-Object -Sum).Sum / 1MB), 1)
                Write-Host ('  {0,-14} {1,10} MB  {2}' -f '运行库安装器', $RedistMB, ($RedistWant -join ' + ')) -ForegroundColor Green
                Write-Host "                 $RedistDst" -ForegroundColor DarkGray
            }
            else {
                Write-Host ('  [警告] 产物里缺运行库安装器：{0}' -f ($RedistMiss -join '、')) -ForegroundColor Yellow
                Write-Host "         期望位置：$RedistDst" -ForegroundColor DarkGray
                Write-Host '         检查：① 这次打包有没有带 -prereqs（Win64 客户端会自动带）' -ForegroundColor DarkGray
                Write-Host '               ② Config/DefaultEngine.ini 里有没有 [GameInput] IncludeRedistFiles=True' -ForegroundColor DarkGray
            }
        }

        if ($PackTarget -eq 'Server') {
            # 服务器目标是【散件】：没有 Paks 目录，而且目录名带 Server 后缀（WindowsServer）。
            # 以前这里按 Windows\CrossingVoidServer\Content\Paks 找，必然找不到 → 假警报。
            $ServerRoot = Get-ChildItem -LiteralPath $OutDir -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "$PlatformDir*" } | Select-Object -First 1
            if ($ServerRoot) {
                $Loose = Get-ChildItem -LiteralPath $ServerRoot.FullName -Recurse -File -ErrorAction SilentlyContinue
                $LooseMB = [math]::Round((($Loose | Measure-Object Length -Sum).Sum) / 1MB, 1)
                Write-Host ''
                Write-Host "[产物] $($ServerRoot.FullName)（服务器散件）" -ForegroundColor Yellow
                Write-Host ''
                Write-Host ('  {0,-14} {1,10}' -f '散件文件数', $Loose.Count)
                Write-Host ('  {0,-14} {1,10} MB' -f '总大小', $LooseMB) -ForegroundColor White
                Write-Host '  （服务器不分包、没有 pakchunk，这份目录就是给 git 用的）' -ForegroundColor DarkGray
            }
            else {
                Write-Host ''
                Write-Host "[警告] 没找到服务器产物目录：$OutDir 下没有 $PlatformDir* 目录" -ForegroundColor Yellow
            }
        }
        elseif (Test-Path -LiteralPath $PaksDir) {
            # IoStore 之后一个 chunk = 三个文件：.pak(壳) + .ucas(正文) + .utoc(目录)
            $AllFiles = Get-ChildItem -LiteralPath $PaksDir -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Extension -in '.pak', '.ucas', '.utoc' }

            Write-Host ''
            Write-Host "[产物] $PaksDir" -ForegroundColor Yellow
            Write-Host ''

            if ($AllFiles.Count -eq 0) {
                if ($Mode -eq 'Quick') {
                    # 快速验证本来就不打 pak —— 这是预期，不是失败
                    Write-Host '  （快速验证不打 pak：看分块归属请用下面的清单/报告，不是这里）' -ForegroundColor DarkGray
                }
                else {
                    Write-Host '  !! 没有产出任何 .pak / .ucas / .utoc —— chunk 打包没生效' -ForegroundColor Red
                }
            }
            else {
                function Get-ContainerBytes {
                    param($Files, [string]$Ext)
                    $sum = ($Files | Where-Object { $_.Extension -eq $Ext } |
                            Measure-Object -Property Length -Sum).Sum
                    if ($null -eq $sum) { return [double]0 }
                    return [double]$sum
                }

                $Containers = $AllFiles |
                    Group-Object { ($_.BaseName -replace "-$PlatformDir$", '') } |
                    Sort-Object Name

                Write-Host ('  {0,-26} {1,9} {2,11} {3,9} {4,11}' -f '容器', 'pak(MB)', 'ucas(MB)', 'utoc(KB)', '合计(MB)') -ForegroundColor DarkGray
                Write-Host ('  ' + ('-' * 74)) -ForegroundColor DarkGray

                $TotalMB = 0.0
                $ChunkCount = 0
                $PatchCount = 0

                foreach ($c in $Containers) {
                    $pakMB  = (Get-ContainerBytes $c.Group '.pak')  / 1MB
                    $ucasMB = (Get-ContainerBytes $c.Group '.ucas') / 1MB
                    $utocKB = (Get-ContainerBytes $c.Group '.utoc') / 1KB
                    $sumMB  = $pakMB + $ucasMB + ($utocKB / 1KB)

                    $TotalMB += $sumMB
                    if ($c.Name -ne 'global') { $ChunkCount++ }
                    if ($c.Name -match '_P$') { $PatchCount++ }

                    $Color = 'Gray'
                    if ($c.Name -match '_P$') { $Color = 'Magenta' }
                    elseif ($c.Name -eq 'global') { $Color = 'DarkGray' }

                    Write-Host ('  {0,-26} {1,9:N2} {2,11:N2} {3,9:N0} {4,11:N2}' -f $c.Name, $pakMB, $ucasMB, $utocKB, $sumMB) -ForegroundColor $Color
                }

                Write-Host ('  ' + ('-' * 74)) -ForegroundColor DarkGray
                Write-Host ('  {0,-26} {1,9} {2,11} {3,9} {4,11:N2}' -f "共 $ChunkCount 个 chunk 容器", '', '', '', $TotalMB) -ForegroundColor White
                Write-Host ''

                if ($PatchCount -gt 0) {
                    Write-Host "  [OK] 发现 $PatchCount 个 patch 容器（*_P）—— 热更新链路通" -ForegroundColor Green
                }
                elseif ($Mode -eq 'Patch') {
                    Write-Host '  [!!] Patch 模式但没产出 *_P 容器 —— 检查 ReleaseVersion 是否和 Base 一致' -ForegroundColor Yellow
                }
                else {
                    Write-Host "  [OK] 产出 $ChunkCount 个 chunk 容器 —— chunk 打包生效" -ForegroundColor Green
                }

                $ManifestOut = Join-Path $LogDir "paklist-$Mode-$Platform-$PackTarget-$Stamp.txt"
                $AllFiles | Sort-Object Name | ForEach-Object {
                    "{0}`t{1}`t{2}" -f $_.Name, $_.Length, [math]::Round($_.Length / 1MB, 2)
                } | Set-Content -LiteralPath $ManifestOut -Encoding UTF8
                Write-Host ''
                Write-Host "  [清单] $ManifestOut" -ForegroundColor DarkGray
            }

            # ---- 归档 staging 清单，然后生成分包报告 ----
            # 注意：UAT 的 PrePak_<CookPlatform>_UFSFiles.txt 是**临时文件**，
            # 下一次打包会清掉，所以必须在这里立刻归档 + 立刻算报告。
            $StagedList = Join-Path $EngineRoot "Engine\Programs\AutomationTool\Saved\Logs\PrePak_${PlatformDir}_UFSFiles.txt"
            $ArchiveSub = Join-Path $LogDir 'ManifestHistory'
            if (-not (Test-Path -LiteralPath $ArchiveSub)) {
                New-Item -ItemType Directory -Path $ArchiveSub -Force | Out-Null
            }
            if (Test-Path -LiteralPath $StagedList) {
                $VerTag = if ([string]::IsNullOrWhiteSpace($ReleaseVersion)) { 'nover' } else { $ReleaseVersion }
                $ArchName = "StagedFiles_$PlatformDir`_$PackTarget`_$Mode`_$VerTag`_$Stamp.txt"
                Copy-Item -LiteralPath $StagedList -Destination (Join-Path $ArchiveSub $ArchName) -Force
                $LineCount = (Get-Content -LiteralPath $StagedList | Measure-Object -Line).Lines
                Write-Host ''
                Write-Host "  [归档] $ArchName（$LineCount 行）" -ForegroundColor DarkGray
            }
            else {
                Write-Host ''
                Write-Host "  [警告] 没找到 staged 清单：$StagedList" -ForegroundColor Yellow
                Write-Host '         分包报告会因此缺少"未分类清单"，检查引擎路径。' -ForegroundColor Yellow
            }

            # ---- 分包报告（页面/AX 都读它）----
            if ($PackTarget -ne 'Server') {
                $ReportScript = Join-Path $PSScriptRoot 'Build-ChunkReport.ps1'
                if (Test-Path -LiteralPath $ReportScript) {
                    try {
                        & $ReportScript -ProjectRoot $ProjectRoot -ArchiveDir $OutDir `
                            -Platform $Platform -Target $PackTarget -Mode $Mode -PackLogPath $LogFile
                    }
                    catch {
                        Write-Host "  [警告] 分包报告生成失败：$($_.Exception.Message)" -ForegroundColor Yellow
                        Write-Host '         （打包本身是成功的，报告只影响页面上的统计）' -ForegroundColor Yellow
                    }
                }
            }
        }
        else {
            Write-Host ''
            Write-Host "[警告] 没找到产物目录：$PaksDir" -ForegroundColor Yellow
            Write-Host '       检查 -archivedirectory 与平台目录名。' -ForegroundColor Yellow
        }
    }
}

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
if ($AllOk) {
    Write-Host ' 全部打包成功' -ForegroundColor Green
} else {
    Write-Host ' 存在失败的打包任务' -ForegroundColor Red
}
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

# 打包结束：把临时改写过的 DefaultGame.ini 还原回去（绝不留一个被改过的工程配置）
if ($bGameIniOverridden -and (Test-Path -LiteralPath $GameIniBackupPath)) {
    Move-Item -LiteralPath $GameIniBackupPath -Destination $GameIniPath -Force
    Write-Host '[配置] DefaultGame.ini 已还原（地图清单 / 服务器散件开关）' -ForegroundColor DarkGray
    Remove-Item -LiteralPath (Join-Path $ProjectRoot 'Saved\PackLogs\maps-override.txt') -Force -ErrorAction SilentlyContinue   # 还原了就把标记清掉
}

if ($AllOk) { exit 0 } else { exit 1 }

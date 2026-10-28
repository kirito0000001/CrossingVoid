
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
    Quick   不建基线版本，只验证 chunk 生不生效（不打 pak，快很多）

    三个模式都必须经过 stage —— pak 是从 staging 目录生成的，
    少了 -stage 会报 "Only staged builds can be paked"。
    Shipping 目标缺失或过期时，脚本先单独编译它，再让 UAT 带 -nocompile 跑。
    全量 cook 很慢，零境实测约 43 分钟；cook 缓存会复用于后续打包。

.PARAMETER Target
    打包目标类型：
      Client  客户端（玩家跑的包）
      Server  专用服务器（出散件、走 git，不分包）
      Both    参数保留，但当前会直接报错 —— 见下面

    对应关系：
      Client → -clientconfig=Shipping
      Server → -serverconfig=Shipping -server
      Both   → 服务器要关 ZenStore 才出得来散件，客户端分包必须开 ZenStore，
               两者不能在同一次打包里共存，所以脚本直接拒绝；请分两次打。

    服务器目标依赖 Source\<工程名>Server.Target.cs（Type=TargetType.Server）；没有就退回 <工程名>Server，让 UAT 报 target 不存在。

.PARAMETER Platform
    Win64 / Android
    注意：Server 目标只在 Win64 上有意义；Android 不支持专用服务器。

.PARAMETER Configuration
    Shipping（正式包，默认）/ Development（测试包）。
    Development 下 PrintString、日志、控制台可用；只支持 Win64。
    产物目录和 Shipping **共用同一条**（上传脚本认死了那条路径，不能挪），
    只有基线和报告按配置分开：<ArchiveDir>\Release 与 <ArchiveDir>\Release-Development。
    第一次用 Patch 前先用《版本更新》建 Development 基线。

.PARAMETER ReleaseVersion
    Base 模式必填（如 1.0）。基线会落在 <ReleaseRoot>\<版本>\<平台>\。
    Patch 模式也必填 —— 必须指明「基于哪个基线版本」算差异，引擎强制要求
    （只给 -generatepatch 不给 -basedonreleaseversion 会直接抛异常）。

.PARAMETER ArchiveDir
    产物输出根目录。默认 F:\DaBaoV\Crossingvoid

.PARAMETER ReleaseRoot
    基线（Releases）根目录。留空 = <ArchiveDir>\Release（推荐，基线永远跟着产物目录）。
    UAT 的"建基线"(-createreleaseversionroot) 和"算补丁"(-basedonreleaseversionroot) 都用它，
    所以读和写必须是同一个值 —— 插件「运行设置（额外）→ 基线位置」会自动派生并只读显示。

.PARAMETER Cook
    强制全量重新 cook。第一次打包或改了大量资产时用。

.PARAMETER SkipBuild
    允许在不需要编译时附加 -nocompile（省时间）。默认已是这个行为。

.PARAMETER ForceBuild
    强制本次编译 C++（即使 Shipping 目标已存在）。

.PARAMETER 统一补丁（2026-10-27 起）
    《常规补丁》（-Mode Patch，客户端 / Win64 / Shipping）现在会**自己判断**这次改了什么：
      · 只有 C++ 变化（没动资产/配置/Shader）→ 编译 + 把变化的运行时二进制镜像到归档，跳过 cook/pak（快）；
      · 有资产/配置/Shader 变化 → 走完整流程：编译 + cook + -generatepatch + pak（代码改动也随 stage 进归档）；
      · 什么都没有 → 什么都不做。
    以前面板上的「代码补丁」按钮（-CodeOnly）已经删掉，这一条路统一由《常规补丁》接管。

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

    # 打包配置：Shipping 正式包 / Development 测试包（PrintString、日志、控制台可见）。
    # Development 只支持 Win64；产物目录和 Shipping 共用，基线和报告按配置分开
    # （<ArchiveDir>\Release / <ArchiveDir>\Release-Development）。
    [ValidateSet('Shipping', 'Development')]
    [string]$Configuration = 'Shipping',

    [string]$ReleaseVersion = '',

    # 给玩家看的版本号（写进 Config\DefaultEngine.ini 的
    # AndroidRuntimeSettings.VersionDisplayName）。只影响 APK 上显示的版本；
    # StoreVersion（versionCode）刻意不动。
    [string]$PlayerVersion = '',

    [string]$ArchiveDir = 'F:\DaBaoV\Crossingvoid',

    # 基线（Releases）根目录；留空 = <ArchiveDir>\Release。
    # 建基线和算补丁都用这一个值，别只改一处。
    [string]$ReleaseRoot = '',

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
    # 详见 Docs\打包说明.md 的「pdb 与符号文件」一节。
    [switch]$KeepSymbols,

    # 只清"能再生"的缓存，清完就退出（不打包）。页面上的《清除缓存》按钮走的就是这条。
    # 清的三样：Saved\StagedBuilds（UAT 组装产物）、Saved\Cooked（cooked 数据）、
    #            Saved\Shaders（shader 缓存）。
    # 绝不碰：Intermediate\Build（编译产物，重编很贵）、Content、归档目录、Releases 基线。
    # 配 -DryRun 只列不删。
    [switch]$ClearCache,

    # 只读查询：报告 / 产物 / 基线三件事，给页面上的状态行用。查完就退出。
    [switch]$Status,

    # 跳过分块指派（不走 oplog）：分块完全交给 Config\DefaultPakFileRules.ini 的目录规则。
    # 排障用 —— OplogChunkAssigner 读不到 oplog（报「Zen server is not running」）时可以试这条路。
    # 风险：规则覆盖不到的资产会落进 chunk 0，打完必须比对 AllChunksInfo.csv / 报告里的分块。
    [switch]$SkipChunkAssigner,

    # 让 cooker 用 IPv4（127.0.0.1）连 Zen，而不是 [::1]。
    # 2026-10-27 排查：UAT 的 OplogChunkAssigner 用 cook 写进 ue.projectstore 的 host:port，
    # 本机那条 [::1]:8558 请求会失败（Zen 本身健康），导致 staging 读 oplog 报"Zen server is not running" 。
    # 默认：让 cooker 用 IPv4（127.0.0.1）连 Zen。2026-10-27 实测本机走 [::1] 的 oplog 请求必失败
    # （UAT 报 Zen server is not running，实际 Zen 健康），强制 IPv4 就正常。
    # 真要用 IPv6 时加 -ZenIPv6。
    [switch]$ZenIPv6,

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

# 中途抛错也要把临时改写的 DefaultGame.ini 还原回去（地图清单 / 服务器散件开关共用一份备份）。
# 正常收尾在脚本末尾还原；这个 trap 管的是"没走到收尾就失败"的情况 ——
# 否则工程配置会停在「只 cook 勾选的那几张地图 / Zen 被关掉」的临时状态。
trap {
    if ($bGameIniOverridden -and $GameIniBackupPath -and (Test-Path -LiteralPath $GameIniBackupPath)) {
        try {
            Move-Item -LiteralPath $GameIniBackupPath -Destination $GameIniPath -Force
            if ($ProjectRoot) {
                Remove-Item -LiteralPath (Join-Path $ProjectRoot 'Saved\PackLogs\maps-override.txt') -Force -ErrorAction SilentlyContinue
            }
            Write-Host '[配置] 出错，DefaultGame.ini 已还原' -ForegroundColor Yellow
        }
        catch {
            Write-Host ("[配置] 还原 DefaultGame.ini 失败：{0}" -f $_.Exception.Message) -ForegroundColor Red
        }
    }
    Write-Host ("[错误] {0}" -f $_.Exception.Message) -ForegroundColor Red
    throw
}

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

# ---- 工程文件：不再写死 CrossingVoid.uproject ----
# 工程根下通常只有一个 .uproject；有多个时优先用「和工程目录同名」的那个。
$ProjectCandidates = @(Get-ChildItem -LiteralPath $ProjectRoot -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
if ($ProjectCandidates.Count -eq 0) {
    throw "[检查失败] $ProjectRoot 下找不到 .uproject"
}
if ($ProjectCandidates.Count -eq 1) {
    $ProjectFile = $ProjectCandidates[0].FullName
}
else {
    $PreferredProject = $ProjectCandidates |
        Where-Object { $_.BaseName -ieq (Split-Path $ProjectRoot -Leaf) } |
        Select-Object -First 1
    if (-not $PreferredProject) {
        throw ("[检查失败] 工程根下有多个 .uproject，无法确定用哪个：`n  {0}" -f (($ProjectCandidates | ForEach-Object { $_.FullName }) -join "`n  "))
    }
    $ProjectFile = $PreferredProject.FullName
}
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile)

# ---- target 名：不再写死 CrossingVoid / CrossingVoidServer ----
# 读 Source\*.Target.cs 里的 Type = TargetType.Game / Server：
#   · 客户端优先用和工程同名的 Game 目标，没有就用第一个 Game 目标；
#   · 服务器优先用 <工程名>Server，没有就用第一个 Server 目标；
#   · 一个 Server 目标都没有时退回 <工程名>Server —— 真去打服务器会让 UAT 报
#     「target 不存在」，比在这里猜一个能编的目标更清楚。
function Get-TargetBaseName {
    param($File)
    return ($File.Name -replace '\.Target\.cs$', '')
}

$TargetFiles = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Source') -Filter '*.Target.cs' -File -ErrorAction SilentlyContinue)
$GameTargets = @($TargetFiles | Where-Object {
        (Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue) -match 'Type\s*=\s*TargetType\.Game'
    })
$ServerTargets = @($TargetFiles | Where-Object {
        (Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue) -match 'Type\s*=\s*TargetType\.Server'
    })

$PreferredGame = $GameTargets | Where-Object { (Get-TargetBaseName $_) -ieq $ProjectName } | Select-Object -First 1
if (-not $PreferredGame) { $PreferredGame = $GameTargets | Select-Object -First 1 }
$ClientTargetName = if ($PreferredGame) { Get-TargetBaseName $PreferredGame } else { $ProjectName }

$PreferredServer = $ServerTargets | Where-Object { (Get-TargetBaseName $_) -ieq "$($ProjectName)Server" } | Select-Object -First 1
if (-not $PreferredServer) { $PreferredServer = $ServerTargets | Select-Object -First 1 }
$ServerTargetName = if ($PreferredServer) { Get-TargetBaseName $PreferredServer } else { "$($ProjectName)Server" }

$EngineRoot  = 'D:\UnrealEngine-5.8.2'
$RunUAT      = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

# 基线（Releases）根目录：留空就跟产物目录走。
# 这样换工程/换输出目录时，基线不会还留在上一个工程的路径下。
if ([string]::IsNullOrWhiteSpace($ArchiveDir)) {
    $ArchiveDir = 'F:\DaBaoV\Crossingvoid'
}
if ($Configuration -eq 'Development' -and $Platform -ne 'Win64') {
    throw '[配置] Development 只支持 Win64；Android 的测试包请用 Shipping。'
}
# 产物目录：Shipping 和 Development 【共用同一个】，不做任何按配置的子目录 ——
# 发布/上传脚本认死了这条路径，产物一挪它就传错东西（2026-10-21 定的）。
# 但基线反过来必须分开：混在一起时，Development 的《版本更新》会覆盖正式基线
# （同一个 <ReleaseRoot>\<版本>\<平台>），之后正式线算补丁就拿了开发版当基准。
if ([string]::IsNullOrWhiteSpace($ReleaseRoot)) {
    $ReleaseSub = if ($Configuration -eq 'Development') { 'Release-Development' } else { 'Release' }
    $ReleaseRoot = Join-Path $ArchiveDir $ReleaseSub
}

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

# 往 ini 的某个段里写一条 key=value：有就改、没有就插在该段第一行之后；段不存在就补一个。
# 返回 $true 表示内容真的变了（没变就不落盘，避免把 mtime 搅动、让 UBT 以为配置改了）。
# 落盘时保留原文件风格（BOM / 无 BOM 各自保持）—— 工程里的 ini 是 CRLF + BOM，
# 写成别的样子 UBT 会把 makefile 判成"配置已修改"，又多一次全量重编。
function Set-IniKey {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Section,
        [Parameter(Mandatory)][string]$Key,
        [AllowEmptyString()][string]$Value
    )

    # BOM 必须自己看字节：[System.IO.File]::ReadAllText 会自动把 BOM 吃掉，
    # 靠 $Raw[0] 判断永远是 false —— 那样写回时就把 BOM 抹了（2026-10-27 单测踩到）。
    $Bytes = [System.IO.File]::ReadAllBytes($Path)
    $HasBom = ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF)
    $Raw = [System.Text.Encoding]::UTF8.GetString($Bytes)
    if ($HasBom) { $Raw = $Raw.Substring(1) }

    $Lines = [System.Collections.Generic.List[string]]::new()
    foreach ($L in ($Raw -split "`r?`n")) { $Lines.Add($L) }

    $NewLine  = "$Key=$Value"
    $KeyRegex = '^\s*' + [regex]::Escape($Key) + '\s*='
    $SectionIdx = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i].Trim() -eq $Section) { $SectionIdx = $i; break }
    }

    if ($SectionIdx -lt 0) {
        # 段不存在：补一个空行 + 段头 + 这条 key
        while ($Lines.Count -gt 0 -and $Lines[$Lines.Count - 1].Trim() -eq '') { $Lines.RemoveAt($Lines.Count - 1) }
        if ($Lines.Count -gt 0) { $Lines.Add('') }
        $Lines.Add($Section)
        $Lines.Add($NewLine)
    }
    else {
        $Done = $false
        for ($i = $SectionIdx + 1; $i -lt $Lines.Count; $i++) {
            $t = $Lines[$i].Trim()
            if ($t.StartsWith('[')) { break }            # 到下一段了，说明本段没有这个 key
            if ($t -match $KeyRegex) {
                if ($t -eq $NewLine) { return $false }   # 已经是目标值，不落盘
                $Lines[$i] = $NewLine
                $Done = $true
                break
            }
        }
        if (-not $Done) { $Lines.Insert($SectionIdx + 1, $NewLine) }
    }

    $Text = ($Lines -join "`r`n")
    if (-not $Text.EndsWith("`r`n")) { $Text += "`r`n" }
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($HasBom)))
    return $true
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
Write-Host " $ProjectName  打包" -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ""

Assert-Path $ProjectFile '工程文件 .uproject'
Assert-Path $RunUAT      'RunUAT.bat（检查 EngineRoot 是否正确）'

# ============================ 统一补丁：只改代码时的那条快路 ============================
# 以前是面板上一个独立的「代码补丁」按钮（-CodeOnly）。2026-10-27 起并进《常规补丁》：
#   · 由主循环里的 Invoke-CodePatch -Auto 调用：先扫变化，只有 C++ 变化才走这里（编译 + 镜像二进制、不 cook）；
#   · 一旦发现资产/配置/Shader 变化就返回 $false，交回主流程走完整的 编译 + cook + pak。
# 不再提供"强制绕过"的开关 —— 变化判断交给扫描，人不用选。
# 设计文档：Docs\2026-10-20-纯代码补丁模式设计.md
function Invoke-CodePatch {
    param([switch]$Auto)

    # 内部保留这个变量名，逻辑里原有的"是否可以强制"判断都走它；统一入口下永远是 false。
    $ForceCodePatch = $false

    if ($ClearCache -or $Status) {
        throw '[统一补丁] 代码补丁速路不能和 -ClearCache / -Status 同时使用。'
    }
    if ($Target -eq 'Both') {
        throw '[统一补丁] -Target Both 不支持；客户端和服务器请分两次。'
    }
    if ($Platform -ne 'Win64') {
        throw '[代码补丁] 目前只支持 Win64；Android 的代码在 APK 的 .so 里，需要重打 APK。'
    }
    if ($Configuration -ne 'Shipping') {
        throw '[代码补丁] 只支持 Shipping；Development 测试包请用《开始打包》/《快速验证》。'
    }

    # ---- 工具函数 ----
    function Get-CodePatchCutoff {
        param(
            [string]$Root,
            [string]$Platform,
            [string]$Target,
            [string]$ArchiveDir,
            [string]$ArchivePlatformDir
        )
        $reportPath = Join-Path $Root "Saved\PackLogs\ChunkReports\$Platform-$Target\latest.json"
        if (Test-Path -LiteralPath $reportPath) {
            try {
                $report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
                if (-not [string]::IsNullOrWhiteSpace($report.generatedAt)) {
                    return [datetime]::Parse($report.generatedAt)
                }
            }
            catch { }
        }
        $manifestPath = Join-Path $ArchiveDir "$ArchivePlatformDir\Manifest_UFSFiles_$Platform.txt"
        if (Test-Path -LiteralPath $manifestPath) {
            return (Get-Item -LiteralPath $manifestPath).LastWriteTime
        }
        return $null
    }

    function Get-CodePatchChangeSet {
        param([string]$Root, [datetime]$Cutoff)

        $result = [ordered]@{
            Code        = [System.Collections.Generic.List[string]]::new()
            Header      = [System.Collections.Generic.List[string]]::new()
            BuildConfig = [System.Collections.Generic.List[string]]::new()
            Content     = [System.Collections.Generic.List[string]]::new()
            Config      = [System.Collections.Generic.List[string]]::new()
            Shader      = [System.Collections.Generic.List[string]]::new()
        }

        $scanRoots = @(
            (Join-Path $Root 'Source'),
            (Join-Path $Root 'Plugins'),
            (Join-Path $Root 'Content'),
            (Join-Path $Root 'Config'),
            (Join-Path $Root 'Shaders')
        )
        foreach ($scanRoot in $scanRoots) {
            if (-not (Test-Path -LiteralPath $scanRoot)) { continue }
            Get-ChildItem -LiteralPath $scanRoot -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
                $path = $_.FullName
                if ($path -match '\\Intermediate\\|\\Binaries\\|\\Saved\\|\\DerivedDataCache\\|\\.git\\|\\.vs\\|\\.idea\\') { return }
                if ($_.Name -ieq 'DefaultPakFileRules.ini') { return }   # 打包脚本每次自动生成，不算人工配置变更
                if ($_.LastWriteTime -le $Cutoff) { return }

                $ext = $_.Extension.ToLowerInvariant()
                if ($ext -eq '.uproject' -or $ext -eq '.uplugin' -or $ext -eq '.cs') {
                    $result.BuildConfig.Add($path)
                }
                elseif ($path -match '\\Content\\') {
                    $result.Content.Add($path)
                }
                elseif ($path -match '\\Config\\' -and $ext -eq '.ini') {
                    $result.Config.Add($path)
                }
                elseif ($ext -eq '.usf' -or $ext -eq '.ush') {
                    $result.Shader.Add($path)
                }
                elseif ($ext -eq '.cpp') {
                    $result.Code.Add($path)
                }
                elseif ($ext -eq '.h') {
                    $result.Header.Add($path)
                }
            }
        }

        # 根目录的 .uproject 不在 scanRoots 里
        Get-ChildItem -LiteralPath $Root -Filter '*.uproject' -File -ErrorAction SilentlyContinue | ForEach-Object {
            if ($_.LastWriteTime -gt $Cutoff) {
                $result.BuildConfig.Add($_.FullName)
            }
        }

        return $result
    }

    function Show-CodePatchChanges {
        param($Changes, [string]$Root)
        $total = 0
        $categories = @(
            @{ Name = '代码';     Items = $Changes.Code },
            @{ Name = '头文件';   Items = $Changes.Header },
            @{ Name = '构建配置'; Items = $Changes.BuildConfig },
            @{ Name = '内容';     Items = $Changes.Content },
            @{ Name = '配置';     Items = $Changes.Config },
            @{ Name = 'Shader';   Items = $Changes.Shader }
        )
        foreach ($category in $categories) {
            $items = @($category.Items)
            if ($items.Count -eq 0) { continue }
            $total += $items.Count
            Write-Host ("  {0,-8} {1}" -f $category.Name, $items.Count) -ForegroundColor Yellow
            $items | Select-Object -First 5 | ForEach-Object {
                $fullPath = if ($_ -is [System.IO.FileInfo]) { $_.FullName } else { [string]$_ }
                $rel = $fullPath.Substring($Root.Length).TrimStart('\')
                Write-Host ("      {0}" -f $rel) -ForegroundColor DarkGray
            }
            if ($items.Count -gt 5) {
                Write-Host ("      ...（还有 {0} 个）" -f ($items.Count - 5)) -ForegroundColor DarkGray
            }
        }
        if ($total -eq 0) {
            Write-Host '  （基准之后没有文件变化）' -ForegroundColor DarkGray
        }
    }

    function Get-ModuleDllNames {
        param([string]$ManifestPath)
        $names = [System.Collections.Generic.List[string]]::new()
        try {
            $manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($null -ne $manifest.Modules) {
                foreach ($prop in $manifest.Modules.PSObject.Properties) {
                    if ($prop.Value) { $names.Add([string]$prop.Value) }
                }
            }
        }
        catch { }
        return $names
    }

    function Get-ManifestBuildId {
        param([string]$ManifestPath)
        try {
            $manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($manifest.BuildId) { return [string]$manifest.BuildId }
        }
        catch { }
        return ''
    }

    function Copy-CodePatchFile {
        param(
            [string]$SourcePath,
            [string]$DestPath,
            [string]$ArchiveRelativePath,
            $ChangedFiles,
            $ChangedPaths,
            [bool]$DryRun
        )
        if (-not (Test-Path -LiteralPath $SourcePath)) { return }
        if ($ChangedPaths.ContainsKey($ArchiveRelativePath)) { return }

        $sourceItem = Get-Item -LiteralPath $SourcePath
        $sourceHash = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if (Test-Path -LiteralPath $DestPath) {
            $destHash = (Get-FileHash -LiteralPath $DestPath -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($sourceHash -eq $destHash) { return }
        }

        $ChangedPaths[$ArchiveRelativePath] = $true
        $ChangedFiles.Add([pscustomobject]@{
            path      = $ArchiveRelativePath.Replace('\', '/')
            sizeBytes = $sourceItem.Length
            sha256    = $sourceHash
            action    = 'copy'
        })

        if ($DryRun) { return }

        $destDir = Split-Path -Parent $DestPath
        if (-not (Test-Path -LiteralPath $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }
        $tmpPath = "$DestPath.new"
        Copy-Item -LiteralPath $SourcePath -Destination $tmpPath -Force
        Move-Item -LiteralPath $tmpPath -Destination $DestPath -Force

        $newHash = (Get-FileHash -LiteralPath $DestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($newHash -ne $sourceHash) {
            throw "[代码补丁] 复制后校验失败：$DestPath"
        }
    }

    # ---- 参数与路径 ----
    $TargetName = if ($Target -eq 'Server') { $ServerTargetName } else { $ClientTargetName }
    $ArchivePlatformDir = if ($Target -eq 'Server') { 'WindowsServer' } else { 'Windows' }
    $ArchiveRootFull = [System.IO.Path]::GetFullPath($ArchiveDir)
    $ArchiveProjectDir = Join-Path $ArchiveRootFull "$ArchivePlatformDir\$ProjectName"
    $ArchiveEngineDir = Join-Path $ArchiveRootFull "$ArchivePlatformDir\Engine"
    $SourceProjectBin = Join-Path $ProjectRoot 'Binaries\Win64'
    $SourceEngineBin = Join-Path $EngineRoot 'Engine\Binaries\Win64'
    $DestProjectBin = Join-Path $ArchiveProjectDir 'Binaries\Win64'
    $DestEngineBin = Join-Path $ArchiveEngineDir 'Binaries\Win64'

    Write-Host ''
    Write-Host '========================================' -ForegroundColor Cyan
    Write-Host ' 纯代码补丁（只编译 Shipping）' -ForegroundColor Cyan
    Write-Host '========================================' -ForegroundColor Cyan
    Write-Host ("  工程     : {0}" -f $ProjectFile)
    Write-Host ("  目标     : {0} / {1}" -f $Platform, $Target)
    Write-Host ("  归档     : {0}" -f $ArchiveProjectDir)
    Write-Host ''

    # ---- 1. 变化扫描 ----
    $Cutoff = Get-CodePatchCutoff -Root $ProjectRoot -Platform $Platform -Target $Target -ArchiveDir $ArchiveRootFull -ArchivePlatformDir $ArchivePlatformDir
    if ($null -eq $Cutoff) {
        throw '[代码补丁] 找不到完整打包基准（报告 latest.json / Manifest_UFSFiles）。先跑一次完整打包。'
    }
    Write-Host ("[变化扫描] 基准：{0:yyyy-MM-dd HH:mm}" -f $Cutoff) -ForegroundColor DarkGray
    $Changes = Get-CodePatchChangeSet -Root $ProjectRoot -Cutoff $Cutoff
    Show-CodePatchChanges -Changes $Changes -Root $ProjectRoot

    # 引擎构建信息比基准新：可能是引擎升级/重编，cooked 数据未必兼容。
    $engineVersionPath = Join-Path $EngineRoot 'Engine\Build\Build.version'
    $engineVersionNewer = (Test-Path -LiteralPath $engineVersionPath) -and ((Get-Item -LiteralPath $engineVersionPath).LastWriteTime -gt $Cutoff)

    $blocking = $Changes.Content.Count + $Changes.Config.Count + $Changes.Shader.Count + $Changes.BuildConfig.Count
    $codeChanged = ($Changes.Code.Count + $Changes.Header.Count) -gt 0

    # 统一入口的"什么都没变"分支：代码和内容都没动 → 不用打补丁（别白等一次 cook）。
    if ($Auto -and $blocking -eq 0 -and -not $engineVersionNewer -and -not $codeChanged) {
        Write-Host ''
        Write-Host '[自动判断] 没有检测到代码或内容变化 —— 这次不需要打补丁。' -ForegroundColor Green
        return 'none'
    }

    if (($blocking -gt 0 -or $engineVersionNewer) -and -not $ForceCodePatch) {
        if ($Auto) {
            # 统一入口的判断结果：这次不只是改了代码 → 交回主流程走完整补丁（编译 + cook + pak）。
            Write-Host ''
            Write-Host ("[自动判断] 检测到内容变化（资产 {0} / 配置 {1} / Shader {2} / 构建配置 {3}）→ 走完整补丁：编译 + cook + pak" -f `
                $Changes.Content.Count, $Changes.Config.Count, $Changes.Shader.Count, $Changes.BuildConfig.Count) -ForegroundColor Yellow
            if ($engineVersionNewer) {
                Write-Host '           引擎 Build.version 比基准新：必须完整 cook。' -ForegroundColor Yellow
            }
            return 'full'
        }
        Write-Host ''
        Write-Host '[统一补丁] 检测到资产 / 配置 / Shader / 构建配置变化，或引擎 Build.version 比基准新，纯代码补丁不安全。' -ForegroundColor Red
        if ($engineVersionNewer) {
            Write-Host '           引擎 Build.version 比基准新：如果是引擎升级/重编，必须完整 cook。' -ForegroundColor Yellow
        }
        Write-Host '           请改用《版本更新》；确认不需要 cook 时才需要完整流程。' -ForegroundColor Yellow
        return 'full'
    }
    if ($Changes.Header.Count -gt 0) {
        Write-Host '[统一补丁] 警告：头文件有改动；若涉及 UPROPERTY/UFUNCTION/UCLASS 布局，必须完整 cook。' -ForegroundColor Yellow
    }

    # ---- 2. 归档 / 进程检查 ----
    if (-not (Test-Path -LiteralPath $ArchiveProjectDir)) {
        throw ("[代码补丁] 归档目录不存在：{0}`n  先跑一次完整打包（或把面板上的输出目录指对）。" -f $ArchiveProjectDir)
    }
    $processNames = @($TargetName, "$TargetName-Win64-Shipping")
    if ($Target -ne 'Server') { $processNames += $ProjectName }
    $running = @(Get-Process -Name $processNames -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
        $runningNames = ($running | Select-Object -ExpandProperty ProcessName -Unique) -join ', '
        throw ("[代码补丁] 游戏/服务器还在运行：{0}。先关掉再编。" -f $runningNames)
    }

    # ---- 3. 编译 Shipping ----
    $BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    Assert-Path $BuildBat 'Build.bat'
    $BuildArgs = @($TargetName, 'Win64', 'Shipping', "-Project=$ProjectFile", '-WaitMutex')
    if (-not $KeepSymbols) { $BuildArgs += @('-NoDebugInfo', '-NoLinkerDebugInfo') }
    $BuildCommandText = "& '$BuildBat' " + (($BuildArgs | ForEach-Object { "`"$_`"" }) -join ' ')
    Write-Host '[编译] ' -NoNewline -ForegroundColor Yellow
    Write-Host $BuildCommandText -ForegroundColor Green

    $codePatchLog = ''
    if ($DryRun) {
        Write-Host '[DryRun] 不编译、不覆盖；下面只列出会镜像的二进制。' -ForegroundColor Yellow
    }
    else {
        $logDir = Join-Path $ProjectRoot 'Saved\PackLogs'
        if (-not (Test-Path -LiteralPath $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
        $codePatchLog = Join-Path $logDir ("code-patch-{0}-{1}-{2}.log" -f $Platform, $Target, (Get-Date -Format 'yyyyMMdd-HHmmss'))
        Write-Host ("[日志] {0}" -f $codePatchLog) -ForegroundColor DarkGray
        & $BuildBat @BuildArgs 2>&1 | Tee-Object -FilePath $codePatchLog
        if ($LASTEXITCODE -ne 0) {
            throw "[代码补丁] Shipping 编译失败，退出码 $LASTEXITCODE。归档没有改动。日志：$codePatchLog"
        }
        Write-Host '[编译] 完成' -ForegroundColor Green
    }

    # ---- 4. 判断 monolithic / modular，并检查归档布局 ----
    if (-not (Test-Path -LiteralPath $SourceProjectBin)) {
        throw ("[代码补丁] 找不到构建输出：{0}" -f $SourceProjectBin)
    }
    # 目标对应的 app 名：modular + Shared 时是 UnrealGame / UnrealServer；
    # monolithic 时是 target 名（FantasyProject / FantasyProjectServer）。
    $AppExeName = if ($Target -eq 'Server') { 'UnrealServer-Win64-Shipping.exe' } else { 'UnrealGame-Win64-Shipping.exe' }
    $AppModuleName = $AppExeName -replace '\.exe$', '.modules'
    $AppVersionName = $AppExeName -replace '\.exe$', '.version'

    # 不能只扫 *Shipping*.modules：同一目录里可能同时有客户端和服务器的 manifest。
    $SourceProjectModules = @()
    foreach ($candidate in @(
        (Join-Path $SourceProjectBin "$TargetName-Win64-Shipping.modules"),
        (Join-Path $SourceProjectBin $AppModuleName)
    )) {
        if (Test-Path -LiteralPath $candidate) {
            $SourceProjectModules = @(Get-Item -LiteralPath $candidate)
            break
        }
    }
    $engineModules = @()
    foreach ($candidate in @(
        (Join-Path $SourceEngineBin $AppModuleName),
        (Join-Path $SourceEngineBin "$TargetName-Win64-Shipping.modules")
    )) {
        if (Test-Path -LiteralPath $candidate) {
            $engineModules = @(Get-Item -LiteralPath $candidate)
            break
        }
    }

    $DestProjectModules = @()
    foreach ($candidate in @(
        (Join-Path $DestProjectBin "$TargetName-Win64-Shipping.modules"),
        (Join-Path $DestProjectBin $AppModuleName)
    )) {
        if (Test-Path -LiteralPath $candidate) {
            $DestProjectModules = @(Get-Item -LiteralPath $candidate)
            break
        }
    }
    $SourceIsModular = ($SourceProjectModules.Count -gt 0)
    $ArchiveIsModular = ($DestProjectModules.Count -gt 0)
    if ($SourceIsModular -ne $ArchiveIsModular) {
        throw ("[代码补丁] 构建布局与归档布局不一致（source modular={0}, archive modular={1}）。先完整打包一次。" -f $SourceIsModular, $ArchiveIsModular)
    }
    Write-Host ("[布局] {0}" -f $(if ($SourceIsModular) { 'modular（游戏模块 DLL）' } else { 'monolithic（单个 Shipping exe）' })) -ForegroundColor DarkGray

    # ---- 5. 镜像运行时二进制 ----
    $ChangedFiles = [System.Collections.Generic.List[object]]::new()
    $ChangedPaths = @{}
    $copyArgs = @{
        ChangedFiles = $ChangedFiles
        ChangedPaths = $ChangedPaths
        DryRun       = [bool]$DryRun
    }

    # 项目侧的 Shipping exe（modular 时可能是 UnrealGame-...，monolithic 时是 <Target>-...）
    # 不能只写 *Shipping*.exe：同一目录里会同时有客户端和服务器两个 exe。
    $ProjectExeCandidates = @(
        (Join-Path $SourceProjectBin "$TargetName-Win64-Shipping.exe"),
        (Join-Path $SourceProjectBin $AppExeName)
    )
    foreach ($exePath in $ProjectExeCandidates) {
        if (Test-Path -LiteralPath $exePath) {
            Copy-CodePatchFile -SourcePath $exePath -DestPath (Join-Path $DestProjectBin (Split-Path $exePath -Leaf)) -ArchiveRelativePath "$ProjectName/Binaries/Win64/$(Split-Path $exePath -Leaf)" @copyArgs
            break
        }
    }

    # modular：按项目 .modules 清单只拷游戏/插件模块 DLL
    if ($SourceIsModular) {
        foreach ($moduleManifest in $SourceProjectModules) {
            Copy-CodePatchFile -SourcePath $moduleManifest.FullName -DestPath (Join-Path $DestProjectBin $moduleManifest.Name) -ArchiveRelativePath "$ProjectName/Binaries/Win64/$($moduleManifest.Name)" @copyArgs
            foreach ($dllName in @(Get-ModuleDllNames -ManifestPath $moduleManifest.FullName)) {
                $sourceDll = Join-Path $SourceProjectBin $dllName
                if (Test-Path -LiteralPath $sourceDll) {
                    Copy-CodePatchFile -SourcePath $sourceDll -DestPath (Join-Path $DestProjectBin $dllName) -ArchiveRelativePath "$ProjectName/Binaries/Win64/$dllName" @copyArgs
                }
            }
        }
    }

    # 项目 Binaries 下的运行时 DLL（tbb / D3D12 / DML 等）；排除 editor DLL 和模块 DLL（模块由 manifest 精确控制）
    Get-ChildItem -LiteralPath $SourceProjectBin -Recurse -File -Filter '*.dll' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notlike 'UnrealEditor-*' -and (-not $SourceIsModular -or ($_.Name -notlike 'UnrealGame-*' -and $_.Name -notlike 'UnrealServer-*')) } |
        ForEach-Object {
            $rel = $_.FullName.Substring($SourceProjectBin.Length).TrimStart('\')
            Copy-CodePatchFile -SourcePath $_.FullName -DestPath (Join-Path $DestProjectBin $rel) -ArchiveRelativePath "$ProjectName/Binaries/Win64/$($rel.Replace('\', '/'))" @copyArgs
        }

    # 项目侧 .version（模块 manifest 已在上面按 target/app 精确拷贝）
    foreach ($candidate in @(
        (Join-Path $SourceProjectBin "$TargetName-Win64-Shipping.version"),
        (Join-Path $SourceProjectBin $AppVersionName)
    )) {
        if (Test-Path -LiteralPath $candidate) {
            Copy-CodePatchFile -SourcePath $candidate -DestPath (Join-Path $DestProjectBin (Split-Path $candidate -Leaf)) -ArchiveRelativePath "$ProjectName/Binaries/Win64/$(Split-Path $candidate -Leaf)" @copyArgs
            break
        }
    }

    # 引擎侧：只有 modular 才镜像；monolithic 的引擎代码已经在 exe 里
    if ($SourceIsModular -and (Test-Path -LiteralPath $SourceEngineBin)) {
        foreach ($moduleManifest in $engineModules) {
            Copy-CodePatchFile -SourcePath $moduleManifest.FullName -DestPath (Join-Path $DestEngineBin $moduleManifest.Name) -ArchiveRelativePath "Engine/Binaries/Win64/$($moduleManifest.Name)" @copyArgs
            foreach ($dllName in @(Get-ModuleDllNames -ManifestPath $moduleManifest.FullName)) {
                $sourceDll = Join-Path $SourceEngineBin $dllName
                if (Test-Path -LiteralPath $sourceDll) {
                    Copy-CodePatchFile -SourcePath $sourceDll -DestPath (Join-Path $DestEngineBin $dllName) -ArchiveRelativePath "Engine/Binaries/Win64/$dllName" @copyArgs
                }
            }
        }
        # 引擎侧的 exe / .version（modular + Shared 时在 Engine/Binaries/Win64）
        foreach ($candidate in @(
            (Join-Path $SourceEngineBin $AppExeName),
            (Join-Path $SourceEngineBin "$TargetName-Win64-Shipping.exe")
        )) {
            if (Test-Path -LiteralPath $candidate) {
                Copy-CodePatchFile -SourcePath $candidate -DestPath (Join-Path $DestEngineBin (Split-Path $candidate -Leaf)) -ArchiveRelativePath "Engine/Binaries/Win64/$(Split-Path $candidate -Leaf)" @copyArgs
                break
            }
        }
        foreach ($candidate in @(
            (Join-Path $SourceEngineBin $AppVersionName),
            (Join-Path $SourceEngineBin "$TargetName-Win64-Shipping.version")
        )) {
            if (Test-Path -LiteralPath $candidate) {
                Copy-CodePatchFile -SourcePath $candidate -DestPath (Join-Path $DestEngineBin (Split-Path $candidate -Leaf)) -ArchiveRelativePath "Engine/Binaries/Win64/$(Split-Path $candidate -Leaf)" @copyArgs
                break
            }
        }
    }

    # ---- 6. 报告 ----
    $buildId = ''
    if ($SourceIsModular -and $SourceProjectModules.Count -gt 0) {
        $buildId = Get-ManifestBuildId -ManifestPath $SourceProjectModules[0].FullName
    }
    elseif ($SourceIsModular -and $engineModules.Count -gt 0) {
        $buildId = Get-ManifestBuildId -ManifestPath $engineModules[0].FullName
    }
    $engineChanged = @($ChangedFiles | Where-Object { $_.path -like 'Engine/*' }).Count -gt 0
    $warnings = @()
    if ($Changes.Header.Count -gt 0) {
        $warnings += '头文件有改动；若涉及 UPROPERTY/UFUNCTION/UCLASS 布局，必须完整 cook'
    }
    if ($engineVersionNewer) {
        $warnings += '引擎 Build.version 比基准新；如果是引擎升级/重编，必须完整 cook'
    }

    $reportDir = Join-Path $ProjectRoot 'Saved\PackLogs\CodePatch'
    if (-not (Test-Path -LiteralPath $reportDir)) {
        New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    }
    $reportPath = Join-Path $reportDir 'last-code-patch.json'
    $report = [ordered]@{
        schemaVersion = 1
        generatedAt   = (Get-Date -Format 'yyyy-MM-ddTHH:mm:sszzz')
        project       = $ProjectName
        platform      = $Platform
        target        = $Target
        layout        = $(if ($SourceIsModular) { 'modular' } else { 'monolithic' })
        buildId       = $buildId
        engineChanged = $engineChanged
        engineVersionNewer = $engineVersionNewer
        buildLog      = $codePatchLog
        archiveRoot   = $ArchiveRootFull
        files         = @($ChangedFiles)
        changedSinceLastFullPack = [ordered]@{
            code        = $Changes.Code.Count
            headers     = $Changes.Header.Count
            content     = $Changes.Content.Count
            config      = $Changes.Config.Count
            shaders     = $Changes.Shader.Count
            buildConfig = $Changes.BuildConfig.Count
        }
        forced        = [bool]$ForceCodePatch
        warnings      = $warnings
    }
    if (-not $DryRun) {
        [System.IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 8), (New-Object System.Text.UTF8Encoding($false)))
    }

    # ---- 7. 汇总 ----
    Write-Host ''
    if ($ChangedFiles.Count -eq 0) {
        Write-Host '[代码补丁] 没有检测到运行时二进制变化（可能已经是最新）。' -ForegroundColor Green
    }
    else {
        Write-Host ("[代码补丁] 已镜像 {0} 个文件：" -f $ChangedFiles.Count) -ForegroundColor Green
        $ChangedFiles | Select-Object -First 10 | ForEach-Object {
            Write-Host ("  {0}  {1:N0} B" -f $_.path, $_.sizeBytes) -ForegroundColor Gray
        }
        if ($ChangedFiles.Count -gt 10) {
            Write-Host ("  ...（还有 {0} 个）" -f ($ChangedFiles.Count - 10)) -ForegroundColor DarkGray
        }
    }
    if ($DryRun) {
        Write-Host ('  [DryRun] 不写报告，也不覆盖归档：{0}' -f $reportPath) -ForegroundColor DarkGray
    }
    else {
        Write-Host ("  报告：{0}" -f $reportPath) -ForegroundColor DarkGray
    }
    if (-not [string]::IsNullOrWhiteSpace($codePatchLog)) {
        Write-Host ("  编译日志：{0}" -f $codePatchLog) -ForegroundColor DarkGray
    }
    Write-Host '  下一步：重新生成启动器 files[] / fragments；本次没有 cook/pak。' -ForegroundColor Yellow
    # 三态返回：'code' = 只改代码，已处理完；'full' = 交回主流程走 cook+pak；'none' = 什么都没变
    return 'code'
}

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
    # 《快速验证》的报告是单独一份；Development 也单独一份，避免覆盖 Shipping 报告。
    $RepSuffix = ''
    if ($Configuration -ne 'Shipping') { $RepSuffix += "-$Configuration" }
    if ($Mode -eq 'Quick') { $RepSuffix += '-Quick' }
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
        [string]$OutDir,
        [string]$TargetName       # 本次要打的 target 名（从 Source\*.Target.cs 推出来）
    )

    $args = [System.Collections.Generic.List[string]]::new()
    $AutoClean = ($Configuration -ne 'Shipping')   # Development 强制全量 cook，避免和 Shipping 的 cooked 数据混

    $args.Add('BuildCookRun')
    $args.Add("-project=$ProjectFile")
    $args.Add('-noP4')
    $args.Add('-utf8output')
    $args.Add("-platform=$Platform")

    # UAT 靠这个参数认游戏 target。不传的话 -cook 会直接抛
    # "Game target not found. Game target is required with -cook or -cookonthefly"
    # （-client / -server 只说明要打哪一侧，不提供 target 名）。
    $args.Add("-target=$TargetName")

    # ---- 选定的地图（空则交给 ini 的 +MapsToCook）----
    if (-not [string]::IsNullOrWhiteSpace($Maps)) {
        foreach ($m in ($Maps -split '[,;]')) {
            $mapName = $m.Trim()
            if ($mapName -ne '') { $args.Add("-map=$mapName") }
        }
    }

    # ---- 客户端 / 服务器 的关键分歧点 ----
    if ($PackTarget -eq 'Server') {
        $args.Add("-serverconfig=$Configuration")
        $args.Add('-server')
        # 服务器不需要客户端资源以外的很多东西，但 chunk 划分逻辑一致
        # （默认按 clientconfig 算，服务器侧沿用同一套 -manifests 规则）
    }
    else {
        $args.Add("-clientconfig=$Configuration")
        # 这里**不要**加 -client：工程的 CrossingVoid.Target.cs 声明的是 Game 类型，
        # 而 -client 要求 Client 类型的目标，两者混用 UAT 会直接抛
        # "Cannot mix game and client targets"。
        # 对 Game 目标来说，-clientconfig=$Configuration 就是对应配置的客户端打包。
    }

    # ---- 检查目标是否存在 ----
    # stage 需要 <工程>\Binaries\Win64\<TargetName>-Win64-<Configuration>.target
    $NeedBuild = $true
    if ($Platform -eq 'Win64') {
        $ShippingTarget = Join-Path $ProjectRoot "Binaries\Win64\$TargetName-Win64-$Configuration.target"
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
        if (-not $ZenIPv6) { $args.Add('-NoZenAutoLaunch') }
        if ($PackTarget -eq 'Server') {
            # 服务器【出散件】：不打包、不分 chunk。
            # 服务端更新走 git（文件级差异比 chunk 细），打成 pak 反而没法用；
            # 所以这里既不加 -manifests / -pak，也不加 -RunChunkAssigner。
        }
        else {
            $args.Add('-manifests')
            # oplog 分包链路。不传这个参数 → OplogChunkAssigner 不运行 →
            # pak 阶段退回读磁盘上的旧 ChunkManifest（历史产物永远停在 0/2/3）
            if (-not $SkipChunkAssigner) { $args.Add('-RunChunkAssigner') }
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
        if (-not $ZenIPv6) { $args.Add('-NoZenAutoLaunch') }
        if ($Cook -or $AutoClean) { $args.Add('-clean') }
        if ($PackTarget -ne 'Server') {
            $args.Add('-manifests')
            if (-not $SkipChunkAssigner) { $args.Add('-RunChunkAssigner') }
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

            # 基线目录：<ReleaseRoot>\<版本>\<平台>\
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
            $Cache = Test-CookCacheDirty -Root $ProjectRoot -CookPlatform $PlatformDir -Name $ProjectName
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
# ============================ 工程版本号（内容版本 / 网络兼容版本分开）============================
# 面板上的「玩家版本」= **内容版本**，打包时写进两个地方：
#   · Config\DefaultEngine.ini → AndroidRuntimeSettings.VersionDisplayName（安卓包里显示的版本，后面那段写）
#   · Config\DefaultGame.ini   → [CrossingVersion] GameVersion（全平台；游戏运行时一行就能读）
#
# **刻意不写 GeneralProjectSettings.ProjectVersion**：那个字段会被引擎拿去算网络兼容性
#   UnrealEngine.cpp:2539-2541  // "Make sure networking checksum has access to project version"
#   → FNetworkVersion::SetProjectVersion(ProjectSettings.ProjectVersion)
#   → GetLocalNetworkVersion() = CRC32("工程名 + 项目版本 + 引擎 NetCL + 网络协议版本")
#   → IsNetworkCompatible() 是【严格相等】→ 不一致 = 在 connect 阶段被引擎直接拒连（连不上，不是提示）
# 2026-10-27 踩过：内容版本写进 ProjectVersion，就等于"每次发版全网旧包都连不上"。
# 所以 ProjectVersion 只当"网络协议版本"的杠杆，只在真的改了网络协议/包结构时手动抬一次。
# **必须写在下面「临时改写 DefaultGame.ini」之前**：地图那一步是"备份→改写→打完还原"，
# 写在它后面会被还原掉，等于没写。
if (-not [string]::IsNullOrWhiteSpace($PlayerVersion)) {
    $GameIniForVersion = Join-Path $ProjectRoot 'Config\DefaultGame.ini'
    if ($DryRun) {
        Write-Host "[版本] (DryRun) 本应把 GameVersion 写成 $PlayerVersion（不动 ProjectVersion）" -ForegroundColor Yellow
    }
    elseif (Test-Path -LiteralPath $GameIniForVersion) {
        if (Set-IniKey -Path $GameIniForVersion -Section '[CrossingVersion]' -Key 'GameVersion' -Value $PlayerVersion) {
            Write-Host "[版本] GameVersion = $PlayerVersion（内容版本，游戏运行时可直接读）" -ForegroundColor Green
        }
        else {
            Write-Host "[版本] GameVersion 已经是 $PlayerVersion，不动" -ForegroundColor DarkGray
        }
    }
    else {
        Write-Host "[版本][警告] 找不到 $GameIniForVersion，GameVersion 没写进去" -ForegroundColor Yellow
    }
}
else {
    Write-Host '[版本][警告] 没填「玩家版本」—— 这次产物里没有内容版本（游戏读到的是空/默认值）' -ForegroundColor Yellow
}

# 顺手把"网络兼容版本"报出来：它变了 = 旧客户端会被引擎拒连。
# 正常情况下应该一直是同一个值（空 = 引擎退回 1.0.0）。
$NetCompatVer = ''
$GameIniForNet = Join-Path $ProjectRoot 'Config\DefaultGame.ini'
if (Test-Path -LiteralPath $GameIniForNet) {
    $InNetSection = $false
    foreach ($Line in (Get-Content -LiteralPath $GameIniForNet -Encoding UTF8)) {
        $t = $Line.Trim()
        if ($t.StartsWith('[')) { $InNetSection = ($t -eq '[/Script/EngineSettings.GeneralProjectSettings]'); continue }
        if ($InNetSection -and $t -match '^ProjectVersion\s*=') { $NetCompatVer = ($t -split '=', 2)[1].Trim(); break }
    }
}
if ([string]::IsNullOrWhiteSpace($NetCompatVer)) {
    Write-Host '[版本] 网络兼容版本 ProjectVersion：未设置（引擎退回 1.0.0，两端一致就不会拒连）' -ForegroundColor DarkGray
}
else {
    Write-Host ("[版本] 网络兼容版本 ProjectVersion：{0}（改它 = 旧客户端连不上，只随网络协议变动）" -f $NetCompatVer) -ForegroundColor DarkGray
}

$GameIniPath = Join-Path $ProjectRoot 'Config\DefaultGame.ini'
# 备份一份原件；下面「地图清单」和「服务器散件」两处改写共用这一份，跑完统一还原
$GameIniBackupPath = Join-Path $ProjectRoot 'Config\DefaultGame.ini.bak-maps-tmp'
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
# 详见 Docs\打包说明.md 的「服务器为什么出散件」一节。
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

        # ---- 临时把打包方式切成散件 ----
        # 这三个键不一定写在 DefaultGame.ini 里（UE 的默认值是 True，很多工程只显式写其中一两个）。
        # 所以不能"找不到就报错"：已有的改成 False，缺的补进 [ProjectPackagingSettings] 段。
        # 只动这个段里的键，别碰同名但属于别的段的配置。
        $LooseKeys = @('UsePakFile', 'bUseIoStore', 'bUseZenStore')
        $PackagingSection = '[/Script/UnrealEd.ProjectPackagingSettings]'
        $Lines = [System.Collections.Generic.List[string]]@(Get-Content -LiteralPath $GameIniPath -Encoding UTF8)
        $Seen = @{}
        $InPackaging = $false
        $LastSectionHeader = -1

        for ($i = 0; $i -lt $Lines.Count; $i++) {
            $t = $Lines[$i].Trim()
            if ($t.StartsWith('[') -and $t.EndsWith(']')) {
                $InPackaging = ($t -ieq $PackagingSection)
                if ($InPackaging) { $LastSectionHeader = $i }
                continue
            }
            if (-not $InPackaging) { continue }

            foreach ($k in $LooseKeys) {
                if ($t -match ('^' + [regex]::Escape($k) + '\s*=')) {
                    $Lines[$i] = "$k=False"
                    $Seen[$k] = $true
                    break
                }
            }
        }

        # 缺的键补到「最后一个」ProjectPackagingSettings 段末尾：
        # 段可能不止一个（这个工程就有两段），放在最后一段能确保它后面没有同名值覆盖。
        $Missing = @($LooseKeys | Where-Object { -not $Seen.ContainsKey($_) })
        if ($Missing.Count -gt 0) {
            if ($LastSectionHeader -lt 0) {
                $Lines.Add('')
                $Lines.Add($PackagingSection)
                foreach ($k in $Missing) { $Lines.Add("$k=False") }
            }
            else {
                $InsertAt = $Lines.Count
                for ($i = $LastSectionHeader + 1; $i -lt $Lines.Count; $i++) {
                    $t = $Lines[$i].Trim()
                    if ($t.StartsWith('[') -and $t.EndsWith(']')) { $InsertAt = $i; break }
                }
                for ($j = 0; $j -lt $Missing.Count; $j++) {
                    $Lines.Insert($InsertAt + $j, "$($Missing[$j])=False")
                }
            }
        }

        # 写完自检：三个键都必须以 False 出现，缺一个都说明这次改动没落全
        foreach ($k in $LooseKeys) {
            $HasFalse = $false
            foreach ($line in $Lines) {
                if ($line.Trim() -match ('^' + [regex]::Escape($k) + '\s*=\s*False\s*$')) { $HasFalse = $true; break }
            }
            if (-not $HasFalse) { throw "[服务器散件] 写入 $k=False 失败，拒绝继续" }
        }

        [System.IO.File]::WriteAllLines($GameIniPath, $Lines, (New-Object System.Text.UTF8Encoding($true)))
        Write-Host ("[服务器散件] 临时关掉打包容器：{0}（缺的键已补进 {1}；打完自动还原）" -f ($LooseKeys -join ' / '), $PackagingSection) -ForegroundColor Yellow

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
if ($DryRun -and -not [string]::IsNullOrWhiteSpace($PlayerVersion)) {
    # DryRun 的语义是"只打印、不动手"。这段以前不看 DryRun，侦察一次就顺手把工程里的版本号
    # 改了（2026-10-27 干跑踩到：0.1.1 被写成 0.6.1，还得手改回来）。
    Write-Host "[安卓版本号] (DryRun) 本应把 VersionDisplayName 写成 $PlayerVersion（不改工程文件）" -ForegroundColor Yellow
}
elseif (-not [string]::IsNullOrWhiteSpace($PlayerVersion)) {
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

# 收据（*.target）里记着这次构建用的**插件描述符绝对路径**（"Path": "$(ProjectDir)/Plugins/X/X.uplugin"，
# Type: UFS），UAT 的 staging 会照着它逐个 PluginDescriptor.FromFile 去读
# （CopyBuildToStagingDirectory.Automation.cs:1936）。收据只有"真编一次"才会重写，所以插件目录改名/搬家
# 之后它就是一张过期地图 ⇒ staging 直接 DirectoryNotFoundException：
#   2026-10-11 就是这么炸的 —— Plugins\DreamGameplayTask-master 改名成 DreamGameplayTask，
#   而 Android 收据还是 9/19 写的；Android 分支原来"有收据就不编"，于是打包死在 staging。
# 时间戳比较抓不到这一类（改名不动 mtime），所以单独查一遍路径还在不在。
function Test-ReceiptPluginPaths {
    param([Parameter(Mandatory)][string]$ReceiptPath)

    $missing = @()
    if (-not (Test-Path -LiteralPath $ReceiptPath)) {
        return [pscustomobject]@{ Valid = $true; Missing = $missing }
    }

    $text = Get-Content -LiteralPath $ReceiptPath -Raw -Encoding utf8
    foreach ($m in [regex]::Matches($text, '"Path"\s*:\s*"\$\(ProjectDir\)/(Plugins/[^"]+\.uplugin)"')) {
        $relative = $m.Groups[1].Value -replace '/', '\'
        if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot $relative))) {
            $missing += $relative
        }
    }
    return [pscustomobject]@{ Valid = ($missing.Count -eq 0); Missing = $missing }
}

$AllOk = $true

foreach ($Run in $RunList) {
    $PackTarget = $Run.Name
    $OutDir     = $Run.OutDir
    $TargetName = if ($PackTarget -eq 'Server') { $ServerTargetName } else { $ClientTargetName }

    # ---- 统一补丁：只改了代码就跳过 cook/pak ----
    # 《常规补丁》不再分两个按钮：先让速路扫一遍变化 ——
    #   · 只有 C++ 变化 → 编译 + 镜像二进制，直接结束（几分钟）；
    #   · 有资产/配置/Shader 变化 → 它返回 $false，继续往下走完整的 编译 + cook + generatepatch + pak。
    # 服务器不走这条（散件走 git，没有 pak 可言）；安卓没有代码补丁这回事。
    if ($Mode -eq 'Patch' -and $PackTarget -eq 'Client' -and $Platform -eq 'Win64' -and $Configuration -eq 'Shipping') {
        $UnifiedResult = Invoke-CodePatch -Auto
        if ($UnifiedResult -eq 'code') {
            Write-Host ''
            Write-Host '[统一补丁] 本次只有代码变化：二进制已镜像到归档，不 cook / 不 pak。' -ForegroundColor Green
            Write-Host '           下一步：重新生成启动器 files[] / fragments 并上传变化的文件。' -ForegroundColor Yellow
            exit 0
        }
        if ($UnifiedResult -eq 'none') {
            Write-Host ''
            Write-Host '[统一补丁] 没有任何变化 —— 这次不打包。' -ForegroundColor Green
            exit 0
        }
        # 'full'：继续往下走 编译 + cook + generatepatch + pak
    }

    # ---- 目标 Shipping 二进制缺失时：先【单独】把它编出来 ----
    # 关键点：不要交给 UAT 的 -build。UAT 那一次 UBT 调用会同时带上
    # CrossingVoidEditor + UnrealPak + ShaderCompileWorker + 本目标，
    # 编辑器开着时 Live Coding 持有 Global\LiveCoding_...UnrealEditor.exe，
    # UBT 检查到编辑器目标就被整批拒绝（Unable to build while Live Coding is active）。
    # 单独编本目标时，UBT 只检查本目标的互斥体 —— 服务器目标的互斥体编辑器并不持有，
    # 所以能编过。这样用户不用关编辑器，打包也不碰 Live Coding。
    if ($Platform -eq 'Win64') {
        $ReceiptName = $TargetName
        $ReceiptPath = Join-Path $ProjectRoot "Binaries\Win64\$ReceiptName-Win64-$Configuration.target"
        $TargetExe = Join-Path $ProjectRoot "Binaries\Win64\$ReceiptName-Win64-$Configuration.exe"

        # 判定：收据不存在 → 编；收据在但源码比二进制新 → 也编
        $NeedCompile = $false
        $WhyCompile  = ''
        if (-not (Test-Path -LiteralPath $ReceiptPath)) {
            $NeedCompile = $true
            $WhyCompile  = '收据不存在（这个目标从没编过）'
        }
        else {
            $NewestSrc = Get-NewestProjectSource
            $BinaryStamp = if (Test-Path -LiteralPath $TargetExe) {
                (Get-Item -LiteralPath $TargetExe).LastWriteTime
            }
            else {
                (Get-Item -LiteralPath $ReceiptPath).LastWriteTime
            }
            if ($NewestSrc.Time -and ($NewestSrc.Time -gt $BinaryStamp)) {
                $NeedCompile = $true
                $WhyCompile  = ('源码比目标二进制新：{0}（源码 {1:yyyy-MM-dd HH:mm:ss} / 二进制 {2:yyyy-MM-dd HH:mm:ss}）' -f (Split-Path $NewestSrc.Path -Leaf), $NewestSrc.Time, $BinaryStamp)
            }
            else {
                # 时间戳抓不到"插件目录改名/搬家"（改名不动 mtime），而收据里记的是绝对路径 —— 单独查一遍
                $ReceiptCheck = Test-ReceiptPluginPaths -ReceiptPath $ReceiptPath
                if (-not $ReceiptCheck.Valid) {
                    $NeedCompile = $true
                    $WhyCompile  = ('收据里记的插件路径已不存在：{0}（收据是上次构建时写的，插件目录改过名/搬过家 —— staging 会照着这张过期地图去读）' -f ($ReceiptCheck.Missing -join '、'))
                }
            }
        }
        if ($ForceBuild) {
            # -ForceBuild 时编译交给 UAT 那一轮（Build-UatArgs 会保留 -build），这里就不再单独编一遍
            $NeedCompile = $false
        }

        if ($NeedCompile -and $DryRun) {
            # DryRun 的语义是"只打印不真干"（见 Docs\打包说明.md），所以这里只报"本应编译"
            Write-Host "[编译] (DryRun) 本应编译：$WhyCompile" -ForegroundColor Yellow
        }
        elseif ($NeedCompile) {
            Write-Host ''
            Write-Host "[编译] $WhyCompile —— 先单独编译 $ReceiptName 的 $Configuration 目标（只编这个目标，不会被 Live Coding 挡住）" -ForegroundColor Yellow
            $BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
            $BuildArgs = @($ReceiptName, 'Win64', $Configuration, "-Project=$ProjectFile", '-WaitMutex')
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
            # 加 -KeepSymbols 时两个都不给，回到"产出来留档"。详见 Docs\打包说明.md。
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
            Write-Host ("[编译] {0} {1} 目标是最新的，跳过编译" -f $ReceiptName, $Configuration) -ForegroundColor DarkGray
        }
    }
    else {
        # ---- 安卓：同理，目标缺失**或过期**时单独编它 ----
        # 为什么必须单独编：打包一律带 -nocompile，UAT 不会替我们编译；
        # 而第一次打安卓时 Binaries\Android 下根本没有产物，不先编出来必然失败。
        #
        # 2026-10-11 补上"过期"这一半：原来这里只看收据**在不在**，于是插件改名 / 源码更新之后照样
        # 跳过编译 ⇒ staging 拿着 9/19 那张收据去读已经改名的插件目录，直接 DirectoryNotFoundException。
        # Win64 分支 2026-09-19 已经吃过"收据在 = 二进制新"的亏（见 Get-NewestProjectSource 的注释），
        # 安卓这边现在按同一套判断：源码比二进制新，或收据里记的插件路径已不存在。
        $ReceiptName = $TargetName
        $AndroidBin = Join-Path $ProjectRoot 'Binaries\Android'
        $AndroidReceipt = $null
        if (Test-Path -LiteralPath $AndroidBin) {
            $AndroidReceipt = Get-ChildItem -LiteralPath $AndroidBin -Filter "$ReceiptName-Android*Shipping*.target" -ErrorAction SilentlyContinue |
                Select-Object -First 1
        }
        # 有的版本收据名不带平台后缀，兜一层
        if (-not $AndroidReceipt -and (Test-Path -LiteralPath $AndroidBin)) {
            $AndroidReceipt = Get-ChildItem -LiteralPath $AndroidBin -Filter '*Shipping*.target' -ErrorAction SilentlyContinue |
                Select-Object -First 1
        }

        $NeedAndroidBuild = $false
        $WhyAndroidBuild  = ''
        if (-not $AndroidReceipt) {
            $NeedAndroidBuild = $true
            $WhyAndroidBuild  = "$ReceiptName 的 Android Shipping 目标还没编过（第一次会比较久：arm64 + 引擎模块）"
        }
        else {
            $AndroidSo = Get-ChildItem -LiteralPath $AndroidBin -Filter "$ReceiptName-Android*Shipping*.so" -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if (-not $AndroidSo) {
                $NeedAndroidBuild = $true
                $WhyAndroidBuild  = 'Android Shipping 二进制（*.so）不存在，只剩收据'
            }
            else {
                $NewestSrc = Get-NewestProjectSource
                if ($NewestSrc.Time -and ($NewestSrc.Time -gt $AndroidSo.LastWriteTime)) {
                    $NeedAndroidBuild = $true
                    $WhyAndroidBuild  = ('源码比 Android 二进制新：{0}（源码 {1:yyyy-MM-dd HH:mm:ss} / 二进制 {2:yyyy-MM-dd HH:mm:ss}）' -f (Split-Path $NewestSrc.Path -Leaf), $NewestSrc.Time, $AndroidSo.LastWriteTime)
                }
                else {
                    # 时间戳抓不到"插件目录改名/搬家"（改名不动 mtime），而收据里记的是绝对路径 —— 单独查一遍
                    $ReceiptCheck = Test-ReceiptPluginPaths -ReceiptPath $AndroidReceipt.FullName
                    if (-not $ReceiptCheck.Valid) {
                        $NeedAndroidBuild = $true
                        $WhyAndroidBuild  = ('收据里记的插件路径已不存在：{0}（收据是上次构建时写的，插件目录改过名/搬过家 —— staging 会照着这张过期地图去读）' -f ($ReceiptCheck.Missing -join '、'))
                    }
                }
            }
        }

        # -ForceBuild 时编译交给 UAT 那一轮（Build-UatArgs 会保留 -build），这里就不再单独编一遍
        if ($ForceBuild) { $NeedAndroidBuild = $false }

        if ($NeedAndroidBuild -and $DryRun) {
            Write-Host "[编译] (DryRun) 本应编译：$WhyAndroidBuild" -ForegroundColor Yellow
        }
        elseif ($NeedAndroidBuild) {
            Write-Host ''
            Write-Host "[编译] $WhyAndroidBuild —— 先单独编译它（不会被 Live Coding 挡住，编辑器不用关）" -ForegroundColor Yellow
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
        else {
            Write-Host ("[编译] {0} Android Shipping 目标是最新的，跳过编译" -f $ReceiptName) -ForegroundColor DarkGray
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

    # ---- 客户端：删掉【另一种配置】留下的运行时二进制 ----
    # 两种配置共用同一个产物目录（上传脚本认死路径），而 exe / dll / .modules 的文件名里带配置后缀。
    # 不删的话：打完 Development 再打 Shipping，目录里还留着 *-Win64-Development.exe，
    # 而上传脚本是整目录枚举（只排除 *.pdb），会把两个 exe 一起传上去。
    # 只动 Binaries 下带配置后缀的文件：pak / 资源 / 根目录启动 shim 一律不碰。
    if ($Platform -eq 'Win64' -and $PackTarget -eq 'Client') {
        $OtherConfig = if ($Configuration -eq 'Development') { 'Shipping' } else { 'Development' }
        $BinDir = $null
        $ClientRoot = Join-Path $Run.OutDir $PlatformDir
        if (Test-Path -LiteralPath $ClientRoot) {
            $Found = Get-ChildItem -LiteralPath $ClientRoot -Directory -Recurse -Filter 'Binaries' -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($Found) { $BinDir = $Found.FullName }
        }
        if ($BinDir) {
            $StaleFiles = @(Get-ChildItem -LiteralPath $BinDir -Recurse -File -Filter "*-$OtherConfig.*" -ErrorAction SilentlyContinue)
            if ($DryRun) {
                Write-Host ("[配置] (DryRun) 本应删掉上一种配置的 {0} 个文件（*-{1}.*）—— {2}" -f $StaleFiles.Count, $OtherConfig, $BinDir) -ForegroundColor Yellow
            }
            elseif ($StaleFiles.Count -gt 0) {
                foreach ($StaleFile in $StaleFiles) { Remove-Item -LiteralPath $StaleFile.FullName -Force -ErrorAction SilentlyContinue }
                Write-Host ("[配置] 已删掉上一种配置的 {0} 个文件（*-{1}.*）—— 两种配置共用产物目录，不删会混包" -f $StaleFiles.Count, $OtherConfig) -ForegroundColor Yellow
            }
        }
    }

    $built    = Build-UatArgs -PackTarget $PackTarget -OutDir $OutDir -TargetName $TargetName
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
        # 归档目录名 = 工程名（UAT 的 ShortProjectName），不是 target 名；
        # 服务器那一侧是 WindowsServer\FantasyProject\...，exe 才叫 FantasyProjectServer-...
        $SubName = $ProjectName
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

        # ---- 版本戳：这批产物是哪个版本、什么时候打的 ----
        # 2026-10-26 那次事故的根因是"碎片生成拿着 10/11 的旧包当新包"（源目录里的 APK/OBB 都停在
        # 10-11 23:22，版本还是 0.6.0）。这里在归档根写版本戳：下游（碎片生成 / 发布）
        # 先读它、比对版本和时间，拿错目录就能当场停下，而不是把旧包当新号发出去。
        #
        # 2026-10-28 拆分：一个 Version.json 同时承担整包和补丁，下游得靠 mode 猜，
        # 多平台并列时还要在 root / ../Version-Windows.json / ../Version.json 之间试。
        # 现在固定三个文件，都在归档根（不再写旧的 Version.json，发布脚本直接对接这三个）：
        #   Version-Full.json   整包（Base / Quick）
        #   Version-Patch.json  补丁（Patch），多一个 baseVersion = 基于哪条线上基线
        #   Latest.json         指针：最后一次写的是哪份、什么时候、插件版本
        # 三个都不能进 Windows/Android 子目录 —— 否则会被启动器的整目录枚举一起发布。
        $ArtifactDir = if ($PackTarget -eq 'Server') { 'WindowsServer' } else { $PlatformDir }
        $ArtifactPath = Join-Path $OutDir $ArtifactDir

        # 版本戳按「平台-目标-配置-模式」各放一份：Windows / 安卓 / 服务器互不覆盖。
        # 目录名和报告目录（Saved\PackLogs\ChunkReports\...）用同一套规则，排查时一一对应。
        # 放在 <ArchiveDir>\Version\ 下，不进 Windows/Android 子目录 —— 那里会被启动器整目录发布。
        $StampScope = "$Platform-$PackTarget"
        if ($Configuration -eq 'Development') { $StampScope += '-Development' }
        if ($Mode -eq 'Quick') { $StampScope += '-Quick' }
        $StampDir = Join-Path $OutDir "Version\$StampScope"
        Write-Host ''
        Write-Host ("[版本戳] scope = {0}" -f $StampScope) -ForegroundColor Cyan
        Write-Host ("         目录 = {0}" -f $StampDir) -ForegroundColor DarkGray
        if (-not (Test-Path -LiteralPath $StampDir)) {
            New-Item -ItemType Directory -Path $StampDir -Force | Out-Null
            Write-Host '         目录不存在，已新建' -ForegroundColor DarkGray
        }

        # 根目录上属于旧布局的戳（含 2026-10-28 之前的 Version.json）一律清掉，
        # 免得发布那边读到一个没人维护的旧文件。
        foreach ($OldName in @('Version.json', 'Version-Full.json', 'Version-Patch.json', 'Latest.json')) {
            $OldPath = Join-Path $OutDir $OldName
            if (Test-Path -LiteralPath $OldPath) {
                Remove-Item -LiteralPath $OldPath -Force -ErrorAction SilentlyContinue
                Write-Host ("         清理旧布局的根目录戳：{0}" -f $OldName) -ForegroundColor Yellow
            }
        }

        $GeneratedAt = (Get-Date).ToString('yyyy-MM-ddTHH:mm:sszzz')
        # Base / Quick 都是整包（Quick 只是不建基线、不打 pak，自检会按 mode 拦发布）；
        # 只有 Patch 是补丁，需要 baseVersion。
        $Kind = if ($Mode -eq 'Patch') { 'Patch' } else { 'Full' }

        # 插件版本：从 uplugin 读。脚本被单独拷到 <工程>\Tools 时没有这个文件，留空即可。
        $PluginVersion = ''
        $PluginManifest = Join-Path (Split-Path -Parent $PSScriptRoot) 'CrossingChunkEditor.uplugin'
        if (Test-Path -LiteralPath $PluginManifest) {
            try {
                $PluginVersion = [string](Get-Content -LiteralPath $PluginManifest -Raw -Encoding UTF8 | ConvertFrom-Json).VersionName
            }
            catch { }
        }

        # 产物清单：posix 相对路径（相对归档根）+ sizeBytes + sha256。
        # sha256 在这里算一次，下游（碎片生成 / 发布清单）直接拿去用，不用再扫一遍。
        #
        # 范围按包型分：
        #   · 版本包（Base）：发布目录【全量】—— 首次安装 / 大版本更新要能逐文件体检。
        #   · 补丁 / 快速验证：只记这次打包产出的关键产物（容器 + 可执行）。
        #     补丁本来就只带变化块，列全量没意义，还白花哈希时间。
        # 全量时排除符号（*.pdb、*_Symbols_*）和安卓的打包工具目录 win-x64
        # —— 它们不是发布内容，列进去会让体检误报。
        # 服务器是散件走 git、Mode 固定 Quick，落到"关键产物"分支，只记 exe。
        $bFullManifest = ($Kind -eq 'Full' -and $Mode -eq 'Base')
        $VersionStampFiles = @()
        if (Test-Path -LiteralPath $ArtifactPath) {
            if ($bFullManifest) {
                $StampItems = @(Get-ChildItem -LiteralPath $ArtifactPath -Recurse -File -ErrorAction SilentlyContinue |
                    Where-Object {
                        if ($_.Extension -ieq '.pdb') { return $false }
                        $Rel = $_.FullName.Substring($ArtifactPath.Length).TrimStart('\', '/')
                        if ($Rel -like 'win-x64\*') { return $false }
                        if ($Rel -like '*_Symbols_*\*') { return $false }
                        return $true
                    })
            }
            else {
                # 安卓的发布文件都在顶层（apk/obb）：递归会把打包工具 win-x64\UnrealAndroidFileTool.exe
                # 也收进清单（发布那边可能照着上传），所以安卓只扫一层。Windows/服务器要递归（pak/exe 在子目录）。
                $StampItems = if ($Platform -eq 'Android') {
                    Get-ChildItem -LiteralPath $ArtifactPath -File -ErrorAction SilentlyContinue
                }
                else {
                    Get-ChildItem -LiteralPath $ArtifactPath -Recurse -File -ErrorAction SilentlyContinue
                }
                $StampItems = @($StampItems | Where-Object {
                    ($_.Extension -in '.apk', '.obb', '.exe', '.pak', '.utoc', '.ucas') -and ($_.LastWriteTime -ge $StartTime)
                })
            }

            $StampItems = @($StampItems | Sort-Object FullName)
            $StampTotal = $StampItems.Count
            [double]$StampTotalBytes = 0
            foreach ($StampItem in $StampItems) { $StampTotalBytes += $StampItem.Length }

            Write-Host ''
            Write-Host ("[版本戳] 清单范围：{0}" -f $(if ($bFullManifest) { '整包全量（发布目录里所有会被上传的文件）' } else { '本次关键产物（容器 + 可执行）' })) -ForegroundColor Cyan
            Write-Host ("         目录：{0}" -f $ArtifactPath) -ForegroundColor DarkGray
            Write-Host ("         共 {0} 个文件、{1:N2} GB；逐个算 sha256（只读磁盘，不改产物）" -f $StampTotal, ($StampTotalBytes / 1GB)) -ForegroundColor Cyan

            $HashStart = Get-Date
            $StampIndex = 0
            $VersionStampFiles = @($StampItems | ForEach-Object {
                $StampIndex++
                $Rel = $_.FullName.Substring($OutDir.Length).TrimStart('\', '/').Replace('\', '/')
                Write-Host ("         [{0}/{1}] {2}（{3:N1} MB）" -f $StampIndex, $StampTotal, $Rel, ($_.Length / 1MB)) -ForegroundColor DarkGray
                [pscustomobject]@{
                    path      = $Rel
                    sizeBytes = $_.Length
                    sha256    = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                    modified  = $_.LastWriteTime.ToString('yyyy-MM-ddTHH:mm:ss')
                }
            })
            Write-Host ("         sha256 完成，用时 {0:N1} 秒" -f ((Get-Date) - $HashStart).TotalSeconds) -ForegroundColor DarkGray
        }
        else {
            Write-Host "[版本戳] 产物目录不存在，清单留空：$ArtifactPath" -ForegroundColor Yellow
        }

        $VersionStamp = [ordered]@{
            schemaVersion  = 2
            kind           = $Kind
            version        = $PlayerVersion
            versionSource  = 'CrossingChunkEditor 面板「玩家版本」→ [CrossingVersion] GameVersion（内容版本，不参与 NetCL）'
            # 补丁是相对哪条线上基线生成的。整包没有这个字段的含义，留空。
            # 自检拿它判断"补丁是不是过期了"：baseVersion 必须等于当前线上主版本段。
            baseVersion    = $(if ($Kind -eq 'Patch') { $ReleaseVersion } else { '' })
            generatedAt    = $GeneratedAt
            mode           = $Mode
            platform       = $Platform
            target         = $PackTarget
            configuration  = $Configuration
            releaseVersion = $ReleaseVersion
            project        = $ProjectName
            archiveDir     = $OutDir
            artifactDir    = $ArtifactDir
            scope          = $StampScope
            # 网络兼容版本 = 引擎握手比对的 NetCL 的一部分（GeneralProjectSettings.ProjectVersion）。
            # 记下来是为了排查"旧包连不上"：两端这行不一致 = 引擎直接拒连，跟内容版本无关。
            # 空 = 没设置，引擎退回 1.0.0（两端都空就是一致的）。
            networkVersion          = $NetCompatVer
            networkVersionEffective = $(if ([string]::IsNullOrWhiteSpace($NetCompatVer)) { '1.0.0' } else { $NetCompatVer })
            networkVersionSet       = (-not [string]::IsNullOrWhiteSpace($NetCompatVer))
            files          = $VersionStampFiles
        }

        $VersionFileName = if ($Kind -eq 'Patch') { 'Version-Patch.json' } else { 'Version-Full.json' }
        $VersionPath = Join-Path $StampDir $VersionFileName
        $Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        [System.IO.File]::WriteAllText($VersionPath, (($VersionStamp | ConvertTo-Json -Depth 5) + "`r`n"), $Utf8NoBom)

        # 指针：这个 scope 最后一次打包写的是哪一份。
        # latestFile 用【相对归档根】的 posix 路径（如 Version/Win64-Client/Version-Patch.json）：
        # 自带位置信息，调用方拿到指针就能直接定位，不用再知道 Latest 自己在哪一层。
        $LatestPath = Join-Path $StampDir 'Latest.json'
        $Latest = [ordered]@{
            schemaVersion = 2
            kind          = 'Latest'
            scope         = $StampScope
            latestFile    = "Version/$StampScope/$VersionFileName"
            latestKind    = $Kind
            version       = $PlayerVersion
            baseVersion   = $(if ($Kind -eq 'Patch') { $ReleaseVersion } else { '' })
            generatedAt   = $GeneratedAt
            pluginVersion = $PluginVersion
            platform      = $Platform
            target        = $PackTarget
            configuration = $Configuration
        }
        [System.IO.File]::WriteAllText($LatestPath, (($Latest | ConvertTo-Json -Depth 4) + "`r`n"), $Utf8NoBom)

        Write-Host ''
        Write-Host ("[版本戳] 已写入 {0}" -f $(if ($PlayerVersion) { $PlayerVersion } else { '(未填版本)' })) -ForegroundColor Green
        Write-Host ("         kind={0} · baseVersion={1} · mode={2} · {3} 个文件" -f $Kind, $(if ($Kind -eq 'Patch') { $ReleaseVersion } else { '—' }), $Mode, $VersionStampFiles.Count) -ForegroundColor Green
        Write-Host ("         清单：{0}" -f $VersionPath) -ForegroundColor DarkGray
        Write-Host ("         指针：{0} -> {1}" -f $LatestPath, $Latest.latestFile) -ForegroundColor DarkGray

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
        }
        else {
            Write-Host ''
            if ($Platform -eq 'Android') {
                # 安卓把 pak 数据打进 APK/OBB（main.<版本>.<包名>.obb），产物目录里根本没有
                # Content\Paks —— 这不是失败，只是「容器体积统计」这一项对安卓不适用。
                # 清单归档和分包报告不受影响（见下面那一段）。
                Write-Host '  （安卓：pak 打进 APK/OBB，没有 Content\Paks —— 容器体积统计跳过）' -ForegroundColor DarkGray
                $AndroidRoot = Join-Path $OutDir $PlatformDir
                $AndroidFiles = @(Get-ChildItem -LiteralPath $AndroidRoot -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Extension -in '.apk', '.obb' })
                if ($AndroidFiles.Count -gt 0) {
                    Write-Host "[产物] $AndroidRoot" -ForegroundColor Yellow
                    foreach ($f in $AndroidFiles) {
                        Write-Host ('  {0,-54} {1,10:N1} MB' -f $f.Name, ($f.Length / 1MB))
                    }
                }
            }
            else {
                Write-Host "[警告] 没找到产物目录：$PaksDir" -ForegroundColor Yellow
                Write-Host '       检查 -archivedirectory 与平台目录名。' -ForegroundColor Yellow
            }
        }

        # ---- 归档 staging 清单 + 生成分包报告（客户端两个平台都做）----
        # 这一段原来挂在「产物目录里有没有 Content\Paks」那个判定里，于是安卓永远走不到：
        # 安卓没有 Content\Paks，整块（归档 + 报告）被一起跳过 —— 表现就是
        # 「安卓从来没有分包报告」，页面在安卓平台永远显示「报告：还没有」。
        # 现在把两件事解耦：「有没有容器可以量体积」只影响体积统计（上面那一段），
        # 「有没有清单可以归档、可以算报告」两个平台都一样做。
        # 注意：UAT 的 PrePak_<CookPlatform>_UFSFiles.txt 是**临时文件**，
        # 下一次打包会清掉，所以必须在这里立刻归档 + 立刻算报告。
        if ($PackTarget -ne 'Server') {
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
            $ReportScript = Join-Path $PSScriptRoot 'Build-ChunkReport.ps1'
            if (Test-Path -LiteralPath $ReportScript) {
                try {
                    & $ReportScript -ProjectRoot $ProjectRoot -ArchiveDir $OutDir `
                        -Platform $Platform -Target $PackTarget -Mode $Mode -Configuration $Configuration -PackLogPath $LogFile
                }
                catch {
                    Write-Host "  [警告] 分包报告生成失败：$($_.Exception.Message)" -ForegroundColor Yellow
                    Write-Host '         （打包本身是成功的，报告只影响页面上的统计）' -ForegroundColor Yellow
                }
            }
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

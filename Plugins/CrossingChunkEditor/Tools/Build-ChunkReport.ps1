<#
.SYNOPSIS
    打包结果报告 —— 产出 chunk-report.json（给《二游打包》页用，以后 AxTools 也读同一份）

.DESCRIPTION
    必须在**打包之后立刻**跑：UAT 的 staged 清单（PrePak_*.txt）是临时文件，
    下一次打包会把它清掉。Pack-CrossingVoid.ps1 已经接好，不用手动调。

    数据来源：
      · 规则与优先级 : Config\DefaultPakFileRules.ini（生成器刚产出的，书写顺序 = 优先级）
      · 文件归属     : UAT 的 PrePak_<CookPlatform>_UFSFiles.txt（本次真正要进包的文件清单）
      · 容器体积     : <ArchiveDir>\<平台目录>\<项目>\Content\Paks\ 下的 pak/ucas/utoc
      · 包体积       : Saved\Cooked\<平台目录>\<项目>\Metadata\AllChunksInfo.csv（可选，判"修改"用）
      · 上一次报告   : Saved\PackLogs\ChunkReports\<平台>-<目标>\latest.json

.PARAMETER WhatIf
    只算不写（排查用）

.EXAMPLE
    .\Build-ChunkReport.ps1 -Platform Win64 -Target Client -Mode Quick
#>
param(
    # 留空 = 从脚本所在位置自动推（逐级向上找 *.uproject）
    [string]$ProjectRoot = '',
    [string]$ArchiveDir  = 'D:\Build\CrossingVoid',
    [ValidateSet('Win64', 'Android')][string]$Platform = 'Win64',
    [ValidateSet('Client', 'Server')][string]$Target = 'Client',
    [ValidateSet('Quick', 'Base', 'Patch')][string]$Mode = 'Quick',
    [string]$PackLogPath = '',
    [string]$UatLogDir = 'D:\UnrealEngine-5.8.2\Engine\Programs\AutomationTool\Saved\Logs',
    [string]$EngineRoot = 'D:\UnrealEngine-5.8.2',
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'

# 工程根不再写死。本脚本会被放在两处，都要能自己推出来：
#   <工程>\Tools\                              —— 旧布局
#   <工程>\Plugins\CrossingChunkEditor\Tools\  —— 插件自带（本仓库的形态）
# 装在 Engine\Plugins\ 下时推不出来，那种情况必须显式传 -ProjectRoot。
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

$PlatformDir  = if ($Platform -eq 'Win64') { 'Windows' } else { 'Android' }
$CookPlatform = $PlatformDir
$SubName      = if ($Target -eq 'Server') { 'CrossingVoidServer' } else { 'CrossingVoid' }
$ProjectName  = Split-Path -Leaf $ProjectRoot

$RuleFile     = Join-Path $ProjectRoot 'Config\DefaultPakFileRules.ini'

# "本次要进包的文件清单"有两个来源，格式完全一样（都是 UAT 的 DumpManifest：
# 每行 `"源路径" "暂存路径"`），谁在就用谁：
#   ① PrePak 清单 —— 打了 pak 才有，而且是临时文件（下次打包就被清掉）
#   ② 归档里的 Manifest_UFSFiles_<平台>.txt —— staging 阶段就会产出
#      （证据：服务器包从来不打 pak，归档里照样有这份清单）
# 这样《快速验证》不打 pak 也能算出归属，报告不会因此缺数。
$StagedListCandidates = @(
    (Join-Path $UatLogDir "PrePak_${CookPlatform}_UFSFiles.txt"),
    (Join-Path $ArchiveDir "$PlatformDir\$SubName\Manifest_UFSFiles_$CookPlatform.txt")
)
$StagedList = $StagedListCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

$PaksDir      = Join-Path $ArchiveDir "$PlatformDir\$SubName\Content\Paks"
$AllChunksCsv = Join-Path $ProjectRoot "Saved\Cooked\$PlatformDir\$ProjectName\Metadata\AllChunksInfo.csv"
# 《快速验证》单独放一份报告：它不打 pak，体积那几列必然是 0，
# 混在一起会把正式报告（版本更新 / 常规补丁的体积、变化）冲掉。
$ReportSuffix = if ($Mode -eq 'Quick') { '-Quick' } else { '' }
$ReportDir    = Join-Path $ProjectRoot "Saved\PackLogs\ChunkReports\$Platform-$Target$ReportSuffix"
$LatestPath   = Join-Path $ReportDir 'latest.json'

if (-not (Test-Path -LiteralPath $RuleFile)) {
    throw "[检查失败] 找不到规则文件 $RuleFile"
}
if (-not $StagedList) {
    throw ("[检查失败] 找不到本次进包的文件清单，两个来源都没有：`n  {0}`n  {1}`n（报告必须在打包之后立刻生成）" -f $StagedListCandidates[0], $StagedListCandidates[1])
}
Write-Host "  文件清单 : $StagedList" -ForegroundColor DarkGray
if (-not (Test-Path -LiteralPath $PaksDir)) {
    if ($Platform -eq 'Android') {
        # 安卓把 pak 数据打进 APK/OBB，产物目录里没有 Content\Paks —— 这不是"没打 pak"，
        # 而是"容器体积这一项对安卓量不到"。清单和报告照出，体积列页面会显示「不统计」。
        Write-Host '  （安卓：pak 打进 APK/OBB，没有容器文件 —— 体积列会是不统计；文件数/变化照常）' -ForegroundColor DarkGray
    }
    else {
        Write-Host '  （没有 pak 容器：本模式不打 pak，所以体积那几列会是 0）' -ForegroundColor DarkGray
    }
}
if (-not (Test-Path -LiteralPath $ReportDir)) { New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null }

# ============================ 1. 规则（顺序即优先级） ============================
$Sections = @()
$cur = $null
foreach ($line in (Get-Content -LiteralPath $RuleFile -Encoding UTF8)) {
    $t = $line.Trim()
    if ($t -match '^\[(.+)\]$') {
        $cur = [pscustomobject]@{ Name = $Matches[1]; Pak = ''; Masks = @(); ChunkId = -1 }
        $Sections += $cur
        continue
    }
    if ($null -eq $cur) { continue }
    if ($t -match '^OverridePaks="pakchunk(\d+)"') {
        $cur.ChunkId = [int]$Matches[1]
        $cur.Pak     = "pakchunk$($cur.ChunkId)"
    } elseif ($t -match '^\+Files="(.+)"') {
        $cur.Masks += $Matches[1]
    }
}
$Sections = $Sections | Where-Object { $_.ChunkId -ge 0 -and $_.Masks.Count -gt 0 }
if ($Sections.Count -eq 0) { throw "[检查失败] $RuleFile 里没解析到任何规则段" }

# 「本次新增」桶的号（生成器写的 [CC_NewContent] 段）
$NewChunkId = ($Sections | Where-Object { $_.Name -eq 'CC_NewContent' } | Select-Object -First 1).ChunkId

function Get-ChunkForPath {
    param([string]$FullPath)
    $p = $FullPath.Replace('\', '/')
    foreach ($s in $Sections) {
        foreach ($m in $s.Masks) {
            if ($m.EndsWith('/...')) {
                # 目录型掩码：.../工程名/Content/Art/Sound/...
                $core = $m.Substring(0, $m.Length - 4) -replace '^\.\.\./', ''
                if ($p -like "*/$core/*") { return $s.ChunkId }
            }
            else {
                # 文件名型掩码：.../工程名/Content/ShaderArchive-*
                $core = $m -replace '^\.\.\./', ''
                if ($p -like "*/$core") { return $s.ChunkId }
            }
        }
    }
    return -1
}

# 相对路径（AssetMaterial/FXs/触地灰尘/x.uasset）→ 内容浏览器里的 /Game 路径
# （未分类清单按这个显示，用户在内容浏览器里一眼能对上）
function Get-GameFolderOf {
    param([string]$RelPath)
    $norm = $RelPath.Replace('\', '/')
    $idx = $norm.LastIndexOf('/')
    if ($idx -lt 0) { return '/Game' }
    return '/Game/' + $norm.Substring(0, $idx)
}

# ============================ 2. 本次要进包的文件 → 归属 ============================
$FileRows  = @()   # [relPath, size, chunkId]
$ChunkHit  = @{}   # chunkId → 文件数
$UnclassByFolder = @{}   # 目录 → [文件数, 字节]

$OwnPrefix = "/$ProjectName/Content/"     # 只统计工程自己的内容，引擎/插件内容不进这栏

foreach ($line in (Get-Content -LiteralPath $StagedList -Encoding UTF8)) {
    $m = [regex]::Match($line, '^"([^"]+)"')
    if (-not $m.Success) { continue }
    $src = $m.Groups[1].Value
    $norm = $src.Replace('\', '/')

    $chunk = Get-ChunkForPath $src
    if (-not $ChunkHit.ContainsKey($chunk)) { $ChunkHit[$chunk] = 0 }
    $ChunkHit[$chunk]++

    if (-not $norm.Contains($OwnPrefix)) { continue }   # 后面只处理工程自己的内容

    $rel = $norm.Substring($norm.IndexOf($OwnPrefix) + $OwnPrefix.Length)
    $FileRows += ,@($rel, 0, $chunk)

    if ($chunk -eq $NewChunkId) {
        # 用「显式赋值 + 标量」的写法：字典里存数组再做算术，在 PowerShell 里很容易踩坑
        $dir = Get-GameFolderOf $rel
        if ($UnclassByFolder.ContainsKey($dir)) {
            $UnclassByFolder[$dir] = [int]$UnclassByFolder[$dir] + 1
        }
        else {
            $UnclassByFolder[$dir] = 1
        }
    }
}


# ============================ 3. 包体积（用于"修改"判定） ============================
$PackageSize = @{}
if (Test-Path -LiteralPath $AllChunksCsv) {
    foreach ($row in (Get-Content -LiteralPath $AllChunksCsv -Encoding UTF8 | Select-Object -Skip 1)) {
        $parts = $row -split ','
        if ($parts.Count -lt 5) { continue }
        $pkg = $parts[1].Trim()
        $size = 0
        if (-not [int]::TryParse($parts[4].Trim(), [ref]$size)) { continue }
        # /Game/A/B/C  →  A/B/C.uasset（和 staged 清单里的相对路径对上）
        # 同一个 package 在 staged 清单里也可能是 .umap（关卡、WP 的 _Generated_ 单元），
        # 所以两个后缀都登记一份 —— 不然地图永远没有大小可比，"改"就测不到。
        $rel = $pkg -replace '^/Game/', ''
        $PackageSize["$rel.uasset"] = $size
        $PackageSize["$rel.umap"]   = $size
    }
}
for ($i = 0; $i -lt $FileRows.Count; $i++) {
    $key = $FileRows[$i][0]
    if ($PackageSize.ContainsKey($key)) {
        $FileRows[$i] = @($key, [int]$PackageSize[$key], $FileRows[$i][2])
    }
}

# ============================ 3.4 上一次报告（哈希和"过渡期"判断都要用） ============================
$Previous = $null
$PreviousFiles = @{}
if (Test-Path -LiteralPath $LatestPath) {
    $Previous = Get-Content -LiteralPath $LatestPath -Encoding UTF8 -Raw | ConvertFrom-Json
    foreach ($row in $Previous.files) { $PreviousFiles[$row[0]] = $row }
}

# 「过渡期」= 上一次报告的行还没有第 4 列哈希（本次改版之前的报告）。
# 那一趟没有哈希可比，只能拿"源文件时间比上次报告晚"当兜底 —— 否则你打完这一包
# 仍然看不到自己刚改的东西，要再打一包才认（改版第一次跑就是这种情况）。
$TransitionMode = $false
$PrevReportTime = [datetime]::MinValue
if ($Previous -and $Previous.files.Count -gt 0 -and (@($Previous.files[0]).Count -lt 4)) {
    $TransitionMode = $true
    try { $PrevReportTime = [datetime]::Parse($Previous.generatedAt) } catch { $PrevReportTime = [datetime]::MinValue }
}
$TransitionChanged = @{}

# ============================ 3.5 源资产哈希（"改"的判据之一） ============================
# 光比 cook 出来的字节数抓不到"同字节数的修改"：材质实例改个标量、Blueprint 改个默认值，
# 编出来大小一模一样；地图（.umap）、大块数据（.ubulk）连大小都没有。所以再算一遍
# **源资产**的哈希 —— 只要 Content 下那个文件字节变了，就算"改"。
#   · 用源文件而不是 cook 产物：开了 ZenStore 之后 cooked 的 uasset 根本不在磁盘上
#     （Saved\Cooked 里只剩 ShaderArchive / 字体这些散件），源文件才是稳定可比的那一份。
#   · 源文件不存在（cook 期才生成的 _Generated_ 之类）就记空串，那个文件退回只比大小。
#   · 截断成 16 位十六进制（64 bit）：够用，json 里每个文件也只多 ~19 字节。
$HashStart = Get-Date
$HashedCount = 0
$SourceMissing = 0
for ($i = 0; $i -lt $FileRows.Count; $i++) {
    $Rel = $FileRows[$i][0]
    $SrcPath = Join-Path $ProjectRoot ("Content\" + ($Rel -replace '/', '\'))
    $Hash = ''
    if (Test-Path -LiteralPath $SrcPath -PathType Leaf) {
        # 过渡期才去 stat 源文件时间（正常那趟不需要，省 1 万次系统调用）
        if ($TransitionMode -and ([System.IO.File]::GetLastWriteTime($SrcPath) -gt $PrevReportTime)) {
            $TransitionChanged[$Rel] = $true
        }
        try {
            $Hash = (Get-FileHash -LiteralPath $SrcPath -Algorithm MD5).Hash.Substring(0, 16)
            $HashedCount++
        }
        catch {
            # 个别文件读不了（被编辑器锁住、权限）不能让报告整个挂掉 ——
            # 那个文件记空哈希，退回只比大小。
            $Hash = ''
            $SourceMissing++
        }
    }
    else {
        $SourceMissing++
    }
    $FileRows[$i] = @($Rel, $FileRows[$i][1], $FileRows[$i][2], $Hash)
}
$HashSeconds = [math]::Round(((Get-Date) - $HashStart).TotalSeconds, 1)

# ============================ 4. 容器体积 ============================
$ContainerBytes = @{}
if (Test-Path -LiteralPath $PaksDir) {
    $all = Get-ChildItem -LiteralPath $PaksDir -File | Where-Object { $_.Extension -in '.pak', '.ucas', '.utoc' }
    foreach ($g in ($all | Group-Object { ($_.BaseName -replace "-$PlatformDir$", '') })) {
        $sum = ($g.Group | Measure-Object -Property Length -Sum).Sum
        $ContainerBytes[$g.Name] = [int64]$sum
    }
}

# 有没有「容器体积」这回事：Windows 的 pak 落在 Content\Paks 里，能量；
# 安卓的 pak 打进 APK/OBB，量不到（快速验证本来也不打 pak）。
# 报告里记一个明确的开关，页面据此把体积列显示成「不统计」而不是误导性的 0 B。
# 文件数 / 变化 / 未分类清单都不受影响 —— 那几样只依赖 staged 清单。
$HasContainerStats = ($ContainerBytes.Count -gt 0)

# ============================ 5. 和上一次报告对比 ============================
# （$Previous / $PreviousFiles 在 3.4 就读好了 —— 3.5 的过渡期判断要用）
$CurrentMap = @{}
foreach ($row in $FileRows) { $CurrentMap[$row[0]] = $row }

if ($Previous) {
    $AddedPaths    = @($FileRows    | Where-Object { -not $PreviousFiles.ContainsKey($_[0]) })
    $RemovedPaths  = @($PreviousFiles.Keys | Where-Object { -not $CurrentMap.ContainsKey($_) })
    # 「改」= cook 体积变了 或 源资产内容变了（第 4 列哈希，见 3.5）。
    # 老报告的行只有 3 列、没有哈希 → 那条就只比大小，行为退回从前（兼容）。
    # 注意：PowerShell 的括号里不能靠"下一行以 -and 开头"续行（会被当成新语句报错，
    # 实测：行尾停在运算符上才续行），所以这里一个判断一行，别把长条件拆行。
    $ModifiedPaths = @($FileRows | Where-Object {
            $prev = $PreviousFiles[$_[0]]
            if (-not $prev) { return $false }
            if ($TransitionChanged.ContainsKey($_[0])) { return $true }
            if ($prev[1] -gt 0 -and $_[1] -gt 0 -and $prev[1] -ne $_[1]) { return $true }
            if ($prev.Count -ge 4 -and $prev[3] -and $_[3] -and $prev[3] -ne $_[3]) { return $true }
            return $false
        })
}
else {
    # 首次报告：没有对比基准，不能把"全部文件"都算成新增
    $AddedPaths = @(); $RemovedPaths = @(); $ModifiedPaths = @()
}

# 每个 chunk 一张计数表（用独立的哈希表存标量，避免「字典里塞对象再改属性」那类坑）
$DeltaAdded    = @{}
$DeltaRemoved  = @{}
$DeltaModified = @{}
function Add-Counter {
    param([hashtable]$Table, [int]$Key, [int]$Delta = 1)
    if ($Table.ContainsKey($Key)) { $Table[$Key] = [int]$Table[$Key] + $Delta }
    else { $Table[$Key] = $Delta }
}
foreach ($row in $AddedPaths)    { Add-Counter $DeltaAdded    ([int]$row[2]) }
foreach ($row in $ModifiedPaths) { Add-Counter $DeltaModified ([int]$row[2]) }
foreach ($p in $RemovedPaths) {
    $prev = $PreviousFiles[$p]
    Add-Counter $DeltaRemoved ([int]$prev[2])
}

# ============================ 6. 组装 chunks 段 ============================
$ChunkNames = @{}
$RuleDoc = Get-Content -LiteralPath (Join-Path $ProjectRoot 'Config\DefaultCrossingChunk.ini') -Encoding UTF8 -ErrorAction SilentlyContinue
foreach ($line in $RuleDoc) {
    $nameMatch = [regex]::Match($line, 'ChunkName="([^"]*)"')
    $idMatch   = [regex]::Match($line, 'ChunkId=(\d+)')
    if ($nameMatch.Success -and $idMatch.Success) {
        $ChunkNames[[int]$idMatch.Groups[1].Value] = $nameMatch.Groups[1].Value
    }
}

$ChunksOut = @()
foreach ($c in ($Sections | Select-Object -ExpandProperty ChunkId | Sort-Object -Unique)) {
    $onChunk = @($Sections | Where-Object { $_.ChunkId -eq $c })
    $isNewBucket = $onChunk.Name -contains 'CC_NewContent'
    $name = if ($ChunkNames.ContainsKey($c)) { $ChunkNames[$c] }
            elseif ($isNewBucket) { '本次新增（自动）' }
            else { "Chunk $c" }
    $contBytes = 0
    if ($ContainerBytes.ContainsKey("pakchunk$c")) { $contBytes = $ContainerBytes["pakchunk$c"] }
    $delta = [ordered]@{ added = 0; removed = 0; modified = 0; bytes = 0L }
    if ($DeltaAdded.ContainsKey($c))    { $delta['added']    = $DeltaAdded[$c] }
    if ($DeltaRemoved.ContainsKey($c))  { $delta['removed']  = $DeltaRemoved[$c] }
    if ($DeltaModified.ContainsKey($c)) { $delta['modified'] = $DeltaModified[$c] }
    $ChunksOut += [ordered]@{
        id          = $c
        name        = $name
        kind        = if ($isNewBucket) { 'unclassified' } else { 'rule' }
        matchedFiles= if ($ChunkHit.ContainsKey($c)) { $ChunkHit[$c] } else { 0 }
        bytes       = $contBytes
        delta       = $delta
    }
}

# ============================ 7. 未分类清单 ============================
$PrevUnclassFolders = @{}
if ($Previous) { foreach ($u in $Previous.unclassified) { $PrevUnclassFolders[$u.folder] = $true } }

# 未分类的字节：从文件行里加总（包体积那一步已经填过 size）
$UnclassBytes = @{}
foreach ($row in $FileRows) {
    if ([int]$row[2] -ne [int]$NewChunkId) { continue }
    $d = Get-GameFolderOf $row[0]
    if ($UnclassBytes.ContainsKey($d)) { $UnclassBytes[$d] = [int64]$UnclassBytes[$d] + [int64]$row[1] }
    else { $UnclassBytes[$d] = [int64]$row[1] }
}

$UnclassOut = @()
foreach ($k in ($UnclassByFolder.Keys | Sort-Object)) {
    $b = 0
    if ($UnclassBytes.ContainsKey($k)) { $b = $UnclassBytes[$k] }
    $UnclassOut += [ordered]@{
        folder = $k
        files  = [int]$UnclassByFolder[$k]
        bytes  = $b
        isNew  = -not $PrevUnclassFolders.ContainsKey($k)
    }
}

# ============================ 8. 体检 ============================
$Issues = @()
foreach ($c in $ChunksOut) {
    if ($c.matchedFiles -eq 0) {
        $Issues += [ordered]@{
            level = 'warn'; code = 'EmptyRule'; target = "chunk$($c.id)"
            message = "chunk$($c.id)「$($c.name)」一条文件都没命中"
        }
    }
}
if ($UnclassOut.Count -gt 0) {
    $Issues += [ordered]@{
        level = 'info'; code = 'UnclassifiedNotEmpty'; target = "chunk$NewChunkId"
        message = "还有 $($UnclassOut.Count) 个目录没归类（会进 pakchunk$NewChunkId，作为本次新增）"
    }
}

$ChangedSet = @{}
foreach ($t in @($DeltaAdded, $DeltaRemoved, $DeltaModified)) {
    foreach ($k in $t.Keys) { $ChangedSet[$k] = $true }
}
$ChangedChunks = @($ChangedSet.Keys | Sort-Object)
$DownloadBytes = 0
foreach ($c in $ChangedChunks) { $DownloadBytes += [int64]$ContainerBytes["pakchunk$c"] }

$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'

# ============================ 9. 打包设置快照 ============================
# 把「这次是哪套设置打出来的」记进报告：引擎/Android/打包相关字段 + 本次勾选的地图。
# 以后排查"这包是用哪套设置打的"不用再去翻 ini，也能和上一次对比。
function Get-IniValue {
    param([string]$File, [string]$Key)
    if (-not (Test-Path -LiteralPath $File)) { return $null }
    $hit = Select-String -LiteralPath $File -Encoding UTF8 -Pattern ('^\s*' + [regex]::Escape($Key) + '=') | Select-Object -First 1
    if ($null -eq $hit) { return $null }
    return ($hit.Line -replace ('^\s*' + [regex]::Escape($Key) + '='), '').Trim()
}

$EngineIniPath = Join-Path $ProjectRoot 'Config\DefaultEngine.ini'
$GameIniFullPath = Join-Path $ProjectRoot 'Config\DefaultGame.ini'
$SettingsSnapshot = [ordered]@{}

# Android / 平台设置（写在 DefaultEngine.ini 的 AndroidRuntimeSettings 段里）
foreach ($key in @('PackageName', 'VersionDisplayName', 'StoreVersion', 'ApplicationDisplayName',
                   'MinSDKVersion', 'TargetSDKVersion', 'bBuildForArm64', 'bBuildForX8664',
                   'bPackageDataInsideApk', 'bEnableBundle', 'Orientation', 'bSupportsVulkan',
                   'bMultiTargetFormat_ASTC', 'bMultiTargetFormat_ETC2')) {
    $v = Get-IniValue -File $EngineIniPath -Key $key
    if ($null -ne $v -and $v -ne '') { $SettingsSnapshot[$key] = $v }
}

# 打包相关（DefaultGame.ini 的 ProjectPackagingSettings 段）
foreach ($key in @('UsePakFile', 'bUseIoStore', 'bUseZenStore', 'bGenerateChunks',
                   'PackageCompressionFormat', 'bBuildHttpChunkInstallData')) {
    $v = Get-IniValue -File $GameIniFullPath -Key $key
    if ($null -ne $v -and $v -ne '') { $SettingsSnapshot[$key] = $v }
}

# 本次实际传的 -map=（从打包日志的命令行里抓，最接近"这次真的 cook 了什么"）
if (-not [string]::IsNullOrWhiteSpace($PackLogPath) -and (Test-Path -LiteralPath $PackLogPath)) {
    $mapHits = Select-String -LiteralPath $PackLogPath -Encoding UTF8 -Pattern '-map=(\S+)' -AllMatches |
        ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value.Trim('"') } | Select-Object -Unique
    if ($mapHits) { $SettingsSnapshot['MapsToCookThisRun'] = ($mapHits -join ', ') }
}

# 图标：不是 ini 值，是文件约定（Windows 靠 Build\Windows\Application.ico，
# 安卓靠 Build\Android\res\drawable*/icon.png）。"换了图标忘了放/忘了重打"
# 是最容易发生的一种"打完才想起来"，所以也记进快照。
$WinIconFile = Join-Path $ProjectRoot 'Build\Windows\Application.ico'
$SettingsSnapshot['WindowsIconExists'] = [bool](Test-Path -LiteralPath $WinIconFile)
if (Test-Path -LiteralPath $WinIconFile) {
    $SettingsSnapshot['WindowsIconTime'] = (Get-Item -LiteralPath $WinIconFile).LastWriteTime.ToString('yyyy-MM-dd HH:mm')
}
$AndroidIconFiles = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Build\Android\res') -Recurse -Filter 'icon.png' -ErrorAction SilentlyContinue)
$SettingsSnapshot['AndroidIconCount'] = $AndroidIconFiles.Count
if ($AndroidIconFiles.Count -gt 0) {
    $SettingsSnapshot['AndroidIconTime'] = ($AndroidIconFiles | Sort-Object LastWriteTime -Descending |
        Select-Object -First 1).LastWriteTime.ToString('yyyy-MM-dd HH:mm')
}

# 和引擎自带默认图标比 hash：一样就说明"还没换过"（比看大小/靠感觉可靠）
$AndroidResDir = Join-Path $ProjectRoot 'Build\Android\res'
$EngineAndroidResDir = Join-Path $EngineRoot 'Engine\Build\Android\Java\res'
$IconCompared = 0
$IconDefault = 0
foreach ($icon in $AndroidIconFiles) {
    $rel = $icon.FullName.Substring($AndroidResDir.Length).TrimStart('\')
    $engineIcon = Join-Path $EngineAndroidResDir $rel
    if (-not (Test-Path -LiteralPath $engineIcon)) { continue }
    $IconCompared++
    if ((Get-FileHash -LiteralPath $icon.FullName -Algorithm MD5).Hash -eq
        (Get-FileHash -LiteralPath $engineIcon -Algorithm MD5).Hash) { $IconDefault++ }
}
$SettingsSnapshot['AndroidIconCompared'] = $IconCompared
$SettingsSnapshot['AndroidIconDefaultCount'] = $IconDefault

$Report = [ordered]@{
    schemaVersion  = 1
    reportId       = "$Stamp-$Mode"
    generatedAt    = (Get-Date -Format 'yyyy-MM-ddTHH:mm:sszzz')
    mode           = $Mode
    platform       = $Platform
    target         = $Target
    project        = $ProjectName
    packLog        = if ($PackLogPath -ne '') { Split-Path -Leaf $PackLogPath } else { '' }
    previousReport = if ($Previous) { $Previous.reportId } else { $null }
    chunks         = $ChunksOut
    unclassified   = $UnclassOut
    issues         = $Issues
    download       = [ordered]@{ changedChunks = $ChangedChunks; bytes = $DownloadBytes }
    settings       = $SettingsSnapshot
    containerStats = $HasContainerStats
    files          = $FileRows
}

if ($WhatIf) {
    Write-Host '[WhatIf] 不写文件。摘要：' -ForegroundColor Yellow
} else {
    $json = $Report | ConvertTo-Json -Depth 6 -Compress
    $reportPath = Join-Path $ReportDir "$Stamp-$Mode.json"
    [System.IO.File]::WriteAllText($reportPath, $json, (New-Object System.Text.UTF8Encoding($false)))
    [System.IO.File]::WriteAllText($LatestPath, $json, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "  报告已写入: $reportPath" -ForegroundColor Green
}

# ============================ 控制台摘要 ============================
Write-Host ''
Write-Host '========== 分包报告 ==========' -ForegroundColor Cyan
Write-Host ('  {0,-6} {1,-24} {2,10} {3,14} {4,10}' -f 'chunk', '名称', '文件数', '体积(MB)', '变化') -ForegroundColor DarkGray
foreach ($c in $ChunksOut) {
    $mb = [math]::Round($c.bytes / 1MB, 1)
    $d  = $c.delta
    $chg = if ($d.added -or $d.removed -or $d.modified) { "+$($d.added) -$($d.removed) ~$($d.modified)" } else { '' }
    Write-Host ('  {0,-6} {1,-24} {2,10} {3,14} {4,10}' -f $c.id, $c.name, $c.matchedFiles, $mb, $chg)
}
Write-Host ("  源资产哈希：{0} 个文件 / {1} 秒（{2} 个源文件不存在或读不了，那些只比大小）" -f $HashedCount, $HashSeconds, $SourceMissing) -ForegroundColor DarkGray
if ($TransitionMode) {
    Write-Host ('  过渡期：上一次报告里没有哈希列 —— 这一趟用「源文件时间比上次报告晚」兜底，算作「改」的有 {0} 个' -f $TransitionChanged.Count) -ForegroundColor Yellow
    Write-Host '          （插件 cook 期重写过、内容其实没变的资源也会被算进来；下一趟起只看内容哈希，就干净了）' -ForegroundColor DarkGray
}
Write-Host ''
if ($UnclassOut.Count -gt 0) {
    Write-Host "  未分类 $($UnclassOut.Count) 个目录（→ pakchunk$NewChunkId）：" -ForegroundColor Yellow
    foreach ($u in ($UnclassOut | Sort-Object -Property @{Expression={-$_.files}} | Select-Object -First 10)) {
        $tag = if ($u.isNew) { '新增' } else { '老欠账' }
        Write-Host ("    [{0}] {1}  ({2} 个文件)" -f $tag, $u.folder, $u.files) -ForegroundColor Yellow
    }
}
if (-not $HasContainerStats) {
    # 没有容器可量的时候别报「需下载约 0 MB」—— 那会被读成"不用下载"。
    # 变化清单本身是有的（它是文件级的差异），只是下载量算不出来。
    if ($ChangedChunks.Count -gt 0) {
        Write-Host ("  变化的容器：{0}（这次没有容器文件可量，下载量算不出来）" -f ($ChangedChunks -join ', ')) -ForegroundColor Green
    }
    elseif ($Previous) {
        Write-Host '  和上一次报告相比没有变化' -ForegroundColor Green
    }
    else {
        Write-Host '  首次报告 —— 没有对比基准，下次打包才会有"变化"数据' -ForegroundColor DarkGray
    }
}
elseif ($ChangedChunks.Count -gt 0) {
    Write-Host ("  玩家本次需下载约 {0} MB（变化的容器：{1}）" -f [math]::Round($DownloadBytes / 1MB, 1), ($ChangedChunks -join ', ')) -ForegroundColor Green
}
elseif ($Previous) {
    Write-Host '  和上一次报告相比没有变化' -ForegroundColor Green
}
else {
    Write-Host '  首次报告 —— 没有对比基准，下次打包才会有"变化"数据' -ForegroundColor DarkGray
}
Write-Host ''

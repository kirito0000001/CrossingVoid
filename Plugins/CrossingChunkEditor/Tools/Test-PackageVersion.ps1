<#
.SYNOPSIS
  校验产物目录的版本戳，防止"旧包被当成新版本发出去"。

.DESCRIPTION
  2026-10-26 那次事故：安卓碎片用的源目录里躺着 10-11 打的 APK/OBB（版本 0.6.0），
  但这次发布用的是新版本号 —— 结果新号发的是旧包，而且没有任何一处会报错。

  Pack-CrossingVoid.ps1 每次打包成功后会在归档根写版本戳。2026-10-28 起固定三份：
    Version-Full.json   整包（kind=Full）
    Version-Patch.json  补丁（kind=Patch，带 baseVersion = 基于哪条线上基线）
    Latest.json         指针：最后一次写的是哪份、生成时间、插件版本
  本脚本只认 Latest.json 这个指针：它指向哪份就读哪份，缺失或指向的文件不存在都直接判失败。

  本脚本就是"发布前先看一眼"的那道闸：拿错目录、版本对不上、包太旧、补丁基线过期、
  清单里的文件和实际不符，都直接失败。

  用法（生成碎片 / 上传之前跑一次）：
    .\Test-PackageVersion.ps1 -ArchiveDir "F:\DaBaoV\Fantasy" -ExpectedVersion 0.6.1
    .\Test-PackageVersion.ps1 -ArchiveDir "F:\DaBaoV\Fantasy" -ExpectedVersion 0.6.1 -Platform Android
    .\Test-PackageVersion.ps1 -ArchiveDir "F:\DaBaoV\Fantasy" -ExpectedVersion 0.6.1 -ExpectedBaseVersion 0.6

  退出码：0 = 通过，1 = 不通过（原因逐条打在屏幕上，最后一条 [FAIL] 就是拦下它的理由）。

.PARAMETER ArchiveDir
  归档根目录（面板上的「输出目录」，脚本的 -ArchiveDir）。版本戳就在这一层。

.PARAMETER ExpectedVersion
  这次打算发的版本号。留空 = 只检查"有没有版本戳 / 包新不新"，不比对版本。

.PARAMETER ExpectedBaseVersion
  期望的基线版本（补丁才有）。给了就校验 baseVersion 是否一致 —— 补丁是相对哪条线上基线
  生成的，对不上就是"在旧基线上打的补丁"，发出去会和线上版本错位。

.PARAMETER Platform
  Android / Win64。给了就顺带确认该平台的产物文件真的在（Android：.apk + .obb），
  并核对 Latest.json 指向的是不是这个平台（不一致只 WARN，因为刚打过另一个平台是正常的）。

.PARAMETER MaxAgeHours
  版本戳超过这么多小时就当"旧包"（默认 24）。

.PARAMETER VerifyHashes
  逐个重算清单里文件的 sha256（默认只比大小）。打包产物动辄上 GB，全量哈希很慢，
  只在怀疑"文件被换过"时打开。

.PARAMETER ExpectedNetworkVersion
  期望的"网络兼容版本"（GeneralProjectSettings.ProjectVersion，空 = 引擎退回 1.0.0）。
  给了就校验：客户端包和服务器包这个值必须一样，否则引擎在握手阶段直接拒连。
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveDir,
    [string]$ExpectedVersion = '',
    [string]$ExpectedBaseVersion = '',
    [ValidateSet('', 'Android', 'Win64')][string]$Platform = '',
    [int]$MaxAgeHours = 24,
    [switch]$VerifyHashes,
    [string]$ExpectedNetworkVersion = ''
)

$ErrorActionPreference = 'Stop'

$Fail = @()
function Ok   { param([string]$m) Write-Host "  [OK]   $m" -ForegroundColor Green }
function Warn { param([string]$m) Write-Host "  [WARN] $m" -ForegroundColor Yellow }
function Bad  { param([string]$m) $script:Fail += $m; Write-Host "  [FAIL] $m" -ForegroundColor Red }

Write-Host ''
Write-Host '=== 产物版本自检 ===' -ForegroundColor Cyan
Write-Host "  归档目录 : $ArchiveDir"
if ($ExpectedVersion) { Write-Host "  期望版本 : $ExpectedVersion" }
Write-Host ''

if (-not (Test-Path -LiteralPath $ArchiveDir)) {
    Bad "目录不存在：$ArchiveDir"
    Write-Host ''
    Write-Host '自检不通过' -ForegroundColor Red
    exit 1
}

# ---- 找版本戳：只认 Latest.json 这个指针 ----
# 不再按固定名去猜：指针缺失/指向的文件不存在，本身就说明这个目录不完整，必须拦下。
$LatestPath = Join-Path $ArchiveDir 'Latest.json'
if (-not (Test-Path -LiteralPath $LatestPath)) {
    Bad "没有版本戳指针：$LatestPath"
    Write-Host '         这个目录不是用 CrossingChunkEditor 打的包（或 Latest.json 被删了）。' -ForegroundColor DarkGray
    Write-Host '         旧包被当成新包发出去，通常就是从这里开始的 —— 目录指错了，没人发现。' -ForegroundColor DarkGray
    Write-Host ''
    Write-Host '自检不通过' -ForegroundColor Red
    exit 1
}

try {
    $Latest = Get-Content -LiteralPath $LatestPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
catch {
    Bad "Latest.json 解析失败：$($_.Exception.Message)"
    Write-Host ''
    Write-Host '自检不通过' -ForegroundColor Red
    exit 1
}

if ([string]::IsNullOrWhiteSpace([string]$Latest.latestFile)) {
    Bad 'Latest.json 没有 latestFile —— 不知道该读哪份版本戳'
    Write-Host ''
    Write-Host '自检不通过' -ForegroundColor Red
    exit 1
}

$StampPath = Join-Path $ArchiveDir ([string]$Latest.latestFile)
if (-not (Test-Path -LiteralPath $StampPath)) {
    Bad ("Latest.json 指向的 {0} 不存在 —— 指针过期，目录不完整" -f $Latest.latestFile)
    Write-Host ''
    Write-Host '自检不通过' -ForegroundColor Red
    exit 1
}

try {
    $Stamp = Get-Content -LiteralPath $StampPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
catch {
    Bad "版本戳解析失败（$StampPath）：$($_.Exception.Message)"
    Write-Host ''
    Write-Host '自检不通过' -ForegroundColor Red
    exit 1
}

$Ver = [string]$Stamp.version
$ArtifactDir = [string]$Stamp.artifactDir
$Mode = [string]$Stamp.mode
$Config = [string]$Stamp.configuration
$BaseVer = [string]$Stamp.baseVersion
$Kind = [string]$Stamp.kind
$GeneratedAt = [string]$Stamp.generatedAt

Ok ("版本戳：{0}（kind={1}）" -f (Split-Path -Leaf $StampPath), $Kind)

# ---- 时间：这东西到底多旧 ----
$AgeText = '?'
if ([string]::IsNullOrWhiteSpace($GeneratedAt)) {
    Bad '版本戳没有 generatedAt —— 分不清是新包还是上次留下的'
}
else {
    try {
        $PackedAt = [datetime]::Parse($GeneratedAt)
        $Age = (Get-Date) - $PackedAt
        $AgeText = ('{0:N1} 小时前' -f $Age.TotalHours)
        if ($Age.TotalHours -gt $MaxAgeHours) {
            Bad ("版本戳太旧：{0}（{1}，上限 {2} 小时）—— 这大概率是上一次打包留下的旧包" -f $GeneratedAt, $AgeText, $MaxAgeHours)
        }
        else {
            Ok ("打包时间 {0}（{1}）" -f $GeneratedAt, $AgeText)
        }
    }
    catch {
        Bad "generatedAt 解析不了：$GeneratedAt"
    }
}

# ---- 版本号：跟这次要发的对不对得上 ----
if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion)) {
    if ($Ver -eq $ExpectedVersion) {
        Ok "版本号一致：$Ver"
    }
    else {
        Bad ("版本号不一致：产物里是 '{0}'，这次要发的是 '{1}' —— 就是【旧包拿新号】那一类" -f $Ver, $ExpectedVersion)
    }
}
elseif ([string]::IsNullOrWhiteSpace($Ver)) {
    Warn '产物里没有版本号（打包时没填「玩家版本」）'
}
else {
    Ok "版本号：$Ver"
}

# ---- 类型：整包还是补丁 ----
if ($Kind -notin @('Full', 'Patch')) {
    Bad ("kind 不合法：'{0}'（应该是 Full 或 Patch）" -f $Kind)
}

# ---- 补丁的基线版本：判断补丁是不是过期 ----
if ($Kind -eq 'Patch') {
    if ([string]::IsNullOrWhiteSpace($BaseVer)) {
        Bad '补丁没有 baseVersion —— 不知道基于哪条线上基线打的，不能发'
    }
    elseif ([string]::IsNullOrWhiteSpace($ExpectedBaseVersion)) {
        Ok ("补丁基线：{0}（要校验就传 -ExpectedBaseVersion）" -f $BaseVer)
    }
    elseif ($BaseVer -eq $ExpectedBaseVersion) {
        Ok "补丁基线一致：$BaseVer"
    }
    else {
        Bad ("补丁基线不一致：补丁基于 '{0}'，期望 '{1}' —— 旧基线上打的补丁，发出去会和线上错位" -f $BaseVer, $ExpectedBaseVersion)
    }
}
elseif (-not [string]::IsNullOrWhiteSpace($ExpectedBaseVersion)) {
    Warn '这是整包（kind=Full），没有 baseVersion；-ExpectedBaseVersion 被忽略'
}

# ---- 网络兼容版本：它才是"连不连得上"的开关（内容版本再新也救不了）----
# 引擎握手用的 NetCL = CRC32(工程名 + ProjectVersion + 引擎 NetCL + 网络协议版本)，严格相等才放行。
# 所以客户端包和服务器包这一项必须一致；不一致时客户端在 connect 阶段就被拒，看不到任何提示。
$NetVerEff = [string]$Stamp.networkVersionEffective
if ([string]::IsNullOrWhiteSpace($NetVerEff)) {
    $NetVerEff = '1.0.0'   # 老版本戳没有这个字段：引擎在未设置时就是退回 1.0.0
}
$NetVerSet = $Stamp.networkVersionSet
if ($null -eq $NetVerSet) { $NetVerSet = -not [string]::IsNullOrWhiteSpace([string]$Stamp.networkVersion) }

if ([string]::IsNullOrWhiteSpace($ExpectedNetworkVersion)) {
    if ($NetVerSet -eq $false) {
        Ok ('网络兼容版本：{0}（未显式设置，引擎默认值；两端都用默认值就是一致的）' -f $NetVerEff)
    }
    else {
        Warn ('网络兼容版本：{0}（**显式设置过** —— 客户端和服务器包必须一模一样，否则连不上）' -f $NetVerEff)
    }
}
else {
    $ExpectedNetEff = $ExpectedNetworkVersion.Trim()
    if ($ExpectedNetEff -ieq $NetVerEff) {
        Ok ('网络兼容版本一致：{0}' -f $NetVerEff)
    }
    else {
        Bad ("网络兼容版本不一致：产物里是 '{0}'，期望 '{1}' —— 这类不一致引擎会直接拒连（不是提示）" -f $NetVerEff, $ExpectedNetEff)
    }
}

# ---- 这批包能不能发 ----
if ($Mode -eq 'Quick') {
    Bad '《快速验证》的产物：不打 pak、不能发布'
}
else {
    Ok "模式：$Mode · 配置：$Config"
}

# ---- 产物文件在不在 ----
if ($ArtifactDir) {
    $ArtifactPath = Join-Path $ArchiveDir $ArtifactDir
    if (-not (Test-Path -LiteralPath $ArtifactPath)) {
        Bad "产物目录不存在：$ArtifactPath"
    }
    else {
        if ($Platform -eq 'Android' -or $ArtifactDir -eq 'Android') {
            $Apks = @(Get-ChildItem -LiteralPath $ArtifactPath -Filter '*.apk' -File -ErrorAction SilentlyContinue)
            $Obbs = @(Get-ChildItem -LiteralPath $ArtifactPath -Filter '*.obb' -File -ErrorAction SilentlyContinue)
            if ($Apks.Count -eq 0) { Bad "Android 产物里没有 .apk：$ArtifactPath" }
            else { Ok ("APK：{0}（{1:N1} MB，{2:yyyy-MM-dd HH:mm}）" -f $Apks[0].Name, ($Apks[0].Length / 1MB), $Apks[0].LastWriteTime) }
            if ($Obbs.Count -eq 0) {
                Warn '没有 .obb（如果数据打进了 APK，这是正常的）'
            }
            else {
                Ok ("OBB：{0}（{1:N1} MB，{2:yyyy-MM-dd HH:mm}）" -f $Obbs[0].Name, ($Obbs[0].Length / 1MB), $Obbs[0].LastWriteTime)
            }
        }
        elseif ($Platform -eq 'Win64') {
            $Exes = @(Get-ChildItem -LiteralPath $ArtifactPath -Filter '*.exe' -File -Recurse -ErrorAction SilentlyContinue)
            if ($Exes.Count -eq 0) { Bad "Windows 产物里没有 .exe：$ArtifactPath" }
            else { Ok ("EXE：{0}（{1:N1} MB）" -f $Exes[0].Name, ($Exes[0].Length / 1MB)) }
        }
        else {
            $FileCount = @(Get-ChildItem -LiteralPath $ArtifactPath -Recurse -File -ErrorAction SilentlyContinue).Count
            Ok "产物目录：$ArtifactDir（$FileCount 个文件）"
        }
    }
}

# ---- 产物清单：文件在不在、大小/哈希对不对 ----
# path 是相对归档根的 posix 路径；下游拿它就能直接拼 URL 或本地路径。
$StampFiles = @($Stamp.files)
if ($StampFiles.Count -eq 0) {
    Bad '版本戳里没有产物清单（files 为空）—— 这个包没有可发布的产物'
}
else {
    $MissingFiles = @()
    $SizeBad = @()
    $HashBad = @()

    foreach ($f in $StampFiles) {
        $Rel = [string]$f.path
        if ([string]::IsNullOrWhiteSpace($Rel)) {
            Bad '产物清单里有一项没有 path'
            continue
        }

        $Full = Join-Path $ArchiveDir ($Rel -replace '/', '\')
        if (-not (Test-Path -LiteralPath $Full)) {
            $MissingFiles += $Rel
            continue
        }

        $Item = Get-Item -LiteralPath $Full
        if ($null -ne $f.size -and $Item.Length -ne [int64]$f.size) {
            $SizeBad += $Rel
        }

        if ($VerifyHashes -and -not [string]::IsNullOrWhiteSpace([string]$f.sha256)) {
            $Actual = (Get-FileHash -LiteralPath $Full -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($Actual -ne ([string]$f.sha256).ToLowerInvariant()) {
                $HashBad += $Rel
            }
        }
    }

    if ($MissingFiles.Count -gt 0) {
        Bad ("版本戳里的产物不存在：{0}" -f ($MissingFiles -join '、'))
    }
    if ($SizeBad.Count -gt 0) {
        Bad ("产物大小和版本戳不一致（文件被换过？）：{0}" -f ($SizeBad -join '、'))
    }
    if ($HashBad.Count -gt 0) {
        Bad ("产物 sha256 和版本戳不一致（文件被换过？）：{0}" -f ($HashBad -join '、'))
    }
    if ($MissingFiles.Count -eq 0 -and $SizeBad.Count -eq 0 -and $HashBad.Count -eq 0) {
        $HashNote = if ($VerifyHashes) { '，sha256 一致' } else { '（默认只比大小；要重算哈希加 -VerifyHashes）' }
        Ok ("产物清单 {0} 项，文件都在{1}" -f $StampFiles.Count, $HashNote)
    }
}

Write-Host ''
if ($Fail.Count -gt 0) {
    Write-Host ("自检不通过（{0} 项）：" -f $Fail.Count) -ForegroundColor Red
    foreach ($f in $Fail) { Write-Host "  - $f" -ForegroundColor Red }
    Write-Host ''
    Write-Host '结论：别发。先把包重打一次，或者把源目录指对（指向这次刚打出来的产物目录）。' -ForegroundColor Yellow
    exit 1
}

Write-Host '自检通过 —— 版本 / 时间 / 产物都对得上，可以拿去生成碎片和发布' -ForegroundColor Green
Write-Host ''
exit 0

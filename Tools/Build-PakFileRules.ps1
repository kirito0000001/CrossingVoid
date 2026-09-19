<#
.SYNOPSIS
    零境交错 · 分包规则生成器：CrossingChunkRuleSet  →  DefaultPakFileRules.ini

.DESCRIPTION
    把 Config\DefaultCrossingChunk.ini 里的「目录 → chunk」规则，翻译成引擎能吃的
    Config\DefaultPakFileRules.ini。

    为什么用 PakFileRules（UE 5.8 自带，不用改引擎、不用写 C#）：
      · Engine\Config\BasePakFileRules.ini 是官方说明
      · AutomationTool\Scripts\CopyBuildToStagingDirectory.Automation.cs
          GetPakFileRules()    第 3120 行 —— 读「引擎 Base + 工程 Default」两个 ini
          ApplyPakFileRules()  第 3448 行 —— 逐文件匹配；bOverrideChunkManifest=true 时
                                          清空 cooker 给的归属，改成 OverridePaks
          第 3499 行 —— 目标 chunk 不存在就现场新建

    两条关键语义：
      1. 规则按「书写顺序」生效，每个文件只采纳第一条匹配的规则
         → 所以本脚本把规则按「目录层级从深到浅」排序，
           天然实现「子目录优先、未分配走父目录」
      2. 兜底规则放最后 → 所有没被任何 chunk 认领的 Content 文件归基础包

.PARAMETER RuleIni
    规则来源（默认 Config\DefaultCrossingChunk.ini）

.PARAMETER OutputIni
    生成目标（默认 Config\DefaultPakFileRules.ini，整份重写）

关于「未分类资源进哪个 chunk」（没有参数可调，就是按顺序自动排）：
    未分类 = 规则里还没登记过的目录 = 本次新增内容。
    它们不进基础包（chunk 0），而是单独成包，号 = 现有最大 chunk 号 + 1。
    这是一次常规补丁的载体：补丁一个一个往后排，启动器只需要下载最新的那个包。
    到下一次「大版本更新」时，整份规则会重新规划一遍，这些补丁号自然就作废了。

.PARAMETER NoCatchAll
    不生成兜底规则（未分类资源会留在引擎默认分配器给的 chunk 里）

.PARAMETER ProjectName
    工程名，用来把匹配掩码限定在本工程自己的内容上（默认从 -RuleIni 路径推出来）。
    不加这个限定的话，`.../Content/...` 这种掩码会把引擎自带的 Engine\Content 也一起卷进来。

.PARAMETER WhatIf
    只打印将要生成的规则，不写文件

.EXAMPLE
    .\Build-PakFileRules.ps1

.EXAMPLE
    .\Build-PakFileRules.ps1 -WhatIf
#>
param(
    [string]$RuleIni = 'C:\CrossingVoid\Config\DefaultCrossingChunk.ini',
    [string]$OutputIni = 'C:\CrossingVoid\Config\DefaultPakFileRules.ini',
    [string]$RuleSection = '/Script/CrossingChunk.CrossingChunkRuleSet',
    [string]$ProjectName = '',
    [switch]$NoCatchAll,
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'

# 工程名：默认从规则文件路径推（<工程>\Config\DefaultCrossingChunk.ini）
if ([string]::IsNullOrWhiteSpace($ProjectName)) {
    $configDir = Split-Path -Parent $RuleIni
    $ProjectName = Split-Path -Leaf (Split-Path -Parent $configDir)
}

# ============================ 读取规则 ============================
if (-not (Test-Path -LiteralPath $RuleIni)) {
    throw "[检查失败] 找不到规则文件：`n  $RuleIni"
}

$RuleLines = Get-Content -LiteralPath $RuleIni -Encoding UTF8

$SectionIndex = -1
for ($i = 0; $i -lt $RuleLines.Count; $i++) {
    if ($RuleLines[$i].Trim() -eq "[$RuleSection]") { $SectionIndex = $i; break }
}
if ($SectionIndex -lt 0) {
    throw "[检查失败] 规则文件里找不到段 [$RuleSection]：`n  $RuleIni"
}

function Get-Field {
    param([string]$Text, [string]$Pattern)
    $m = [regex]::Match($Text, $Pattern)
    if ($m.Success) { return $m.Groups[1].Value }
    return ''
}

$Chunks     = @()
$Assignments = @()
$Warnings   = @()

for ($i = $SectionIndex + 1; $i -lt $RuleLines.Count; $i++) {
    $line = $RuleLines[$i].Trim()
    if ($line -match '^\[.*\]$') { break }          # 进入下一段，规则段结束
    if ($line -notmatch '^\+Chunks=\(') { continue } # 只认 +Chunks=(...)

    $ChunkId = 0
    $idText  = Get-Field $line 'ChunkId=(\d+)'
    if ($idText -ne '') { $ChunkId = [int]$idText }

    $ChunkName = Get-Field $line 'ChunkName="([^"]*)"'
    if ($ChunkName -eq '') { $ChunkName = "chunk$ChunkId" }

    # Folders=((Path="..."),(Path="..."))，停止在 ,Priority= 之前
    $foldersBlock = Get-Field $line 'Folders=\((.*?)\),Priority='
    $folderPaths  = @()
    foreach ($m in [regex]::Matches($foldersBlock, 'Path="([^"]+)"')) {
        $folderPaths += $m.Groups[1].Value
    }

    # ForceExcludeFolders 夹在 = 和 ,Comment= 之间；本工程目前是空的
    $excludeBlock  = Get-Field $line 'ForceExcludeFolders=(.*?),Comment='
    $excludePaths  = @()
    foreach ($m in [regex]::Matches($excludeBlock, 'Path="([^"]+)"')) {
        $excludePaths += $m.Groups[1].Value
    }

    $InInstall = (Get-Field $line 'bIncludeInInstallPackage=(True|False)') -eq 'True'
    $Comment   = Get-Field $line 'Comment="([^"]*)"'

    $Chunks += [pscustomobject]@{
        ChunkId   = $ChunkId
        ChunkName = $ChunkName
        InInstall = $InInstall
        Comment   = $Comment
        Folders   = $folderPaths
        Excludes  = $excludePaths
    }

    foreach ($folder in $folderPaths) {
        $Assignments += [pscustomobject]@{
            Folder    = $folder.TrimEnd('/')
            ChunkId   = $ChunkId
            ChunkName = $ChunkName
            Excludes  = $excludePaths
        }
    }
}

if ($Chunks.Count -eq 0) {
    throw "[检查失败] 段 [$RuleSection] 里一条 +Chunks= 都没解析到（规则文件：$RuleIni）"
}

# ============================ 校验 ============================
$dupes = $Assignments | Group-Object Folder | Where-Object { $_.Count -gt 1 }
foreach ($d in $dupes) {
    $ids = ($d.Group | ForEach-Object { "chunk$($_.ChunkId)" }) -join ', '
    $Warnings += "目录 $($d.Name) 同时出现在：$ids —— 只保留层级最深/最先声明的那条"
}

# 同一个目录被多个 chunk 认领时，保留 chunk 序号小的那个（再按深度排序覆盖）
$Assignments = $Assignments |
    Sort-Object Folder, ChunkId |
    Group-Object Folder |
    ForEach-Object { $_.Group | Select-Object -First 1 }

# 深度从深到浅 —— 这是「子目录优先」的全部秘密
$Assignments = $Assignments | Sort-Object `
    @{ Expression = { ($_.Folder -split '/').Count }; Descending = $true },
    @{ Expression = { $_.Folder }; Descending = $false }

# ============================ 生成内容 ============================

# 兜底规则的目标 chunk：按顺序自动排，不给参数调。
#   未分类 = 规则里没登记过的目录 = 本次新增内容 → 单独成包（一次常规补丁的载体），
#   而不是混进基础包（chunk 0）。号 = 现有最大 chunk 号 + 1。
$MaxChunkId = ($Chunks | Measure-Object -Property ChunkId -Maximum).Maximum
if ($null -eq $MaxChunkId) { $MaxChunkId = -1 }
$UnclassifiedChunkId = [int]$MaxChunkId + 1

function ConvertTo-Mask {
    param([string]$GamePath)
    # /Game/AssetMaterial/ImageS/CharaterS
    #   →  .../<工程名>/Content/AssetMaterial/ImageS/CharaterS/...
    #
    # 为什么要带工程名：匹配的是「源文件路径」，形如
    #   C:\<工程>\Saved\Cooked\Windows\<工程>\Content\...
    # 只用 .../Content/... 的话，引擎自带的
    #   D:\UnrealEngine-5.8.2\Engine\Content\...
    # 也会被同一条规则网进来（引擎内容本来该留在基础包）。带上工程名就只会命中自己的内容。
    $rel = $GamePath
    if ($rel.StartsWith('/Game/')) { $rel = $rel.Substring('/Game/'.Length) }
    elseif ($rel.StartsWith('/Game')) { $rel = $rel.Substring('/Game'.Length).TrimStart('/') }
    $rel = $rel.Trim('/')
    return ".../$ProjectName/Content/$rel/..."
}

function ConvertTo-RootMask {
    # 整棵工程 Content 树（兜底规则用）
    return ".../$ProjectName/Content/..."
}

function ConvertTo-SectionName {
    param([int]$ChunkId, [string]$Folder)
    $safe = ($Folder -replace '^/Game/?', '') -replace '[^A-Za-z0-9]', '_'
    if ($safe -eq '') { $safe = 'Root' }
    if ($safe.Length -gt 48) { $safe = $safe.Substring(0, 48) }
    return "CC_${ChunkId}_$safe"
}

$out = [System.Collections.Generic.List[string]]::new()
$out.Add('; ==========================================================================')
$out.Add(';  本文件由 Tools\Build-PakFileRules.ps1 自动生成 —— 不要手改')
$out.Add(';')
$out.Add(";  生成时间 : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$out.Add(";  规则来源 : $RuleIni")
$out.Add(";  规则段   : [$RuleSection]")
$out.Add(";  规则条数 : $($Assignments.Count)  条目录规则" + $(if (-not $NoCatchAll) { ' + 1 条兜底' } else { '' }))
$out.Add(';')
$out.Add(';  引擎侧说明在 Engine\Config\BasePakFileRules.ini；')
$out.Add(';  规则按书写顺序生效、每文件只采纳第一条匹配 —— 所以下面按目录层级从深到浅排。')
$out.Add('; ==========================================================================')
$out.Add('')

$seq = 0
foreach ($a in $Assignments) {
    $seq++
    $mask = ConvertTo-Mask $a.Folder
    $out.Add("; [$($a.ChunkId)] $($a.ChunkName)   ←  $($a.Folder)")
    $out.Add("[$(ConvertTo-SectionName -ChunkId $a.ChunkId -Folder $a.Folder)]")
    $out.Add('bOverrideChunkManifest=true')
    $out.Add("OverridePaks=`"pakchunk$($a.ChunkId)`"")
    $out.Add('bOnlyChunkedBuilds=true')
    $out.Add("+Files=`"$mask`"")
    foreach ($ex in $a.Excludes) {
        if ($ex -ne '') {
            $out.Add("-Files=`"$(ConvertTo-Mask $ex)`"")
        }
    }
    $out.Add('')
}

if (-not $NoCatchAll) {
    $out.Add('; --------------------------------------------------------------------------')
    $out.Add('; 基础包固定项：打包产出的「引擎侧文件」——它们不是我们的素材，但启动就要用')
    $out.Add(';')
    $out.Add('; 为什么必须钉死在 chunk 0：')
    $out.Add(';   · ShaderArchive-* / ShaderTypeInfo-* 是 shader 库，游戏启动时就要打开它；')
    $out.Add(';     缺了在 Shipping 下可能直接起不来（不是「加载慢一点」的问题）')
    $out.Add(';   · PipelineCaches/*.upipelinecache 是管线缓存，跟着基础包走最省事')
    $out.Add(';   · Slate/* 是鼠标指针、logo 这类进游戏前就用的图标')
    $out.Add('; 放在兜底 A 之前，免得被「未分类 = 本次新增」那条网走。')
    $out.Add('; --------------------------------------------------------------------------')
    $out.Add('[CC_BaseArtifacts]')
    $out.Add('bOverrideChunkManifest=true')
    $out.Add('OverridePaks="pakchunk0"')
    $out.Add('bOnlyChunkedBuilds=true')
    $out.Add("+Files=`".../$ProjectName/Content/PipelineCaches/...`"")
    $out.Add("+Files=`".../$ProjectName/Content/ShaderArchive-*`"")
    $out.Add("+Files=`".../$ProjectName/Content/ShaderTypeInfo-*`"")
    $out.Add("+Files=`".../$ProjectName/Content/Slate/...`"")
    $out.Add('')
    $out.Add('; --------------------------------------------------------------------------')
    $out.Add("; 兜底 A：本工程 Content 里「没有规则认领」的文件 = 本次新增内容")
    $out.Add(";        → pakchunk$UnclassifiedChunkId（一次常规补丁的载体）")
    $out.Add(';')
    $out.Add('; 为什么不是 chunk 0：规则里没登记过的目录 = 这次新加的资源。')
    $out.Add('; 把它们单独成包，启动器就只需要下一个新包，基础包一点不动。')
    $out.Add('; 想让某批新资源自己成包，就在规则里给它标一个 chunk（右键文件夹即可）。')
    $out.Add('; （哪条规则网住了多少文件，看打包日志里的 `Pak rule ... with N matching files`）')
    $out.Add('; --------------------------------------------------------------------------')
    $out.Add('[CC_NewContent]')
    $out.Add('bOverrideChunkManifest=true')
    $out.Add("OverridePaks=`"pakchunk$UnclassifiedChunkId`"")
    $out.Add('bOnlyChunkedBuilds=true')
    $out.Add("+Files=`"$(ConvertTo-RootMask)`"")
    $out.Add('')
    $out.Add('; --------------------------------------------------------------------------')
    $out.Add('; 兜底 B：其它一切带 Content 的东西（引擎自带内容、插件内容）→ pakchunk0 基础包')
    $out.Add(';')
    $out.Add('; 没有这一条的话，这些「引擎依赖」会留在默认分配器给的 chunk 10 里，')
    $out.Add('; 变成玩家必须额外下载、却又不是我们素材的包。它们属于基础包。')
    $out.Add('; 注意顺序：必须在兜底 A 之后 —— 规则是「首个匹配生效」。')
    $out.Add('; --------------------------------------------------------------------------')
    $out.Add('[CC_BaseRest]')
    $out.Add('bOverrideChunkManifest=true')
    $out.Add('OverridePaks="pakchunk0"')
    $out.Add('bOnlyChunkedBuilds=true')
    $out.Add('+Files=".../Content/..."')
    $out.Add('')
}

# ============================ 汇总 ============================
Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' 分包规则生成' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''
Write-Host ('  {0,-6} {1,-22} {2,10}' -f 'chunk', '名称', '目录数') -ForegroundColor DarkGray
Write-Host ('  ' + ('-' * 48)) -ForegroundColor DarkGray
foreach ($c in ($Chunks | Sort-Object ChunkId)) {
    Write-Host ('  {0,-6} {1,-22} {2,10}' -f $c.ChunkId, $c.ChunkName, $c.Folders.Count)
}
Write-Host ''
Write-Host "  目录规则合计 : $($Assignments.Count) 条" -ForegroundColor Gray
if (-not $NoCatchAll) {
    Write-Host "  收尾 1       : shader 库/管线缓存/图标 → pakchunk0（基础包固定项）" -ForegroundColor Gray
    Write-Host "  收尾 2       : 本工程未分类（本次新增）→ pakchunk$UnclassifiedChunkId" -ForegroundColor Gray
    Write-Host "  收尾 3       : 引擎/插件内容           → pakchunk0（基础包）" -ForegroundColor Gray
}
Write-Host "  输出         : $OutputIni" -ForegroundColor Gray

foreach ($w in $Warnings) {
    Write-Host "  [警告] $w" -ForegroundColor Yellow
}

# 非 /Game 路径（比如插件内容）目前没法翻译成正确的掩码，明确提醒一声，
# 免得规则写进去却一条都匹配不上。
$nonGame = $Assignments | Where-Object { -not $_.Folder.StartsWith('/Game') }
foreach ($n in $nonGame) {
    Write-Host ("  [警告] {0} 不在 /Game 下：目前只会翻译 /Game 路径，这条规则不会生效" -f $n.Folder) -ForegroundColor Yellow
}
Write-Host ''

if ($WhatIf) {
    Write-Host '[WhatIf] 不写文件，以下是将要生成的内容：' -ForegroundColor Yellow
    $out | ForEach-Object { Write-Host $_ }
    return
}

$outDir = Split-Path -Parent $OutputIni
if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}
Set-Content -LiteralPath $OutputIni -Value $out -Encoding UTF8
Write-Host "  已写入 $($out.Count) 行" -ForegroundColor Green
Write-Host ''

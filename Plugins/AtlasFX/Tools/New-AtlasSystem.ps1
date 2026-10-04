<#
.SYNOPSIS
  从图集工具的 *_sequence.json 生成 DreamFX 的 .dfs 系统源（NS_Atlas<图集名>）。

.DESCRIPTION
  「图集工具 → Niagara 特效」这条链的闭环脚本：把 D:\NewData\CrossingVoidZDProject\Tools\Atlas\<名>\<名>_sequence.json
  里的帧矩形 / 画布矩形 / 尺寸 / 帧率，直接写进 .dfs 的 DI 配置 JSON，生成一个可直接 build 的 Niagara 系统源。

  为什么要把帧表写死在 .dfs 里（而不是让 DI 自己在编辑器里烘）：
  DI 的 RefreshFromSource() 只挂在 PostEditChangeProperty 上，DreamFX 用反射写属性不触发它，
  构建出来的 DI 会是空表（运行时回退成整张贴图当一帧）。详见 Plugins/AtlasFX/DESIGN.md。

.PARAMETER Sequence
  图集工具产出的 *_sequence.json 路径（必需）。

.PARAMETER Atlas
  图集名。默认取 sequence.json 里的 "atlas"。

.PARAMETER Fps
  播放帧率。sequence.json 里没有帧率，要从 PaperFlipbook 上抄（Defatk1 是 15）。

.PARAMETER Flipbook
  DI 的 Flipbook 字段（可选）。默认 /Game/AssetMaterial/FXs/通用Flipbook/<Atlas>1。<Atlas>1。
  传空字符串则不在 DI JSON 里写 Flipbook。

.PARAMETER Texture
  兜底用的图集贴图对象路径。默认 /Game/AssetMaterial/FXs/通用Flipbook/Textures/<Atlas>。

.PARAMETER Material
  精灵渲染器用的材质。默认 /AtlasFX/M_FXAtlasSheet。

.PARAMETER Out
  输出的 .dfs 路径。默认 C:\CrossingVoid\DFX\Effects\NS_Atlas<Atlas>.dfs。

.EXAMPLE
  pwsh -File Plugins/AtlasFX/Tools/New-AtlasSystem.ps1 `
      -Sequence D:\NewData\CrossingVoidZDProject\Tools\Atlas\Defatk\Defatk_sequence.json -Fps 15 -DryRun

.EXAMPLE
  # 生成 + 立刻构建（构建前必须先关掉 Unreal Editor）
  pwsh -File Plugins/AtlasFX/Tools/New-AtlasSystem.ps1 `
      -Sequence D:\...\Defatk_sequence.json -Fps 15 -Force -Build
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Sequence,
    [string]$Atlas,
    [double]$Fps = 15,
    [string]$Flipbook,
    [string]$Texture,
    [string]$Material = '/AtlasFX/M_FXAtlasSheet',
    [double]$SizeScale = 0.1,
    [double]$PlayRate = 1.0,
    [double]$StartFrame = 0.0,
    [string]$Out,
    [switch]$Force,
    [switch]$DryRun,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$Inv = [System.Globalization.CultureInfo]::InvariantCulture
function Fmt([double]$v) { return $v.ToString('0.0#####', $Inv) }

if (-not (Test-Path -LiteralPath $Sequence)) { throw "找不到 sequence.json：$Sequence" }
$seqPath = (Resolve-Path -LiteralPath $Sequence).Path
$seq = Get-Content -LiteralPath $seqPath -Raw -Encoding UTF8 | ConvertFrom-Json

if (-not $Atlas) { $Atlas = $seq.atlas }
if (-not $Atlas) { throw 'sequence.json 里没有 "atlas" 字段，请用 -Atlas 指定图集名。' }

$frames = @($seq.frames | Sort-Object index)
if ($frames.Count -eq 0) { throw 'sequence.json 里没有 frames。' }

if (-not $Flipbook) { $Flipbook = "/Game/AssetMaterial/FXs/通用Flipbook/$Atlas`1.$Atlas`1" }
if (-not $Texture) { $Texture = "/Game/AssetMaterial/FXs/通用Flipbook/Textures/$Atlas" }
if (-not $Out) { $Out = Join-Path 'C:\CrossingVoid\DFX\Effects' "NS_Atlas$Atlas.dfs" }

# ---- 校验（这些是已知的表达能力边界，出问题要说出来，不要静默生成）----
$warn = New-Object System.Collections.Generic.List[string]
if ($seq.frameCount -and $seq.frameCount -ne $frames.Count) {
    $warn.Add("frameCount=$($seq.frameCount) 与实际 frames 条数 $($frames.Count) 不一致，按实际条数生成。")
}
$rotated = @($frames | Where-Object { $_.rotated })
if ($rotated.Count -gt 0) {
    $warn.Add("有 $($rotated.Count) 帧 rotated=true（图集工具旋转了贴图），当前 DI/材质不支持旋转帧，会显示成歪的。")
}
$sizes = @($frames | ForEach-Object { "$($_.sourceSize.w)x$($_.sourceSize.h)" } | Select-Object -Unique)
if ($sizes.Count -gt 1) {
    $warn.Add("sourceSize 不统一（$($sizes -join ', ')），当前实现只支持全帧同一画布，取第一帧。")
}
$noSrc = @($frames | Where-Object { -not $_.spriteSourceSize })
if ($noSrc.Count -gt 0) { $warn.Add("有 $($noSrc.Count) 帧缺 spriteSourceSize，画布矩形会退化成 0,0,sourceSize。") }

# ---- 组装 DI 配置 JSON ----
# 帧表不再手写：DI 在资产加载时（UNiagaraDataInterfaceSpriteAtlas::PostLoad）自己从 Flipbook
# 烘出「图集矩形 / 画布矩形 / 画布尺寸 / 贴图尺寸 / 帧率」并存回资产，源签名没变就不重烘。
# 所以默认只写一行 Flipbook；只有显式 -Flipbook ""（不绑 Flipbook）时才写手工兜底数据。
$di = [ordered]@{}
if ($Flipbook) {
    $di['Flipbook'] = $Flipbook
} else {
    # 兜底路径：手工数据源。CanvasRects 用 DI 自己的约定 —— (画布内 x, y, 帧宽, 帧高)，
    # 不是 sequence.json 的 spriteSourceSize + sourceSize（那个是「画布尺寸」的写法）。
    $di['AtlasTexture'] = $Texture
    $di['ManualFrameRects'] = @($frames | ForEach-Object {
            [ordered]@{ x = [int]$_.frame.x; y = [int]$_.frame.y; z = [int]$_.frame.w; w = [int]$_.frame.h }
        })
    $di['ManualCanvasRects'] = @($frames | ForEach-Object {
            $ss = $_.spriteSourceSize
            [ordered]@{
                x = [int]$(if ($ss) { $ss.x } else { 0 })
                y = [int]$(if ($ss) { $ss.y } else { 0 })
                z = [int]$_.frame.w
                w = [int]$_.frame.h
            }
        })
    $di['ManualCanvasSize'] = [ordered]@{ x = [int]$frames[0].sourceSize.w; y = [int]$frames[0].sourceSize.h }
    $di['ManualFps'] = $Fps
}
$diJson = ($di | ConvertTo-Json -Depth 8 -Compress)
$diEscaped = $diJson -replace '"', '\"'

$fpsText = $Fps.ToString('0.#####', $Inv)
$flipbookNote = if ($Flipbook) { "Flipbook $($Flipbook.Split('/')[-1].Split('.')[0]) 的 FramesPerSecond = $fpsText" } else { $fpsText }

# ---- 模板 ----
$dfs = @"
// NS_Atlas$Atlas —— $Atlas 图集特效（文本源，DreamFX 生成；由 Plugins/AtlasFX/Tools/New-AtlasSystem.ps1 从 sequence.json 生成）
//
// 数据层：AtlasFX 插件的 C++ 数据接口 UNiagaraDataInterfaceSpriteAtlas（只支持 CPU 模拟）。
// 播放层：/AtlasFX/Modules/Play_SpriteAtlas —— 由 DFX/Modules/M_PlaySpriteAtlas.dfm 生成，
//         PlayMode 默认 0 = 按 Flipbook 帧率推进帧号，并把当前帧的「图集矩形 / 画布矩形 / 尺寸包」
//         写进 Particles.DynamicMaterialParameter / ...Parameter1 / ...Parameter2
//         （注意第一个属性名没有数字，它对应材质 DynamicParameter 的索引 0）。
// 采样层：$Material —— Unlit + Additive，用 DynamicParameter(0) 和 (2) 把
//         归一化 UV 换算到当前帧的矩形里。
//
// 改这个文件后：关掉编辑器 → pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Effects/NS_Atlas$Atlas.dfs
System(Name="Effects/NS_Atlas$Atlas", Root="Plugin.AtlasFX")
{
    Settings = {
        WarmupTime  = 0.0;
        FixedBounds = box(-300, -300, -300, 300, 300, 300);

        // 让 Play_SpriteAtlas 这个短名能被解析到 /AtlasFX/Modules 下。
        // ModulePaths 是「追加」搜索根，不会顶掉引擎自带的 /Niagara/Modules。
        ModulePaths = [ "/AtlasFX/Modules" ];
    }

    Properties = {
        // 数据接口参数的配置是「一个 JSON 对象字符串」。对象引用写裸路径字符串
        // （导出器会写成 {"refPath":"..."}，适配层在喂给引擎前会把它改回裸字符串，两种都收）。
        //
        // 帧表不手写：DI 在资产加载时（UNiagaraDataInterfaceSpriteAtlas::PostLoad）从 Flipbook
        // 自动烘出「图集矩形 / 画布矩形 / 画布尺寸 / 贴图尺寸 / 帧率」并存回资产；源签名
        // （Flipbook 路径 + 帧数 + 帧率 + 每帧 Sprite 的源矩形）没变就不重复烘。
        // ⇒ 换图集 = 只改这一行 Flipbook 路径（图集工具只负责出 png + Flipbook）。
        // 手工兜底数据源（AtlasTexture + ManualFrameRects/ManualCanvasRects/ManualCanvasSize/ManualFps）
        // 也支持：用 -Flipbook "" 生成，那种情况下 DI 不自动烘。
        //
        // 生成来源：$seqPath（$($frames.Count) 帧 / 贴图 $($seq.texture.w)x$($seq.texture.h) / 画布 $($frames[0].sourceSize.w)x$($frames[0].sourceSize.h) / $flipbookNote）
        DI<SpriteAtlas> Atlas = "$diEscaped";

        float SizeScale  = $(Fmt $SizeScale) [ Group="图集"; SortPriority=10;
            Description="1 像素对应多少世界单位（0.1 = 1 毫米）。面片尺寸 = 当前帧尺寸 × 这个值。" ];
        float PlayMode   = 0.0 [ Group="图集"; SortPriority=15;
            Description="播放模式：0 = 按 Flipbook 帧率（默认）；1 = 按粒子生命长度播完一遍；2 = 用 FrameIndex 直接指定帧号。" ];
        float StartFrame = $(Fmt $StartFrame) [ Group="图集"; SortPriority=20;
            Description="起始帧偏移。接 Random Float in Range 就是随机起帧。" ];
        float FrameIndex = 0.0 [ Group="图集"; SortPriority=25;
            Description="模式2专用：直接指定当前帧号（可接 Float from Curve 做曲线控制）。" ];
        float PlayRate   = $(Fmt $PlayRate) [ Group="图集"; SortPriority=30;
            Description="播放速率倍率。模式0：1.0 = 正好 Flipbook 帧率；模式1：1.0 = 一个生命周期播完一遍；负值倒放。" ];
    }

    Emitter $Atlas
    {
        Settings = {
            SimTarget          = CPU;      // 数据接口第一步只支持 CPU
            LocalSpace         = false;
            Determinism        = true;
            RandomSeed         = 1337;
            AllocationMode     = Fixed;
            PreAllocationCount = 8;
        }

        EmitterUpdate = {
            // Self + Infinite：发射器自己管生命周期，一直循环发射。
            EmitterState(LifeCycleMode = Self, LoopBehavior = Infinite);
            // SpawnRate 是「间隔累加器」：SpawnRate=1.0 时第一颗粒子要等 1 秒才凑够
            // （NiagaraEmitterInstanceImpl.cpp:386/1730 的 SpawnInterval 机制）。
            // 加一条即时 burst，保证 0 秒就有东西可看。
            SpawnBurst_Instantaneous(SpawnCount = 1, SpawnTime = 0.0);
            SpawnRate(SpawnRate = 1.0);
        }

        ParticleSpawn = {
            // V2 才是非废弃版本（旧的 Spawn/Initialization/InitializeParticle 会报 DFX6004）。
            // LifetimeMode 是静态开关，必须写在它管辖的 Lifetime 之前。
            Spawn/Initialization/V2/InitializeParticle(
                LifetimeMode = DirectSet,
                Lifetime     = 1.0
            );
        }

        ParticleUpdate = {
            // ParticleState 提供年龄/归一化年龄。没有它 NormalizedAge 永远是 0，
            // 图集就卡在第 0 帧 —— 这是「静默错误」，headless 构建读不到 stack issue。
            ParticleState();

            Play_SpriteAtlas(
                Atlas      = User.Atlas,
                PlayMode   = User.PlayMode,
                SizeScale  = User.SizeScale,
                StartFrame = User.StartFrame,
                FrameIndex = User.FrameIndex,
                PlayRate   = User.PlayRate
            );
        }

        SpriteRenderer Sprite
        {
            Material     = "$Material";
            Alignment    = Unaligned;
            FacingMode   = FaceCamera;
            SortMode     = ViewDepth;
            // 数据接口给的是任意矩形，不是等分网格，所以 Sub Image Size 必须保持 (1,1)，
            // 让 UV 原样 0..1 交给材质函数去换算。
            SubImageSize = (1, 1);

            Bind SpriteSize -> Particles.SpriteSize;
            Bind Color      -> Particles.Color;
        }
    }
}
"@

foreach ($w in $warn) { Write-Warning $w }

if ($DryRun) {
    Write-Host "--- 目标：$Out （DryRun，不写文件）---"
    Write-Host $dfs
    return
}

if ((Test-Path -LiteralPath $Out) -and -not $Force) {
    throw "已存在 $Out；要覆盖请加 -Force（或者用 -Out 换个路径）。"
}
$dir = Split-Path -Parent $Out
if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
$utf8 = New-Object System.Text.UTF8Encoding($false)   # 无 BOM：DreamFX 按 UTF-8 读源
[System.IO.File]::WriteAllText($Out, $dfs, $utf8)
Write-Host "已生成：$Out （$($frames.Count) 帧，贴图 $($seq.texture.w)x$($seq.texture.h)，画布 $($frames[0].sourceSize.w)x$($frames[0].sourceSize.h)，$($Fps) fps）"

if ($Build) {
    $dfx = Join-Path $PSScriptRoot '..\..\DreamFX\.skill\dfx.ps1'
    Write-Host "构建：$dfx build $Out"
    & pwsh -NoProfile -File $dfx build $Out -Engine 'D:\UnrealEngine-5.8.2'
} else {
    Write-Host "下一步（先关掉 Unreal Editor）：pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Effects/NS_Atlas$Atlas.dfs"
}

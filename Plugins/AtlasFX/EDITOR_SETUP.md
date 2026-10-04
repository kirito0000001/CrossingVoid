# AtlasFX 上手

> 原理、决策、引擎实证写在 `DESIGN.md`。这份只讲「现在有什么、怎么用、坏了查哪」。
>
> **2026-10-04 现状**：三层全部由**文本源**生成，编辑器里手点节点的那套做法已经作废
> （文本源在 `DFX/` 和 `Plugins/AtlasFX/DShader/`，编辑器开着改也能自动重编）。

---

## 0. 现在有什么

| 层 | 资产 | 文本源（人写的） | 改完怎么生效 |
| --- | --- | --- | --- |
| 数据 | `Sprite Atlas`（C++ 数据接口） | `Plugins/AtlasFX/Source/AtlasFX/` | **关编辑器** → `Build.bat`（见 §6） |
| 播放 | `/AtlasFX/Modules/Play_SpriteAtlas` | `DFX/Modules/M_PlaySpriteAtlas.dfm` | 存盘即自动重编 |
| 采样 | `/AtlasFX/M_FXAtlasSheet` | `Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss` | 存盘即自动重编 |
| 示例系统 | `/AtlasFX/Effects/NS_AtlasDefAtk` | `DFX/Effects/NS_AtlasDefAtk.dfs` | 存盘即自动重编 |

**资产是生成物，改资产白改** —— 下次从文本源重编就覆盖回去。要改行为，改文本源。

---

## 1. 给一张新图集做特效（推荐：跑脚本）

```powershell
pwsh -File Plugins/AtlasFX/Tools/New-AtlasSystem.ps1 `
    -Sequence D:\NewData\CrossingVoidZDProject\Tools\Atlas\<图集名>\<图集名>_sequence.json `
    -Fps 15 -DryRun          # 先看看要生成什么，不落盘
```

去掉 `-DryRun` 就写文件（已存在要加 `-Force`），加 `-Build` 顺手构建（**构建前先关编辑器**）。

脚本产出的 `.dfs` 里已经配好：DI 指向 Flipbook、六个用户参数、模块调用、Sprite 渲染器 + 材质。
之后你只需要做两件事：

1. **把图集资产搬进工程**：`Content/AssetMaterial/FXs/通用Flipbook/` 下放 `Textures\<图集名>`（贴图）
   和 `<图集名>1`（PaperFlipbook）。图集工具出 png，UE 里导入成贴图 + 做成 Flipbook。
2. **材质的 `Sheet` 贴图参数换成新图集**（材质是共用的，`Sheet` 是 `TextureSampleParameter2D`；
   也可以建个材质实例覆盖，不动原材质）。

---

## 2. 接进已有特效（不用脚本）

1. 左下「**参数**」面板 → 「**用户参数**」那一行右侧的 **`+`** → 搜 `Sprite Atlas` → 加进来 → **F2** 改名 `Atlas`。
2. 选中它 → 细节面板把 **`Flipbook`（序列帧资产）** 指到你的 PaperFlipbook → **完事**。
   帧表（图集矩形 / 画布矩形 / 画布尺寸 / 贴图尺寸 / 帧率）会自己烘出来。
3. 发射器 → **粒子更新（Particle Update）** → `+` → 搜 `Play_SpriteAtlas` 加进来，
   `Atlas` 输入选 **`User.Atlas`**，其余参数接用户参数或直接填常量。
4. 渲染器（Sprite 渲染器）：
   - **Material** = `M_FXAtlasSheet`
   - **Sub Image Size** 保持 `(1, 1)`
   - 材质参数区那三个 Dynamic Material 绑定**保持默认**（默认就指向 DMP 0/1/2）
5. ⚠️ **绝对不要写 `Particles.SubImageIndex`** —— 见 §7 第一行。

### 帧表什么时候重烘

- 换 Flipbook / 换图集贴图 / 改手工兜底表 / 改帧率 → **立刻**重烘。
- 每次**加载资产**会比一次「源签名」（Flipbook 路径 + 帧数 + 帧率 + 每帧 Sprite 的源 UV/尺寸），
  不一致就重烘并存盘。⇒ **换图集只需要改这一行 Flipbook**。
- 想手动强制重烘：细节面板上的 **「重烘帧表（Refresh From Source）」** 按钮。
- 手工兜底（不用 Flipbook 时）：清空 `Flipbook`，往下填「图集矩形（兜底）」「画布矩形（兜底）」
  「画布尺寸（兜底）」「帧率（兜底）」，再指一张「图集贴图（兜底）」。
  ⚠️「画布矩形（兜底）」的写法是 `(画布内 x, 画布内 y, 帧宽, 帧高)` —— **不是**画布总尺寸。

---

## 3. 三种帧来源（`Play Mode`）

| 值 | 帧号公式 | 什么时候用 |
| --- | --- | --- |
| `0`（默认） | `StartFrame + 已存活秒数 × Fps × PlayRate` | 常规。`Fps` 是 DI 从 Flipbook 读出来的（Defatk1 = 15），`PlayRate = 1.0` 就是原速，负值倒放 |
| `1` | `StartFrame + NormalizedAge × 帧数 × PlayRate` | 特效必须在**一个生命周期内正好播完一遍**（寿命改长改短都不用重调速率） |
| `2` | `StartFrame + FrameIndex` | 自己控帧：`FrameIndex` 接引擎自带的 **Float from Curve**，用曲线做停帧 / 加速 / 倒放 |

`StartFrame` 接 `Random Float in Range` 就是随机起帧。帧号会自动回绕到 `[0, 帧数)`，负速率也正确。

---

## 4. `Fit Frame` 与尺寸

| `Fit Frame` | 面片尺寸 | 效果 |
| --- | --- | --- |
| `0`（默认） | 整块画布 | **画布还原**：帧放回它在画布里的位置，和原始精灵一模一样。帧大小不一时不会忽大忽小 |
| `1` | 当前帧自己的矩形 | **铺满**：省面片面积，但每帧尺寸都变（DefAtk 的帧从 60×93 到 301×361，会「抖」） |

`Size Scale` = 1 像素对应多少世界单位（UE 默认 1.0 = 1 厘米）。
`1.0` 时画布 928×640 → 9.28×6.4 米，DefAtk 闪电本体 1.7～3.6 米。

材质**不需要开关**：铺满模式下模块把「帧的画布矩形」伪装成整块画布，画布还原那套数学自动退化成铺满。

---

## 5. 换贴图 / 要受光版

- **换图集贴图**：材质 `M_FXAtlasSheet` 里的 `Sheet` 参数，或建材质实例覆盖。
- **要受光版**（烟 / 尘 / 雾这类要压暗、要融进场景的）：把 `.dss` 复制一份，
  改 `ShadingModel = DefaultLit`、`BlendMode = Translucent`，**UV 节点一个都不用动**。
  理由见 `DESIGN.md` §10。

材质本身在算的事：把面片 UV 换算成「画布内坐标」→ 判断在不在当前帧的矩形里（不在就遮掉）
→ 再换算成图集贴图上的 UV。矩形数据是模块通过三个动态材质参数喂进来的。

---

## 6. 编译与生效速查

```powershell
# ① C++ 数据接口：必须先关编辑器（Live Coding 加不进新模块）
D:\UnrealEngine-5.8.2\Engine\Build\BatchFiles\Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject

# ② 模块 / 材质 / 系统（文本源）：编辑器开着会自动重编，也可以手动跑
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Modules/M_PlaySpriteAtlas.dfm
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Effects/NS_AtlasDefAtk.dfs
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss -Shaders -Force
```

⚠️ headless 构建**不编 VM shader**，所以模块体里的分支类错误只在编辑器里才会暴露。

---

## 7. 出问题先查

| 现象 | 先查 |
| --- | --- |
| **只有第 0 帧能显示，第 1 帧起全空** | 有没有给 `Particles.SubImageIndex` 赋值（本模块里、或别人的模块里）。它会被渲染器当子图网格坐标加进材质的 `TexCoord.y`，偏移量正好等于帧号 —— **必须删掉**，见 `DESIGN.md` §8 第 7 条 |
| 干净的整张贴图当一帧显示 | 运行时 DI 是**空表**（`FrameCount = 0`）。查 Flipbook 有没有指、PaperSprite 的 Source UV / Source Dimension 有没有设。模块自带这个兜底就是为了让你一眼认出它（兜底用的尺寸是 Defatk 的 597×487） |
| 一片噪点 | DI 调用根本没执行，材质拿到的是未初始化的垃圾值 |
| 帧不走 | 粒子更新里有没有 `ParticleState`（没有它 `NormalizedAge` 永远是 0）；模块有没有勾 Update |
| 全黑 | 渲染器的三个 Material Binding 有没有指向 DMP（默认就对）；材质有没有勾 Used with Niagara Sprites；Blend Mode 是不是 Additive |
| 面片是正方形 / 被拉伸 | `Size Scale` 太小就调大；`Fit Frame` 是不是你要的那个 |
| 边缘有邻居帧的杂色 | 数据接口里的「取帧内缩」调到 1～2 |
| 模块加进来报找不到函数 | 数据接口那层没编成功（§6 第 ① 条），或者编辑器没重启 |
| 播放的还是旧行为 | 模块是**编译时内联**进发射器的：改完模块要重开预览，或在 Niagara 工具栏点一下 Compile / Apply |
| 编辑器参数面板有 6 条矩形、运行时却读不到 | 数据接口的 `CopyToInternal` 有没有覆写（`NiagaraComponent` 用 `NewObject + CopyTo` 建运行时实例），见 `DESIGN.md` §8 第 9 条 |

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
| 尺寸（网格用） | `/AtlasFX/Modules/Sprite_Atlas_Size` | `DFX/Modules/M_SpriteAtlasSize.dfm` | 存盘即自动重编 |
| 采样 | `/AtlasFX/M_FXAtlasSheet` | `Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss` | 存盘即自动重编 |
| 示例系统（精灵） | `/AtlasFX/Effects/NS_AtlasDefAtk` | `DFX/Effects/NS_AtlasDefAtk.dfs` | 存盘即自动重编 |
| 示例系统（网格） | `/AtlasFX/Effects/NS_AtlasDefAtk_Mesh` | `DFX/Effects/NS_AtlasDefAtk_Mesh.dfs` | 存盘即自动重编 |

两个示例系统播的是同一份 Defatk 图集，配方逐项对齐（同一个数据接口、同一个播放模块、同一个材质），
区别只有渲染器：精灵版挂 `SpriteRenderer`，网格版挂 `MeshRenderer` + `Sprite_Atlas_Size`。
**想验证"两版画面是否一致"，把它们并排丢进关卡对比即可。**

⚠️ **模块是编译时内联进系统的**：改完 `.dfm` 光重编模块不生效，**必须重编用到它的 `.dfs`**（见 §7）。

**资产是生成物，改资产白改** —— 下次从文本源重编就覆盖回去。要改行为，改文本源。

---

## 1. 给一张新图集做特效（推荐：跑脚本）

```powershell
pwsh -File Plugins/AtlasFX/Tools/New-AtlasSystem.ps1 `
    -Sequence D:\NewData\CrossingVoidZDProject\Tools\Atlas\<图集名>\<图集名>_sequence.json `
    -Fps 15 -DryRun          # 先看看要生成什么，不落盘
```

去掉 `-DryRun` 就写文件（已存在要加 `-Force`），加 `-Build` 顺手构建（**构建前先关编辑器**）。

脚本产出的 `.dfs` 里已经配好：DI 指向 Flipbook、六个用户参数、模块调用、**Sprite 渲染器** + 材质。
（脚本目前只出**精灵版**。要网格版就照 §2 的「换成网格渲染器」那一段改：渲染器换成 Mesh 渲染器、
多挂一个 `Sprite_Atlas_Size`、`Override Materials` 指同一个材质、`Facing Mode` 一个字别动。）
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

### 换成网格渲染器（Mesh 渲染器）

精灵渲染器读 `Particles.SpriteSize`，**网格渲染器读 `Particles.Scale`** —— 不是同一个属性。
所以网格版比精灵版多挂一个模块，其余（数据接口 / 播放模块 / 材质）完全一样。

1. 发射器细节面板「渲染」那一栏，把 **Sprite 渲染器换成 Mesh 渲染器**（或新建一个网格渲染器）。
2. **网格** 指到你自己的面片。工程自带的平面是 `/ZDBridge/FX/FXDefault`
   （92.8 × 64.0，面法线在局部 Y 上，轴心在底边）。
3. **勾上「启用材质重载」，再在 `Override Materials` 数组里填 `M_FXAtlasSheet`** ——
   **两个都要**：只填数组不勾选，数组会被静默忽略，画面上是网格自带的材质
   （`FXDefault` 上是灰白棋盘格 `WorldGridMaterial`）。
   ⚠️ 网格渲染器**没有 `Material` 这一栏**，别找它。
4. **`Sub Image Size` 保持 `(1, 1)`**（同精灵版）。
5. **朝向：`Facing Mode` 一个字都别动**（保持默认 `Default`）。填成 `Camera Plane`
   会让面片每帧朝相机转、**特效再也转不动它**，而且和 `MeshYaw = -90` 撞车 ⇒ 面片侧对镜头、
   **完全看不见且不报任何错**。详见 `DESIGN.md` §14.3。
6. 粒子更新里，在 `Play_SpriteAtlas` **之后**再加一个 `/AtlasFX/Modules/Sprite_Atlas_Size`，
   参数按下表填（它读的就是上一步播放模块写好的尺寸，顺序不能反）：

   | 参数 | 填什么 |
   | --- | --- |
   | `Mesh Size` | 面片在网格局部空间的 `(宽, 高)`。`FXDefault` = `(92.8, 64.0)` |
   | `Min Size` | 尺寸下限，一般填成和 `Mesh Size` 一样 `(92.8, 64.0)`。**别填 0** —— 上游尺寸为 0 时面片会静默消失 |
   | `Depth Scale` | 真平面填 `1.0`；拿 Cube 当薄片用时调小（0.05 = 5 厘米厚） |
   | `Uniform Scale` | 宽高整体倍率，`1.0` |
   | `Mesh Yaw` | `-90`（本工程 2D 相机沿 X 轴看、`FXDefault` 面法线在 Y 上）。**必须和 `Facing Mode` 配对**：`Default` 配 `-90`、`Camera Plane` 配 `0` |

7. 发射器 `Settings` 里建议开 **`Local Space = true`** —— 面片才会跟着发射器 / 角色挂点一起转
   （网格版示例系统就是这么配的）。

**看不见时的三步核对**（顺序照做，能覆盖九成情况）：

1. 渲染器「启用材质重载」勾了没？（不勾 ⇒ 灰白棋盘格，说明材质压根没用上）
2. `Mesh Yaw` 和 `Facing Mode` 是不是配对的那一组？（配错 ⇒ 侧对镜头 ⇒ 整片消失）
3. 转到**正对红色 X 轴**的方向看 —— `Default` 朝向下面片固定在局部平面里，
   从自由相机的任意角度看都可能是一条线，这是正常的，不是坏了。

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

**一个材质同时服务两种渲染器**：`.dss` 的用法标记里同时开着
`bUsedWithNiagaraSprites` 和 `bUsedWithNiagaraMeshParticles`（`Material.h:751 / :759`），
外加 `TwoSided = true`（网格的绕序不一定朝相机，关掉背面剔除省得整片消失）。
所以换渲染器**不用换材质**，`Material` 那一栏直接指同一个 `M_FXAtlasSheet`。

### 材质预览里看到"整张贴图"是正常的

材质编辑器预览**没有粒子数据** ⇒ 三个动态材质参数全是 0 ⇒ 正常路径算出来 `Local = 0/0 = NaN` ⇒ 全透明。
所以材质里加了一条**兜底**：三个参数全 0 时退回「直接用网格 UV 采样整张贴图」。
⇒ **预览里应当能看到一张完整的图集**，这是设计如此，不是坏了。

反过来它也是判据：**运行时在网格上看到"整张图集"= 三个动态参数没送到**
（数据接口空表 / 播放模块没挂 / 渲染器的 Material 绑定没指对）。

---

## 6. 编译与生效速查

```powershell
# ① C++ 数据接口：必须先关编辑器（Live Coding 加不进新模块）
D:\UnrealEngine-5.8.2\Engine\Build\BatchFiles\Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject

# ② 模块 / 材质 / 系统（文本源）：编辑器开着会自动重编，也可以手动跑
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Modules/M_PlaySpriteAtlas.dfm
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Modules/M_SpriteAtlasSize.dfm
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Effects/NS_AtlasDefAtk.dfs
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Effects/NS_AtlasDefAtk_Mesh.dfs
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss -Shaders -Force
```

⚠️ headless 构建**不编 VM shader**，所以模块体里的分支类错误只在编辑器里才会暴露。

⚠️ **改完 `.dfm` 一定要把用到它的 `.dfs` 也重编**：Niagara 系统在挂模块时把模块图**内联**进自己的脚本，
只重编模块，系统里跑的还是旧逻辑（症状："我明明改了怎么没反应"）。

⚠️ **材质标签页开着时存不了盘**：会报 `DSH8229 ... was built but could not be saved` 或
`DSH8206 ... is open in an asset editor`。把那个标签页关掉再编译，或者干脆关掉编辑器跑一遍。

改完想核对资产里到底写进去没有（**反编译只对系统有效**）：

```powershell
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 decompile /AtlasFX/Effects/NS_AtlasDefAtk_Mesh -Out C:\CrossingVoid\Saved\dfx_dec_mesh.txt
# 产物是 UTF-16LE，用 [IO.File]::ReadAllText($p,[Text.Encoding]::Unicode) 读
```

---

## 7. 出问题先查

| 现象 | 先查 |
| --- | --- |
| **只有第 0 帧能显示，第 1 帧起全空** | 有没有给 `Particles.SubImageIndex` 赋值（本模块里、或别人的模块里）。它会被渲染器当子图网格坐标加进材质的 `TexCoord.y`，偏移量正好等于帧号 —— **必须删掉**，见 `DESIGN.md` §8 第 7 条 |
| 干净的整张贴图当一帧显示 | 运行时 DI 是**空表**（`FrameCount = 0`）。查 Flipbook 有没有指、PaperSprite 的 Source UV / Source Dimension 有没有设。模块自带这个兜底就是为了让你一眼认出它（兜底用的尺寸是 Defatk 的 597×487）。**另一种可能**是三个动态材质参数没送到 ⇒ 材质自己的兜底生效，见 §5 末 |
| 一片噪点 | DI 调用根本没执行，材质拿到的是未初始化的垃圾值 |
| 帧不走 | 粒子更新里有没有 `ParticleState`（没有它 `NormalizedAge` 永远是 0）；模块有没有勾 Update |
| 全黑 | 渲染器的三个 Material Binding 有没有指向 DMP（默认就对）；材质有没有勾 Used with Niagara Sprites；Blend Mode 是不是 Additive |
| 面片是正方形 / 被拉伸 | `Size Scale` 太小就调大；`Fit Frame` 是不是你要的那个 |
| 边缘有邻居帧的杂色 | 数据接口里的「取帧内缩」调到 1～2 |
| 模块加进来报找不到函数 | 数据接口那层没编成功（§6 第 ① 条），或者编辑器没重启 |
| 播放的还是旧行为 | 模块是**编译时内联**进发射器的：改完模块要**重编 `.dfs`**（只重编模块不够），或重开预览、在 Niagara 工具栏点一下 Compile / Apply |
| 编辑器参数面板有 6 条矩形、运行时却读不到 | 数据接口的 `CopyToInternal` 有没有覆写（`NiagaraComponent` 用 `NewObject + CopyTo` 建运行时实例），见 `DESIGN.md` §8 第 9 条 |
| **网格版整个看不见** | 按顺序查三点：① 渲染器的「启用材质重载」**勾了没**（不勾 ⇒ 显示灰白棋盘格而不是消失）；② `Facing Mode` 与 `Mesh Yaw` **是不是配对的那一组**（`Default`+`-90` / `Camera Plane`+`0`，配错 ⇒ 侧对镜头 ⇒ 消失且不报错）；③ 换到**正对红色 X 轴**的角度看 —— `Default` 朝向下面片就固定在局部平面里，别的角度看到一条线是正常的。再不行看下一条 |
| 网格有粒子但一片空白（没有棋盘格、也没有图） | `Min Size` 是不是填了 0，或上游 `Particles.SpriteSize` 是 0 ⇒ `Particles.Scale` 的宽/高算成 0 ⇒ 网格**静默消失**（引擎不报错）。`Sprite_Atlas_Size` 的 `Min Size` 就是为这个准备的，填成和 `Mesh Size` 一样 |
| 网格版画面跟着**相机**转，特效转不动它 | 渲染器 `Facing Mode` 被填成了 `Camera Plane`（或 `Camera Position` / `Velocity`）。这几种模式每帧用相机重建朝向，会**覆盖**掉 `Particles.MeshOrientation`。改回不填（默认 `Default`） |
| **材质编辑器预览里是空的** | 预览没有粒子数据 ⇒ 三个动态参数全 0 ⇒ 正常路径 `NaN` ⇒ 全透明。材质有兜底，**应当能看到整张图集**；若空白说明兜底没生效（见 §5）。运行时在网格上看到"整张图集"则是相反的含义：参数没送到 |
| **一个系统里所有发射器突然全都看不见** | 先逐个确认发射器是不是**启用的**：资产里的 `Enabled` 状态可能被留成 `false`（源码里没写过这个属性也会中招），禁用后什么都不跑却毫无提示。排查脚本/重建系统时建议在源码里显式写 `Enabled = true;`，见 `DESIGN.md` §14.6 第 4 条 |

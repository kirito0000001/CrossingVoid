# AtlasFX 第一步：需要你在编辑器里点出来的东西

C++ 侧（插件 `AtlasFX` 的 **Runtime** 模块）只提供一个东西：

| 提供者 | 名字 | 干什么 |
| --- | --- | --- |
| C++ | Data Interface **Sprite Atlas**（`UNiagaraDataInterfaceSpriteAtlas`） | 帧表：帧数 / 图集矩形 / 画布矩形 / 画布尺寸 / 贴图尺寸 / 帧率 |

下面 6 个是二进制 `.uasset`，只能点出来。**先做第 0 步编译。**

---

## 0. 编译

1. 关掉 Unreal Editor（Live Coding 也停掉）。
2. 命令行：

```
D:\UnrealEngine-5.8.2\Engine\Build\BatchFiles\Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject
```

3. 打开工程 → Edit → Plugins 确认 **Atlas FX** 已启用。
4. 随便开一个 Niagara System，在 **User Parameters** 点 `+` → **Data Interface** → 列表里应该能看到 **Sprite Atlas**。
   - 看不到 = DI 没注册上（`PostInitProperties` 没跑到）或插件模块没编出来。

### 要创建的资产清单

| # | 资产 | 建议路径 | 类型 |
| --- | --- | --- | --- |
| 1 | `Play_SpriteAtlas_Age` | `/AtlasFX/Modules/` | Niagara Module Script |
| 2 | `Play_SpriteAtlas_Time` | `/AtlasFX/Modules/` | Niagara Module Script |
| 3 | `Play_SpriteAtlas_Frame` | `/AtlasFX/Modules/` | Niagara Module Script |
| 4 | `SpriteAtlasSize` | `/AtlasFX/Modules/` | Niagara Module Script |
| 5 | `MF_SpriteAtlasUV` | `/AtlasFX/Materials/` | Material Function |
| 6 | `M_FXAtlasSheet` | 和 `NS_FXSheet` 放一起 | Material（Unlit） |

（插件内容在 Content Browser 里的根是 **Plugins → Atlas FX**，磁盘路径 `C:\CrossingVoid\Plugins\AtlasFX\Content\`。）

---

## 1. 数据接口：绑 DefAtk 的 Flipbook

1. 在 System 的 **User Parameters** 里 `+` → Data Interface → **Sprite Atlas**，命名 `Atlas`。
2. 选中它，Details：

| 属性 | 值 |
| --- | --- |
| Flipbook | DefAtk 的 Flipbook 资产 |
| Rect Inset Pixels | `0.5` |
| Atlas Texture / Manual * | 全部留空 |

3. 点 **Refresh From Source** 按钮。
4. 展开 **Baked**，和 `D:\NewData\CrossingVoidZDProject\Tools\Atlas\Defatk\Defatk_sequence.json` 对一遍：

| Baked 字段 | 期望值 |
| --- | --- |
| Frame Rects | 6 项 |
| Texture Size | (597, 487) |
| Canvas Size | (928, 640) |
| Fps | 12 |

> 注意：工具在 `pack` 模式**不写** Fps，帧率只从 Flipbook 来（这就是为什么 DI 要绑 Flipbook）。
> 帧表是烘在 DI 上的普通数组，打包后不依赖 Paper2D。**改了 Flipbook 要回来重点一次 Refresh。**

---

## 2. 材质函数 `MF_SpriteAtlasUV`

输入（Input 节点，名字照抄）：

| 名字 | 类型 | 默认 | 接什么 |
| --- | --- | --- | --- |
| `UV` | Vector2 | — | 材质里的 TexCoord |
| `AtlasRect` | Vector4 | — | `Dynamic Parameter` 0 |
| `CanvasRect` | Vector4 | — | `Dynamic Parameter` 1 |
| `Sizes` | Vector4 | — | `Dynamic Parameter` 2 = (贴图W, 贴图H, 画布W, 画布H) |
| `LocalScale` | Vector2 | (1,1) | 可选：局部缩放 |
| `LocalOffset` | Vector2 | (0,0) | 可选：局部平移 |
| `FitCanvas` | Scalar | 0 | 0 = 铺满帧；1 = 画布还原 |

输出：`UV`(Vector2)、`Mask`(Scalar)。

节点（编号就是连接顺序）：

```
 1  Subtract            : UV - Constant2Vector(0.5,0.5)
 2  Multiply            : (1) * LocalScale
 3  Add                 : (2) + Constant2Vector(0.5,0.5)
 4  Add                 : (3) + LocalOffset                        -> uv01
 5  BreakOutFloat2Components(AtlasRect)  -> Rxy, Rzw
 6  BreakOutFloat2Components(Sizes)      -> TexSize(xy), CanvasSize(zw)
 7  BreakOutFloat2Components(CanvasRect) -> Cxy, Czw
 8  Multiply            : uv01 * Rzw
 9  Add                 : (8) + Rxy                                -> fillPx   （图集像素）
10  Divide              : (9) / TexSize                            -> fillUV
11  Multiply            : uv01 * CanvasSize                        -> canvasPx （画布像素）
12  Subtract            : (11) - Cxy                               -> localPx
13  Add                 : (12) + Rxy                               -> canvasAtlasPx
14  Divide              : (13) / TexSize                           -> canvasUV
15  Divide              : (12) / Czw                               -> t         （帧内 0..1）
16  Subtract            : Constant(1) - t                          -> oneMinusT
17  Min                 : t 与 oneMinusT                           -> edgeDist(float2)
18  BreakOutFloat2Components(edgeDist) -> ex, ey
19  Min                 : ex 与 ey                                 -> edgeDistScalar（帧内 >= 0）
20  Multiply            : (19) * Constant(1000)
21  Saturate            : (20)                                     -> canvasMask
22  LinearInterpolate   : fillUV, canvasUV, Alpha = FitCanvas       -> 输出 UV
23  LinearInterpolate   : Constant(1), canvasMask, Alpha=FitCanvas   -> 输出 Mask
```

> 为什么要有 `Mask`：画布还原模式下，面片是整张画布大小，帧只占其中一块；块外的像素必须乘 0，
> 否则会采到图集里邻居的像素（加性混合下就是明显的脏边）。
> `Multiply × 1000` 是硬边的近似（过渡带 = 帧宽的 0.1%，597 宽大约 0.6 像素），嫌不够硬就 ×10000。

---

## 3. 材质 `M_FXAtlasSheet`（Unlit）

1. 混合模式照抄 `M_FXSheet`（加性 or 半透明）。
2. 材质细节面板勾上 **Used with Niagara Sprites** 和 **Used with Niagara Mesh Particles**。
3. 图：

```
TextureCoordinate          -> uv
DynamicParameter (Index 0) -> AtlasRect
DynamicParameter (Index 1) -> CanvasRect
DynamicParameter (Index 2) -> Sizes
MaterialFunctionCall(MF_SpriteAtlasUV) 接上以上 + FitCanvas = ScalarParameter("FitCanvas", 默认 0)
TextureSampleParameter2D("Sheet")  UV 接函数的 UV 输出
Multiply : 采样结果(RGBA) * 函数的 Mask 输出
   -> Emissive = RGB
   -> Opacity  = A
```

4. 每个动作用一个 Material Instance，平时只需要改 `Sheet`（想还原画布位置就把 `FitCanvas` 设 1）。

---

## 4. 三个播放模块

三个都是 **Module Script**，只勾 **Particle Update**（不勾 Spawn）。

三个模块**都会**写这三个动态材质参数（这是材质取数据的唯一通道）：

| 属性 | 内容 |
| --- | --- |
| `Particles.DynamicMaterialParameter1` | 当前帧的**图集矩形** (x, y, w, h) 像素 |
| `Particles.DynamicMaterialParameter2` | 当前帧的**画布矩形** (x, y, w, h) 像素 |
| `Particles.DynamicMaterialParameter3` | `AppendVector(GetTextureSize, GetCanvasSize)` = (贴图W, 贴图H, 画布W, 画布H) |

外加 `Particles.SubImageIndex` = 当前帧号（方便调试，也让渲染器自带 SubUV 顺带能用）。

### 4.1 `Play_SpriteAtlas_Age` —— 按粒子年龄播（最常用）

输入：`Atlas`(DI)、`Play Rate`(float, 12)、`Start Frame`(float, 0)、`Frame Offset`(float, 0)

```
1  Multiply : Particles.Age * Play Rate
2  Add      : (1) + Start Frame
3  Add      : (2) + Frame Offset                          -> Raw
4  GetFrameCount(Atlas)                                   -> N   (int，直连即可；不行就插 Convert)
5  Divide   : Raw / N
6  Fraction : (5)
7  Multiply : (6) * N                                     -> Frame
8  GetAtlasRect(Atlas, Frame)                             -> Rect
9  GetCanvasRect(Atlas, Frame)                            -> CRect
10 GetTextureSize(Atlas) / GetCanvasSize(Atlas)           -> TexSize / CSize
11 AppendVector : TexSize ++ CSize                        -> Sizes (float4)
Map Set: DMP1=Rect, DMP2=CRect, DMP3=Sizes, Particles.SubImageIndex=Frame
```

帧率由 `Play Rate` 决定，**和寿命无关**；想一炮只播一遍就把 Lifetime 设成 `帧数 / 帧率`。
随机起帧：把 `Random Float in Range`(0, 帧数) 接到 `Frame Offset`。

### 4.2 `Play_SpriteAtlas_Time` —— 用外部时间播（多粒子/多次爆发同步）

和 4.1 一样，只把第 1 步换成 `Current Time * Play Rate`（多一个 `Current Time` float 输入，默认 0）。
驱动方每次刷新写同一个时间值，所有粒子就完全对齐；用它也能绕开 time dilation。

### 4.3 `Play_SpriteAtlas_Frame` —— 我直接给帧号（可接曲线）

输入：`Atlas`(DI)、`Frame Index`(float, 0)

```
1  GetAtlasRect(Atlas, Frame Index)      -> Rect
2  GetCanvasRect(Atlas, Frame Index)     -> CRect
3  GetTextureSize / GetCanvasSize -> AppendVector -> Sizes
Map Set: 同 4.1，SubImageIndex = Frame Index
```

**不做回绕**：帧号越界时 DI 会 clamp 到首/尾帧。所以它是"一炮到底"和"接曲线"的正路 ——
把 `Float from Curve` / `Curve` 的输出接进来就是自定义帧序。

---

## 5. 模块 `SpriteAtlasSize` —— 让面片的物理尺寸跟着帧走

输入：`Atlas`(DI)、`Fit Frame`(float, 0)、`Base Plane Size`(Vector2, 默认 (0.928, 0.640))、`Uniform Scale`(float, 1)

```
1  GetCanvasSize(Atlas)                       -> CSize
2  GetAtlasRect(Atlas, Particles.SubImageIndex) -> BreakOut -> ARect.zw
3  LinearInterpolate : CSize, ARect.zw, Alpha = Fit Frame      -> Target
4  Divide : Target / Base Plane Size                            -> S
5  Multiply : S * Uniform Scale
6  AppendVector : (5) ++ Constant(1)                            -> Particles.Scale (float3)
```

- `Base Plane Size` = ZDBridge 那块 `PlaneMesh` 的原始尺寸（0.928 × 0.640 米，1 像素 = 1 毫米）。
- `Fit Frame` = 0：面片按整张画布缩放（画布还原模式用这个）。
  `Fit Frame` = 1：面片贴着当前帧缩放（铺满模式用这个）。
- 用公告板精灵的话，改第 6 步：写 `Particles.SpriteSize` = `S * Base Plane Size`（像素单位）。

---

## 6. 接起来跑 DefAtk

建议**新建**一个 `NS_AtlasDefAtk`，别动线上那个 `NS_FXSheet`：

1. Emitter 的 **Sim Target 必须是 CPU Sim**（第一步 DI 只放行 CPU sim）。
2. Emitter Spawn：`Spawn Rate` = 0，`Spawn Burst Instantaneous` = 1。
3. Initialize Particle：`Lifetime` = 0.5（6 帧 @ 12fps = 0.5 秒，正好一遍）。
4. Particle Update: `Play_SpriteAtlas_Age`，`Play Rate` = 12。
5. Particle Update（或 Spawn）: `SpriteAtlasSize`，勾上它自己写的 `Particles.Scale`。
6. 渲染器：Sprite 或 Mesh 都行 —— 材质用 `M_FXAtlasSheet` 的 MI，**`Sub Image Size` 保持 (1,1)**。
7. 渲染器 Material Parameters 区，确认三个绑定存在（默认就应该是这个）：

| 渲染器属性 | 指向的属性 | 对应材质里 |
| --- | --- | --- |
| `Dynamic Material Binding` | `Particles.DynamicMaterialParameter1` | `Dynamic Parameter` 0 |
| `Dynamic Material 1 Binding` | `Particles.DynamicMaterialParameter2` | `Dynamic Parameter` 1 |
| `Dynamic Material 2 Binding` | `Particles.DynamicMaterialParameter3` | `Dynamic Parameter` 2 |

> 这条映射是查过源码的：`NiagaraSpriteRendererProperties.cpp:355-357` 把 `DynamicMaterialBinding/1/2`
> 分别塞进 `MaterialParam0/1/2`，`NiagaraRendererSprites.cpp:735-749` 再把它们写进
> `DefaultDynamicMaterialParameter0/1/2`，材质模板 `MaterialTemplate.ush:302-330` 里
> `Dynamic Parameter` 的 Index 0/1/2 正是读这三个。所以 **Niagara 属性名是 1 起、材质 Index 是 0 起**。

8. 期望结果：6 张闪电依次播一遍，每帧的形状/尺寸各不相同（trim 还原生效），0.5 秒后粒子消失。

---

## 7. 出问题先查这几条

| 现象 | 先查 |
| --- | --- |
| 全黑 / 显示整张贴图 | 渲染器那三个 Material Binding 有没有指向 DMP1/2/3；材质有没有勾 Used with Niagara... |
| 帧不走 | 模块是不是只勾了 Spawn 没勾 Update；`Particles.Age` 有没有被读到；Play Rate 是不是 0 |
| 边缘有邻居帧的杂色 | `Rect Inset Pixels` 调大到 1~2 |
| 帧尺寸抖动 / 位置不对 | 回 DI 点一次 `Refresh From Source`；确认 Flipbook 里的 Sprite 都来自同一张图集贴图 |
| 画布还原模式下整块不见 | DI 的 `Canvas Size` 是不是 0；`Sizes.zw` 有没有接进材质 |
| GPU 发射器上不生效 | 第一步只支持 CPU sim —— Sim Target 改回 CPU |
| 编辑器里能看到 DI 但 System 里报找不到函数 | 插件没重新编译（关编辑器 → 跑第 0 步） |

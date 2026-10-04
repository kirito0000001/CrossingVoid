# AtlasFX —— 精灵图集 × Niagara 播放模块（设计稿 v2）

状态：**设计已定稿，功能清单已冻结；第一步的 C++ 已落盘**（插件骨架 + DI），编辑器侧资产照 [`EDITOR_SETUP.md`](EDITOR_SETUP.md) 手点。最后更新：2026-09-29。

---

## 1. 目标与定位

把"图集生成工具"（`D:\UnrealMap\CrossingVoidZDTool` 的「创建图集」+ `Tools\Atlas\ue_atlas.py`）产出的图集，
接进 Niagara，做成一**组可以在任意 Niagara 系统里直接添加的模块化资产**。

**与既有方案的区别（用户原话，m00218）**：
> "我说的是做成 Niagara 模块后，就可以在别的特效里直接使用了，而不需要再用模板 Niagara 重新制作"

- 旧路子（`Docs\特效Niagara-面片与序列同步-设计.md` 定的）= **模板化**：ZDBridge 生成一份共享 `NS_FXSheet` +
  每个动作一个 MI；新特效要照模板再造。
- 本次 = **模块化**：一个自包含的数据对象 + 一个能拖进任意发射器的模块 + 一个能插进任意材质的材质函数。
  新特效接入 = 加一个 User Parameter + 拖一个模块 + 材质里插一个节点。**不新建模板系统**。

`NS_FXSheet` / `M_FXSheet` **保持不动**，特效同步流水线继续跑；两条路并存。

## 2. 硬约束：渲染器只会等分网格

Niagara 的 Sprite / Mesh 渲染器，UV 是 shader 里写死的：

```
UV = (整数格 + 格内UV) × (1/列数, 1/行数)
```

- 铁证：`D:\UnrealEngine-5.8.2\Engine\Plugins\FX\Niagara\Shaders\Private\NiagaraSpriteVertexFactory.ush:1068`
  `Intermediates.TexCoord.xy = (float2(SubImageAH, SubImageAV) + UVForTexturing) * NiagaraSpriteVF.SubImageSize.zw;`
  （`:1059 frac(SubImageIndex)`、`:1062 +0.5f`、`:1064/:1066` 取整格、`:694 NumFrames = SubImageSize.x*SubImageSize.y`）
- 渲染器属性只有 `SubImageSize`（`NiagaraSpriteRendererProperties.h:206`）+ `bSubImageBlend`（`:210`）；
  VF 布局 `ENiagaraSpriteVFLayout`（`:67-104`）**没有任何"UV 矩形"槽位**。
- `Particles.UVScale` 只能格内缩放/翻转（`:716/:724`），不能平移。
- 引擎里**不存在**服务任意矩形图集的 DI / 节点 / 渲染器槽位（无 Flipbook DI；
  `NiagaraDataInterfaceSpriteRendererInfo.cpp:24-31` 只吐 SubImageSize + bSubImageBlend）。

→ 结论：**非等分图集（`pack` 模式 + trim）只能由材质自己算 UV**。
等分网格（`grid` 模式）走渲染器原生 SubUV 即可，本次方案对它是特例（每帧矩形相同）。

## 3. 三件套

```
  UNiagaraDataInterfaceSpriteAtlas (DI)      数据：图集矩形 + 画布矩形 + 帧数 / fps
            │  加成一个 User Parameter，指到自己的 Flipbook / 图集
            ▼
  Module Script: Play Sprite Atlas           播放：算第几帧
            │  速率 / 循环 / 起始帧 / 逐粒子相位 / 暂停 / 倒放 / 时间源开关
            │  写 Particles.DynamicMaterialParameter  = 图集矩形(像素 x,y,w,h)  → 材质索引 0
            │  写 Particles.DynamicMaterialParameter1 = 画布矩形(像素 x,y,w,h)  → 材质索引 1
            │  写 Particles.DynamicMaterialParameter2 = (贴图W, 贴图H, 画布W, 画布H) → 材质索引 2
            │  （**不写 SubImageIndex** —— 写了精灵渲染器会把帧号掺进 TexCoord，见 §8-7）
            ▼
  Material Function: MF_SpriteAtlasUV        采样：矩形 → UV
               对齐方式（画布还原 / 铺满）+ 缩放 + 偏移
```

三件互相独立、各自可混用：

| 件 | 类型 | 谁用 | 怎么接 |
|---|---|---|---|
| 数据 | `UNiagaraDataInterfaceSpriteAtlas` | 任意系统 | 加成 User Parameter，指到 Flipbook 或图集 + 帧表 |
| 播放 | Module Script `Play Sprite Atlas` | 任意发射器 | 拖进 Particle Spawn / Particle Update 模块栈 |
| 采样 | Material Function `MF_SpriteAtlasUV` | 任意材质 | 插在 TextureSample 的 UV 输入上 |

等分网格也能吃（每帧矩形相同），所以现有 grid 特效可逐步迁移，**但不强制**。

## 4. 已定决策

| 项 | 决定 | 来源 |
|---|---|---|
| 插件名 | **AtlasFX**（新开的小插件，不动 ZDBridge） | 用户 m00231 |
| 播放驱动 | **两种都要**：模块上带"时间源"开关（外部时间 / 粒子年龄 / 系统时间） | 用户 m00210 |
| 贴合方式 | **画布还原 + 铺满两套都做，并且要有缩放**（再加偏移） | 用户 m00210 |
| DI 数据源 | **两个都支持**：绑 `UPaperFlipbook`（编辑期自动烘表）+ 图集贴图/帧表数组（兜底） | 用户 m00231 |
| 复用范围 | 跨系统复用（自己的特效之间），但按"以后能直接拿出去"的方式组织 | 用户 m00218 |
| MVP 靶子 | **DefAtk**（6 帧非等分闪电，`D:\NewData\CrossingVoidZDProject\Tools\Atlas\Defatk`） | 用户 m00210 |

### 4.1 逐粒子通道怎么分配

引擎只给了**三个**每粒子 `float4`，且属性名与材质 `Dynamic Parameter` 的 Index **差一位**：
第一个属性名 `Particles.DynamicMaterialParameter` **没有数字**（引擎 `NiagaraModule.cpp:448-451`），
它对应的正是材质的**索引 0**。

| 属性（Niagara 侧） | 材质 Index | 内容 |
|---|---|---|
| `Particles.DynamicMaterialParameter` | 0 | 图集矩形（像素 x,y,w,h） |
| `Particles.DynamicMaterialParameter1` | 1 | 画布矩形（像素 x,y,w,h，画布还原模式用） |
| `Particles.DynamicMaterialParameter2` | 2 | 尺寸包 = (贴图W, 贴图H, 画布W, 画布H) |

⚠️ 写成 `1/2/3` 会整体错位一位 ⇒ 材质读到默认 `(1,1,1,1)` ⇒ UV 算飞 ⇒ **全透明，什么都看不见**（踩过，见 §8-7）。

链路（源码逐段核过）：`NiagaraSpriteRendererProperties.cpp:303-306`（Mesh 渲染器同理）
把渲染器的 `DynamicMaterialBinding` / `DynamicMaterial1Binding` / `DynamicMaterial2Binding`
（默认分别绑上面这三个属性）塞进 VF 的
`MaterialParam0/1/2` → `NiagaraRendererSprites.cpp:735-749` 写进 `DefaultDynamicMaterialParameter0/1/2`
→ `Shaders\Private\MaterialTemplate.ush:302-330` 里 `Dynamic Parameter` 的 Index 0/1/2 读的正是这三个。

缩放 / 偏移 / 对齐方式 = 材质实例参数（不是逐粒子），所以不占这个通道。

### 4.2 功能清单（2026-09-29 用户确认，**已冻结**）

**① DI `UNiagaraDataInterfaceSpriteAtlas`（数据）**

- 数据源两种：绑 `UPaperFlipbook`（编辑期自动烘表）／ 手工「图集贴图 + 帧矩形数组」兜底
- 对外给：`GetFrameCount` / `GetAtlasRect(i)` / `GetCanvasRect(i)` / `GetCanvasSize` / `GetFps`
  （矩形一律**像素、左上原点**；归一化留给材质）
- 表烘在本对象上（普通数组属性），打包后不依赖 Paper2D 资产；表一变靠 `AppendCompileHash` 触发 Niagara 重编

**② Module `Play Sprite Atlas`（播放）—— 三种帧来源，正是用户原话那三句**

| 用户的话 | 引脚 |
|---|---|
| "有时候就自己写播放速度和时间" | `Current Time`(秒) + `Play Rate`(帧/秒，**负值=倒放**) |
| "有时候根据生命长度自适应" | 按 `Particles.Age` 推帧（`Rate` 或"一个生命周期播 N 遍"） |
| "有时候可以用浮点直接设置当前是第几帧（这样也可以做曲线自定义）" | `Frame Index`(float) 直通；接引擎自带 **Float from Curve** 动态输入 = 曲线自定义帧序 |

- **MVP 实现形态（已落地）**：一个薄模块 `Play_SpriteAtlas`，用 `Play Mode` 输入切换三种帧源
  （`0` = 按 Flipbook 帧率 / `1` = 按粒子生命长度播完一遍 / `2` = `Frame Index` 直通）。
  原设计是三个模块 `Play_SpriteAtlas_Age` / `_Time` / `_Frame`，实际合并成一个：
  三种模式共用同一条"回绕 → 取矩形 → 写 DMP → 写 SpriteSize"的尾巴，拆开只是三份重复。
  源文件 `DFX/Modules/M_PlaySpriteAtlas.dfm`
- **MVP 有**：`Start Frame` / `Frame Offset`（接 `Random Float in Range` 就是随机起帧）
- **MVP 没有（留第二步）**：`End Frame` / `Loop` 开关 / "一个生命周期播 N 遍" / `Pause` / 倒放 / 按粒子 ID 哈希的确定性随机起帧。
  现在三种模式都是**恒定回绕**（`Raw - floor(Raw/N)*N`，负值也落在 `[0,N)`），
  "一炮只播一遍"靠 `Lifetime = 帧数 / 帧率` 实现
- 输出：`Particles.DynamicMaterialParameter` / `...Parameter1` / `...Parameter2`（见 §4.1）
  - ⚠️ **第一个属性名没有数字**（引擎 `NiagaraModule.cpp:448-451`），它对应材质 `DynamicParameter` 的**索引 0**，
    `...Parameter1` → 索引 1、`...Parameter2` → 索引 2（`NiagaraSpriteRendererProperties.cpp:355-358` 做映射）。
    写成 `1/2/3` 会整体错位一位，材质读到默认值 `(1,1,1,1)` ⇒ UV 算飞 ⇒ **全透明，什么都看不见**（踩过）

**③ 尺寸模块 `Sprite Atlas Size`（可选）—— 2026-10-04 状态：仍未做**

- 「面片尺寸跟随帧」目前由播放模块的 `Size Scale` / `Fit Frame` 兼任（见 §4.2 ②）
- 原设计：输入 `Atlas` + `Base Plane Size`(默认 `0.928, 0.640`，即 `FXDefault` 面片尺寸) +
  `Fit Frame`(**float 0/1，不是 bool** —— 避开 Select 节点，直接喂 `Lerp`) + `Uniform Scale`
- 原设计里「帧号直接读 `Particles.SubImageIndex`」**已作废**：那个属性绝对不能写（§8-7）。
  真要做，帧号得从播放模块的输出引脚拉过来，或者自己再算一遍。
- 输出 `Particles.Scale = (目标W / BaseW, 1, 目标H / BaseH)`
  - `Fit Frame = 0` → 目标 = 画布尺寸（**画布还原**模式用）
  - `Fit Frame = 1` → 目标 = 该帧的图集矩形尺寸（**铺满**模式用）
- 作用：**分辨率比规范大、比例不变**时画面不会变小（928×640 → 1856×1280 时 scale=(2,1,2)）
- Mesh 渲染器的按粒子缩放走 `ScaleBinding`（`NiagaraMeshRendererProperties.h:332`，默认绑 `Particles.Scale`）；
  Sprite 渲染器走 `SpriteSizeBinding`（`NiagaraSpriteRendererProperties.h:305`，默认 `Particles.SpriteSize`）

**④ Material Function `MF_SpriteAtlasUV`（采样）**

- 输入 `UV` / `AtlasRect` / `CanvasRect` / `Sizes`(= 贴图W,贴图H,画布W,画布H) /
  `LocalScale` / `LocalOffset` / `FitCanvas`(**标量** 0/1)
- **输出两个**：`UV` + `Mask`（掩码是 v1 稿漏掉的一步，见下）
- `localUV = (UV - 0.5) * LocalScale + 0.5 + LocalOffset`（1/0 时不变）
- 铺满：`UV = (AtlasRect.xy + localUV * AtlasRect.zw) / TexSize`
- 画布还原：面片是**整张画布**大小 → `canvasPx = localUV * CanvasSize`、
  `t = (canvasPx - CanvasRect.xy) / CanvasRect.zw`、
  `UV = (AtlasRect.xy + canvasPx - CanvasRect.xy) / TexSize`，
  且 `Mask = saturate(min(t, 1-t) * 1000)`：**帧外的像素必须乘 0**，否则会采到图集里邻居的像素
  （加性混合下就是明显的脏边）
- 像素内缩在 DI 侧做（`RectInsetPixels`，默认 0.5px），吃掉 `padding=2 / extrude=0` 的邻帧渗色
- **比例真的变了**（比如 1:1 的图）：不自动处理 → 报警告 + 换面片（面片形状是几何，不改）

## 5. 数据层：Paper2D 侧的事实（决定能不能只靠资产还原）

（勘察报告：子代理 52c14c0a，UE 5.8.2 源码只读）

**帧在图集贴图里的像素矩形 —— 可唯一还原，且打包后仍在：**

| 阶段 | 字段 | 位置 |
|---|---|---|
| 编辑器 | `SourceUV`（像素，左上原点） | `PaperSprite.h:75-77` |
| 编辑器 | `SourceDimension`（像素 w,h） | `PaperSprite.h:79-81` |
| **运行时** | `BakedSourceUV` | `PaperSprite.h:95-97`（**无 WITH_EDITORONLY_DATA**） |
| **运行时** | `BakedSourceDimension` | `PaperSprite.h:99-101` |
| **运行时** | `BakedSourceTexture` | `PaperSprite.h:103-104` |

运行时统一出口：`PaperSprite.cpp:1657-1666` `const FVector2D StartUV = BakedSourceUV / ImportedSize; const FVector2D SizeUV = BakedSourceDimension / ImportedSize; return FSlateAtlasData(BakedSourceTexture, StartUV, SizeUV);`
烘烤路径：`PaperSprite.cpp:1892-1901`（无 `AtlasGroup` 时 `BakedSourceUV = SourceUV; BakedSourceDimension = SourceDimension;`）。

**帧在原始未裁剪画布中的矩形 —— 编辑器可还原，运行时不可：**

- 缺的只有 **画布尺寸 `sourceSize`（928×640）**：只存在于编辑器专用字段
  `SourceImageDimensionBeforeTrimming`（`PaperSprite.h:56-58`），打包后不存在。
- trim 偏移没有独立字段，被**烘进 pivot**：`PaperSprite.cpp:1564-1573`
  ```cpp
  FVector2D TopLeftUV = SourceUV;
  FVector2D Dimension = SourceDimension;
  if (bTrimmedInSourceImage) { TopLeftUV = SourceUV - OriginInSourceImageBeforeTrimming; Dimension = SourceImageDimensionBeforeTrimming; }
  ```
  反解式就写在导入器里（`PaperJsonSpriteSheetImporter.cpp:621-622`）：
  `spriteSourceSize.x/y = SpritePosInSheet - pivotPos + sourceSize * Pivot`。
- 其它编辑器专用项：`PivotMode`(:134-136)、`CustomPivotPoint`(:138-140)、`bTrimmedInSourceImage`(:60-62)、
  `bRotatedInSourceImage`(:64-66)。**运行时读不到 pivot 比例**（本例恒 0.5/0.5）。
- 注意命名陷阱：`PaperSprite.h:270 GetSourceSize()` 返回的是**裁剪后的帧尺寸**，不是 JSON 的 `sourceSize`；
  全文件没有名为 `SourceSize` 的 UPROPERTY。
- 导入器还会**丢弃**东西：非 trimmed 分支（`PaperJsonSpriteSheetImporter.cpp:158-163`）直接把 JSON 的
  `spriteSourceSize`/`sourceSize` 覆盖成帧尺寸；`SpriteSourceSize` 全插件解析后从未被读取。

**→ 由此确定的实现策略：DI 在编辑器里把表烘好、存进自己的普通（非编辑器专用）数组属性。**
表里同时带"图集矩形 + 画布矩形 + 画布尺寸"，打包后直接可用，运行时不必碰任何 `UObject`。

`UPaperFlipbook` 侧没有坐标字段，只有 Sprite 指针 + FrameRun：
`PaperFlipbook.h:15-32`（`FPaperFlipbookKeyFrame`）、`:59-65`（`FramesPerSecond` / `KeyFrames`）、
`GetKeyFrameChecked` `:121-125`、`GetNumFrames` `:83-84`、`GetKeyFrameIndexAtTime` `:92-93`。

**另一条数据来源**：`sync_character_sequences.py:326-406` 根本没走 `.paper2dsprites` 导入器，而是手工写
`source_texture`(:378) / `source_uv`(:379) / `source_dimension`(:380)（注释："和 .paper2dsprites 导入器内部做的事是同一件"）。
→ 数据侧完全可控，需要加字段随时能加。

## 6. 图集工具侧的事实（`D:\UnrealMap\CrossingVoidZDTool`）

- 真身：`Tools\Atlas\ue_atlas.py`（1250 行 / 56452 B / 2026-09-15，SHA256 前 16 位 `BF44AF2D0B2579D0`），
  由 .NET 8 WinUI3 宿主经 `Services\Atlas\AtlasFolderPackService.cs:132-170` 起子进程调用；
  成败看 `_atlas_report.json` 而不是退出码（`AtlasFolderPackService.cs:19`）。
  作废的原型版在 `C:\Users\liuyu\WorkBuddy\2026-09-09-21-03-05\ue_atlas_tool\ue_atlas.py`（480 行，无清单接口）。
- 输入是**清单**（图片+精灵名+序号），不是扫目录；只认 PNG。
- **两个模式都在跑**：`pack`（MaxRects 紧密装箱，给角色图集，`SequenceAtlasPackService.cs:203`）与
  `grid`（等分网格，给 Niagara 特效面片，`:339`）。
- `grid` 模式额外产出 `<name>_grid.json`，里面直接有 Niagara 口径：
  `cell{w,h}` / `pitch{w,h}` / `cols` / `rows` / `niagara{NumFramesX,NumFramesY}` / `order:"row-major, left->right, top->bottom"`。
  **→ "等分网格"这条路工具早备好了，本次要补的是"非等分"。**
- 坐标语义：左上原点、x 右 y 下、像素整数。`frame{x,y,w,h}` **已扣 padding、未计 extrude**；
  `spriteSourceSize` 在 pack 模式 = trim 信息（原画布中的位置/尺寸），grid 模式 = 帧在统一画布中的位置；
  `sourceSize` 在 pack 模式 = 该帧原图尺寸，grid 模式 = 统一画布尺寸；`rotated` 未开 `--rotate` 恒 false。
- `_sequence.json` 的 `order` 是**中文说明字符串**不是数字；`pivot` 恒 0.5/0.5 且只出现在 `.json`/`.paper2dsprites` 里。
- `crop` 默认开启 → `report.size` 是裁过的内容包围盒（Defatk 是 597×487，不是 2048 画布）。
- **工具本身完全没有 fps / 时长 / 循环字段**；fps 来自 `<动作目录>\sequence.json` 的 `Fps`
  （Misaka DefAtk = 12；`sync_character_sequences.py:837-838` 默认 12）。
- 现成的语义验证器：`Services\Atlas\AtlasExtractService.cs`（拆图集时按 `spriteSourceSize` `PasteBack` 回原画布，
  坐标语义有偏差会立刻暴露）。

## 7. 落地分两步

### 第一步（MVP，一条链走通）

1. 建插件 `AtlasFX`（Runtime 模块 + `CanContainContent`），C++ 侧 DI 骨架 —— **我做**
2. Niagara 资产 —— **我做不了，是二进制 `.uasset`**：由用户在编辑器里照 `EDITOR_SETUP.md` 点出来
   `Play Sprite Atlas (Age) / (Time) / (Frame)`、`Sprite Atlas Size`、`MF_SpriteAtlasUV`
3. DI 绑 `UPaperFlipbook` → 编辑期自动烘表（图集矩形 + 画布矩形 + 画布尺寸 + fps）
4. 靶子：**DefAtk** 那 6 帧闪电，挂到现成的 `NS_FXSheet`（Mesh 渲染器，`SubImageSize=(1,1)`）

**验收标准**：闪电按序播；暂停能停在同一格；角色动画暂停/倒放时特效跟着停/倒（走外部时间源）；
改成粒子年龄时间源时能独立播完。

**MVP 的 sim target 取舍**：先只支持 **CPU sim**（`CanExecuteOnTarget` 只放 CPU）。
`NS_FXSheet` 本来就是单粒子 CPU 系统，够用；GPU 版留第二步（要 Buffer + Proxy + HLSL 模板 + `AppendCompileHash`）。

### 第二步

- GPU sim 支持（DI 的 Buffer / Proxy / `.ush` / `BuildShaderParameters` / `SetShaderParameters`）
- 补齐"画布还原 / 铺满 + 缩放 + 偏移"全部模式
- 逐粒子相位、随机起帧（用粒子 ID 哈希，保证回放一致）、倒放、PingPong
- 多图集并存（不同特效各绑各的 DI）
- `pack`（非等分）与 `grid`（等分）两种产物都验一遍

## 8. 已知坑（实现时必须处理）

1. **图集贴图要关 mip**：有 mip 时缩小显示会跨帧取到邻居，加性特效会出现"别的帧的残影"。
2. **帧矩形内缩半个像素**：`padding=2` + `extrude=0` 时边缘会渗色到邻居/空白。
3. **`rotated` 必须检查**：一旦有人开 `--rotate`（`Image.Transpose.ROTATE_270`，顺时针 90°），
   `frame.w/h` 仍是旋转前语义，消费方要自己换轴。
4. **源帧尺寸不统一会跳**：`sourceSize` 各帧不同 → UE 每帧 pivot 偏移不同 → Flipbook 播放上下跳
   （`ue_atlas.py:959-976` 只会警告，建议改 `--mode grid`）。
5. **不要用 `Particles.NormalizedAge` 直接当帧率源**（它是 Age/Lifetime，寿命一变帧率就变，且表达不了相位/起始帧）；
   要的是**已存活秒数**。理想是 `Particles.Age`，但 DreamFX 的常用属性表里没有它（写了报 DFX3046），
   所以模块里用 `Particles.NormalizedAge * Particles.Lifetime` 等价还原 —— 数值上等于 Age，且无副作用
   （见 `DFX/Modules/M_PlaySpriteAtlas.dfm` 的 Body 注释）。
6. **`Sub UV Blending` 对非等分图集不能开**（它按"下一个相邻格"混合，非等分下不成立）。
7. **绝对不要写 `Particles.SubImageIndex`**（2026-10-04 实测踩坑，真凶级）：
   精灵渲染器会把它当子图网格坐标掺进材质读到的 TexCoord ——
   `NiagaraSpriteVertexFactory.ush:1066 SubImageAV = floor(SubImageA * SubImageSize.z)`、
   `:1068 TexCoord.y = (SubImageAV + UV.y) * SubImageSize.w`；
   非等分图集必须 `SubImageSize=(1,1)` ⇒ `SubImageSize.z = 1/1 = 1` ⇒ **`TexCoord.y = 帧号 + UV.y`**。
   症状是「第 0 帧正常、第 1 帧起全空」（第 0 帧偏移恰好为 0）。
   材质自己算 UV 的方案里，SubImageIndex 必须保持默认 0（`NiagaraConstants.cpp:409` 默认 `0.0`；
   渲染器兜底 `DefaultSubImage = 0.0`，`NiagaraRendererSprites.cpp:578`）。
8. DI 侧：`CanExecuteOnTarget` **默认返回 false**，不覆写就**静默失效**；
   `GetFunctions` 已 `UE_DEPRECATED(5.4)`，正确覆写点是
   `NiagaraDataInterface.h:890 virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const`；
   DI 要在编辑器里可见，CDO 的 `PostInitProperties` 里必须
   `FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags)`
   （范本 `NiagaraDataInterfaceSpriteRendererInfo.cpp:73-87`）。
9. **自定义 DI 必须覆写 `CopyToInternal`**（2026-10-04 实测）：`NiagaraComponent.cpp:3928-3951` 给每个
   NiagaraComponent 建 DI 实例时用的是 `NewObject` + `CopyTo`（**不是** `DuplicateObject`），
   而 `UNiagaraDataInterface::CopyTo` 只搬 `CopyToInternal` 里显式拷贝的字段 ——
   不覆写的话运行时实例拿到的是**空帧表**（编辑器面板却显示正常，因为面板看的是资产上的实例）。

## 9. 环境注意事项（本机）

- **`findstr /s` 在本机静默失效**：对已知含关键字的文件返回 exit=1 且无输出（已复现取证）。
  凡"某目录里没有"的判断，必须用 `Get-ChildItem -Recurse -File -Include ... | Select-String` 复核。
- 编译 ZDBridge / AtlasFX 之前必须关闭 Unreal Editor 或 Live Coding，否则 UBT 拒绝替换占用中的模块文件
  （`Plugins\ZDBridge\README.md:41-47`）。
- 引擎：`D:\UnrealEngine-5.8.2`（工程 `CrossingVoid.uproject` 的 EngineAssociation
  `{3AAA80DF-47C5-B2AA-3115-6B8F0BFEA918}` 指向它）。

## 10. 材质的光照模型与混合模式（2026-10-02 核实，带出处）

**问题**：特效材质该用无光照（Unlit）还是受光（Lit）？加性混合下 `A → Opacity` 到底要不要连？

**结论**：

1. 自发光类特效（闪电、火花、能量、斩击、光环）→ **Unlit** 是默认做法。三个理由：
   ① 亮度所见即所得，不受场景光照 / 环境色 / 阴影影响（受光特效进暗场景会变暗、被环境色污染、
   被阴影切成块）；② 省掉整个光照计算；③ 与 Additive 天然配套（见下）。
   知乎《UE4官方课程：材质大师（笔记）》（`zhuanlan.zhihu.com/p/105032305`）给的量级：默认光照材质
   ~101 条指令 vs 无光照 ~31 条 —— **仅作量级参考，未独立验证**（该文 `/tardis/` 取全文失败）。
2. 但"所有特效都不受光"是过度概括。**需要受光的**：烟 / 尘 / 雾（要体积感与方向感）、
   要与场景融合的碎屑 / 水花 / 贴花；NPR 项目走「受光 + 自定义光照函数」；混合特效拆两个渲染器
   （受光的烟 + Unlit 的火）。
3. `M_FXAtlasSheet` 是**帧播放骨架材质**：核心工作是把当前帧矩形换算成 UV，与光照模型无关
   ⇒ 先做 Unlit + Additive；将来要受光版就复制一份改 Shading Model = Default Lit，
   UV 那 10 个节点一个都不用动（只需把 Emissive 改接 BaseColor / Roughness / 法线）。

**加性混合的 Opacity 语义（引擎源码实证）**：

- `D:\UnrealEngine-5.8.2\Engine\Shaders\Private\BasePassPixelShader.usf:2325-2327`：
  ```
  #elif MATERIALBLENDING_ADDITIVE
      Out.MRT[0] = half4(Color * Fogging.a * Opacity, 0.0f);
  ```
  ⇒ 加性下最终贡献 = **自发光 × 雾效 × Opacity**，alpha 写 0。
  **所以 `A → Opacity` 要连**：不连时 Opacity 默认 1，`Defatk` 那 1532 个半透明辉光像素
  会按 RGB 全额加进去（硬边、过曝），柔和的辉光衰减就没了。
  注意：透明区 RGB = 0，加进去也是 0，所以"不连就出白块"的说法不成立 —— 连它的真实理由是**边缘强度**。
- `D:\UnrealEngine-5.8.2\Engine\Shaders\Private\BasePassPixelShader.usf:2669-2673`：
  Additive 的 `BackgroundVisibilityAdd = 0.0f`、`PathThroughputMul = 1.0f` ⇒ 加性**完全不遮挡背景**（纯加色）。
- `D:\UnrealEngine-5.8.2\Engine\Shaders\Private\MaterialTemplate.ush:4592`：
  Additive 仍参与按 Opacity 裁剪 `clip(MaterialOpacity - 1/255.0 - GetMaterialOpacityMaskClipValue())`
  ⇒ Opacity ≲ 0.004 的像素被丢弃。
- 官方文档 *Material Blend Modes in Unreal Engine*（UE 5.8，
  https://dev.epicgames.com/documentation/en-us/unreal-engine/material-blend-modes-in-unreal-engine）：
  Additive 公式 `Final color = Source color + Dest color`，**"not compatible with dynamic lighting"**，
  黑色渲染为透明，适合火焰 / 蒸汽 / 全息；缺点是浅色背景下不易看清（对策 AlphaComposite）。
  Translucent 公式 `Source color * Opacity + Dest color * (1 - Opacity)`，同样不受动态光照。

**图集贴图的底色实测（修正早前假设）**：

- 早前担心 `Defatk.png` 有白底 → 加性叠加会出白块。实测（2026-10-02）：
  文件 `D:\NewData\CrossingVoidZDProject\Tools\Atlas\Defatk\Defatk.png`，169901 字节，
  IHDR `colortype = 6`（RGBA，带真 alpha 通道），597×487；每 6 像素采样 8200 点 →
  **6567 点 alpha = 0**（RGB 亦为 0,0,0）、1532 点半透明（辉光边缘）、101 点 alpha = 255。
- ⇒ **背景是透明黑，不是白**（图片查看器把透明合成到白底，才看着像白）。
  加性叠加不会出白块；透明区 RGB = 0，即使忽略 alpha 也不贡献颜色。
- 教训：不要把查看器的显示效果当成贴图数据；贴图语义要读 IHDR / 采样 alpha 定论。

**混合模式逐个为什么不行（编辑器下拉里那 7 项）**：

| 混合模式 | 公式 / 行为 | 用在 `M_FXAtlasSheet`（闪电图集）上会怎样 |
|---|---|---|
| **Additive** ✅ | `Src + Dst`，不遮挡背景（`BasePassPixelShader.usf:2669-2673` 把 `BackgroundVisibilityAdd` 置 0） | **选它**。闪电是"往画面上加光"，黑/透明区加 0 = 天然透明；多张叠加越叠越亮，正是能量特效要的 |
| Translucent | `Src*Opacity + Dst*(1-Opacity)` | 能用但不对味：辉光会**盖住**背景而不是叠加，颜色偏灰、亮度上不去；需要"能压暗场景"的烟/尘才用它 |
| Masked | 硬裁切，只留 OpacityMask > 阈值的像素 | 辉光边缘全被切成硬边（半透明像素要么全留要么全丢），闪电的柔和光晕直接没了 |
| Opaque | 忽略 alpha，写不透明像素 | 整个 928×640 画布变成一块实心矩形 —— 灾难 |
| Modulate | `Dst * Src`，只能压暗 | 是"乘暗"用的（影子、暗角、去色），闪电会变成一块黑斑 |
| AlphaComposite (Premultiplied Alpha) | 预乘 alpha 的半透明变体 | 官方推荐的**加性的替代品**：加性在浅色天空前看不清时才换它（它能既压暗又提亮、边缘更稳）。现在不需要，但值得记着 |
| AlphaHoldout | 用来把场景"挖洞"的合成工具 | 跟特效播放无关，别选 |

- 结论：**自发光 / 加光类 → Additive；能压暗 / 需要遮挡背景类（烟、尘、暗雾、贴花）→ Translucent**；
  浅色背景下加性看不清 → 换 AlphaComposite。
- 混合模式与 UV 换算无关：将来要换混合模式，材质里那 10 个 UV 节点一个都不用动。

## 11. 文本创作链的现状与遗留（2026-10-04）

**三层文本源 → 生成资产**：

| 层 | 文本源 | 生成物 | 构建方式 |
|---|---|---|---|
| 数据 | `Plugins/AtlasFX/Source/AtlasFX/**`（C++） | `UnrealEditor-AtlasFX.dll` | `Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject`（先关编辑器） |
| 播放 | `DFX/Modules/M_PlaySpriteAtlas.dfm` | `/AtlasFX/Modules/Play_SpriteAtlas` | `pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build ...` |
| 采样 | `Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss` | `/AtlasFX/M_FXAtlasSheet` | 编辑器启动时自动编译（改完保存即生效） |
| 系统 | `DFX/Effects/NS_AtlasDefAtk.dfs` | `/AtlasFX/Effects/NS_AtlasDefAtk` | `dfx.ps1 build` |

**数据通道契约（DI → 材质）**：模块把三包数据写进 `Particles.DynamicMaterialParameter`
（**注意第一个属性名没有数字**）→ 材质 `DynamicParameter` 索引 0 = 图集矩形 `(x,y,w,h)` 像素坐标；
`...Parameter1` → 索引 1 = 画布矩形；`...Parameter2` → 索引 2 = 尺寸包 `(贴图W, 贴图H, 画布W, 画布H)`。
材质三个索引都读。渲染器侧的绑定字段是 `DynamicMaterialBinding/1/2/3Binding`，
默认已指向这四个属性（`NiagaraSpriteRendererProperties.cpp:303-306`）。
⚠️ 名字写成 `1/2/3` 会整体错位一位 ⇒ 材质读到默认 `(1,1,1,1)` ⇒ UV 算飞 ⇒ 全透明（踩过）。

**用户 2026-10-04 决定「先记录进文档、以后再做」的功能**（§4.2 设计稿里有、当时实现没有）：

1. ~~画布还原模式~~ —— **已做**：材质现在按画布矩形做遮罩，`FitFrame` 开关在模块里
   （§4.2 的「画布还原」与「铺满」两条支路合并成了一套数学，材质不用开关）。
2. ~~`Play_SpriteAtlas_Time` / `Play_SpriteAtlas_Frame` 两个变体~~ —— **已做**：
   合成一个模块，用 `PlayMode` 0/1/2 选帧来源（自写速率 / 生命长度自适应 / 直接给帧号）。
3. **独立的 `SpriteAtlasSize` 模块**：把「面片尺寸跟随当前帧」从播放模块里拆出来，
   给想自己写播放逻辑的人用。**（仍未做）**
4. **GPU 模拟支持**：DI 的 `CanExecuteOnTarget` 目前只认 CPUSim。**（仍未做）**

**会咬人的坑**：

- **自定义 DI 必须覆盖 `CopyToInternal`**：每个 NiagaraComponent 都给自己造一份 DI 实例
  （`NiagaraComponent.cpp:3949-3950` 用 `NewObject` + `CopyTo`，**不是** `DuplicateObject`），
  而 `UNiagaraDataInterface::CopyTo`（`NiagaraDataInterface.cpp:216`）只搬 `CopyToInternal` 里复制的东西。
  不覆盖 ⇒ 运行时那份 DI 是空实例（实测 `FrameRects=0 CanvasRects=0 TextureSize=(1,1) Flipbook=<null>`）
  ⇒ `GetRectClamped` 静默回退 `(0,0,1,1)`。样板见 `NiagaraDataInterfaceTexture.cpp:126-128`。
- **模块体里不要写 `if/else`**：VectorVM 后端会把分支展平成 select
  （`ir_vm_flatten_branches_to_selects_visitor.cpp:110-185`），展平器在「A 分支有赋值、B 分支没有对应赋值」
  时会拿**变量原值**当另一路（`:151-166`）；那一路若来自 DI 出参就会编译失败：
  `error: Component selction_result of variable selction_result has no valid offset. Possibly uninitialized data being used.`
  （`ir_vm_gen_bytecode_visitor.cpp:629-638`）⇒ 用 `step` 掩码 + 线性组合代替分支。实测 2026-10-04，
  Spawn 与 Update 两个脚本一起挂。
- 换图集 = 换 `.dfs` 里那一行 Flipbook 路径，帧表自动烘（§13），不用手抄数字。
- 反编译镜像 `C:\CrossingVoid\DShader\Decompiled\**` 会被编辑器自动编译成 `/Game/Decompiled/**` 的垃圾资产；
  不需要镜像时把那些 `.dss` 删掉。
- DreamShader 有**来源守卫**：`.dss` 只肯覆盖自己生成的资产，接手手搓资产要先备份挪走（`DSH8103`）。

## 12. 闭环工具：`Plugins/AtlasFX/Tools/New-AtlasSystem.ps1`

从图集工具的 `*_sequence.json` 生成整套 `.dfs` 系统源（发射器 / 渲染器 / 模块接线 / 用户参数）：

```powershell
# 生成（默认写到 DFX/Effects/NS_Atlas<图集名>.dfs，已存在要 -Force）
pwsh -File Plugins/AtlasFX/Tools/New-AtlasSystem.ps1 `
    -Sequence D:\NewData\CrossingVoidZDProject\Tools\Atlas\Defatk\Defatk_sequence.json -Fps 15 -Force
# 构建（先关掉编辑器）
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Effects/NS_AtlasDefatk.dfs
```

- `-Fps` **必须自己给**：sequence.json 里没有帧率，要从 PaperFlipbook 上抄（`Defatk1` = 15）。
- 自动警告：`rotated=true` 的帧（当前不支持旋转帧）、各帧 `sourceSize` 不统一、缺 `spriteSourceSize`、
  `frameCount` 与实际条数不符 —— 这些都是已知表达边界，不静默糊过去。
- `-DryRun` 只打印不写文件；`-Build` 生成后直接调 `dfx.ps1 build`。
- 生成的 DI JSON 默认**只有一行 Flipbook**（帧表交给 §13 的自动烘表）；
  只有显式 `-Flipbook ""` 时才写手工兜底数据 `AtlasTexture` + `ManualFrameRects/ManualCanvasRects/ManualCanvasSize/ManualFps`。

## 13. 帧表自动化：DI 自己从 Flipbook 烘表（2026-10-04 实现）

**目标**（用户 2026-10-04 原话："计算帧表其实本来就不是特效这边的事情"）：图集侧只负责出
`png` + `PaperFlipbook`，Niagara 侧的 `.dfs` 只写一行 Flipbook 路径，帧表由 DI 自己算。

**实现**（`Plugins/AtlasFX/Source/AtlasFX/Private/NiagaraDataInterfaceSpriteAtlas.cpp`）：

- `RefreshFromSource()` —— 烘表的唯一入口（编辑器按钮 / 改属性 / 加载时都走它）。
  开头把当前源算成签名存进 `BakedSourceSignature`。
- `ComputeSourceSignature()` —— `v<烘表算法版本>|fb:<Flipbook 路径>|<帧数>|<帧率>|` + 每帧
  `<Sprite 路径>@<源UV>+<裁剪后尺寸>><trim 原点>`。换图 / 重排图集 / 改帧率都会变。
  **改烘表逻辑时把版本号 +1**，强制所有资产下次加载重烘（v2 = 下面的贴图尺寸退路修复）。
- `PostLoad()` —— 签名不一致就 `RefreshFromSource()` + `MarkPackageDirty()`。
  DreamFX 的 headless build 也会烘：适配器先加载已有资产（`DFX5003` 就是证据），PostLoad 触发，最后保存写回。
- `NeedsRebakeFromSource()` —— **非编辑器构建恒 false**。`RefreshFromSource` 的 Flipbook 分支整段在
  `#if WITH_EDITOR` 里（要读 `PaperSprite` 的编辑器专用字段，`PaperSprite.h:269-270`），
  运行时跑一遍会把烘好的表清空。

**无 RHI 进程的坑**：headless build 里 `UTexture2D::GetSizeX()` 返回 **0**
（`Texture2D.cpp:349-363`：`PrivatePlatformData` 为空时直接 `return 0`），
而材质用 `Sizes.xy` 做 UV 归一化 ⇒ 0 会让 UV 变 NaN ⇒ 面片全透明。
退路：`GetImportedSize()`（`Texture2D.cpp:638-645`，非 cooked 包返回源图尺寸）→ 再拿不到就沿用上次的值。

**验证**（2026-10-04；此时 `.dfs` 的 DI 只剩一行 Flipbook）：
`pwsh -File Plugins/DreamFX/.skill/dfx.ps1 decompile /AtlasFX/Effects/NS_AtlasDefAtk` 读回
`FrameRects` 6 条、`CanvasRects` 6 条、`CanvasSize 928×640`、`TextureSize 597×487`、`Fps 15`、
`BakedSourceSignature "v2|fb:/Game/AssetMaterial/FXs/通用Flipbook/Defatk1.Defatk1|6|15.0000|…"`。

**注意 `CanvasRects` 的约定**：DI 烘出来的是 `(画布内 x, y, 帧宽, 帧高)`，
而图集工具 `sequence.json` 的 `spriteSourceSize + sourceSize` 是「位置 + **画布尺寸**」的写法 ——
两者不一样；脚本在手工兜底模式下按 DI 的约定生成。

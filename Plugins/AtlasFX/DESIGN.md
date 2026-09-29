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
            │  写 Particles.DynamicMaterialParameter1 = 图集矩形(像素 x,y,w,h)
            │  写 Particles.DynamicMaterialParameter2 = 画布矩形(像素 x,y,w,h)
            │  写 Particles.DynamicMaterialParameter3 = (贴图W, 贴图H, 画布W, 画布H)
            │  写 Particles.SubImageIndex = 当前帧号（调试 / 顺带让自带 SubUV 可用）
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

引擎只给了**三个**每粒子 `float4`（`Source\Niagara\Private\NiagaraModule.cpp:449-451` 只定义
`Particles.DynamicMaterialParameter1/2/3`），且 **Niagara 属性名从 1 起、材质 `Dynamic Parameter` 的 Index 从 0 起**：

| 属性 | 材质 Index | 内容 |
|---|---|---|
| `Particles.DynamicMaterialParameter1` | 0 | 图集矩形（像素 x,y,w,h） |
| `Particles.DynamicMaterialParameter2` | 1 | 画布矩形（像素 x,y,w,h，画布还原模式用） |
| `Particles.DynamicMaterialParameter3` | 2 | 尺寸包 = (贴图W, 贴图H, 画布W, 画布H) |

链路（源码逐段核过）：`NiagaraSpriteRendererProperties.cpp:355-357` / `NiagaraMeshRendererProperties.cpp:725-727`
把渲染器的 `DynamicMaterialBinding` / `DynamicMaterial1Binding` / `DynamicMaterial2Binding`
（默认分别绑 `SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM` / `_1` / `_2`，即 DMP1/2/3）塞进 VF 的
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

- **MVP 实现形态**：三个薄模块 `Play_SpriteAtlas_Age` / `_Time` / `_Frame`，
  而不是一个带枚举开关的大模块 —— 手工点图省事；跑通后再考虑合并
- **MVP 有**：`Start Frame` / `Frame Offset`（接 `Random Float in Range` 就是随机起帧）
- **MVP 没有（留第二步）**：`End Frame` / `Loop` 开关 / "一个生命周期播 N 遍" / `Pause` / 倒放 / 按粒子 ID 哈希的确定性随机起帧。
  现在 Age/Time 两条是**恒定回绕**（`frac(Raw/N)*N`），
  "一炮只播一遍"靠 `Lifetime = 帧数 / 帧率` 实现；"_Frame" 那条**不回绕**，越界由 DI clamp 到首/尾帧
- 输出：`Particles.DynamicMaterialParameter1/2/3` + `Particles.SubImageIndex`（见 §4.1）

**③ 尺寸模块 `Sprite Atlas Size`（可选）**

- 输入 `Atlas` + `Base Plane Size`(默认 `0.928, 0.640`，即 `FXDefault` 面片尺寸) +
  `Fit Frame`(**float 0/1，不是 bool** —— 避开 Select 节点，直接喂 `Lerp`) + `Uniform Scale`
- 帧号直接读 `Particles.SubImageIndex`（播放模块写的），不用再拉一根线进来
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
5. **不要用 `Particles.NormalizedAge`** 当帧率源（它是 Age/Lifetime，寿命一变帧率就变，且表达不了相位/起始帧）；
   用 `Particles.Age` 或自建累加器。
6. **`Sub UV Blending` 对非等分图集不能开**（它按"下一个相邻格"混合，非等分下不成立）。
7. DI 侧：`CanExecuteOnTarget` **默认返回 false**，不覆写就**静默失效**；
   `GetFunctions` 已 `UE_DEPRECATED(5.4)`，正确覆写点是
   `NiagaraDataInterface.h:890 virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const`；
   DI 要在编辑器里可见，CDO 的 `PostInitProperties` 里必须
   `FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags)`
   （范本 `NiagaraDataInterfaceSpriteRendererInfo.cpp:73-87`）。

## 9. 环境注意事项（本机）

- **`findstr /s` 在本机静默失效**：对已知含关键字的文件返回 exit=1 且无输出（已复现取证）。
  凡"某目录里没有"的判断，必须用 `Get-ChildItem -Recurse -File -Include ... | Select-String` 复核。
- 编译 ZDBridge / AtlasFX 之前必须关闭 Unreal Editor 或 Live Coding，否则 UBT 拒绝替换占用中的模块文件
  （`Plugins\ZDBridge\README.md:41-47`）。
- 引擎：`D:\UnrealEngine-5.8.2`（工程 `CrossingVoid.uproject` 的 EngineAssociation
  `{3AAA80DF-47C5-B2AA-3115-6B8F0BFEA918}` 指向它）。

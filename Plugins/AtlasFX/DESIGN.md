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

> **实际落地形态（2026-10-04）**：采样那一件做成了**整张材质** `/AtlasFX/M_FXAtlasSheet`
> （不是材质函数，见 §10、§10.2）；另外多出一件 **尺寸换算** `/AtlasFX/Modules/Sprite_Atlas_Size`
> —— 精灵渲染器读 `Particles.SpriteSize`、网格渲染器读 `Particles.Scale`，网格路线必须靠它换算
> （见 §4.2 ③、§14）。

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
- **MVP 有**：`Start Frame` / `Frame Offset`（接 `Random Float in Range` 就是随机起帧）/ `Loop`
- **还没做（留第二步）**：`End Frame` / `Pause` / 倒放 / 按粒子 ID 哈希的确定性随机起帧。
- **播放契约（2026-10-05 定案；用户原话："播放的时候，帧速度都和flip一样，时长也一样，默认都只播放一次"）**：
  - 帧速度 = Flipbook 自带帧率（`Atlas.GetFps()`，Defatk 是 15）
  - 一次播放的总时长 = 帧数 ÷ 帧率（Defatk 6 帧 ÷ 15 = 0.4 秒）
  - 默认只播一遍，播完停在最后一帧；`Loop = 1` 才回到旧的恒定回绕
  - 落地方式：① 新增模块 `Sprite_Atlas_Duration`（`Usage = ParticleSpawn`，源文件 `DFX/Modules/M_SpriteAtlasDuration.dfm`）
    写 `Particles.Lifetime = max(FrameCount,1) / max(Fps,0.0001) * max(DurationScale,0.0001)`；
    ② `Play_SpriteAtlas` 的帧号从"恒定回绕"改成 `Frame = lerp(clamp(Raw, 0, N-1), 回绕值, saturate(Loop))`
    （**不用 `if`** —— VectorVM 会把分支展平成 select，见 `ir_vm_flatten_branches_to_selects_visitor.cpp:110-185`）
  - 发射器配套：`EmitterState(LifeCycleMode=Self, InactiveResponse=Complete, LoopBehavior=Once, LoopDurationMode=Fixed, LoopDuration=1.0)`
    + `SpawnBurst_Instantaneous(SpawnCount=1, SpawnTime=0.0)`，**不要 `SpawnRate`**（形状照抄工程里的 `Leng刀光`）
  - ⚠️ `Sprite_Atlas_Duration` 必须是 `ParticleSpawn`：`Play_SpriteAtlas` 在 `ParticleUpdate`，
    那时改 `Lifetime` 只会让 `NormalizedAge` 跳变，帧号跟着跳
  - ⚠️ 一次性特效在编辑器预览里播完就停，要重看得按时间轴重启（不是坏了）
- 输出：`Particles.DynamicMaterialParameter` / `...Parameter1` / `...Parameter2`（见 §4.1）
  - ⚠️ **第一个属性名没有数字**（引擎 `NiagaraModule.cpp:448-451`），它对应材质 `DynamicParameter` 的**索引 0**，
    `...Parameter1` → 索引 1、`...Parameter2` → 索引 2（`NiagaraSpriteRendererProperties.cpp:355-358` 做映射）。
    写成 `1/2/3` 会整体错位一位，材质读到默认值 `(1,1,1,1)` ⇒ UV 算飞 ⇒ **全透明，什么都看不见**（踩过）

**③ 尺寸模块 `Sprite Atlas Size` —— 2026-10-04 状态：已做**（原设计为「可选」，落地时因为 Mesh 路线必需而变成必件）

- 源文件 `DFX/Modules/M_SpriteAtlasSize.dfm` → 资产 `/AtlasFX/Modules/Sprite_Atlas_Size`
- **为什么必需**：两个渲染器读的不是同一个属性 —— 网格渲染器按粒子缩放走 `ScaleBinding`
  （`NiagaraMeshRendererProperties.h:332`，默认绑 `Particles.Scale`），精灵渲染器走 `SpriteSizeBinding`
  （`NiagaraSpriteRendererProperties.h:305`，默认 `Particles.SpriteSize`）。播放模块只写 `SpriteSize`
  （它要同时服务两种渲染器的尺寸语义），所以网格版必须再挂一个模块把它换算成 `Particles.Scale`。
- **实现（与 §4.2 原设计的差异）**：原设计的 `Atlas` / `Base Plane Size` / `Fit Frame` 三个输入**都没有**落地。
  「目标尺寸」直接读播放模块写好的 `Particles.SpriteSize`（世界单位，已乘过 `SizeScale`），
  本模块只做一次换算，不重复算帧号 ⇒ 播放逻辑仍然只有一份。原设计里
  「帧号直接读 `Particles.SubImageIndex`」也**作废**：那个属性绝对不能写（§8-7）。
- 输入：

  | 输入 | 默认 | 含义 |
  |---|---|---|
  | `MeshSize`（Vector2） | `(92.8, 64.0)` | 面片在网格局部空间的尺寸。工程面片 `FXDefault` 就是 `(92.8, 64.0)`（宽 × 高，单位同世界单位） |
  | `MinSize`（Vector2） | `(92.8, 64.0)` | **尺寸下限**。`Particles.SpriteSize` 为 0 时宽度会算成 0 ⇒ 面片静默消失（不报任何错）。这个下限保证面片永远可见，同时是排查判据：面片以下限尺寸出现 = 上游 `SpriteSize` 是 0 |
  | `DepthScale` | `1.0` | 厚度方向缩放（网格局部 Y）。真平面保持 1.0；拿 Cube 当薄片时调小（0.05 = 5 厘米厚） |
  | `UniformScale` | `1.0` | 宽高整体倍率，不影响厚度 |
  | `MeshYaw` | `0.0` | 绕 Z 的朝向修正（度）。**必须和渲染器的 `FacingMode` 配对**，见 §14。默认 0 = 保持面片自然朝向（法线 Y，正对相机）；要左右镜像改 `180` |

- 函数体（三行，无分支）：
  ```
  float2 Target = max(Particles.SpriteSize, MinSize) * UniformScale;
  Particles.Scale = float3(Target.x / max(MeshSize.x, 0.001),
                           DepthScale,
                           Target.y / max(MeshSize.y, 0.001));
  float HalfYaw = MeshYaw * 0.00872664626;          // 0.5 * PI / 180，度 → 半弧度
  Particles.MeshOrientation = float4(0.0, 0.0, sin(HalfYaw), cos(HalfYaw));
  ```
  两个 `max()` 都不是保险而是必需：`MeshSize` 填 0 会让 `Scale` 变 `inf`，`SpriteSize` 是 0 会让宽度变 0 ——
  两种都是**网格直接消失且引擎不报任何错**。`max` 同时能兜住 NaN（HLSL 的 `max` 展开成 `a > b ? a : b`，
  NaN 比较恒 false ⇒ 取下限）。
- 作用：**分辨率比规范大、比例不变**时画面不会变小（928×640 → 1856×1280 时 `Scale = (2, 1, 2)`）
- 必须排在 `Play_SpriteAtlas` **之后**（Niagara 按栈序执行，它读的就是播放模块写的结果）

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

> **落地形态（2026-10-04）**：没有做成材质函数，而是**整张材质** `/AtlasFX/M_FXAtlasSheet`
> （源 `Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss`）；三包矩形数据直接从三个动态材质参数里取
> （不再需要外部接线），`Mask` 用 `step` 掩码实现，并加了「三参数全 0 时退回直接采样」的兜底（§10.2）。
> 精灵与网格两个渲染器共用这一张材质（§10.1）。

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
10. **网格渲染器：`FacingMode` 与模块的 `MeshYaw` 必须配对**（2026-10-04 / 10-05 两轮实测，静默失效级）：
    面片本来正对镜头（法线 Y），任何**多转的 `MeshYaw`**（例如 `-90`）都会让它侧对镜头 ⇒
    **完全看不见、或只剩一条缝，且引擎不报任何错**。本工程用「不写 `FacingMode`（默认 `Default`）
    + `MeshYaw = 0`」。配对表与引擎依据见 §14.3。
11. **网格渲染器挂材质要两个属性一起给**：`OverrideMaterials` 数组**加上**
    `bOverrideMaterials = true`（构造函数默认 `false`，只给数组会被静默忽略 ⇒ 显示灰白 `WorldGridMaterial`）；
    写 `Material = "..."` 会被 DreamFX 静默丢掉。见 §14.2。
12. **模块是编译时内联进发射器的**：改完 `.dfm` **必须重编 `.dfs`** 才生效，只重编模块不够。见 §14.6-3。
13. **`Particles.Scale` / `Particles.SpriteSize` 为 0 都会让网格静默消失**（不报任何错）：
    尺寸换算模块用 `max(..., MinSize)` 兜下限，别省。见 §4.2 ③ 与 §14.6-2。

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

### 10.1 用法标记：精灵与网格共用一个材质（2026-10-04 实测）

`M_FXAtlasSheet` 的 `#pragma material(...)` 必须同时具备这两条：

```
#pragma material(ShadingModel = Unlit, BlendMode = Additive, TwoSided = true,
                 bUsedWithNiagaraSprites = true, bUsedWithNiagaraMeshParticles = true,
                 Backend = Graph)
```

- **`bUsedWithNiagaraMeshParticles = true`** —— 字段名就在 `Material.h:759`（`bUsedWithNiagaraSprites` 在 `:751`）。
  ⚠️ **不是** `bUsedWithNiagaraMeshes`、也**不是**旧的 `bUsedWithMeshParticles`（那是 Cascade 时代的 `:757`）。
  DreamShader 的 pragma 走 `UMaterial` 反射逐字段落实（`DreamShaderCompiler/Private/Reflection/DreamShaderMaterialSettings.cpp`），
  名字写错会报 `DSH7123 Invalid material setting path`，所以"check 过了"就说明名字对、标记真的写进资产了。
- **`TwoSided = true`** —— 网格渲染器喂进来的面片绕序不一定朝相机（网格自带朝向 + `FacingMode` 都可能让它背对），
  关掉背面剔除省得整个特效消失。加性混合下面片只有一组三角形，双面不会出现"前后两层叠加变亮"（Cube 才会）。
  ZDBridge 手搓的同用途材质也是这么配的（`ZDBridgeEffectNiagara.cpp:229 Material->TwoSided = true`）。
- 采样数学完全一样，两个渲染器共用一个材质即可 —— 差别只在渲染器喂进来的 `TexCoord` 和面片朝向。

### 10.2 动态参数兜底：全 0 时退回直接采样（2026-10-04 加）

材质的正常路径要求三个动态材质参数都有效，否则 `CanvasRect.zw` 是 0 ⇒ `Local = 0/0 = NaN` ⇒
遮罩恒 0 ⇒ **全透明**。出现这种情况的地方有两个：材质编辑器的预览（本来就没有粒子数据）、
以及上游模块没写参数（DI 空表、模块没挂、渲染器绑定没解析到）。

兜底写法（保持**无分支**，理由见 §11「模块体里不要写 `if/else`」）：

```
float DataSum = Sizes.x + Sizes.y + Sizes.z + Sizes.w + CanvasRect.z + CanvasRect.w;
float NoData  = step(DataSum, 0.0001);                  // 全 0 ⇒ 1
float4 CMain     = Sheet.Sample(UV) * Mask;             // 正常路径：画布还原
float4 CFallback = Sheet.Sample(UE.TexCoord(Index = 0)); // 兜底：网格自己的 UV 采样整张贴图
float4 C = lerp(CMain, CFallback, NoData);
```

副作用是它顺便成了**判据**：材质编辑器预览里应看到一张干净的整张图集；
运行时若网格上也出现"整张图集"，就说明三个动态参数没送到（对照 §7 的排查表）。

## 11. 文本创作链的现状与遗留（2026-10-04）

**三层文本源 → 生成资产**：

| 层 | 文本源 | 生成物 | 构建方式 |
|---|---|---|---|
| 数据 | `Plugins/AtlasFX/Source/AtlasFX/**`（C++） | `UnrealEditor-AtlasFX.dll` | `Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject`（先关编辑器） |
| 播放 | `DFX/Modules/M_PlaySpriteAtlas.dfm` | `/AtlasFX/Modules/Play_SpriteAtlas` | `pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build ...` |
| 尺寸（网格用） | `DFX/Modules/M_SpriteAtlasSize.dfm` | `/AtlasFX/Modules/Sprite_Atlas_Size` | `dfx.ps1 build`（见 §14） |
| 采样 | `Plugins/AtlasFX/DShader/M_FXAtlasSheet.dss` | `/AtlasFX/M_FXAtlasSheet` | 编辑器启动时自动编译（改完保存即生效） |
| 系统（精灵） | `DFX/Effects/NS_AtlasDefAtk.dfs` | `/AtlasFX/Effects/NS_AtlasDefAtk` | `dfx.ps1 build` |
| 系统（网格） | `DFX/Effects/NS_AtlasDefAtk_Mesh.dfs` | `/AtlasFX/Effects/NS_AtlasDefAtk_Mesh` | `dfx.ps1 build`（见 §14） |

⚠️ **模块是编译时内联进系统的**：改完 `.dfm` 必须把用到它的 `.dfs` 一起重编，只重编模块不生效（§14.6-3）。

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
3. ~~独立的 `SpriteAtlasSize` 模块~~ —— **已做**（2026-10-04）：落地为 `/AtlasFX/Modules/Sprite_Atlas_Size`
   （源 `DFX/Modules/M_SpriteAtlasSize.dfm`）。与当时设想的差异：不要 `Atlas` 输入（直接读播放模块
   写好的 `Particles.SpriteSize`）、`Base Plane Size` 改叫 `MeshSize` 并**加了 `MinSize` 下限**、
   多了 `DepthScale` 与 `MeshYaw`。网格渲染器路线必需（§4.2 ③、§14）。
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

## 14. 网格渲染器（Mesh renderer）路线（2026-10-04 打通）

**为什么要单独做一条线**：两个渲染器读的尺寸属性不同、面片朝向的来源也不同（§14.5）。
网格渲染器能做精灵做不到的事 —— 拿任意网格当特效面片（刀光、模型、薄片 Cube）、
跟着角色挂点一起旋转、用网格自己的 LOD 与材质槽；代价是**朝向和尺寸都得自己摆**。
这一章把打通它踩到的坑与结论固化下来。

### 14.1 新增的资产与文本源

| 层 | 资产 | 文本源（人写的） |
|---|---|---|
| 尺寸 | `/AtlasFX/Modules/Sprite_Atlas_Size` | `DFX/Modules/M_SpriteAtlasSize.dfm` |
| 示例系统 | `/AtlasFX/Effects/NS_AtlasDefAtk_Mesh` | `DFX/Effects/NS_AtlasDefAtk_Mesh.dfs` |

示例系统的发射器 `DefAtkMesh` 与精灵版（`/AtlasFX/Effects/NS_AtlasDefAtk`）逐项对齐：
同一个 DI、同一个播放模块、同一个材质，只多挂一个 `Sprite_Atlas_Size`，
渲染器从 `SpriteRenderer` 换成 `MeshRenderer`，发射器 `LocalSpace = true`。
⇒ 两版画面应当逐帧对得上，可以互为对照。

### 14.2 渲染器配置（整块照抄）

```
MeshRenderer Mesh
{
    Meshes             = ["/ZDBridge/FX/FXDefault"];
    OverrideMaterials  = ["/AtlasFX/M_FXAtlasSheet"];
    bOverrideMaterials = true;      // ⚠️ 少这一行等于没配材质
    bSubImageBlend     = false;     // 一帧一帧硬切
    SortMode           = ViewDepth;
    Bind CustomSorting -> Particles.NormalizedAge;
    Bind Scale -> Particles.Scale;
    Bind Color -> Particles.Color;

    // FacingMode 故意不写 ⇒ 走默认 Default，见 §14.3
}
```

三个必须知道的点：

1. **网格渲染器没有 `Material` 字段**（`NiagaraMeshRendererProperties.h:196`
   "If Override Material is not specified, the mesh's material is used."）。
   挂自己的材质只能走**两个属性一起给**：`OverrideMaterials` 数组（`:273`）
   **加上** `bOverrideMaterials` 开关（`:226`，界面上叫「启用材质重载」，构造函数默认 `false` ——
   `NiagaraMeshRendererProperties.cpp:424`）。渲染时只看后者（`:917` / `:1140 if (bOverrideMaterials)`）
   ⇒ **只给数组不勾选，数组被静默忽略**，画面上是网格自带材质（`FXDefault` 上是
   `WorldGridMaterial` ⇒ 灰白棋盘格）。
   ⚠️ DreamFX **只认识数组那个**；写 `Material = "..."` 会被**静默丢掉**
   （它按反射找 `FObjectProperty "Material"`，网格渲染器上找不到，直接 `return true`，什么都不报）。
2. **`bSubImageBlend = false`**：一帧一帧硬切，不做相邻子图插值。网格渲染器构造函数默认是 `true`
   （`NiagaraMeshRendererProperties.cpp:426`）、版本升级路径又把它改成 `false`（`:603`），
   新旧资产值不一样 ⇒ 显式写死省得随版本漂（`Leng刀光` 也是 `false`）。
3. **`SubImageSize` 必须保持 `(1, 1)`**（同 §2：非等分图集靠材质自己算 UV）。

### 14.3 朝向：`FacingMode` 与 `MeshYaw` 必须配对（本次最大的坑）

| 渲染器 `FacingMode` | 模块 `MeshYaw` | 结果 |
|---|---|---|
| **不写**（= 默认 `Default`） | **`0.0`** | ✅ **本工程用这套**：网格保持局部朝向、可以被特效旋转，而面片本来就正对相机（法线 Y） |
| `CameraPlane` | `0.0` | ✅ 也能看见：朝向矩阵每帧把面片摆正对相机，但**特效永远转不动它** |
| `CameraPlane` | `-90.0` | ❌ 朝向矩阵已经摆正了，多出来的 -90 又转 90° ⇒ 侧对镜头 ⇒ **完全看不见**，且引擎不报任何错 |
| 不写（`Default`） | `-90.0` | ❌ 面片被多转 90°（法线从 Y 到 X）⇒ 侧对镜头 ⇒ 看不见（**2026-10-05 的真凶**） |

引擎依据（`D:\UnrealEngine-5.8.2\Engine\Plugins\FX\Niagara\Shaders\Private\NiagaraMeshParticleUtils.ush`）：

- `:365 if (FacingMode != MESH_FACING_DEFAULT)` —— 只有非 `Default` 才算并乘上朝向矩阵；
- `:369 SRT.Rotation = mul(SRT.Rotation, FacingMat)` —— 朝向矩阵叠加在**粒子自身旋转之后** ⇒ **覆盖**它；
- `:262 FacingDir = -CameraForwardDir`、`:305 RefVector = Params.CameraUpDir`（仅 `CameraPlane` 用）
  ⇒ `CameraPlane` / `CameraPosition` / `Velocity` 三种模式都是"每帧拿相机重建朝向"。

⇒ **`CameraPlane` 下 `Particles.MeshOrientation` 写了也白写**（特效无法旋转网格）；
**`Default` 下完全不套朝向矩阵，网格完全听特效的**。2D 特效要的是后者。
另外 `ENiagaraMeshFacingMode` 的枚举说明也写明了这点（`NiagaraMeshRendererProperties.h:22-34`：
`CameraPosition` = "Has the mesh local-space X-axis point towards the camera's position"）。

**本工程的轴事实**（FBX 结构 + 实测定死，与 §14.3 表格配套使用）：

- `/ZDBridge/FX/FXDefault`（源文件 `Plugins/ZDBridge/Content/FX/FXDefault.fbx`）：
  **面法线在局部 Y 轴上**、宽沿局部 X（92.8）、高沿局部 Z（64.0）、**轴心在底边**（局部 Z = 0）。
- 本工程 2D 相机**沿 Y 轴看**（X 左右 / Z 上下 / Y 景深，与 Paper2D 侧视约定一致）。
  **2026-10-05 定案**：地图视口、游戏相机、PaperZD 序列预览视口**三者同轴** —— 用户实测
  `AtlasFX.PaperZD.PreviewYaw 0` 时预览与游戏完全一致。
  早先「相机沿 X 轴看」是从 `Leng刀光` 的 `AddVelocity(0, -500, 0)`（特效沿 Y 飞）**推错**的：
  那只说明"屏幕水平方向是 Y"；而 `Leng刀光` 用网格渲染器且**不写** `Particles.MeshOrientation`
  （保持自然朝向）正好印证相机是沿 Y 看的。
- ⇒ 面法线本来就是 Y ⇒ **`MeshYaw = 0.0`**（保持自然朝向）。
  默认值写在模块里（`M_SpriteAtlasSize.dfm`），系统侧也**显式传** `MeshYaw = 0.0`；
  **不要透传 `User.MeshYaw`** —— 重编不会重置用户参数的存量值，旧值会把面片转歪。
- 画面左右镜像就把符号翻成 `180`；只有面法线本来在局部 X 上的面片才需要 `-90`。

### 14.4 `LocalSpace = true` 的用途

发射器的 `Settings = { LocalSpace = true; }`（`Leng刀光` 同样这么配）——
**用户原话：「本地空间是为了让 mesh 可以旋转」**。作用有两层：

1. 粒子在发射器局部空间里模拟 ⇒ 特效挂到角色挂点上时，面片跟着挂点走，不会被世界坐标"钉住"；
2. 配合 `FacingMode = Default` + `Particles.MeshOrientation`，**特效能自己转面片**
   （旋转 Niagara 组件，面片跟着一起转）。

⚠️ 第 2 条**只有 `Default` 模式下成立** —— `CameraPlane` 会在每帧覆盖掉 `MeshOrientation`（§14.3）。

### 14.5 网格与精灵的差异速查

| 项 | 网格渲染器 | 精灵渲染器 |
|---|---|---|
| 按粒子尺寸属性 | `Particles.Scale`（`ScaleBinding`，`NiagaraMeshRendererProperties.h:332`） | `Particles.SpriteSize`（`SpriteSizeBinding`，`NiagaraSpriteRendererProperties.h:305`） |
| 朝向来源 | 网格自带朝向 + `Particles.MeshOrientation`（Quat） | 渲染器的朝向模式（`FacingMode` / 对齐方式 / 相机） |
| 材质怎么挂 | `OverrideMaterials` **+** `bOverrideMaterials` | `Material` / 材质数组 |
| 动态材质参数通路 | `DynamicMaterialBinding/1/2/3Binding` → VF `DynamicParam0-3`（`NiagaraMeshRendererProperties.cpp:725-728`） | 同构（`NiagaraSpriteRendererProperties.cpp:355-358`） |
| 左右镜像 | **与精灵相反**：`NiagaraMeshVertexFactory.ush:587 CameraUpDir = ResolvedView.ViewUp` | `NiagaraSpriteVertexFactory.ush:557 CameraUp = **-**ResolvedViewUp` |

- **动态材质参数索引 0/1/2/3 在网格上完全可用**：两个顶点工厂的参数通路结构一致
  （`NiagaraMeshVertexFactory.ush:106-115 / :293-305 / :521-530 / :792-801` 对
  `NiagaraSpriteVertexFactory.ush:130-133 / :359-368 / :955-958`）；
  `MaterialTemplate.ush:302-349 GetDynamicParameter(...)` 按索引取位段
  （索引 0 = 位 `0x0001-0x0008`、1 = `0x0010-0x0080`、2 = `0x0100-0x0800`、3 = `0x1000-0x8000`）。
  有效位掩码由渲染器算：`NiagaraMeshRendererProperties.cpp:763-771 MaterialParamValidMask = GetDynamicParameterCombinedChannelMask(...)`；
  而 `NiagaraRendererProperties.cpp:785-809` → `:760-783 GetDynamicParameterChannelMask` 在找不到
  "限制通道"的静态变量时**默认返回 `0xf`（四个通道全有效）** ⇒ 本项目这种"模块直接写属性"的用法
  不会掉进"掩码为 0 ⇒ 材质拿到默认值"的坑。
  （`Leng刀光` 只写索引 0、`CommnMet_1` 只读索引 0，所以**索引 1/2 在网格上是本项目第一次实测**，结果通过。）
- `Particles.MeshOrientation` 是引擎标准属性（`Quat` 类型，默认 `0,0,0,1`，
  `NiagaraConstants.cpp:301/343/444/578`），DreamFX 能解析并写入。

### 14.6 这条线踩过的坑（收口）

1. **朝向配错 = 静默消失**：最初是 `CameraPlane` + `MeshYaw = -90`，面片被转成侧对镜头（2026-10-05 又栽一次：`Default` + `MeshYaw = -90` 同样侧对）⇒
   什么都看不见，而**引擎不报任何错、日志一个字都没有**。排查时极易误判成"材质坏了 / 特效没生成"。
   配合 §14.3 的配对表逐项核对。
2. **`Particles.Scale` 为 0 = 静默消失**（同样不报错）：源头是 `Particles.SpriteSize` 是 0。
   现在由 `Sprite_Atlas_Size` 的 `MinSize` 下限兜住（§4.2 ③）。
3. **模块是内联的**：Niagara 系统在 `AddModule` 时把模块图**内联**进自己的脚本
   ⇒ **改完 `.dfm` 必须重编 `.dfs` 才生效**，只重编模块不够。
   这一条会让"我明明改了怎么没反应"反复出现（本次 `MinSize` 下限迟迟没进系统就是它）。
4. **重建过的系统资产可能带着 `Enabled` 状态**：三个临时对照发射器在资产里是 `Enabled = false`
   （源码里从来没写过这个属性），于是"四个发射器全都看不见"被误判成**回归** ——
   其实是三个压根没跑。**多发射器系统排查前先确认每个发射器都是启用的**；
   关键发射器建议在源码里显式写 `Enabled = true;`。
5. **材质编辑器预览是空的**：预览里没有粒子数据 ⇒ 三个动态参数全 0 ⇒ 正常路径全透明。
   这是 §10.2 加兜底的直接原因；看到"整张图集"就说明走的是兜底路径。

### 14.7 当时用的排查手法（可复用）

"什么都看不见"且**没有任何报错**时，用**对照发射器把变量一刀切开**比读日志有用得多：

| 发射器 | 材质 | 模块 | 回答的问题 |
|---|---|---|---|
| 主发射器 | `M_FXAtlasSheet` | 播放 + 尺寸 | 目标本身 |
| 对照 A | 工程现成的 `/Game/GameActor2D/SAO_Kirito/ExAsset/CommnMet_4`（`Leng刀光` 用的那张） | 无 | 渲染器 + 网格 + 缩放这条路通不通 |
| 对照 B | `M_FXAtlasSheet` 的**纯色打点版** | 无 | 材质在网格粒子上画不画得出来 |
| 对照 C | 同 B | 只挂尺寸模块 | 是不是尺寸模块的问题 |

再配一版**UV 探针材质**（红 = `TexCoord.x`、绿 = `TexCoord.y`、蓝 = 动态参数是否到达），
一次就能同时读出"网格 UV 是不是 0..1"和"三个动态参数到没到"。实测结论：
网格的 `TexCoord` 正常落在 0..1、动态参数也确实送达 ⇒ 数据通路本身是好的，问题只在朝向。
排查完记得删掉对照组，并把材质换回正式版（正式版函数体在 `.dss` 文件头注释里留了备份）。

## 15. 模板与向导分类（2026-10-05）

用户需求（原话）："可以把插件里做干净，做两个Niagara模板吧，标签是Crossingvoid2D"；
随后改名（原话）："名字换成CrossingvoidAtlas吧"。

### 15.1 两个模板

原先做验证用的两个示例特效 `NS_AtlasDefAtk` / `NS_AtlasDefAtk_Mesh` **直接改造成模板**（用户原话："我是不需要了"）：

| 文本源 | 资产 | 渲染器 |
|---|---|---|
| `DFX/Templates/NS_Atlas2D_Sprite.dfs` | `/AtlasFX/Templates/NS_Atlas2D_Sprite` | Sprite 渲染器 |
| `DFX/Templates/NS_Atlas2D_Mesh.dfs` | `/AtlasFX/Templates/NS_Atlas2D_Mesh` | Mesh 渲染器（`FXDefault` 面片 + `MeshYaw = 0`） |

两个模板默认挂 Defatk 图集当示例，换图集只改 DI 的 Flipbook 路径；发射器名 `Atlas2D` / `Atlas2D_Mesh`。
旧的 `Plugins/AtlasFX/Content/Effects/NS_AtlasDefAtk*.uasset` 已从 git 删除
（`Content/MapS/FXtestMap.umap` 里那两个 Actor 会因此丢掉系统引用，需要在编辑器里换成 `/AtlasFX/Templates/NS_Atlas2D_*`）。

### 15.2 向导分类：`CrossingvoidAtlas`

「创建 Niagara 系统」对话框左侧那栏来自 `UTaggedAssetBrowserConfiguration` 的 `FilterRoot`。
引擎基础配置是 `/Niagara/DefaultAssets/TABC_SystemWizard`（`ProfileName = NiagaraWizard.System`，6129 B），
是引擎资产、不该动；所以 AtlasFX 另放一个**扩展资产** `/AtlasFX/TABC_Atlas2DWizard`
（`bIsExtension = true` + 同一个 ProfileName），对话框打开时会把它复制进基础配置
（`STaggedAssetBrowser.cpp:194-318 ApplyFilterExtensions`）。

- 左侧列表 = `FilterRoot` 的 **children**（引擎那份是 5 个：`All` / `Recent` / `Template` / `Learning Content` / `Lightweight`）；
  `Sections` 只是给 children 做 `FDataHierarchyElementMetaData_SectionAssociation` 用的分组。
- ⚠️ **section 只能进 `Sections`，不能同时挂成 root 的 child** —— `IsDataValid` 会把 child 逐个比对
  `ExtensionFilterClasses`，而 `UTaggedAssetBrowserSection` 不在名单里（报错）。
- ⚠️ **分类里的过滤器只能用目录，不能用用户资产标签**：`UTaggedAssetBrowserFilter_UserAssetTag` 的 AR 预过滤写的是
  `Filter.TagsAndValues.Add("UAT.<标签>")`（`TaggedAssetBrowser_CommonFilters.cpp:63-68`），而 `UAT.*` **进不了 Asset Registry** ——
  实测连引擎自己的 `UAT.Template` / `UAT.Lightweight` / `UAT.LearningContent` 都查到 **0** 个资产。
  所以分类改用 `UTaggedAssetBrowserFilter_Directories`（`DirectoryPaths = /AtlasFX/Templates`，`FilterName = CrossingvoidAtlas`），实测查到 2 个。
- 标签照写不误：两个模板的包元数据里有 `UAT.CrossingvoidAtlas`（Content Browser 的 Manage Tags 界面能用）。
- 扩展资产必须**已保存并进 Asset Registry**（`GetExtensionAssets` 用 `bIncludeOnlyOnDiskAssets=true` 查）。

### 15.3 收尾命令 `AtlasFXSetup`

模块 `Plugins/AtlasFX/Source/AtlasFXEditor`（Editor 类型，已在 `AtlasFX.uplugin` 注册成第二个模块）。命令幂等，可重复跑：

```
"D:\UnrealEngine-5.8.2\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\CrossingVoid\CrossingVoid.uproject" ^
    -run=AtlasFXSetup -stdout -FullStdOutLogOutput -unattended -nopause -nosplash
```

它做五件事：① 重建 `/AtlasFX/TABC_Atlas2DWizard` 的层级（section + 目录过滤器 + root child + SectionAssociation）；
② `SeedRendererMaterialParameters()`：给两个模板的渲染器补 `Sheet` 纹理槽 + `Sheet ← User.Atlas.ResolvedTexture` 属性绑定（见 §16）；
③ 给两个模板写 `UAT.CrossingvoidAtlas`，并清掉改名前的 `UAT.Crossingvoid2D`；④ `ScanPathsSynchronous("/AtlasFX", true)`；
⑤ **重扫之后**再 `AssetUpdateTags(..., FullUpdate)`，并用和对话框一样的 AR 查询自查。
顺序很重要：`bForceRescan` 会用文件头重建 AR 条目，把先推进去的标签冲掉。

踩过的编译坑：
- `EAssetRegistryTagsCaller` 的定义在 `UObject/AssetRegistryTagsContext.h`（`AssetData.h` 里只有前向声明）⇒ C2027。
- `UE::UserAssetTags::GetUATPrefixedTag` / `RemoveUserAssetTag` **没导出** ⇒ LNK2019；
  自己用头里的 inline 常量拼：`FString::Printf(TEXT("%s%s"), *UE::UserAssetTags::UAT_METADATA_PREFIX, *Tag)`；
  删标签直接用 `Package->GetMetaData().RootMetaDataMap.Remove(...)`。
- `AtlasFXEditor` 要用 DI 的类 ⇒ `AtlasFXEditor.Build.cs` 的私有依赖里加 `"AtlasFX"`（不加就 C1083 找不到头）。

## 16. 零手工：贴图自动来自 DI（2026-10-05）

### 16.1 为什么做

用户反馈（原话）："设置的地方也藏的挺深的，操作步骤和创建材质实例差不多一样多了" ——
在渲染器里填贴图要走「材质参数 → 纹理参数 → 索引[0] → 材质参数名 `Sheet` + 纹理」，四层深；
而真正想要的是：**建完系统只要选一个 Flipbook，别的都不用管**。

### 16.2 机制（引擎自带的能力，不是我们发明的）

渲染器的 `FNiagaraRendererMaterialParameters`（`NiagaraRendererProperties.h:252-287`）里有一组
`AttributeBindings`（`TArray<FNiagaraMaterialAttributeBinding>`，`NiagaraCommon.h:1450-1471`），
每项 = `MaterialParameterName` + `NiagaraVariable`（基变量）+ `NiagaraChildVariable`（DI 的子变量）+ `ResolvedNiagaraVariable`。
运行时链路（全部有出处）：

1. `NiagaraRenderer.cpp:509-579` 每帧遍历 `AttributeBindings`；
2. `:561` 判定 `基变量.GetType().IsDataInterface() && 子变量.GetType() == FNiagaraTypeDefinition::GetUTextureDef()`；
3. `:565` 取值 `FNiagaraEmitterInstance::GetBoundRendererValue_GT(...)`（`NiagaraEmitterInstance.cpp:110-144`）：
   `:112` 基变量是 DI → `:121` `UObj->CanExposeVariables()` → `:124` `UObj->GetExposedVariableValue(子变量, ...)`；
4. `:569` `MatDyn->SetTextureParameterValue(材质参数名, 贴图)`；
   `:567` 要求 `Tex->GetResource()` 非空（贴图还没上传时走 `:573-577` 用母材质的默认值兜底）；
5. 顺序：属性绑定在 `:509-579` 先跑，`TextureParameters` 在 `:591-597` 后跑 ⇒ **纹理槽填了就以槽为准**，
   留空（`None`）就跳过 ⇒ 属性绑定的贴图生效。两条并存：默认零手工，想换贴图再填槽。

### 16.3 落地

- DI（`NiagaraDataInterfaceSpriteAtlas.h/.cpp`）新增只读属性 `ResolvedTexture`（`TObjectPtr<UTexture2D>`，
  分类「图集|烘好的表」），烘表时赋值：Flipbook 分支取 `Sprite->GetSourceTexture()`（失败退 `AtlasTexture`），
  兜底分支取 `AtlasTexture`；并且 **`CopyToInternal` 也要复制它** —— 渲染器取值是从**实例 DI** 上取的
  （`NiagaraEmitterInstance.cpp:114 RendererBindings.GetDataInterface`），不复制运行时就是空的。
- DI 覆盖三个虚函数：`CanExposeVariables() → true`、`GetExposedVariables()`（塞进
  `FNiagaraVariableBase(FNiagaraTypeDefinition(UTexture::StaticClass()), TEXT("ResolvedTexture"))`）、
  `GetExposedVariableValue()`（把 `ResolvedTexture` 写进 `OutData`）。
  **类型必须是 `UTexture`**（`NiagaraModule.cpp:1073 UTextureDef = FNiagaraTypeDefinition(UTextureClass)`，
  和 `:561` 的判定同一个定义）；子变量名就是属性名，写法和引擎的
  `UNiagaraDataInterfaceRenderTarget2D::ExposedRTVar`（`NiagaraDataInterfaceRenderTarget2D.cpp:119`）一致。
- 命令 `AtlasFXSetup` 把绑定写进两个模板的渲染器：基变量**按类型**在系统用户参数里找
  （`System->GetExposedParameters().GetParameters(...)` + `Variable.GetType().GetClass() == UNiagaraDataInterfaceSpriteAtlas::StaticClass()`，
  所以使用者把 `Atlas` 改名也不会断），子变量固定 `ResolvedTexture`。
  不调 `CacheValues()`（它要 `UNiagaraEmitterBase*`），直接 `ResolvedNiagaraVariable = NiagaraVariable` ——
  对 `User.*` 用户参数来说 `CacheValues` 本来就是恒等变换（`NiagaraCommon.cpp:1017-1036`）。
- 局限：属性绑定**不参与** DreamFX 的文本链 —— `DreamFXGenerator.cpp:1511-1516` 对 `MaterialParam`
  直接报 `DFX5093: 'MaterialParam' is reserved syntax and is not implemented in v1`。
  所以这条只能由 C++ 命令写进资产；`.dfs` 里看不到，**改完模板不要用 `.dfs` 覆盖回去**（会丢）。
- 对使用者：从模板建的新系统**零手工**；既有系统也**不用重建** —— 命令的目标已经从「两个模板」扩成
  **「所有把 AtlasFX 的 DI 当用户参数用的系统」**（用 Asset Registry 枚举全部 Niagara 系统，逐个拿
  `FindAtlasDataInterfaceParameter` 判定；系统里没有我们的 DI 就一个字都不动）。
  2026-10-05 跑过一次：89 个系统里命中 1 个（`/Game/GameActor2D/Misaka/Material/DefAtk/FX_Defatk`），补了 2 项。
- ⚠️ 命令是引擎刚起来就跑的，Asset Registry 还在**异步扫描** ⇒ 必须先
  `SearchAllAssets(/*bSynchronousSearch=*/true)`，否则 `GetAssetsByClass` 返回空表
  （第一次跑就是「顺手补了 0 个已存在的系统」）。
- 验证有没有生效：跑完看日志
  `渲染器材质参数：<资产> 补了 N 项（Sheet 纹理槽 + Sheet ← User.Atlas.ResolvedTexture 属性绑定；已有的跳过）`。
  日志里基变量打印成 `User.Atlas` ⇒ 用户参数的**存储名带 `User.` 前缀**，绑定里就得写这个名字。

## 17. 贴图尺寸的坑：异步编译期读到替身 32×32（2026-10-05）

现象：用户把图集资产搬到新目录后，预览变成「一坨细碎条纹」（大片细密的白色/淡蓝色条纹）。

定位（反编译存盘的系统）：DI 里 `TextureSize = {x:32, y:32}`，其余全对（6 帧、画布 928×640、fps 15）。
材质拿 `Sizes.xy` 做 UV 归一化 ⇒ `UV = (AtlasRect.xy + Local*AtlasRect.zw) / (32,32)`，最大到 (9.4, 11.3)
⇒ **图集在帧的画布区域内平铺约 9×11 遍**，就是那坨条纹。

根因（引擎实证）：`UTexture2D::GetSizeX()`（`Texture2D.cpp:349-363`）在 `IsDefaultTexture()` 为真时返回
`GetDefaultTexture2D(this)->GetSizeX()`，而 `GetDefaultTexture2D`（`Texture2D.cpp:230-239`）的注释是
**"Get the optimal placeholder to use during texture compilation"**，`UTexture::IsDefaultTexture()`
（`Texture.cpp:511-516`）的注释是 **""IsDefaultTexture" actually means that a temporary default stand-in is being
used because the texture is being async built"**。搬资产让 DDC 失效 ⇒ 贴图平台数据在后台编译 ⇒
这期间 `GetSizeX()` 返回**替身贴图的 32**。DI 恰好在这段时间烘了表，就把 32×32 存进了资产。

修复（三层）：
1. 读尺寸前 `Texture->BlockOnAnyAsyncBuild()`（`Texture.cpp:518-529`，引擎自己的写法：`FinishCachePlatformData()` +
   `FTextureCompilingManager::Get().FinishCompilation({this})`；声明在 `Texture.h:2050`）。
2. `ClampTextureSizeToRects()`：贴图尺寸如果比「帧矩形的最大边界」还小就一定是错的（不依赖贴图、纯由帧表推出），
   改用帧矩形范围并打 Warning。对这张图集给出 595×485（真值 597×487，误差 0.3%）。
3. 材质 UV 加 `saturate`：正常情况是恒等变换，出问题时最坏采错一块而不是花屏。
4. `BakeVersion` 2 → 3 ⇒ 所有旧资产的签名立刻过期 ⇒ **加载时自动重烘**，用户不用点任何按钮。

验证：无头加载该系统，日志 `LogAtlasFX: ... 加载时自动重烘帧表 —— 6 帧 / 15.00 fps / 画布 928x640 / 贴图 597x487`。

顺带记两条：`UPaperSprite::GetSourceTexture()`（`PaperSprite.cpp:1442-1470`，整段在 `#if WITH_EDITOR`）
只认 `SourceTexture` 这个软引用、**不退回** `BakedSourceTexture`，而后者是 `protected`（`PaperSprite.h:103-104`）
⇒ 插件读不到（硬写会 `C2248`）。所以取贴图链是 `GetSourceTexture()` → DI 自己的 `AtlasTexture`。

---

## 18. PaperZD 序列编辑器特效预览（2026-10-05）

### 18.1 起因

用户原话：「然后你可以做一个扩展吗，给PaperZD做一下特效可预览，我用的特效通知是
`Content/BaseC/ExCordLibrary/Notify/TxSpawn.uasset`。你能看懂的话就顺便做进插件吧，这样子也方便你制作预览」

现象：在 PaperZD 的 AnimSequence 编辑器里播放动画，通知轨上的 `TxSpawn` 到点了，但视口里什么都不生成。

### 18.2 为什么预览里通知不生成特效

PaperZD 的预览**确实会触发通知**，卡在 OwningInstance 上：

| 环节 | 出处 | 事实 |
| --- | --- | --- |
| 通知默认在编辑器里也触发 | `PaperZDAnimNotify_Base.cpp:19` | `bShouldFireInEditor = true;` |
| 触发条件 | `PaperZDAnimPlayer.cpp:174` | `if (OwningInstance != nullptr \|\| Notify->bShouldFireInEditor)` |
| 预览播放器怎么调 | `PaperZDAnimationSourceViewportClient.cpp:107` | `Player->TickPlayback(CurrentAnimSequencePtr.Get(), PlaybackTime, DeltaSeconds, bLooping);` —— **不传 OwningInstance** |
| 通知拿到什么 | `PaperZDAnimNotify.cpp:53-56` | `OnReceiveNotify(OwningInstance)` ⇒ 预览里恒为 **nullptr** |

而工程在用的 `TxSpawn` 是蓝图，第一步就是 `Cast<BI_2DCharAnimBP>(OwningInstance)`
（资产里能读到 `K2Node_DynamicCast_AsBI_2DChar_Anim_BP` / `/Game/BaseC/ExCordLibrary/BIs/BI_2DCharAnimBP`），
拿到 nullptr 直接跳过 ⇒ 什么都不生成。它依赖角色/AnimBP，而预览世界（`EWorldType::EditorPreview`）
里只有 `PaperZDAnimationSourceViewportClient.cpp:36` 造的一个裸 `UPrimitiveComponent`，没有 AnimBP。

**PaperZD 自带的 Niagara 通知能预览**，是因为它压根不看 OwningInstance，而是挂在
`SequenceRenderComponent` 上生成（`PaperZDAnimNotify_NiagaraEffect.cpp:53-79`）：
`SpawnSystemAttached(PSTemplate, SequenceRenderComponent.Get(), ...)` / 不挂接时
`SpawnSystemAtLocation(SequenceRenderComponent.Get(), ...)`。

### 18.3 扩展的做法（非侵入）

放在编辑器模块 `AtlasFXEditor`：`Private/AtlasFXPaperZDPreview.h` + `.cpp`，模块启动时挂编辑器 ticker。

1. **找预览播放器**：`TObjectIterator<UPaperZDAnimPlayer>`，用反射读私有的
   `RegisteredRenderComponent`（`PaperZDAnimPlayer.h:52`，`UPROPERTY(Transient) TWeakObjectPtr<UPrimitiveComponent>`，
   没有 getter），组件的世界 `WorldType == EWorldType::EditorPreview` 才算数。每 60 个 tick 扫一次
   （`TObjectIterator` 要遍历对象表，不能每帧来）。
2. **读时间**：`GetCurrentAnimSequence()` + `GetCurrentPlaybackTime()`（都是公开 API）。
3. **算跨帧**：逐字复刻 `PaperZDAnimNotify.cpp:27-50` 的规则（含回绕 `bLooped` 与反向分支），
   外加同一个 `bWasActiveLastFrame` 去重，保证边界上不会触发两次。
4. **取 FX 数据**：反射。Niagara 系统**按类型找**（`FObjectPropertyBase::PropertyClass->IsChildOf(UNiagaraSystem::StaticClass())`，
   蓝图里叫 `Niagara` 还是 `PSTemplate` 都认）；`Offset`/`Scale`（`FVector`）、`Rotation`（`FRotator`，也兼容 `FVector`）、
   `NotAttach`（`FBoolProperty`）按名字找，找不到就用默认值。
5. **生成**：挂接时 `SpawnSystemAttached(..., EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/true, ENCPoolMethod::None, true)`，
   `NotAttach` 勾了则 `SpawnSystemAtLocation`。语义与 `TxSpawn` 蓝图一致。
6. **收尾**：同一条通知重新触发（循环播放 / 来回拖时间轴）先 `DestroyComponent()` 上一次的；换序列、
   播放器失效、模块卸载、CVar 关掉时全部收干净。

**跳过原生通知**：类来自 `/Script/PaperZD` 且不是蓝图生成类的通知（`UPaperZDAnimNotify_NiagaraEffect`
之类）本来就能预览，再生成一次就重了 ⇒ `ShouldIgnoreNotify()` 直接跳过。

**不改任何工程资产、不改 PaperZD**。已排除的方案：给预览注入 OwningInstance（viewport client 里写死 nullptr，
外部注入不进去）、改 `TxSpawn` 蓝图、改 PaperZD 本体（第三方插件，改了会被上游更新覆盖）。

### 18.4 开关与边界

* 控制台变量 `AtlasFX.PaperZD.PreviewFX`（默认 1，设 0 关闭并收掉已生成的特效）。
* **通知表的时机坑（2026-10-05 实测，已自愈）**：通知表只在「换序列」时重建
  （`AtlasFXPaperZDPreview.cpp:387` 的 `SequencePath != State.SequencePath` 分支），而编辑器刚打开时
  通知的类/属性可能还没加载完 ⇒ 那一瞬间会一条都认不出来，日志写
  `跟踪预览序列 DefAtk：0 条通知会生成特效（序列上共 3 条）`，且**此后永不重试**
  （现场：编辑器 10:02:07 扫出 0 条，同一条序列在 10:09:56 用命令行扫是正常的 1 条）。
  ⇒ 现在 `UpdatePlayer` 在「跟踪到 0 条、但序列上确实有通知」时**每 2 秒补扫一次**，
  补上后打 `预览序列 %s 的通知表补扫成功：现在能认出 %d 条通知。`；
  首次认不出时还会逐条 `DumpNotifyDiagnostics`（每条序列只打一次，不刷屏）。
* 预览世界只有一个（`PaperZDAnimationSourceViewportClient` 创建的那个），且它的播放器**永远**拿不到
  OwningInstance ⇒ 不会和通知自身的逻辑重复生成。
* 编辑器 ticker 与预览视口的刷新率不一定同频，极快的一次性特效在预览里可能看不出差别 —— 这是预览，不是最终效果。
* 只认 `EWorldType::EditorPreview` ⇒ 游戏内（PIE）完全不参与，行为零影响。
* **`Scale` 的工程约定**（2026-10-05 用蓝图连线 dump 逐字核对）：`TxSpawn` 的 `Scale` 是「单值或整向量」两用 ——
  `选择` 节点的 `Index = OR(Equal(Scale.Y, 0), Equal(Scale.Z, 0))`，为真取 `Option 1 = (Scale.X, Scale.X, Scale.X)`，
  为假取 `Option 0 = Scale` 原样。Misaka 的 `DefAtk` 上实测 `Scale = 1.0 / 0.0 / 0.0` ⇒ 展开成 `(1,1,1)`。
  **`SpawnSystemAttached` 本身没有 Scale 参数**（`NiagaraFunctionLibrary.h:96` 那个重载），
  蓝图是先 Spawn 再 `SetRelativeScale3D`；只有 `SpawnSystemAtLocation`（`:93`）带 Scale。
  扩展照抄这个顺序 —— 把 scale 塞进带 `FVector Scale` 的 `SpawnSystemAttached` 重载（`:98`）会把特效压扁。
* **位置的工程约定**：`Location = GetWorldLocation(Comp) + (Owner.Tags 含 "1P" ? Offset : -Offset)`，
  但那个加法节点的 **Z 分量直接从 `Offset` 上 Break 出来**（`Select Vector` 只接了 X/Y）⇒
  **只镜像水平面、不翻高度**。预览的渲染组件没有 Owner ⇒ 走 `-Offset` 那一支。
* **相机的工程约定（2026-10-05 定案：三个视口同轴 ⇒ 预览不需要任何补偿）**：PaperZD 预览视口是
  「X 向右、Z 向上」，即**预览相机沿 Y 轴看**；而**地图视口和游戏里的 2D 相机也沿 Y 轴看**
  —— 用户实测：预览与游戏完全一致（连"一条缝"都一样）。
  最初「特效生成了却看不见」的真因**不在预览**：是网格模板自己写了 `MeshYaw = -90`，
  把面片法线从 Y 转到 X ⇒ 与相机轴垂直 ⇒ **游戏里和预览里都只剩一条缝**（模板已修，见 §14.3）。
  ⇒ 预览**不叠加任何朝向补偿**（曾经的 `AtlasFX.PaperZD.PreviewYaw` 默认 90 是误判，已删除）。
  自查办法：日志里 `预览生成特效：...` 后面跟着 `LogNiagara: Compiling System ... <系统名>`
  就说明特效确实生成过。
* **坐标的工程约定（2026-10-05 定案：让预览模仿游戏，`AtlasFX.PaperZD.MimicGame` 默认开）**：
  两边原本是**两套坐标**，这才是「预览里正好、游戏里 2 倍超出」的根因（用户实测）：
  * 游戏：角色根组件（缩放 1，**原点在脚底**）→ 精灵组件（相对 `(0,0,+40)`、缩放 `0.5`）；
    特效 `SpawnSystemAttached` 到**根组件**上，**不继承**精灵那 0.5 ⇒ 角色 2.57 m、特效按 1:1 尺寸。
  * 预览：PaperZD 自己建的翻转书是 **1:1 + 原点** ⇒ 角色 5.14 m ⇒ 同一个 `Offset`、同一份特效，
    相对角色就是两倍大、位置也差一截。
  用户定案（原话「让预览去模仿游戏：预览把翻转书也缩放 0.5、偏移 +40，特效挂到一个"根组件替身"上」
  与「0.5 缩放和 40 偏移是全角色统一的，那个改不了」）⇒ **只动预览插件**：
  * 预览把翻转书按 `(0,0,+40)` × `0.5` 摆（`AtlasFX.PaperZD.SpriteZ` / `AtlasFX.PaperZD.SpriteScale`）
    ⇒ 画面与游戏一致；
  * 特效按「挂在游戏里那个根组件（缩放 1、原点在脚底）上」折算位置与缩放
    ⇒ 位置与尺寸都与游戏一致，`Offset = 0` 就是脚底（折算系数：根组件相对翻转书
    = `S(1/SpriteScale) ∘ T(0,0,-SpriteZ)`，即相对 `(0,0,-40/0.5 = -80)`、相对缩放 `1/0.5 = 2`）。
  ⚠ **2026-10-06 修正：替身这个组件已经删掉了，改成纯数学折算**。原因是它是个**注册在预览世界里**
  的组件，预览世界先于它销毁时引擎会在 `FScene::Release()` 里抛 ensure
  （`RendererScene.cpp:4337`「Component Name: SceneComponent /Engine/Transient.PaperFlipbookComponent_0 …
  Component Asset: None」，用户在编辑器里直接看到了这个弹窗）。现在特效仍然挂在**翻转书**上，
  只用 `GetRootLocalToFlipbook()` 把「挂在角色根组件上」的相对变换折算过去：
  根组件相对翻转书 = `S(1/SpriteScale) ∘ T(0,0,-SpriteZ)` ⇒ 挂点位置 `RelativeToRoot * 该变换`、
  相对缩放 `Scale / SpriteScale`；世界生成那条路用 `RootToWorld = 该变换 * 翻转书世界变换`。
  数值等价（Misaka `Offset.Z = -88.5` ⇒ 世界 Z 还是 `-88.5`），但不引入任何额外组件，ensure 消失。
  关掉 `AtlasFX.PaperZD.MimicGame` 时 `GetRootLocalToFlipbook()` 返回单位变换 = 特效直接挂翻转书。
  角色 / 蓝图 / 图集 / 现有特效一律不动。
  ⚠ 曾经的 `AtlasFX.PaperZD.FootAlign`（默认 1，已删）只补了「挂点差」、**漏掉了缩放差** ——
  它让预览对上了、游戏却错得更明显；别再走「单点补偿」的思路，要补就补整套坐标。
  ⚠ 更早那版用**当前帧**包围盒算脚底，值随帧/随序列乱跳（-257 / -300 / -261 / -279 cm），也别再走。
  要「零换算」则必须动工程资产（用户已否）：① 游戏侧把 `TXComp` 改成返回翻转书组件
  （现有特效会整体上移约 2.57 m，每条通知的 `Offset` 都要重对）；② 把帧精灵的轴心改成画布底边
  （bottom-center）⇒ 翻转书原点就落在脚底（每个角色的翻转书相对 Z 要跟着调，图集要重导）。
* **别假设「特效画布」与「角色画布」比例相同（2026-10-05 我算错过一次，用户当场纠正）**：
  图集是按 **1 px = 1 cm** 出的（`Sprite_Atlas_Size` 把 DI 的画布像素数直接当厘米用：
  `Scale = (928/92.8, DepthScale, 640/64.0)`，`UniformScale = 1`）⇒ 特效的「自然尺寸」就是
  通知里的 `Scale = 1.0`，**不要**再乘一个「角色画布 / 特效画布」的比值。角色精灵是另一个像素密度
  （Misaka：1:1 时轴心到脚底 257 cm），两套画布本来就不重合，对齐是靠 `Offset` 手工摆的。
  对 `TxSpawn`（挂角色根组件）的正确竖直对齐公式：
  `Offset.Z = 精灵组件相对 Z − 轴心到脚底距离 × 精灵组件缩放`（Misaka：`40 − 257 × 0.5 = −88.5 cm`）。
  网格轴心在**底边中点**，所以 `Offset.Z` 与 `Scale` 无关（缩放绕轴心做，底边不动）。
* **同一条通知重复触发：复用组件并重置，不要"销毁再新建"（2026-10-06 用户报「紧接着再播一次就不显示」）**：
  特效粒子的寿命常常比序列本身长，`DestroyComponent()` + 新建的那一帧里上一轮粒子还在，
  看起来就像"第二次不显示"；而世界生成那条路以前用池化（`ENCPoolMethod::AutoRelease`），
  复用回来的组件未必会自己重新激活。现在：同一条通知上一次的组件还在、且系统没换 ⇒
  重设位置/旋转/缩放 + `Activate(/*bReset=*/true)`（清掉上一轮粒子、从第 0 帧重播）；
  只有通知上的 Niagara 资产换了才销毁。两条生成路径统一用 `ENCPoolMethod::None`，
  组件生命周期完全由预览扩展自己管（换序列 / 关扩展时统一销毁）。
* **往回拖时间轴 ≠ 循环回绕（2026-10-06 用户报「往回拖会固定触发所有特效，不管通知在进度条前面还是后面」）**：
  两者在「时间倒回去」这一点上长得一样，但走过的区间不同 ——
  * **回绕**走「旧时间 → 末尾」+「开头 → 新时间」两段，这两段之和就是这一帧的推进量（几毫秒）；
  * **往回拖**只走拖动区间本身。
  判据用「跳回去的幅度」：`JumpBack = Last - Playtime`，`WrapPath = (Duration - Last) + Playtime`，
  `JumpBack > 2 * WrapPath` 才算回绕。回绕用「尾段 + 头段」的判据（0 帧通知每圈重新触发，与游戏一致）；
  往回拖只认 `Playtime <= T <= Last` 这一段里的通知。
  早先两者共用回绕那支（`Playtime >= T || Last <= T`），于是往回拖一下会把整条序列的特效全放一遍。
* **回绕时挂在 0 帧的通知要能再触发 —— 通知判定不能按「两次时间的差」分方向（2026-10-06 用户报「放在最开头就只会播放一次」）**：
  PaperZD 回绕时是用 `Fmod` 把 `PlaybackMarker` 归零的（`PaperZDAnimPlayer.cpp:131`），传下去的
  `DeltaTime` **仍然是正的**，标志回绕的是 `CurrentTime < PreviousTime` ⇒ 走的还是「正向 + bLooped」
  那一支，条件放宽成 `Playtime >= T || Last <= T`（于是 0 帧的通知每次循环都重新触发）。
  预览早先按 `Playtime - LastPlaybackTime` 分方向，回绕时差值为负 ⇒ 落进反向那一支 ⇒ 只在第一次播。
  现在：`Playtime >= Last` 走正向那一支；`Playtime < Last`（回绕 / 手动往回拖时间轴）按正向 + bLooped 处理。
* **第 0 帧的通知不触发（2026-10-06 用户报的 bug）**：通知挂在 `Time = 0` 时，按播放看不到特效。
  原因不是 PaperZD，是预览**把暂停的帧也算进去了**：时间停在 0 的那些帧走 PaperZD 判据的
  `DeltaTime <= 0` 那一支（`Playtime <= Time && LastPlaybackTime >= Time`，`0 <= 0` 成立）⇒
  通知被判成「已激活」⇒ 真按播放时没有上升沿 ⇒ 永远不触发。游戏里不会踩：一开播就是新的
  playback handle，`bPersistentActiveState` 从 false 开始。修法两条（`UpdatePlayer`）：
  ① `FMath::IsNearlyZero(DeltaTime)` 的帧直接 return，不参与判定；
  ② 记 `bWasPlayingLastFrame`，**暂停 → 播放**的那一下把所有 `bWasActiveLastFrame` 清零，
  等价于游戏里新建 handle（于是从 0 重播也能再触发）。
* **镜像只发生在 NotAttach 那一支**：蓝图里 `SpawnSystemAtLocation` 的位置是
  `GetWorldLocation(Comp) + (Owner.Tags 含 "1P" ? Offset : -Offset)`，而挂点那一支直接用 `Offset`。
  预览的渲染组件没有 Owner（没有标签）⇒ 走 `-Offset` 那一支，与游戏里的普通角色一致。
* **地图里摆的 Actor 只是"摆着看"**：游戏里特效是通知在角色身上生成的，地图里那个 Actor 的
  位置/旋转对游戏没有任何影响 —— 要改游戏里的位置得改通知的 `Offset` 或美术在画布中的位置。
* 自查手段：跑一次 `-run=AtlasFXSetup`，日志里会打印播放器反射结果、序列的通知清单、
  每条通知解析出的 Niagara 系统与 Offset/Rotation/Scale/NotAttach（`DumpPaperZDPreviewTargets()`），
  以及**通知蓝图的全部节点与连线**（`DumpBlueprintGraph()`，用来核对上面两条约定）。

### 18.5 使用

打开 AnimSequence（例如 `DefAtk`）→ 视口里播放 → 通知时间点到了就会看到特效。
如果没反应，先看 Output Log 里有没有 `LogAtlasFXPreview: 接管 PaperZD 预览播放器...`；
没有就是没找到预览播放器（检查 PaperZD 模块是否加载、预览视口是否在实时渲染）。
旧版 DLL 上的临时绕过（**已经不需要了**）：在序列编辑器里切到另一个 AnimSequence 再切回来，会重建通知表。
2026-10-06 起改成自动的，两条：
* 通知表每帧比对**签名**（序列上通知的数量 + 每个通知的名字 + 是否被忽略），变了就重建 ——
  所以「序列编辑时加/删通知、换通知资产」当场生效，不用再切序列；
* 建表时**不再过滤「还没挂 Niagara 系统」的通知**（系统是到点时现读的）—— 所以「先建通知、后挂系统」
  也能立刻出特效。以前这种通知进不了表，只能靠切序列重建（用户报的「刚创建出来还没保存经常不显示」
  就是这个）。没挂系统的通知到点时会打一条一次性日志，不会每圈刷屏。

---

## 19. Content Browser 右键：从 Flipbook 一键建特效（2026-10-06）

**入口**：在 Content Browser 里右键一个 **Paper Flipbook** → 菜单**最前面**的 `AtlasFX` 分区里两项：
「创建 AtlasFX 特效（精灵渲染器）」/「创建 AtlasFX 特效（网格渲染器）」。

**定案（用户 2026-10-06 拍板）**：
1. 只挂 **Flipbook**（不做 Sprite、不做图集源文件）—— DI 吃的是 Flipbook，Sprite 那条要额外造 Flipbook，先不做；
2. 分区放**最前面**（Flipbook 本来没有别的插件分区，不挤别人）；
3. 生成到**同目录**，命名 `NS_<Flipbook名>_Sprite` / `_Mesh`（重名由 `DuplicateAsset` 自动加后缀）；
4. **多选时每个 Flipbook 各生成一个**系统（选中项里混了别的资产时只处理其中的 Flipbook）；
5. **只管自己**：不顺手创建 PaperZD 动画序列（ZDBridge 那边有 `CreatePaperZDSequence`，需要时另外接）。

**实现**（`Plugins/AtlasFX/Source/AtlasFXEditor/Private/AtlasFXFlipbookActions.cpp`）：
- 挂菜单：`UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UPaperFlipbook::StaticClass())` +
  `Menu->AddSection(..., FToolMenuInsert(NAME_None, EToolMenuInsertType::First))` +
  `AddDynamicEntry`；分区按 `FToolMenuOwnerScoped(OwnerName)` 归属，Shutdown 时
  `UnregisterOwnerByName` 一次清掉。注册走 `UToolMenus::RegisterStartupCallback`（菜单系统可能还没建好）。
  参考实现：`Plugins/DreamFX/Source/DreamFXEditor/Private/UI/DreamFXMenus.cpp:359-375`。
- 选中项：`UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection)` → `SelectedAssets`。
- 找 DI：`System->GetExposedParameters().GetDataInterfaces()` 里 `Cast<UNiagaraDataInterfaceSpriteAtlas>`
  —— **按类型找不按名字**，与 `AtlasFXSetupCommandlet.cpp:361-375` 同一套判据。
- 接图集：`Atlas->Flipbook = <右键那个>; Atlas->RefreshFromSource();` 然后**存盘**
  （帧表烘在资产里，不存盘下次打开还是空的），最后 `GEditor->SyncBrowserToObjects`。
- **安全闸**：复制出来的系统若与模板**共用同一个 DI 实例**（说明复制没深拷 DI），直接报错跳过，
  绝不改模板 —— 宁可少建一个也不要污染模板。

**构建上踩到的两件事**（都已修）：
- `UNiagaraDataInterfaceSpriteAtlas` 是 `MinimalAPI`（只导出反射），编辑器模块要直接调
  `RefreshFromSource()` 就必须给它加 `ATLASFX_API`，否则 `LNK2019`；
- `AtlasFXEditor.Build.cs` 补依赖：`Paper2D` / `ToolMenus` / `ContentBrowser` / `AssetTools` /
  `Slate`（`FUIAction`、`FSlateNotificationManager`）/ `SlateCore`（`FSlateIcon`）。

**验证状态**：编译通过、`-run=AtlasFXSetup` 无头跑通（模块加载/卸载干净，无 Error）；
**菜单本身要在编辑器里肉眼确认**（右键 Flipbook 看有没有 AtlasFX 分区、生成后 DI 的 Flipbook 与帧表对不对）。

---

## 20. 网格面片的轴心：从底边改到几何中心（2026-10-06）

**背景**：网格渲染器把**粒子位置**放在面片的**轴心**上，而精灵渲染器的四边形是以粒子位置为**中心**的。
`/ZDBridge/FX/FXDefault` 原来的轴心在**底边**（FBX 顶点 Z `0…0.64`）⇒ 网格版的面片从粒子原点
往**上**长 ⇒ 同一份图集数据，网格版比精灵版整体高出「画出来高度的一半」（Misaka 实测 **120 cm**）。
⚠️ 那个数随 `SizeScale` / `FitFrame` 变（模板默认配置下是 **160 cm**），所以**不是**能写死的常数 ——
一开始想加一个固定的 `-120` 模块，被用户否掉了，改从根上修。

**定案（用户 A 方案：改面片本身，而不是给每个特效加补偿）**：FBX 顶点整体下移 `0.32`，
其余一概不动（`Lcl Rotation=(-90,0,0)`、`Lcl Scaling=100`、`UnitScaleFactor=1` 都没变，
两份 FBX 只差 47 字节：`FileId`/`CreationTime` + 4 个顶点的 Z）。
重导后 `/ZDBridge/FX/FXDefault` 的包围盒 = **中心 (0,0,0)、半尺寸 (46.4, 0, 32)** ⇒ 轴心在**几何中心**，
网格版与精灵版天然对齐 —— **任何 Z 补偿都不要加**（加了反而偏低）。

**代价（预期内）**：存量网格特效的画面整体下移「各自画出来高度的一半」—— 那正是原来偏高的量，
所以它们现在才对齐；`/ZDBridge/FX/FXDefault` 如果别处也在用（`Leng刀光` 等），那些同样会下移。

**顺带加的工具**：`-run=AtlasFXSetup -ReimportMesh=/ZDBridge/FX/FXDefault`
（`AtlasFXSetupCommandlet::ReimportMeshAsset()`）—— 用 `FReimportManager` 重导（沿用资产里存的导入设置），
并打印重导前后的包围盒中心。判据一句话：**Z 中心 = 0 ⇒ 轴心在几何中心；Z 中心 = 高度/2 ⇒ 在底边**。
（命令行重导比 Python 的 `import_asset_tasks` 稳：后者试过，能导进去，但命令工具跑在资产注册表扫完之前
读不到资产、`unreal.log` 也不落盘。）

**相关文本源**：`DFX/Modules/M_SpriteAtlasSize.dfm` 的文件头、`DFX/Templates/NS_Atlas2D_Mesh.dfs` 的
MeshRenderer 段注释都改成了「轴心在几何中心」，并写明"不要再加 Z 补偿"（改的是注释，不影响构建产物）。

### 20.1 对齐量加在哪一层（同日定案）

`Particles.Scale` **只缩放渲染出来的网格几何，不动粒子位置**，所以：

* 挪**面片原点** / `PivotOffsetSpace = Mesh` —— 局部单位，**会被 Scale 放大**
  （网格模板默认配置下 `Scale.z = 320/64 = 5`：局部 40 cm ⇒ 世界 200 cm），
  而且每个系统的 `SizeScale` 不同、放大倍数也不同 ⇒ 补偿量随系统漂移，**死路**；
* 加在 **`Particles.Position`（Spawn 阶段）** —— **世界单位，永远不被放大**，一个数就是一个数；
* 渲染器的 `PivotOffset` + `PivotOffsetSpace = Simulation/World` 也是世界单位，但引擎 5.8 已把它挪进
  「每个网格一项」的结构（`FNiagaraMeshRendererMeshProperties::PivotOffset`，旧的
  `PivotOffset_DEPRECATED` 仍在头文件里），DreamFX 的 `.dfs` 写不进去，只能手点资产。

**定案**：网格模板的 `ParticleSpawn` 末尾调用新模块 `Atlas_Mesh_Offset`
（`DFX/Modules/M_AtlasMeshOffset.dfm` ⇒ `/AtlasFX/Modules/Atlas_Mesh_Offset`），
**默认 `Offset = (0, 0, 40)`**（角色精灵组件在 Z = +40 —— Misaka 蓝图的 Sprite 相对位置；
网格特效跟着抬才与精灵那套对齐）。每个系统可以在模块栈里改那次调用的输入。

⚠️ 两条必须记住：

* **只放 Spawn**：位置逐帧累积，放 `ParticleUpdate` 会越跑越远；
* **存量系统注意叠加**：若某系统的通知 `Offset` 里已经算过这 40，把模块里的 Z 改回 `0`，否则变 80。

### 20.2 ⚠️ DFX 重建模板会清掉「Sheet 绑定」——重建后必须再跑一次命令工具

2026-10-06 踩到：为了让网格模板带上偏移模块，我用 `dfx.ps1 build` 重建了
`DFX/Templates/NS_Atlas2D_Mesh.dfs` ⇒ 渲染器被重新生成 ⇒ **它上面的「Sheet 纹理槽」和
「`Sheet` ← `User.Atlas.ResolvedTexture` 属性绑定」一起没了**。

原因：那条绑定是 `AtlasFXSetupCommandlet::SeedRendererMaterialParameters()` **事后补上去的**
（引擎里网格渲染器的自定义材质只能走 `OverrideMaterials` + `bOverrideMaterials`，而「材质参数 →
属性绑定」这块 DFX 的 `.dfs` 表达不了）。DFX 重建 = 按文本重新生成渲染器 ⇒ 命令工具补的东西就没了。

**症状**：从重建后的模板新建的网格特效渲染成**一整块惨白/纯色的大方块**（材质拿不到图集，
UV 换算退化）。判别方法很简单 —— **只有重建之后新建的系统坏，更早的正常**。

**修法**：再跑一次命令工具（幂等，已有的会跳过）：

```
UnrealEditor-Cmd.exe CrossingVoid.uproject -run=AtlasFXSetup -stdout -unattended -nopause -nosplash
```

日志里会逐个系统打印「补了 N 项」，坏的那些是 `补了 2 项`，正常的 `补了 0 项`。
**规矩：凡是 `dfx.ps1 build` 重建过模板，之后必须跑一次 `-run=AtlasFXSetup`。**

### 20.3 「必须先打开一次特效，别处才显示」——两个洞（2026-10-06 用户报）

查下来是烘表数据持久化的两个洞，都已加固：

1. **签名只描述「源」**。`NeedsRebakeFromSource()` 原来只在
   `BakedSourceSignature != ComputeSourceSignature()` 时重烘 ⇒ 如果在无头进程（或贴图还没编译好）时
   烘出了**残缺结果**（`ResolvedTexture = None`、贴图尺寸退化成 1×1），而**源没变** ⇒ 永远不再重烘。
   现在加了「结果残缺也重烘」的判据：**帧数与 Flipbook 对不上 / `ResolvedTexture` 为空 /
   `TextureSize ≤ 1`** 三者之一即重烘。烘表算法版本 `v3 → v4`，强制所有资产在下次加载时重烘一次。
2. **重烘只在内存里，命令工具一退出就丢**。`PostLoad` 重烘后会 `MarkPackageDirty()`，而
   `-run=AtlasFXSetup` 原来只保存「这次补了绑定」的系统 ⇒ 重烘结果被丢掉。
   现在改成 `SeededCount > 0 || Package->IsDirty()` 都存盘，日志会写「本次没补，但包是脏的
   （DI 加载时重烘过）⇒ 一起存盘」。

验证（2026-10-06）：跑一次命令工具，项目日志里 8 个系统打印
`加载时自动重烘帧表 —— N 帧 / F fps / 画布 928x640 / 贴图 …` 并逐个存盘
（`Defence` / `Ko-fx1s1` / `Ko-fx2s1` / `Ko-fx3s1` / `Effects1` / `Sk1-fx2` / `Sk2-fx1` / …）。

---

## 21. 右键创建的两个后续修正（2026-10-06）

### 21.1 创建后必须编译再存盘（修「右键建完不显示，打开一次就好」）

症状：用右键「创建 AtlasFX 特效（网格渲染器）」建出来的系统在别处不显示，
**必须在 Niagara 编辑器里打开一次**才生效。

原因：Niagara 的编译结果（脚本 + 参数哈希那一套）是**存在资产里**的，而创建那条路是
「`DuplicateAsset` → 改 DI → `SavePackage`」—— 中间**没有人编译** ⇒ 存下去的是一个**没编译**的
系统；打开一次编辑器把它编译掉，之后才生效。

修法（`AtlasFXFlipbookActions.cpp` 的 `CreateSystems`）：改完 DI 之后
`RequestCompile(/*bForce=*/false)` + `WaitForCompilationComplete(false, false)`，**等编译完成再存盘**。

### 21.2 Sprite 表右键：创建 Flipbook 时加 `_Flipbook` 后缀

用户定案：`.paper2dsprites` 导入保持原始命名（那是引擎导入器的行为，改不了也没必要改），
但**建 Flipbook 时加 `_Flipbook` 后缀** —— 引擎自带的「Create Flipbooks」用 sprite 名推导出来的名字
常常和刚导入的 Sprite 表/贴图同名（都叫 `Ko`）⇒ `CreateUniqueAssetName` 于是给出 `Ko1`。

实现：右键 Sprite 表 → 新菜单项「创建 Flipbook（名字加 `_Flipbook`）」
（`AtlasFXFlipbookActions.cpp` 的 `CreateFlipbooksFromSpriteSheets`，复刻
`PaperSpriteSheetAssetTypeActions.cpp:76-164` 的名字推导，只把最终名字改成 `<推导名>_Flipbook`）。

⚠️ 两个坑：

* `UPaperSpriteSheet` 声明在引擎插件 `PaperSpriteSheetImporter` 的 **Private** 头里
  （`Source/PaperSpriteSheetImporter/Private/PaperSpriteSheet.h`）⇒ 外部模块 include 不到，
  只能按类路径 `FindObject<UClass>(nullptr, TEXT("/Script/PaperSpriteSheetImporter.PaperSpriteSheet"))`
  + 反射读 `Sprites` / `SpriteNames`（`TSoftObjectPtr` 的内存布局就是 `FSoftObjectPtr`，
  可以 `reinterpret_cast` 之后 `LoadSynchronous()`）；
* `AtlasFXEditor.Build.cs` 需要 **`Paper2DEditor`** 依赖（`FPaperFlipbookHelpers` / `UPaperFlipbookFactory`）。



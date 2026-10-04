# AtlasFX 上手（照着点就行）

> 这份只讲「点哪里、输什么」。原理写在 `DESIGN.md`。
>
> **⚠️ 2026-10-04 起，这份文档的大部分已经过时**：材质 / 模块 / 系统三层现在都由**文本源**生成，
> 不再需要手点节点 —— 见 `DESIGN.md` §11（现状与遗留）、§12（闭环脚本 `Tools/New-AtlasSystem.ps1`）。
>
> 它仍然有用的地方：**编辑器侧的操作知识**（数据接口怎么加成用户参数、用户参数面板在哪、
> 渲染器和材质该填什么），以及在文本链出问题时拿来对照「手点版长什么样」。

---

## 0. 再编一次（必须先做）

关掉编辑器 → 命令行跑：

```
D:\UnrealEngine-5.8.2\Engine\Build\BatchFiles\Build.bat CrossingVoidEditor Win64 Development -Project=C:\CrossingVoid\CrossingVoid.uproject
```

编完重开编辑器。

> 为什么还要编：新加的 `Get Frame Params` 一次就吐出材质要的三包数据，模块从 15 个节点降到 7 个；另一个 `Wrap Frame` 负责循环回绕。

---

## 1. 数据接口 `Sprite Atlas`（帧表自动烘，不用手填）

1. 左下「**参数**」面板 → 「**用户参数**」那一行右侧的 **`+`** → 搜 `Sprite Atlas` → 加进来。
2. 选中它 → 细节面板里把 **`Flipbook`（序列帧资产）** 指到你的 PaperFlipbook（DefAtk 用 `Defatk1`）。
3. **完事**。帧表（图集矩形 / 画布矩形 / 画布尺寸 / 贴图尺寸 / 帧率）会自己烘出来，
   在细节面板里往下翻能看到「图集矩形」有 6 个数组元素。

- 什么时候重烘：换 Flipbook、换图集贴图、改手工兜底表、改帧率 —— 这些动作立刻重烘；
  另外**每次加载资产**都会比一次「源签名」（Flipbook 路径 + 帧数 + 帧率 + 每帧 Sprite 的源 UV/尺寸），
  不一致就自动重烘并存盘。⇒ **换图集只需要改这一行 Flipbook**，`.dfs` 里的帧表数字已经不需要了。
- 想手动强制重烘：细节面板上的 **「重烘帧表（Refresh From Source）」** 按钮。
- 手工兜底（不用 Flipbook 时）：清空 `Flipbook`，往下填「图集矩形（兜底）」「画布矩形（兜底）」
  「画布尺寸（兜底）」「帧率（兜底）」，再指一张 `图集贴图（兜底）`。
  ⚠️ 「画布矩形（兜底）」的写法是 `(画布内 x, 画布内 y, 帧宽, 帧高)` —— **不是**画布总尺寸。
- 看不到数据先查：`Flipbook` 是不是空的、纸片（PaperSprite）的 **Source UV / Source Dimension** 有没有设。

---

## 2. 材质 `M_FXAtlasSheet`（10 个节点）

1. 内容浏览器右键 → **材质**，命名 `M_FXAtlasSheet`，双击打开。
2. 点图表空白处 → 右边细节面板改三项：
   - **Shading Model** = 无光照 **Unlit**
   - **Blend Mode** = 加性 **Additive**（照抄你线上那个 `M_FXSheet`）
   - 勾上 **Used with Niagara Sprites**
3. 右键搜节点，按这张表连（A = 上面那个输入口，B = 下面那个）：

| # | 搜这个 | 细节面板设置 | 连法 |
| --- | --- | --- | --- |
| 1 | `TextureCoordinate` | — | → ⑤ 的 A |
| 2 | `DynamicParameter` | **Param Index = 0** | → ③、④ |
| 3 | `ComponentMask` | 勾 **R、G** | → ⑥ 的 B |
| 4 | `ComponentMask` | 勾 **B、A** | → ⑤ 的 B |
| 5 | `Multiply` | — | ① × ④ → ⑥ 的 A |
| 6 | `Add` | — | ⑤ + ③ → ⑨ 的 A |
| 7 | `DynamicParameter` | **Param Index = 2** | → ⑧ |
| 8 | `ComponentMask` | 勾 **R、G** | → ⑨ 的 B |
| 9 | `Divide` | — | ⑥ ÷ ⑧ ＝ **UV** |
| 10 | `TextureSampleParameter2D` | 细节面板里命名 **Sheet**；贴图先选 `Defatk` 那张 | UV ← ⑨ |

4. 收尾：⑩ 的 **RGB → 材质节点的 Emissive Color**，**A → Opacity**。
5. 保存。

> **加性混合为什么也要连 Opacity**：引擎里加性的输出是 `自发光 × Opacity`（`BasePassPixelShader.usf:2325-2327`），
> 不连的话 Opacity 默认 1，整块面片（含透明区）都会被加进去。`DefAtk.png` 实测是**透明底**
> （RGBA，alpha 通道正常），连上 Opacity 就干净，不用另外抠图。

> 这个材质干的事：把「当前帧在图集里的矩形」换算成采样坐标。矩形从哪来？下一步的模块喂给它（Dynamic Material Parameter 1/2/3）。

---

## 3. 播放模块 `Play_SpriteAtlas`

> **这一节不用手点了。** 模块现在由文本源生成：
> `DFX/Modules/M_PlaySpriteAtlas.dfm` → `/AtlasFX/Modules/Play_SpriteAtlas`
> （`pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/Modules/M_PlaySpriteAtlas.dfm -Engine D:\UnrealEngine-5.8.2`）。
> 下面这张表是"它到底干了什么"，方便你在编辑器里对照检查。

模块做的事：读帧数 → 按 `Play Mode` 推帧号 → 回绕到 `[0, 帧数)` → 取当前帧的
`Atlas Rect` / `Canvas Rect` / `Sizes` → 写进三个动态材质参数 → 顺便用 `Atlas Rect` 的宽高 × `Size Scale` 写 `Particles.SpriteSize`。

| 输入 | 默认 | 说明 |
| --- | --- | --- |
| `Atlas` | — | 数据接口（选 `User.Atlas`） |
| `Play Mode` | `0.0` | `0` = 按 Flipbook 帧率；`1` = 按生命长度播完一遍；`2` = `Frame Index` 直通 |
| `Play Rate` | `1.0` | 模式 0 里 1.0 = 正好 Flipbook 帧率（Defatk1 = 15 帧/秒）；负值倒放 |
| `Start Frame` | `0.0` | 起始帧偏移（接 `Random Float in Range` 就是随机起帧） |
| `Frame Index` | `0.0` | 模式 2 用；接引擎自带 **Float from Curve** = 曲线控帧 |
| `Size Scale` | `0.1` | 1 像素 = 多少世界单位（0.1 = 1 毫米） |

> ⚠️ 引擎里那三个属性的名字是 `Particles.DynamicMaterialParameter`（**第一个没有数字**）、
> `...Parameter1`、`...Parameter2`，分别对应材质 `DynamicParameter` 的索引 0 / 1 / 2
> （`NiagaraModule.cpp:448-451` 定义名字，`NiagaraSpriteRendererProperties.cpp:355-358` 做映射）。
> 写成 `1/2/3` 会整体错位一位，材质读到默认 `(1,1,1,1)` ⇒ UV 算飞 ⇒ **全透明，什么都看不见**（踩过）。

---

## 4. 挂到系统里跑

你现在这个 `NewNiagaraSystem` 就能用（Minimal 发射器 + Sprite 渲染器 + CPU ✓）。

1. **把用户参数改名成 `Atlas`**：左下「参数」面板里选中那个 `Sprite Atlas` → 按 **F2** → 改成 `Atlas`。
2. 发射器 → **粒子更新（Particle Update）** → 点 `+` → 搜 `Play_SpriteAtlas` → 加进来。
3. 模块的 `Atlas` 输入是个下拉框 → 选 **`User.Atlas`**；`Size Scale` 先留 `0.1`。
4. 渲染器（Sprite 渲染器）：
   - **Material** = `M_FXAtlasSheet`
   - **Sub Image Size** 保持 `(1, 1)`
   - 材质参数区那三个 Dynamic Material 绑定保持默认（默认就指向 DMP1/2/3）
5. 期望结果：6 张闪电依次播一遍，每张形状/尺寸都不一样；一遍 **0.4 秒**（Flipbook 是 15 fps）。
6. 想只播一次：发射器生成里把 `Spawn Rate` 设 0，加一个 `Spawn Burst Instantaneous` = 1。

---

## 5. 跑通之后再做（现在别做）

- **画布还原模式**：材质再加 9 个节点（画布矩形 + 帧外遮罩 + `Fit Canvas` 开关）
- **`Play_SpriteAtlas_Time` / `Play_SpriteAtlas_Frame`**：外部时间驱动 / 直接给帧号（可接曲线）
- **独立的 `SpriteAtlasSize` 模块**、Mesh 渲染器版本（对接 ZDBridge 那套）
- GPU 支持

---

## 6. 出问题先查

| 现象 | 先查 |
| --- | --- |
| 全黑 | 渲染器的三个 Material Binding 有没有指向 DMP1/2/3；材质有没有勾 Used with Niagara Sprites；Blend Mode 对不对 |
| 显示整张贴图 | 材质里 ⑨ 的除法有没有接对（⑧ 是贴图尺寸）；DynamicParameter 的 Index 是不是 0 / 2 |
| 帧不走 | 模块是不是只勾了 Spawn 没勾 Update；`NormalizedAge` 有没有接上 |
| 面片是正方形 / 被拉伸 | ⑧⑨⑩ 那三个节点有没有连；`Size Scale` 太小就调大 |
| 边缘有邻居帧的杂色 | 数据接口里的「取帧内缩」调到 1~2 |
| 模块加进来报找不到函数 | 第 0 步没编成功，或者编辑器没重启 |

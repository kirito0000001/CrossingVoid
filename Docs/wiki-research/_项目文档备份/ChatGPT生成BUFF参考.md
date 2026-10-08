# ChatGPT生成BUFF参考

## 文档用途

本文记录使用 ChatGPT 生成《零境交错》BUFF 图标素材时验证有效的方法。重点不是保存某一张图的固定画法，而是明确哪些内容应交给模型生成、哪些内容应由项目统一添加，以及怎样拆分复杂图标来提高稳定性。

## 核心结论

不要要求模型一次生成完整的最终图标。更稳定的做法是把图标拆成三个相互独立的视觉层：

1. **统一UI底板**：由项目自行添加，例如深蓝圆角方形、通用边框和统一底色。
2. **机制识别符号**：表达BUFF本身，例如怒筋、剑、盾、恢复、锁定等。优先采用轮廓清楚、细节较少的扁平图形。
3. **角色专属特效**：表达角色或属性特色，例如御坂美琴的蓝白电流。可以比机制符号更生动，但不能破坏小尺寸辨识度。

统一底板负责让不同图标属于同一套UI；机制符号负责让玩家立即认出效果；角色特效负责赋予图标个性。三者不必由同一次生成完成。

## 为什么分层生成更稳定

一次性要求模型同时处理底板、主体符号、环绕特效、透明背景和特定游戏画风时，各项要求会互相争夺视觉权重，常见结果包括：

- 为了表现“精致”，把扁平符号画成立体倒角或金属材质。
- 为了表现“电流”，让复杂电弧遮挡主体符号。
- 把参考图中的通用底板误认为本次素材必须生成的内容。
- 虽然整体华丽，但缩小后无法辨认BUFF的实际含义。
- 透明背景、柔光和阴影互相污染，产生脏边。

分层后，每一次生成只解决一个明确问题。即使某一层失败，也只需重新生成该层，不会破坏已经满意的其他部分。

## 正确使用风格参考图

提供参考图时，不能笼统地要求“生成同样风格”，而应先判断参考图中的元素属于哪一层。

以零境BUFF图标为例，参考图通常包含：

- 深蓝圆角底板。
- 白色或浅色的主要识别符号。
- 单一效果色的箭头、残影、辉光或属性图形。
- 四角透明区域。

如果项目会统一添加底板，就不应让模型再次生成底板。此时参考图只用于判断主体比例、轮廓复杂度、色彩数量和辉光强度。

风格一致不等于所有图层都必须同样扁平。统一底板和机制符号可以保持简洁，角色专属特效则可以保留更强的动态感。例如蓝白电流可以使用带白色核心、青色辉光和尖锐分支的表现，而不必强行简化成普通闪电标志。

## 推荐工作流

### 1. 先确定最终图层顺序

生成之前先写清楚合成关系。例如：

```text
统一蓝色底板
→ 环绕电流
→ 白色怒筋主体
→ 必要的整体辉光调整
```

如果不能明确图层顺序，说明当前需求仍然混在一起，不适合直接生成。

### 2. 固定画布与构图比例

所有分层素材必须使用相同的正方形画布、中心点和朝向。推荐先生成较大的正方形原图，再由项目自行缩放。

提示词中应明确图案占画布的比例：

- 主体识别符号通常占画布约 50% 至 60%。
- 环绕特效通常占画布约 75% 至 85%。
- 必须保留安全边距，避免缩小或裁切时切掉辉光。

### 3. 每次只生成一种内容

主体符号的提示词中明确禁止特效、底板、阴影和其他物体。

特效层的提示词中明确要求中心留空，并禁止出现主体符号、人物、文字和底板。

不要依赖“请忽略其他内容”这类模糊表达，应逐项写出禁止出现的元素。

### 4. 使用真正的透明背景

提示词中应明确：

```text
genuinely transparent background with a clean alpha channel
```

对于带柔光的特效，还要补充要求保留半透明边缘，避免模型生成纯色底后留下色边。

### 5. 合成后再判断风格

单独查看某一层时，它可能不像最终游戏图标。应在统一底板上完成合成，并缩小到游戏中的实际显示尺寸后再判断。

最终检查：

- 第一眼能否认出BUFF含义。
- 第二眼能否认出角色或属性特色。
- 主体与特效是否主次分明。
- 是否存在底色残留、白边、辉光脏边或裁切。
- 与同界面的其他BUFF并排时，尺寸和视觉重量是否一致。

## 本次验证得到的具体经验

御坂美琴标记图标最初尝试把怒筋、环绕电流、锁定感和透明背景一次生成。结果虽然华丽，但主体被画成立体红色物体，整体更像独立技能图标，而不是零境BUFF素材。

随后过度追求参考图的统一风格，又把电流简化成几枚普通闪电，并让模型直接生成蓝色底板。虽然更接近通用UI，却损失了御坂美琴特有的电流表现，而且重复制作了项目本来会统一添加的底板。

最终有效的方法是：

- 单独生成白色扁平怒筋，负责表达“御坂生气并盯上目标”。
- 单独生成第一版风格的蓝白环绕电流，负责表达御坂美琴的角色特色。
- 项目自行添加统一蓝色底板，并控制两层素材的比例与位置。

这说明生成BUFF图标时不能只问“像不像同一套图标”，还要判断一致性究竟应该由哪一层承担。统一UI负责公共风格，生成素材应优先保留机制含义与角色个性。

## 可复用提示词：扁平主体符号

将方括号中的内容替换为本次BUFF需要的图形。

```text
Create only one large [主体符号] for a game BUFF icon.

Use a clean, simple, flat geometric silhouette with solid color, smooth readable edges, no texture, and no unnecessary internal details.

Centered on a square transparent canvas, occupying about 55 percent of the canvas. Keep clear and balanced margins. The symbol must remain recognizable when displayed very small.

Genuinely transparent background with a clean alpha channel.

No glow, elemental effects, backing plate, border, frame, text, character, scenery, shadow, or additional objects.
No bevel, extrusion, metallic material, photorealism, or 3D rendering.
```

如果主体需要使用零境常见的白色识别符号，可追加：

```text
Use a flat solid white fill. The shape itself must carry the meaning of the BUFF.
```

## 可复用提示词：环绕特效

将方括号中的内容替换为角色对应的属性效果。

```text
Create only an isolated ring or surrounding layer of [角色专属特效] for compositing into a game BUFF icon.

Arrange the effect around a large completely empty center. The effect should be energetic and expressive, but must not cross or obscure the center area reserved for the main BUFF symbol.

Centered on a square transparent canvas. The outer effect should occupy about 82 percent of the canvas while keeping enough margin for glow and later resizing.

Genuinely transparent background with a clean alpha channel. Preserve soft semi-transparent glow and clean outer edges.

No main symbol, backing plate, border, frame, text, character, scenery, or additional objects.
```

## 御坂美琴环绕电流提示词

```text
Create only a broken circular ring of intense blue-white electricity for compositing into a game BUFF icon.

Use several thick, irregular, sharply curved electrical arcs swirling around a large empty center. The electricity has bright white cores, vivid cyan inner glow, deep electric-blue outer edges, pointed branching tips, and restrained soft blue bloom.

The electrical ring should feel forceful, unstable, and naturally discharged rather than geometric. Keep the center completely empty and transparent for another symbol.

Centered on a square transparent canvas, with the outer electricity occupying about 82 percent of the canvas and maintaining safe margins.

Genuinely transparent background with a clean alpha channel. Preserve the semi-transparent glow around every electrical arc.

No anger symbol, lightning-bolt logo, blue backing plate, circular border, text, character, coin, scenery, or additional objects.
```

## 御坂美琴怒筋提示词

```text
Create only one large white manga-style anger-vein symbol for a game BUFF icon.

Use three separate thick curved segments arranged into a compact triangular anger symbol. Use a flat solid white fill, clean smooth edges, a simple geometric silhouette, and no internal detail.

Centered on a square transparent canvas, occupying about 55 percent of the canvas. Keep balanced empty space around the symbol.

Genuinely transparent background with a clean alpha channel.

No glow, lightning, shadow, blue backing plate, border, frame, text, character, scenery, or additional objects.
No bevel, extrusion, texture, photorealism, or 3D shading.
```

## 常见失败与修正方向

### 图标过于华丽

删除“精致材质”“高级游戏图标”“立体渲染”等容易引发材质化的描述，改为强调扁平轮廓、低细节和小尺寸可读性。

### 主体与特效混在一起

停止继续修改同一张图，拆成两个透明图层重新生成。

### 特效变成普通属性标志

不要只写“闪电”“火焰”或“冰”。应描述特效的运动方式、粗细、核心颜色、边缘颜色、分支形态和整体轮廓。

### 模型重复生成通用底板

在提示词中明确写出：

```text
No backing plate, tile, frame, border, or colored background. The project will add its own UI backing plate later.
```

### 透明背景出现脏边

要求真正的Alpha透明，并让模型保留半透明辉光。如果仍然存在底色，优先重新生成该特效层，不要让模型在包含全部图层的成品上反复抠除背景。

## 最终原则

生成BUFF素材时，先判断哪些内容属于公共UI，哪些内容属于机制识别，哪些内容属于角色个性。公共UI由项目统一控制；机制识别符号保持简单；角色特效允许表达力，但应独立成层。

对复杂图标而言，稳定性来自正确拆分，而不是不断加长一次性提示词。

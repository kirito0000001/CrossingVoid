// AtlasFX x DreamFX: 非均匀矩形图集的播放模块（三种帧来源），以文本为源。
//
// 生成 /AtlasFX/Modules/Play_SpriteAtlas。数据层是 C++ 的 UNiagaraDataInterfaceSpriteAtlas（CPU sim），
// 本模块只负责：
//   1. 按 PlayMode 推出当前帧号（三种模式见下）；
//   2. 取该帧的三包数据（图集矩形 / 画布矩形 / 尺寸包）；
//   3. 写进 Particles.DynamicMaterialParameter / ...Parameter1 / ...Parameter2（材质索引 0/1/2）给材质采样；
//   4. 写 SpriteSize（面片尺寸：FitFrame=0 取整块画布 = 画布还原，=1 取当前帧矩形 = 铺满）。
//
// ⚠️ 本模块**不写** Particles.SubImageIndex（2026-10-04 定位到的真凶，见文件末尾）。
//
// FitFrame（2026-10-04 补，对齐 DESIGN.md §4.2 的「画布还原 + 铺满」）：
//   0 = 画布还原（默认）：面片 = 画布尺寸，帧按它在画布里的位置摆放 ⇒ 与原始精灵完全一致。
//   1 = 铺满：面片 = 当前帧矩形。DefAtk 的帧从 60×93 到 301×361 不等，铺满会让闪电每帧忽大忽小。
//   材质不需要开关：模块在铺满模式下把「帧的画布矩形」伪装成整块画布，材质的画布还原数学自动退化成铺满。
//
// PlayMode（2026-10-04 按用户要求加，对应 DESIGN.md §4.2 的三种帧来源）：
//   0 = 按 Flipbook 帧率（默认）：帧号 = StartFrame + Age × Fps × PlayRate
//       Fps 取 DI 的 GetFps —— 就是 Flipbook 的 FramesPerSecond（Defatk1 = 15）⇒ PlayRate=1.0 即原速。
//   1 = 按粒子生命长度：帧号 = StartFrame + NormalizedAge × 帧数 × PlayRate
//       PlayRate=1.0 = 一个生命周期正好播完一遍（这是 2026-10-04 之前唯一的行为）。
//   2 = 直接给帧号：帧号 = StartFrame + FrameIndex
//       FrameIndex 可接 Float from Curve，用曲线自己控帧（停帧/加速/回放这类花活）。
//
// DI 输入的函数调用沿用 Niagara 自定义 HLSL 的写法：Name.Function(入参..., 出参...)。
// 出参必须是**未初始化的普通局部变量**，否则 CPU VM 编译会报 DFX6006
// （hlslcc 会把已初始化的局部量常量折叠进实参表，而常量不是左值）。
Module(Name="Modules/Play_SpriteAtlas", Root="Plugin.AtlasFX")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "图集|播放";
        Description = "按 Flipbook 帧率 / 粒子生命 / 指定帧号推进精灵图集帧号，并把当前帧矩形与尺寸写入 DynamicMaterialParameter（材质索引 0/1/2）。";
    }

    Inputs = {
        // 注意：这里**不能**给 DI 输入写 JSON 默认配置 —— 模块 Inputs 声明里的字符串默认值
        // 会被当成「资产路径」，构建直接报 DFX4040: no asset at '{...}'（实测 2026-10-04）。
        // JSON 配置只支持写在系统的输入值上（.dfs 的 Properties / 模块调用实参）。
        DI<SpriteAtlas> Atlas;
        float PlayMode   = 0.0 [ Description="播放模式：0 = 按 Flipbook 帧率（默认）；1 = 按粒子生命长度播完一遍；2 = 用 FrameIndex 直接指定帧号。" ];
        float Loop       = 0.0 [ Description="0 = 只播一遍，播完停在最后一帧（默认）；1 = 循环播放（按帧数回绕）。" ];
        float PlayRate   = 1.0 [ Description="播放速率倍率；负值倒放。模式0：1.0 = 正好等于 Flipbook 帧率；模式1：1.0 = 一个生命周期播完一遍。" ];
        float StartFrame = 0.0 [ Description="起始帧偏移；接 Random Float in Range 即可随机起帧。" ];
        float FrameIndex = 0.0 [ Description="模式2专用：直接指定当前帧号（可接 Float from Curve 做曲线控制）。" ];
        float SizeScale  = 0.1 [ Description="1 像素对应的世界单位（0.1 = 1 毫米），用于换算面片尺寸。" ];
        float FitFrame   = 0.0 [ Description="面片取哪个尺寸：0 = 画布还原（面片 = 整块画布，帧放回它在画布里的位置，默认）；1 = 铺满（面片 = 当前帧自己的矩形）。" ];
    }

    Body = {
        // 帧数：DI 的输出是 int32，出参必须自己声明。
        int FrameCount;
        Atlas.GetFrameCount(FrameCount);

        // 源帧率（Flipbook 的 FramesPerSecond，DI 烘表时存下来的）。
        float Fps;
        Atlas.GetFps(Fps);

        float FrameCountF = max(FrameCount, 1);

        // 三种模式推出原始帧号（可能越界/为负）。
        // 模式0 用的是「已存活秒数」= NormalizedAge × Lifetime —— 不直接读 Particles.Age：
        // 它不是 DreamFX 已知属性表（FNiagaraConstants::GetCommonParticleAttributes）的成员，
        // 直接写会报 DFX3046（DreamFXModuleGenerator.cpp:286/588），而按文档「声明类型」的写法
        // 会连初值一起写下去，等于每帧把引擎的 Age 清零。乘 Lifetime 等价且无副作用。
        //
        // ⚠️ 这里**刻意不用 if/else**（2026-10-04 踩坑）：VectorVM 后端会把 if 展平成 select
        // （ir_vm_flatten_branches_to_selects_visitor.cpp:110-185），展平器在「A 分支有赋值、
        // B 分支没有对应赋值」时会拿**变量原值**当另一路（:151-166）；只要那一路的值来自外部
        // 函数出参，就会报
        //   error: Component selction_result of variable selction_result has no valid offset.
        //   Possibly uninitialized data being used.   （ir_vm_gen_bytecode_visitor.cpp:629-638）
        // 三种模式改成 step 掩码 + 线性组合，全程无分支。
        float AgeSec = Particles.NormalizedAge * Particles.Lifetime; // 已存活秒数
        float Mode0  = step(PlayMode, 0.5);                          // PlayMode <= 0.5
        float Mode1  = step(PlayMode, 1.5) - Mode0;                  // 0.5 < PlayMode <= 1.5
        float Mode2  = 1.0 - Mode0 - Mode1;                          // 其余
        float RawFrame = Mode0 * (StartFrame + AgeSec * Fps * PlayRate)
                       + Mode1 * (StartFrame + Particles.NormalizedAge * FrameCountF * PlayRate)
                       + Mode2 * (StartFrame + FrameIndex);

        // 播完怎么办（2026-10-05 按用户要求改）：
        //   默认 Loop = 0 —— **只播一遍**，播完停在最后一帧。时长交给配套的
        //   /AtlasFX/Modules/Sprite_Atlas_Duration 模块（寿命 = 帧数 ÷ 帧率），
        //   所以正常情况下帧号刚好走到最后一帧、粒子就寿终了；clamp 是保险丝：
        //   浮点误差、StartFrame 偏移、PlayRate 偏小都不会让画面突然跳回第 0 帧。
        //   Loop = 1 时保留原来的回绕（正模，负值倒放也天然正确）。
        // 回绕直接用 HLSL 正模算，不调 DI 的 WrapFrame：省一次 VM 调用。
        float Wrapped = RawFrame - floor(RawFrame / FrameCountF) * FrameCountF;
        float Clamped = clamp(RawFrame, 0.0, FrameCountF - 1.0);
        float Frame   = lerp(Clamped, Wrapped, saturate(Loop));

        // 三包数据：图集矩形(x,y,w,h 像素) / 画布矩形 / 尺寸包(贴图W, 贴图H, 画布W, 画布H)。
        // 出参顺序必须与 DI 里 AddOutput 的顺序一致；出参必须是未初始化的普通局部变量。
        float4 AtlasRect;
        float4 CanvasRect;
        float4 Sizes;
        Atlas.GetFrameParams(Frame, AtlasRect, CanvasRect, Sizes);

        // 兜底（诊断用，2026-10-04）：DI 是空表时 GetFrameCount 回 0、GetFrameParams 回 (0,0,1,1)，
        // 面片会缩成 1×SizeScale 的小方块；而材质拿 Sizes=(1,1) 去归一化会把 UV 放大几百倍 ⇒ 采样到
        // 贴图外（wrap 采样器下就是一片噪点）。这里改成「整张贴图当一帧」：AtlasRect 设成整张 597×487，
        // 同时把 Sizes 设成真实尺寸，UV 正好落在 [0,1] ⇒ 画面是一张干净完整的图集。
        // 于是三种情况一眼可分：干净的整张图集 = 运行时 DI 是空表；随帧变形的单帧闪电 = 全通；
        // 噪点 = 拿到的是未初始化的垃圾值（说明 DI 调用根本没执行）。
        // 同样用掩码而不是 if（原因同上：DI 出参 + 分支 = VectorVM 展平报错）。
        float EmptyAtlas = step(FrameCountF, 0.5);   // FrameCount <= 0
        AtlasRect  = AtlasRect  + (float4(0.0, 0.0, 597.0, 487.0)   - AtlasRect)  * EmptyAtlas;
        CanvasRect = CanvasRect + (float4(0.0, 0.0, 928.0, 640.0)   - CanvasRect) * EmptyAtlas;
        Sizes      = Sizes      + (float4(597.0, 487.0, 928.0, 640.0) - Sizes)     * EmptyAtlas;

        // 面片尺寸按 FitFrame 取（用 lerp，不用 if —— 见上面的 VectorVM 展平限制）：
        //   0 = 画布还原：面片 = 整块画布（928×640 像素 × SizeScale），帧只占其中一小块，
        //       位置与大小都跟原始精灵一致 ⇒ 播放时闪电不会忽大忽小/乱跳（这是设计稿 §4.2 的默认）。
        //   1 = 铺满：面片 = 当前帧自己的矩形（60×93 … 301×361），每帧尺寸都变，缩放更省但会「抖」。
        float2 CanvasSize = float2(Sizes.z, Sizes.w);
        float2 TargetSize = lerp(CanvasSize, float2(AtlasRect.z, AtlasRect.w), FitFrame);
        Particles.SpriteSize = TargetSize * SizeScale;

        // 渲染器绑定 → 材质 DynamicParameter 索引（引擎实证 2026-10-04）：
        //   NiagaraModule.cpp:448-451 定义属性名，注意第一个**没有数字**：
        //     Particles.DynamicMaterialParameter   → MaterialParam0 → 材质索引 0
        //     Particles.DynamicMaterialParameter1  → MaterialParam1 → 材质索引 1
        //     Particles.DynamicMaterialParameter2  → MaterialParam2 → 材质索引 2
        //   （映射见 NiagaraSpriteRendererProperties.cpp:355-358 / NiagaraMeshRendererProperties.cpp:725-727）
        // 材质 M_FXAtlasSheet 读索引 0（图集矩形）、1（画布矩形）、2（尺寸包）。
        Particles.DynamicMaterialParameter  = AtlasRect;   // 材质索引 0
        Particles.DynamicMaterialParameter2 = Sizes;       // 材质索引 2

        // 材质**永远**按「画布还原」采样（Local = 画布内坐标 → 帧内 0..1 → 图集 UV）。
        // 铺满模式要的是「整帧铺满面片」，等价于把「帧在画布里的矩形」换成整块画布：
        // 于是 Local 正好等于 TexCoord，UV 退化成 (AtlasRect.xy + TexCoord × AtlasRect.zw) / 贴图尺寸。
        // ⇒ 一个材质同时支持两种模式，不需要材质开关。
        Particles.DynamicMaterialParameter1 = lerp(CanvasRect, float4(0.0, 0.0, CanvasSize.x, CanvasSize.y), FitFrame);

        // ⚠️ 绝对不要写 Particles.SubImageIndex（2026-10-04 定位到的真凶）。
        //
        // 精灵渲染器会把它当作「子图网格坐标」掺进材质读到的 TexCoord：
        //   NiagaraSpriteVertexFactory.ush:1066  SubImageAV = floor(SubImageA * SubImageSize.z)
        //   NiagaraSpriteVertexFactory.ush:1068  TexCoord.y = (SubImageAV + UV.y) * SubImageSize.w
        // SubImageSize=(1,1)（非均匀矩形图集必须这么设）时 SubImageSize.z = 1/1 = 1，
        // 于是 SubImageA = floor(SubImageIndex) + 0.5 → SubImageAV = 帧号，
        // 最终 TexCoord.y = 帧号 + UV.y：
        //   第 0 帧偏移为 0 ⇒ 正常；第 1 帧起整块面片采到贴图外面 ⇒ 画面全空。
        // 这就是「只有第 0 帧能显示」的机制（材质里的矩形数学一直是好的）。
        // 本模块的采样全在材质 M_FXAtlasSheet 里自己算，SubImageIndex 必须保持默认 0
        // （NiagaraConstants.cpp:409 默认值 0.0；渲染器兜底 DefaultSubImage = 0.0，
        //   NiagaraRendererSprites.cpp:578）。
    }
}

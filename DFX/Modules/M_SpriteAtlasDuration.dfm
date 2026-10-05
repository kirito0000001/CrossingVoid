// AtlasFX x DreamFX: 图集播放的「时长」模块，以文本为源。
//
// 生成 /AtlasFX/Modules/Sprite_Atlas_Duration。放在**粒子生成**阶段（ParticleSpawn），
// 紧跟在 InitializeParticle 后面，把粒子寿命改成图集动画自己的总时长：
//
//     寿命 = 帧数 ÷ 帧率
//
// Defatk：6 帧 ÷ 15 帧/秒 = 0.4 秒 —— 与原始 Flipbook 动画的时长一模一样。
//
// 为什么要单独一个模块（2026-10-05 按用户要求做）：
//   用户的原话是「帧速度都和 flip 一样，时长也一样，默认都只播放一次」。
//   帧速度那半在 Play_SpriteAtlas 的模式 0 里已经成立（帧号 = 存活秒数 × 帧率），
//   缺的是时长那半 —— 之前寿命由发射器的 InitializeParticle 写死 1.0 秒，
//   于是 0.4 秒的动画在一颗粒子里重复播 2.5 遍（模块那边帧号是无限回绕的）。
//   把寿命对齐动画时长之后，「播一遍」就自然等于「粒子的一生」，
//   再配 Play_SpriteAtlas 的 Loop=0（默认只播一遍、播完停最后一帧），
//   一颗粒子 = 一次干净的播放。
//
// 它必须是 ParticleSpawn：寿命要在粒子出生的那一帧定下来。
// Play_SpriteAtlas 是 ParticleUpdate（每帧推帧号），它**不能**写 Lifetime ——
// 那时候粒子已经带着寿命出生了（改它只会让 NormalizedAge 突然跳变）。
//
// DI 输入的函数调用沿用 Niagara 自定义 HLSL 的写法：Name.Function(入参..., 出参...)。
// 出参必须是**未初始化的普通局部变量**，否则 CPU VM 编译会报 DFX6006。
Module(Name="Modules/Sprite_Atlas_Duration", Root="Plugin.AtlasFX")
{
    Settings = {
        Usage       = ParticleSpawn;
        Category    = "图集|播放";
        Description = "把粒子寿命设成图集动画的总时长（帧数 ÷ 帧率），让一次播放正好播完一遍，与 Flipbook 的时长一致。";
    }

    Inputs = {
        // 注意：这里**不能**给 DI 输入写 JSON 默认配置 —— 模块 Inputs 声明里的字符串默认值
        // 会被当成「资产路径」，构建直接报 DFX4040: no asset at '{...}'（实测 2026-10-04）。
        // JSON 配置只支持写在系统的输入值上（.dfs 的 Properties / 模块调用实参）。
        DI<SpriteAtlas> Atlas;
        float DurationScale = 1.0 [ Description="寿命倍率：1.0 = 正好一遍（默认，与 Flipbook 时长一致）；2.0 = 两遍的时长（配 PlayMode 0 + Loop 1 看两次）。" ];
    }

    Body = {
        // 帧数：DI 的输出是 int32，出参必须自己声明。
        int FrameCount;
        Atlas.GetFrameCount(FrameCount);

        // 源帧率（Flipbook 的 FramesPerSecond，DI 烘表时存下来的）。
        float Fps;
        Atlas.GetFps(Fps);

        // 两个 max 都是防呆：空表（帧数 0）时给一帧的时长而不是 0 秒（0 秒寿命的粒子
        // 会立刻死掉，画面上什么都看不到）；帧率 0 时用 1/10000 兜底避免除零。
        Particles.Lifetime = max(FrameCount, 1) / max(Fps, 0.0001) * max(DurationScale, 0.0001);
    }
}

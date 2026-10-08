// AtlasFX x DreamFX: 把网格粒子按**世界单位**平移一段，用于整体对齐微调。
//
// 生成 /AtlasFX/Modules/Atlas_Mesh_Offset。
//
// 为什么不去挪面片原点
// ─────────────────
// `Particles.Scale` 只缩放**渲染出来的网格几何**，不动粒子位置。所以：
//   * 挪面片原点 / `PivotOffsetSpace = Mesh` —— **局部单位，会被 Scale 放大**
//     （网格模板默认配置下 Scale = 5：局部 40 cm ⇒ 世界 200 cm），而且每个系统的 SizeScale 不同、
//     放大倍数也不同，等于把补偿量变成一个随系统漂移的数；
//   * 加在 `Particles.Position` 上 —— **世界单位，永远不被放大**，一个数就是一个数。
// 结论：对齐量走这里，面片原点只承载"锚点在哪"这一个语义（2026-10-06 已改成几何中心）。
//
// 默认 (0, 0, 40)
// ──────────────
// 角色精灵组件在 Z = +40（Misaka 蓝图的 Sprite 相对位置 (0,0,40)、缩放 0.5），
// 网格特效跟着抬 40 才和精灵那套对齐（2026-10-06 用户定）。
//
// ⚠️ 只应放在 ParticleSpawn
// ──────────────────────
// 位置是逐帧累积的量：放进 ParticleUpdate 会每帧再加一次，粒子越跑越远。
//
// ⚠️ 存量系统注意叠加
// ──────────────────
// 如果某个系统的通知 `Offset` 里**已经**算过这 40，把这里的 Z 改回 0，否则会变成 80。
Module(Name="Modules/Atlas_Mesh_Offset", Root="Plugin.AtlasFX")
{
    Settings = {
        Usage       = ParticleSpawn;
        Category    = "图集|对齐";
        Description = "把网格粒子按**世界单位**平移一段（默认向上 40，对齐角色精灵组件的 +40）。加在粒子位置上，不受 Particles.Scale 放大。只应放在 ParticleSpawn。";
    }

    Inputs = {
        Vector Offset = (0.0, 0.0, 40.0) [ Description="世界单位的平移量（厘米）。默认 (0,0,40)：角色精灵组件在 Z = +40，网格特效跟着抬 40 与精灵对齐。⚠️ 若该系统的通知 Offset 里已含这 40，改回 (0,0,0) 避免叠加成 80。" ];
    }

    Body = {
        // 网格模板的发射器是 LocalSpace = true ⇒ 粒子位置就是世界单位，不经过 Particles.Scale。
        Particles.Position = Particles.Position + Offset;
    }
}

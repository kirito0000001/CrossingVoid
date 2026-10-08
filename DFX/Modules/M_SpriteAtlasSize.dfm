// AtlasFX x DreamFX: 把图集播放算出的面片尺寸换算成 Mesh 渲染器要的 Particles.Scale，
// 并把面片转到「正对相机」的朝向，以文本为源。
//
// 生成 /AtlasFX/Modules/Sprite_Atlas_Size。
//
// 为什么要有这个模块：精灵渲染器读 Particles.SpriteSize，Mesh 渲染器读 Particles.Scale，
// 两者不是同一个属性。播放模块 Play_SpriteAtlas 已经把「当前帧该占多大（世界单位）」算好写进
// SpriteSize 了，这个模块只做一次换算，不重复算帧号 ⇒ 播放逻辑仍然只有一份。
//
// ⚠️ 必须排在 Play_SpriteAtlas **之后**（Niagara 模块按栈序执行）—— 它读的就是播放模块写的结果。
//
// ── 轴映射（2026-10-04 照引擎源码 + FBX 实测定死，不是拍的）────────────────────
// 朝向矩阵 NiagaraMeshParticleUtils.ush:319-321：
//     XAxis = FacingDir（指向相机）、YAxis = cross(RefVector, FacingDir)、ZAxis = cross(XAxis, YAxis)
// 用法 :368-369 `SRT.Rotation = mul(SRT.Rotation, FacingMat)`，是**行向量右乘**（:387 的
// `mul(MeshOffset, SRT.Rotation)` 同一约定）。NiagaraCommon.ush:5-19 的 NiagaraQuatTo3x3 配
// NiagaraTransformUtils.ush:57 的 `float3x3(Rotation[0]*Scale.x, ...)` ⇒ **矩阵的行 = 基向量的像**，
// 也就是「v * M」等价于 UE 的 FRotator 语义。
// ⇒ 局部 X → 朝相机（法线方向）、局部 Y → **屏幕左**、局部 Z → 屏幕上。
//   注意 Y 是「屏幕左」而不是「屏幕右」：NiagaraMeshVertexFactory.ush:587 的 CameraUpDir =
//   ResolvedView.ViewUp 没取负，算出来 YAxis = -ViewRight。Sprite 那条链路
//   （NiagaraSpriteVertexFactory.ush:557）是 CameraUp = **-**ResolvedViewUp，所以两者差一个水平镜像。
//
// 本工程的面片 Plugins/ZDBridge/Content/FX/FXDefault.fbx（= /ZDBridge/FX/FXDefault）在 UE 里是：
//     宽 92.8 沿 **X**、高 64.0 沿 **Z**、法线 **Y**、轴心在**几何中心**
//   （FBX 顶点是 Blender 本地坐标，Model 节点 Lcl Rotation=(-90,0,0) + Lcl Scaling=100 之后
//     再经 FBX(Y-up) → UE(Z-up) 转换得到；UnitScaleFactor=1。）
//   ⚠️ 2026-10-06 之前轴心在**底边**（顶点 Z 0…0.64）：网格渲染器把粒子位置放在轴心上 ⇒ 面片
//     从粒子原点往**上**长；而精灵渲染器的四边形是以粒子位置为**中心**的 ⇒ 同一份数据网格版
//     整体高出「画出来高度的一半」（Misaka 那边实测 120 cm）。2026-10-06 起 FBX 顶点整体下移
//     0.32（轴心移到几何中心），两版天然对齐 —— **不要再加任何 Z 补偿**，加了反而会偏低。
//     存量网格特效的画面会因此下移「各自画出来高度的一半」：那正是原来偏高的量，所以现在才对上。
// 顺带解释了播放模块 SizeScale 的默认 0.1：928 px × 0.1 = 92.8，正好是面片宽度。
//
// ── 朝向：渲染器**不写** FacingMode（= 默认的 Default），由本模块摆正 ──────────
// 引擎机制（NiagaraMeshParticleUtils.ush）：
//   `:365 if (FacingMode != MESH_FACING_DEFAULT)` 才计算朝向矩阵；
//   `:369 SRT.Rotation = mul(SRT.Rotation, FacingMat)` —— 朝向矩阵在粒子自身旋转**之后**
//   叠加，等于**覆盖**掉粒子写的旋转；`:262 FacingDir = -CameraForwardDir`、
//   `:305 RefVector = Params.CameraUpDir`（只有 CameraPlane 用）。
// ⇒ CameraPlane / CameraPosition / Velocity 都是「每帧拿相机重建朝向」：面片始终跟随相机，
//   **特效永远转不动它**（CameraPlane 下 Particles.MeshOrientation 写了也白写）。
//   实测现象：拖动视口时面片跟着相机转、色彩线抖动。
// ⇒ Default = 完全不套朝向矩阵：网格保持自己的局部朝向，可以被特效旋转，配合发射器
//   LocalSpace = true，面片跟着发射器/挂点一起转 —— 2D 特效要的就是这个。
//
// 本工程面片 /ZDBridge/FX/FXDefault 法线在局部 Y 上，而本工程的 2D 相机**沿 Y 轴看**
// （Paper2D 侧视约定：X = 左右、Z = 上下、Y = 景深；PaperZD 序列编辑器的预览相机在
//   (0,-100,0) 朝 +Y，与游戏相机同轴 —— 2026-10-05 由用户在预览里实测确认：
//   预览里把补偿设成 0 时，看到的画面与游戏里**完全一致**）。
// 所以面片**保持自然朝向**即可：默认 **MeshYaw = 0.0**。
// 工程内可对照的活例子：/Game/GameActor2D/SAO_Kirito/ExAsset/Leng刀光 也是网格渲染器，
// 它**不写** Particles.MeshOrientation（= 自然朝向），在游戏里正对镜头。
//
// ⚠️ 两个开关必须配对，否则面片会侧对镜头而**完全看不见**（引擎不报任何错，日志一个字都没有）：
//       FacingMode = Default（不写）+ MeshYaw = 0     ← 本工程用这套
//       FacingMode = CameraPlane      + MeshYaw = 0
//    历史上「什么都看不见」有两个成因，别再踩：
//      ① 错配成 CameraPlane + 非 0：朝向矩阵已经让面片正对相机，多写的旋转又把它转成一条线；
//      ② 2026-10-05 实测：把 MeshYaw 写成 -90 本身就是错的 —— 它把法线从 Y 转到 X，
//         与相机轴垂直，于是**游戏里**和 PaperZD 预览里都只剩一条缝（不是只有预览坏）。
//
// MeshYaw 留作旋钮：画面左右镜像时改 180；换成作者化好的、法线在 X 上的工程面片时改 -90。
Module(Name="Modules/Sprite_Atlas_Size", Root="Plugin.AtlasFX")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "图集|尺寸";
        Description = "读取 Play_SpriteAtlas 写好的 Particles.SpriteSize（世界单位），换算成 Mesh 渲染器使用的 Particles.Scale（可选带一个绕 Z 的朝向修正）。必须排在播放模块之后。";
    }

    Inputs = {
        Vector2 MeshSize = (92.8, 64.0) [ Description="面片在网格局部空间的尺寸：X = 宽、Y = 高（单位同世界单位）。工程面片 FXDefault = (92.8, 64.0)。宽高分别除以它的 X 和 Y。" ];
        Vector2 MinSize  = (92.8, 64.0) [ Description="尺寸下限（世界单位）。⚠️ Particles.SpriteSize 一旦是 0，算出来的宽度就是 0 —— 面片会静默消失且不报任何错。这个下限保证面片永远可见。排查用的判据：面片以这个尺寸出现 = 上游 SpriteSize 是 0。" ];
        float   DepthScale   = 1.0  [ Description="厚度方向的缩放（网格局部 Y）。真正的平面网格保持 1.0；拿 Cube 当薄片用时调小，例如 0.05 = 5 厘米厚。" ];
        float   UniformScale = 1.0  [ Description="宽高再整体乘一个倍率，不影响厚度。" ];
        float   MeshYaw      = 0.0 [ Description="绕 Z 轴的朝向修正（度）。工程面片 /ZDBridge/FX/FXDefault 的面法线在局部 Y 上，本工程 2D 相机也沿 Y 轴看，所以**保持 0.0**（自然朝向）即可。画面左右镜像时改成 180；换成法线在 X 上的工程面片时改成 -90。⚠️ 必须和渲染器的 FacingMode 配对：Default（不写）配 0，CameraPlane 也配 0；配错会让面片侧对镜头而完全看不见。" ];
    }

    Body = {
        // SpriteSize 是 float2（世界单位，播放模块已经乘过 SizeScale）。
        // ⚠️ max() 是必须的，不是保险：SpriteSize 为 0 时宽度会算成 0，面片静默消失，
        //    引擎不报任何错 —— 这就是 Mesh 路线排查时踩的坑。NaN 也能被 max 兜住
        //    （HLSL 的 max 展开成 a > b ? a : b，NaN 比较恒 false ⇒ 取 MinSize）。
        float2 Target = max(Particles.SpriteSize, MinSize) * UniformScale;

        // max(..., 0.001) 兜住 MeshSize 里填 0 的情况，否则除零会让 Scale 变 inf，
        // 整个网格直接消失（而且不会有任何报错）。
        // 局部 X 是宽、局部 Y 是厚度、局部 Z 是高 —— 见文件头。
        Particles.Scale = float3(
            Target.x / max(MeshSize.x, 0.001),
            DepthScale,
            Target.y / max(MeshSize.y, 0.001)
        );

        // 0.00872664626 = 0.5 * PI / 180，把「度」折半换成弧度。
        // MeshOrientation 是引擎标准属性（NiagaraConstants.cpp:301/444，四元数 xyzw，默认 0,0,0,1）。
        float HalfYaw = MeshYaw * 0.00872664626;
        Particles.MeshOrientation = float4(0.0, 0.0, sin(HalfYaw), cos(HalfYaw));
    }
}

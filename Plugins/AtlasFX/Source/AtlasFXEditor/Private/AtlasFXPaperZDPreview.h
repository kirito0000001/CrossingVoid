// AtlasFXPaperZDPreview —— 让 PaperZD 序列编辑器预览里也能看到特效通知的 Niagara 特效。
//
// 【为什么要做这个】
//   PaperZD 的预览世界（EWorldType::EditorPreview）里，通知**是会**被触发的：
//     PaperZDAnimPlayer.cpp:174   if (OwningInstance != nullptr || Notify->bShouldFireInEditor)
//   而 bShouldFireInEditor 默认就是 true（PaperZDAnimNotify_Base.cpp:19）。
//   但预览播放器调 TickPlayback 时**永远不传 OwningInstance**：
//     PaperZDAnimationSourceViewportClient.cpp:107
//       Player->TickPlayback(CurrentAnimSequencePtr.Get(), PlaybackTime, DeltaSeconds, bLooping);
//   于是通知的 OnReceiveNotify(OwningInstance) 拿到的是 nullptr。
//
//   工程里在用的 TxSpawn 蓝图（Content/BaseC/ExCordLibrary/Notify/TxSpawn.uasset）
//   第一步就是 Cast<BI_2DCharAnimBP>(OwningInstance)，拿到 nullptr 直接跳过 —— 所以
//   序列编辑器里播放时什么特效都不生成。
//
//   PaperZD 自带的 Niagara 通知之所以能预览，是因为它压根不看 OwningInstance，
//   而是挂在 SequenceRenderComponent 上生成（PaperZDAnimNotify_NiagaraEffect.cpp:53-79）。
//
// 【本扩展的做法】不改任何工程资产、不改 PaperZD（第三方插件，改了会被上游更新覆盖）：
//   用编辑器 ticker 盯着预览播放器的 (当前序列, 播放时间)，按 PaperZD 自己的跨帧规则
//   （PaperZDAnimNotify.cpp:27-50）算出这一帧跨过了哪些通知，再用反射从通知对象上读
//   它的 Niagara 系统 / Offset / Rotation / Scale / NotAttach，挂到预览渲染组件上生成。
//
//   反射是必须的：通知是蓝图，属性名由用户定（工程里叫 Niagara/Offset/Rotation/Scale/NotAttach），
//   而播放器的 RegisteredRenderComponent 是 private + UPROPERTY（PaperZDAnimPlayer.h:52），
//   没有 getter，只能反射读。
//
// 【开关】控制台变量 AtlasFX.PaperZD.PreviewFX（1 = 开，0 = 关）。

#pragma once

#include "CoreMinimal.h"

class UPaperZDAnimNotify_Base;

class FAtlasFXPaperZDPreview
{
public:
	/** 模块启动时挂上 ticker。 */
	static void Startup();

	/** 模块卸载时摘 ticker 并收掉已生成的特效。 */
	static void Shutdown();

	/**
	 * 诊断：把一条通知上反射到的 FX 数据打进日志。
	 * 命令行 AtlasFXSetup 用它自查（反射是这套扩展最脆的一环，属性名对不上就没特效）。
	 */
	static void DumpNotifyDiagnostics(const UPaperZDAnimNotify_Base* Notify);

private:
	static bool Tick(float DeltaSeconds);
};

// AtlasFXPaperZDPreview 实现。设计说明见同名 .h。

#include "AtlasFXPaperZDPreview.h"

#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/Players/PaperZDAnimPlayer.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Notifies/PaperZDAnimNotify.h"
#include "Notifies/PaperZDAnimNotify_Base.h"
#include "Templates/TypeHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogAtlasFXPreview, Log, All);

static TAutoConsoleVariable<int32> CVarAtlasFXPreviewFX(
	TEXT("AtlasFX.PaperZD.PreviewFX"),
	1,
	TEXT("1 = 在 PaperZD 序列编辑器预览里自动生成通知上的 Niagara 特效（默认）；0 = 关闭。"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAtlasFXPreviewMimicGame(
	TEXT("AtlasFX.PaperZD.MimicGame"),
	1,
	TEXT("1 = 预览按游戏里的组件变换摆角色（精灵缩放 0.5、相对 Z +40），特效按游戏里「挂在角色根组件上」的位置与缩放折算（默认）；0 = 原样显示。"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAtlasFXPreviewSpriteScale(
	TEXT("AtlasFX.PaperZD.SpriteScale"),
	0.5f,
	TEXT("角色精灵组件在游戏里的缩放（用户确认全角色统一，默认 0.5）。"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAtlasFXPreviewSpriteZ(
	TEXT("AtlasFX.PaperZD.SpriteZ"),
	40.0f,
	TEXT("角色精灵组件在游戏里的相对 Z（用户确认全角色统一，默认 40 cm）。"),
	ECVF_Default);

// 2026-10-05 定案（用户选择「让预览模仿游戏」）：预览与游戏对不上的根因是**两套坐标** ——
//   游戏：角色根组件（缩放 1，原点在脚底）→ 精灵组件（相对 (0,0,+40)，缩放 0.5）；
//         特效挂在**根组件**上，不继承那 0.5 ⇒ 角色 2.57 m、特效按 1:1 的尺寸。
//   预览：PaperZD 自己建的翻转书是 **1:1 + 原点** ⇒ 角色 5.14 m ⇒ 在预览里看着正好的尺寸，
//         到游戏里就「2 倍超出」（用户实测）。两边的差不是「挂点差 2.57 m」那么单一，
//         而是**缩放 + 偏移一起**造成的。
// ⇒ 预览改为：翻转书按 (0,0,+40) × 0.5 摆（画面与游戏一致）。特效**挂在翻转书上**，但相对变换按
//   「挂在游戏里那个根组件（缩放 1、原点在脚底）上」折算过去 —— 根组件相对翻转书是
//   S(1/0.5 = 2) ∘ T(0,0,-40/0.5 = -80)（见 GetRootLocalToFlipbook）⇒ 位置与尺寸都与游戏一致。
//   这三个数来自用户：0.5 缩放与 +40 偏移是**全角色统一**的（改不了，所以让预览来适配它们）。
//
// 这里曾经挂过两个补偿，记下来免得再走一遍：
//   ① AtlasFX.PaperZD.PreviewYaw（曾默认 90）：误判「预览相机沿 Y、游戏相机沿 X」。
//      实际两个相机**同轴**（都沿 Y 轴看），任何 Yaw 补偿都会让预览与游戏不一致。
//      最初「看不见」的真因是网格模板里的 MeshYaw = -90 把面片转成了侧对镜头
//      （**游戏里也一样是一条缝**），模板已改回 0，见 DFX/Modules/M_SpriteAtlasSize.dfm。
//   ② AtlasFX.PaperZD.FootAlign（曾默认 1）：用「精灵底边相对翻转书原点的 Z」把特效压到脚底。
//      它只在「预览保持 1:1」的前提下成立，且把**缩放差**整个漏掉了（那次「预览对、游戏错」就是它）；
//      折算方案下世界坐标本来就对，已删除。见 DESIGN.md §18.4。
//
//   ③ 曾经真的 NewObject 一个 **USceneComponent 替身**挂到翻转书下面（承载上面的折算），
//      但它是注册在预览世界里的组件：预览世界先于它销毁时，引擎会在 FScene::Release() 里
//      ensure（RendererScene.cpp:4337「Component Name: SceneComponent /Engine/Transient.
//      PaperFlipbookComponent_0 … Component Asset: None」）。现在折算纯用数学（GetRootLocalToFlipbook），
//      不引入任何额外组件，这条 ensure 就没了。

namespace
{
	/** 玩家刷新间隔（编辑器 tick 次数）。TObjectIterator 要遍历整个对象表，别每帧都来。 */
	constexpr int32 PlayerScanInterval = 60;

	/** 单个通知的运行期状态。 */
	struct FNotifyRuntimeState
	{
		TWeakObjectPtr<const UPaperZDAnimNotify_Base> Notify;

		/** 复刻 PaperZDAnimNotify.cpp:23-24 的 bPersistentActiveState，避免边界上触发两次。 */
		bool bWasActiveLastFrame = false;

		/** 上一次为该通知生成的特效，重新触发时先收掉，免得拖时间轴拖出一堆。 */
		TWeakObjectPtr<UNiagaraComponent> SpawnedComponent;

		/** 「这条通知还没挂 Niagara 系统」只提示一次，免得每圈刷屏。 */
		bool bWarnedNoSystem = false;
	};

	/** 单个预览播放器的运行期状态。 */
	struct FPreviewPlayerState
	{
		TWeakObjectPtr<UPaperZDAnimPlayer> Player;
		TWeakObjectPtr<const UPaperZDAnimSequence> Sequence;

		/** 用路径而不是指针判断「换序列了没有」：预览播放器可能给出瞬态副本，那样指针每帧都在变。 */
		FString SequencePath;

		float LastPlaybackTime = 0.0f;
		TArray<FNotifyRuntimeState> NotifyStates;

		/** 上一帧播放器在不在播放。暂停 → 播放的那一下等价于游戏里新建 playback handle，要清上升沿状态。 */
		bool bWasPlayingLastFrame = false;

		/** 上一次「一条通知都没认出来」时补扫的时间戳（FPlatformTime::Seconds），用来限流。 */
		double LastNotifyRescanTime = 0.0;

		/** 这条序列是不是已经把「一条都没认出来」的逐条诊断打过一次了，免得刷屏。 */
		bool bDumpedNotifyDiagnostics = false;

		/** 建表时序列里有哪些通知（数量 + 名字 + 是否被忽略）。变了就说明表过期，要重建。 */
		uint32 NotifyTableSignature = 0;
	};

	TArray<FPreviewPlayerState> PlayerStates;
	FTSTicker::FDelegateHandle TickerHandle;
	int32 TicksSincePlayerScan = PlayerScanInterval;

	// ------------------------------------------------------------------
	// 让预览摆成游戏的样子（见文件头注释）
	// ------------------------------------------------------------------

	/** 游戏里精灵组件的相对变换（用户确认全角色统一）。 */
	void GetGameSpriteTransform(float& OutScale, float& OutZ)
	{
		OutScale = FMath::Max(CVarAtlasFXPreviewSpriteScale.GetValueOnAnyThread(), 0.001f);
		OutZ = CVarAtlasFXPreviewSpriteZ.GetValueOnAnyThread();
	}

	/** 每帧把预览翻转书摆成游戏里的样子（PaperZD 自己可能重设过变换，所以每帧都来一遍）。 */
	void ApplyGameLikeSpriteTransform(UPrimitiveComponent* RenderComponent)
	{
		if (!RenderComponent || CVarAtlasFXPreviewMimicGame.GetValueOnAnyThread() == 0)
		{
			return;
		}

		float SpriteScale = 1.0f;
		float SpriteZ = 0.0f;
		GetGameSpriteTransform(SpriteScale, SpriteZ);
		RenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, SpriteZ));
		RenderComponent->SetRelativeScale3D(FVector(SpriteScale));
	}

	/**
	 * 「游戏里的角色根组件」相对翻转书的变换 —— 纯数学，不造任何组件。
	 *
	 * 预览把翻转书摆成了游戏里精灵组件的样子（相对 (0,0,SpriteZ) × SpriteScale），而游戏里特效挂的是
	 * **角色根组件**（缩放 1、原点在脚底）⇒ 根组件相对翻转书就是 `S(1/SpriteScale) ∘ T(0, 0, -SpriteZ)`。
	 * 特效仍然挂在翻转书上，只是把「挂在根组件上」的相对变换折算过去。
	 *
	 * 为什么不自己造一个"根组件替身"挂上去：那是一个**注册在预览世界里**的组件，预览世界先于它销毁时，
	 * 引擎会在 `FScene::Release()` 里抛 ensure（`RendererScene.cpp:4337`：
	 * 「Component Name: SceneComponent /Engine/Transient.PaperFlipbookComponent_0 … Component Asset: None」）。
	 * 数学等价而且不引入额外组件，就没有这个问题。
	 */
	FTransform GetRootLocalToFlipbook()
	{
		// 关掉「模仿游戏」（AtlasFX.PaperZD.MimicGame 0）时不做任何折算：特效直接挂在翻转书上，
		// 就是最早的预览行为，方便对照排查。
		if (CVarAtlasFXPreviewMimicGame.GetValueOnAnyThread() == 0)
		{
			return FTransform::Identity;
		}

		float SpriteScale = 1.0f;
		float SpriteZ = 0.0f;
		GetGameSpriteTransform(SpriteScale, SpriteZ);
		return FTransform(FQuat::Identity,
			FVector(0.0f, 0.0f, -SpriteZ / SpriteScale),
			FVector(1.0f / SpriteScale));
	}

	// ------------------------------------------------------------------
	// 反射读属性
	// ------------------------------------------------------------------

	/** 播放器当前挂的渲染组件。RegisteredRenderComponent 是 private + UPROPERTY，没 getter。 */
	UPrimitiveComponent* GetRenderComponent(const UPaperZDAnimPlayer* Player)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
			UPaperZDAnimPlayer::StaticClass(), TEXT("RegisteredRenderComponent"));
		return Property ? Cast<UPrimitiveComponent>(Property->GetObjectPropertyValue_InContainer(Player)) : nullptr;
	}

	/** 通知上挂的 Niagara 系统：按**类型**找，不认名字（蓝图里叫 Niagara、PSTemplate 都认）。 */
	UNiagaraSystem* GetNotifySystem(const UObject* Notify)
	{
		for (TFieldIterator<FObjectPropertyBase> It(Notify->GetClass()); It; ++It)
		{
			const FObjectPropertyBase* Property = *It;
			if (!Property->PropertyClass || !Property->PropertyClass->IsChildOf(UNiagaraSystem::StaticClass()))
			{
				continue;
			}

			if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Property->GetObjectPropertyValue_InContainer(Notify)))
			{
				return System;
			}
		}
		return nullptr;
	}

	FVector GetVectorProperty(const UObject* Object, const TCHAR* PropertyName, const FVector& Fallback)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), PropertyName);
		if (Property && Property->Struct == TBaseStructure<FVector>::Get())
		{
			return Property->ContainerPtrToValuePtr<FVector>(Object)[0];
		}
		return Fallback;
	}

	FRotator GetRotatorProperty(const UObject* Object, const TCHAR* PropertyName, const FRotator& Fallback)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), PropertyName);
		if (!Property)
		{
			return Fallback;
		}
		if (Property->Struct == TBaseStructure<FRotator>::Get())
		{
			return Property->ContainerPtrToValuePtr<FRotator>(Object)[0];
		}
		if (Property->Struct == TBaseStructure<FVector>::Get())
		{
			return Property->ContainerPtrToValuePtr<FVector>(Object)[0].Rotation();
		}
		return Fallback;
	}

	bool GetBoolProperty(const UObject* Object, const TCHAR* PropertyName, bool Fallback)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : Fallback;
	}

	// ------------------------------------------------------------------
	// 通知筛选与生成
	// ------------------------------------------------------------------

	/**
	 * PaperZD 自带的原生通知（Niagara/Sound/...）本来就挂在 SequenceRenderComponent 上生成，
	 * 预览里已经能跑，再生成一次就重了。判据：类来自 /Script/PaperZD 且不是蓝图生成的类。
	 */
	bool ShouldIgnoreNotify(const UPaperZDAnimNotify_Base* Notify)
	{
		const UClass* NotifyClass = Notify->GetClass();
		const bool bNativePaperZDClass = NotifyClass->GetOutermost()->GetName() == TEXT("/Script/PaperZD");
		return bNativePaperZDClass && !NotifyClass->IsChildOf(UBlueprintGeneratedClass::StaticClass());
	}

	/** 收掉某个玩家身上所有已生成的特效。 */
	void DestroySpawnedComponents(FPreviewPlayerState& State)
	{
		for (FNotifyRuntimeState& NotifyState : State.NotifyStates)
		{
			if (UNiagaraComponent* Component = NotifyState.SpawnedComponent.Get())
			{
				Component->DestroyComponent();
			}
			NotifyState.SpawnedComponent = nullptr;
		}
	}

	void DestroyAllSpawnedComponents()
	{
		for (FPreviewPlayerState& State : PlayerStates)
		{
			DestroySpawnedComponents(State);
		}
	}

	/**
	 * 复刻 TxSpawn 蓝图里「方便制作缩放」那个选择节点：
	 *   Index = OR(Equal(Scale.Y, 0), Equal(Scale.Z, 0))
	 *   真 -> MakeVector(Scale.X, Scale.X, Scale.X)；假 -> 原样用 Scale。
	 * 所以 DefAtk 上填的 (1.0, 0.0, 0.0) 会被展开成 (1,1,1)，而不是把特效压扁。
	 */
	FVector ResolveScale(const UObject* Notify)
	{
		const FVector Raw = GetVectorProperty(Notify, TEXT("Scale"), FVector::OneVector);
		if (Raw.Y == 0.0 || Raw.Z == 0.0)
		{
			return FVector(Raw.X);
		}
		return Raw;
	}

	/**
	 * 为一条通知生成特效，逐条对齐 TxSpawn 蓝图：
	 *   TXComp = 人物根组件；NotAttach 为真走世界生成，否则挂到根组件上；
	 *   位置带「1P 标签翻转」；缩放一律靠 spawn 之后的 SetRelativeScale3D。
	 *
	 * 预览里没有角色，所以特效挂在**翻转书**上，用 GetRootLocalToFlipbook() 把「挂在角色根组件上」
	 * 的相对变换折算过去 —— 位置与缩放都与游戏一致，而且不额外造组件。
	 */
	void SpawnForNotify(FPreviewPlayerState& State, FNotifyRuntimeState& NotifyState, const UPaperZDAnimNotify_Base* Notify, UPrimitiveComponent* RenderComponent)
	{
		UNiagaraSystem* System = GetNotifySystem(Notify);
		if (!System)
		{
			// 这条通知还没挂 Niagara 系统。表里留着它是有意的（见 RebuildNotifyStates 里的注释）：
			// 挂上系统后，下一次到点就会生成，不需要重建表。只提示一次，免得循环播放每圈刷屏。
			if (!NotifyState.bWarnedNoSystem)
			{
				NotifyState.bWarnedNoSystem = true;
				UE_LOG(LogAtlasFXPreview, Log,
					TEXT("通知 %s 到点了，但它还没挂 Niagara 系统，跳过（挂上之后下次到点就会生成）。"),
					*Notify->GetClass()->GetName());
			}
			return;
		}

		UWorld* World = RenderComponent ? RenderComponent->GetWorld() : nullptr;
		if (!World)
		{
			UE_LOG(LogAtlasFXPreview, Warning, TEXT("通知 %s 到点了但生成不了：拿不到预览世界（渲染组件 = %s）"),
				*Notify->GetClass()->GetName(),
				RenderComponent ? *RenderComponent->GetName() : TEXT("<空>"));
			return;
		}

		const FVector Offset = GetVectorProperty(Notify, TEXT("Offset"), FVector::ZeroVector);
		const FRotator Rotation = GetRotatorProperty(Notify, TEXT("Rotation"), FRotator::ZeroRotator);
		const bool bNotAttach = GetBoolProperty(Notify, TEXT("NotAttach"), false);
		const FVector Scale = ResolveScale(Notify);

		// 蓝图里的「计算真正位置」：
		//   Location = GetWorldLocation(Comp) + (Owner.Tags 含 "1P" ? Offset : -Offset)
		// 但蓝图那个加法节点的 Z 分量是直接从 Offset 上 Break 出来的（SelectVector 只接了 X/Y），
		// 也就是**只镜像水平面、不翻高度** —— 横版 2D 里这是对的，照抄。
		// 预览的渲染组件没有 Owner（也就没有标签），走的自然是 -Offset 那一支。
		const AActor* Owner = RenderComponent->GetOwner();
		const bool bFirstPerson = Owner && Owner->Tags.Contains(TEXT("1P"));
		const float Sign = bFirstPerson ? 1.0f : -1.0f;
		const FVector SignedOffset(Offset.X * Sign, Offset.Y * Sign, Offset.Z);

		// 折算：特效相对**角色根组件**的变换（游戏里就是这样挂的）→ 相对**翻转书**的变换。
		const FTransform RootLocalToFlipbook = GetRootLocalToFlipbook();
		const FTransform RelativeToRoot(FQuat(Rotation), Offset, Scale);
		const FTransform RelativeToFlipbook = RelativeToRoot * RootLocalToFlipbook;

		// 世界生成那条路要的是「游戏里根组件」的世界变换（根组件是翻转书的子级，见 GetRootLocalToFlipbook）。
		const FTransform RootToWorld = RootLocalToFlipbook * RenderComponent->GetComponentTransform();

		// 循环播放 / 来回拖时间轴 / 紧接着再播一次：同一条通知会反复触发。
		// 这里**优先复用上一次的组件并重置它**（`Activate(bReset=true)` 把上一轮还没死掉的粒子清干净，
		// 再从第 0 帧重播），而不是"销毁再新建"：
		//   * 特效粒子寿命常常比序列长，销毁+新建的那一帧里旧粒子还在，看起来就像"第二次不显示了"；
		//   * 世界生成那条路以前用池化（AutoRelease），复用回来的组件未必会自己重新激活。
		if (UNiagaraComponent* Previous = NotifyState.SpawnedComponent.Get())
		{
			if (Previous->GetAsset() == System && Previous->GetWorld() == World)
			{
				if (bNotAttach)
				{
					Previous->SetWorldLocationAndRotation(RootToWorld.GetLocation() + SignedOffset, Rotation);
					Previous->SetWorldScale3D(Scale);
				}
				else
				{
					Previous->SetRelativeLocationAndRotation(RelativeToFlipbook.GetLocation(), RelativeToFlipbook.Rotator());
					Previous->SetRelativeScale3D(RelativeToFlipbook.GetScale3D());
				}

				Previous->Activate(/*bReset=*/true);

				UE_LOG(LogAtlasFXPreview, Log,
					TEXT("预览重置特效：%s（通知 %s，时间 %.3f 秒，复用组件 %s，已清掉上一轮的粒子）"),
					*System->GetName(), *Notify->GetClass()->GetName(), Notify->Time, *Previous->GetName());
				return;
			}

			// 通知上的 Niagara 资产换了：旧的收掉，走下面的新建。
			Previous->DestroyComponent();
			NotifyState.SpawnedComponent = nullptr;
		}

		UNiagaraComponent* Spawned = nullptr;
		if (bNotAttach)
		{
			// 蓝图：Spawn System at Location（世界位置 + 偏移，Scale 直接给）。
			// 世界位置取的是**根组件**的世界位置（不是翻转书本身）。
			// 池化用 None：这条路的组件由我们自己复用/销毁（见上面的"复用并重置"分支），
			// 交给池子会出现"复用回来但没重新激活"的组件。
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, System,
				RootToWorld.GetLocation() + SignedOffset,
				Rotation,
				Scale,
				true, true, ENCPoolMethod::None, true);
		}
		else
		{
			// 蓝图：Spawn System Attached —— 挂点这一支的 Location 用的是**没动过的 Offset**
			// （蓝图里挂点这一支不做镜像，镜像只出现在 NotAttach 的「计算真正位置」里）。
			// 这里挂的是翻转书，所以位置/旋转用折算后的 RelativeToFlipbook。
			// Location Type 与蓝图一致：Snap to Target, Including Scale（没传 Scale 时它与
			// KeepRelativeOffset 在引擎里等价，见 NiagaraFunctionLibrary.cpp:254-281）。
			// 注意这个函数**没有 Scale 参数**，缩放靠随后那句 SetRelativeScale3D —— 传折算后的相对缩放。
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System, RenderComponent, NAME_None,
				RelativeToFlipbook.GetLocation(), RelativeToFlipbook.Rotator(),
				EAttachLocation::SnapToTargetIncludingScale,
				true, true, ENCPoolMethod::None, true);
		}

		NotifyState.SpawnedComponent = Spawned;
		if (Spawned)
		{
			Spawned->SetRelativeScale3D(RelativeToFlipbook.GetScale3D());

			UE_LOG(LogAtlasFXPreview, Log,
				TEXT("预览生成特效：%s（通知 %s，时间 %.3f 秒，%s，挂点 %s，Offset %s，Rotation %s，Scale %s，组件 %s）"),
				*System->GetName(), *Notify->GetClass()->GetName(), Notify->Time,
				bNotAttach ? TEXT("世界生成") : TEXT("挂在翻转书上"),
				*RenderComponent->GetName(),
				*Offset.ToString(), *Rotation.ToString(), *Scale.ToString(), *Spawned->GetName());
		}
	}

	// ------------------------------------------------------------------
	// 每帧跟踪
	// ------------------------------------------------------------------

	/** 重新扫一遍预览播放器。预览世界由 PaperZDAnimationSourceViewportClient 创建，只有一个。 */
	void RefreshPlayers()
	{
		TSet<UPaperZDAnimPlayer*> AlivePlayers;
		for (TObjectIterator<UPaperZDAnimPlayer> It; It; ++It)
		{
			UPaperZDAnimPlayer* Player = *It;
			if (!Player)
			{
				continue;
			}

			UPrimitiveComponent* RenderComponent = GetRenderComponent(Player);
			UWorld* World = RenderComponent ? RenderComponent->GetWorld() : nullptr;
			if (!World || World->WorldType != EWorldType::EditorPreview)
			{
				continue;
			}

			AlivePlayers.Add(Player);

			const bool bKnown = PlayerStates.ContainsByPredicate(
				[Player](const FPreviewPlayerState& State) { return State.Player.Get() == Player; });
			if (!bKnown)
			{
				FPreviewPlayerState NewState;
				NewState.Player = Player;
				PlayerStates.Add(MoveTemp(NewState));

				UE_LOG(LogAtlasFXPreview, Log,
					TEXT("接管 PaperZD 预览播放器（渲染组件 %s），序列编辑器里的特效通知现在会真的生成特效。"),
					*RenderComponent->GetName());
			}
		}

		for (int32 Index = PlayerStates.Num() - 1; Index >= 0; --Index)
		{
			if (!AlivePlayers.Contains(PlayerStates[Index].Player.Get()))
			{
				DestroySpawnedComponents(PlayerStates[Index]);
				PlayerStates.RemoveAtSwap(Index);
			}
		}
	}

	/**
	 * 通知表的签名：序列里的通知数量 + 每个通知的名字 + 是否被忽略。
	 *
	 * 用来发现「表过期了」—— 序列编辑时加通知 / 删通知 / 换通知资产都会变，于是不必再靠
	 * 「切到别的 AnimSequence 再切回来」强制重建。故意**不含**通知上挂的 Niagara 系统：
	 * 系统是到点时现读的，换系统不需要重建表（重建会把正在播的特效一起收掉）。
	 */
	uint32 ComputeNotifyTableSignature(const UPaperZDAnimSequence* Sequence)
	{
		if (!Sequence)
		{
			return 0;
		}

		const TArray<UPaperZDAnimNotify_Base*>& Notifies = Sequence->GetAnimNotifies();
		uint32 Hash = GetTypeHash(Notifies.Num());
		for (const UPaperZDAnimNotify_Base* Notify : Notifies)
		{
			Hash = HashCombine(Hash, GetTypeHash(Notify ? Notify->GetFName() : NAME_None));
			Hash = HashCombine(Hash, GetTypeHash(Notify != nullptr && ShouldIgnoreNotify(Notify)));
		}
		return Hash;
	}

	/** 换序列时重建通知表。这一帧不触发任何通知，免得一进去就炸一堆。 */
	void RebuildNotifyStates(FPreviewPlayerState& State, const UPaperZDAnimSequence* Sequence)
	{
		DestroySpawnedComponents(State);
		State.NotifyStates.Reset();
		State.Sequence = Sequence;
		State.SequencePath = Sequence ? Sequence->GetPathName() : FString();
		State.LastPlaybackTime = State.Player.IsValid() ? State.Player->GetCurrentPlaybackTime() : 0.0f;

		if (!Sequence)
		{
			UE_LOG(LogAtlasFXPreview, Log, TEXT("预览播放器当前没有序列，清空跟踪。"));
			return;
		}

		for (UPaperZDAnimNotify_Base* Notify : Sequence->GetAnimNotifies())
		{
			if (!Notify || ShouldIgnoreNotify(Notify))
			{
				continue;
			}

			// **这里不再过滤「还没挂 Niagara 系统」的通知**。
			// 以前过滤掉，于是「先建通知、后挂系统」的那种通知进不了表，只能切到别的序列再切回来才生效
			//（DESIGN §18.5 那条土办法）。现在系统是**到点时现读**的，挂上就能用。
			FNotifyRuntimeState NotifyState;
			NotifyState.Notify = Notify;
			State.NotifyStates.Add(MoveTemp(NotifyState));
		}

		State.NotifyTableSignature = ComputeNotifyTableSignature(Sequence);

		UE_LOG(LogAtlasFXPreview, Log, TEXT("跟踪预览序列 %s：%d 条通知在跟踪名单里（序列上共 %d 条）。"),
			*Sequence->GetName(), State.NotifyStates.Num(), Sequence->GetAnimNotifies().Num());

		// 序列上明明有通知、却一条都没认出来 —— 逐条打印原因，每条序列只打一次。
		// 已知成因：编辑器刚打开时通知的类/属性还没加载完，反射扫不到，2 秒后补扫就能好。
		if (State.NotifyStates.Num() == 0 && Sequence->GetAnimNotifies().Num() > 0 && !State.bDumpedNotifyDiagnostics)
		{
			State.bDumpedNotifyDiagnostics = true;
			UE_LOG(LogAtlasFXPreview, Warning,
				TEXT("预览序列 %s 上 %d 条通知一条都没认出来，逐条打印诊断（会每 2 秒自动补扫一次）："),
				*Sequence->GetName(), Sequence->GetAnimNotifies().Num());
			for (UPaperZDAnimNotify_Base* Notify : Sequence->GetAnimNotifies())
			{
				if (Notify)
				{
					FAtlasFXPaperZDPreview::DumpNotifyDiagnostics(Notify);
				}
			}
		}
	}

	void UpdatePlayer(FPreviewPlayerState& State)
	{
		UPaperZDAnimPlayer* Player = State.Player.Get();

		// 先把角色摆成游戏里的样子（缩放 0.5 / Z +40）；放在早退分支之前，任何情况下都生效。
		ApplyGameLikeSpriteTransform(GetRenderComponent(Player));

		const UPaperZDAnimSequence* Sequence = Player->GetCurrentAnimSequence();
		const FString SequencePath = Sequence ? Sequence->GetPathName() : FString();
		if (SequencePath != State.SequencePath)
		{
			State.bDumpedNotifyDiagnostics = false;
			RebuildNotifyStates(State, Sequence);
			return;
		}

		// 表也可能过期，两种情况都要重建：
		//   ① 序列上加了 / 删了通知，或者换了通知资产（签名变了）；
		//   ② 表里跟踪的通知对象**已经失效** —— 保存通知蓝图会触发蓝图重编（reinstance），
		//      旧实例被换成新对象，弱指针就全废了：症状是「保存之后预览反而不出特效」，
		//      以前只能切到别的序列再切回来（重建表）才恢复。
		const bool bHasDeadNotify = State.NotifyStates.ContainsByPredicate(
			[](const FNotifyRuntimeState& NotifyState) { return !NotifyState.Notify.IsValid(); });
		if (bHasDeadNotify || ComputeNotifyTableSignature(Sequence) != State.NotifyTableSignature)
		{
			UE_LOG(LogAtlasFXPreview, Log, TEXT("预览序列 %s 的通知表过期了（%s），重建。"),
				Sequence ? *Sequence->GetName() : TEXT("<空>"),
				bHasDeadNotify ? TEXT("跟踪的通知对象已失效") : TEXT("通知列表变了"));
			State.bDumpedNotifyDiagnostics = false;
			RebuildNotifyStates(State, Sequence);
			return;
		}

		if (!Sequence)
		{
			State.LastPlaybackTime = Player->GetCurrentPlaybackTime();
			return;
		}

		// 通知表是「换序列」那一刻建的，而编辑器刚打开时通知的类/属性可能还没加载好，
		// 那一瞬间会一条都认不出来；序列不换就再也不会重建 ⇒ 这里每 2 秒补扫一次自愈。
		if (State.NotifyStates.Num() == 0)
		{
			State.LastPlaybackTime = Player->GetCurrentPlaybackTime();

			const double Now = FPlatformTime::Seconds();
			if (Sequence->GetAnimNotifies().Num() > 0 && Now - State.LastNotifyRescanTime >= 2.0)
			{
				State.LastNotifyRescanTime = Now;
				RebuildNotifyStates(State, Sequence);
				if (State.NotifyStates.Num() > 0)
				{
					UE_LOG(LogAtlasFXPreview, Log, TEXT("预览序列 %s 的通知表补扫成功：现在能认出 %d 条通知。"),
						*Sequence->GetName(), State.NotifyStates.Num());
				}
			}
			return;
		}

		UPrimitiveComponent* RenderComponent = GetRenderComponent(Player);
		if (!RenderComponent)
		{
			return;
		}

		const float Playtime = Player->GetCurrentPlaybackTime();
		const float LastPlaybackTime = State.LastPlaybackTime;
		const float DeltaTime = Playtime - LastPlaybackTime;
		State.LastPlaybackTime = Playtime;

		// 序列总长：区分「循环回绕」和「往回拖时间轴」要用它（见下面那个判据）。
		const float SequenceDuration = Sequence ? Sequence->GetTotalDuration() : 0.0f;

		// 暂停 / 没在播放的帧**不参与判定**。否则时间停在 0 的那些帧里，「第 0 帧的通知」会被算成已激活
		// （PaperZD 的判据在 DeltaTime <= 0 那一支是 Playtime <= Time && LastPlaybackTime >= Time，0 <= 0 成立）
		// ⇒ 真正按下播放时反而没有上升沿 ⇒ 第 0 帧的通知永远不触发。
		// 游戏里不会这样：一开播就是新的 playback handle，上升沿状态是干净的。
		const bool bIsPlaying = Player->IsPlaying();

		// 暂停 → 播放：等价于游戏里新建 handle，把上升沿状态清零，从 0 重播时第 0 帧的通知能再触发。
		if (bIsPlaying && !State.bWasPlayingLastFrame)
		{
			for (FNotifyRuntimeState& NotifyState : State.NotifyStates)
			{
				NotifyState.bWasActiveLastFrame = false;
			}
		}
		State.bWasPlayingLastFrame = bIsPlaying;

		// 时间没动就什么都不判（拖时间轴和播放都会让时间动，所以不会漏事件）。
		if (FMath::IsNearlyZero(DeltaTime))
		{
			return;
		}

		for (FNotifyRuntimeState& NotifyState : State.NotifyStates)
		{
			const UPaperZDAnimNotify_Base* Notify = NotifyState.Notify.Get();
			if (!Notify)
			{
				continue;
			}

			// 与 PaperZDAnimNotify.cpp:27-50 同一条规则，但**分支不能按「两次时间的差」来分**：
			// 循环回绕时 PaperZD 是用 Fmod 把 PlaybackMarker 归零的（PaperZDAnimPlayer.cpp:131），
			// 传下去的 DeltaTime 仍然是**正的**，标志回绕的是「CurrentTime < PreviousTime」
			// ⇒ 走的还是「正向 + bLooped」那一支，条件放宽成 `Playtime >= T || Last <= T`。
			// 早先按差值分方向，回绕时差值为负 ⇒ 落进反向那一支 ⇒ 挂在 0 帧的通知只在第一次播
			//（用户 2026-10-06 报的「放在最开头就只会播放一次」）。
			const float NotifyTime = Notify->Time;
			bool bActive = false;
			if (Playtime >= LastPlaybackTime)
			{
				// 正着走：PaperZD 正向播放那一支（它的 bLooped 在这里恒为 false）。
				if (Playtime >= NotifyTime && LastPlaybackTime <= NotifyTime)
				{
					bActive = true;
				}
			}
			else
			{
				// 时间倒回去了，两种可能，用「跳回去的幅度」区分：
				//   * **回绕**：走的是「旧时间 → 末尾」+「开头 → 新时间」两段，这两段之和就是这一帧
				//     的推进量（几毫秒）⇒ 跳回去的幅度远大于它；
				//   * **往回拖时间轴**：跳回去的幅度就等于拖动区间本身。
				// 回绕用「尾段 + 头段」的判据（挂在 0 帧的通知因此每圈都重新触发，与游戏一致）；
				// 往回拖只认「拖过的那一段」—— 否则往回拖一下会把整条序列的特效全放一遍
				//（用户 2026-10-06 报的：无论通知在进度条前面还是后面都会触发）。
				const float JumpBack = LastPlaybackTime - Playtime;
				const float WrapPath = (SequenceDuration - LastPlaybackTime) + Playtime;
				if (SequenceDuration > 0.0f && JumpBack > 2.0f * WrapPath)
				{
					if (Playtime >= NotifyTime || LastPlaybackTime <= NotifyTime)
					{
						bActive = true;
					}
				}
				else if (NotifyTime >= Playtime && NotifyTime <= LastPlaybackTime)
				{
					bActive = true;
				}
			}

			const bool bWasActiveLastFrame = NotifyState.bWasActiveLastFrame;
			NotifyState.bWasActiveLastFrame = bActive;

			if (bActive && !bWasActiveLastFrame)
			{
				UE_LOG(LogAtlasFXPreview, Log, TEXT("通知 %s 到点（通知时间 %.3f 秒，当前播放 %.3f 秒）。"),
					*Notify->GetClass()->GetName(), NotifyTime, Playtime);
				SpawnForNotify(State, NotifyState, Notify, RenderComponent);
			}
		}
	}
}

void FAtlasFXPaperZDPreview::Startup()
{
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FAtlasFXPaperZDPreview::Tick));
	UE_LOG(LogAtlasFXPreview, Log,
		TEXT("PaperZD 序列编辑器特效预览已启用（控制台变量 AtlasFX.PaperZD.PreviewFX=0 可关闭）。"));
}

void FAtlasFXPaperZDPreview::Shutdown()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	DestroyAllSpawnedComponents();
	PlayerStates.Reset();
}

bool FAtlasFXPaperZDPreview::Tick(float DeltaSeconds)
{
	if (CVarAtlasFXPreviewFX.GetValueOnGameThread() == 0)
	{
		// 关掉的时候顺手把已经生成的收干净，别留一堆孤儿组件在预览世界里。
		DestroyAllSpawnedComponents();
		PlayerStates.Reset();
		TicksSincePlayerScan = PlayerScanInterval;
		return true;
	}

	if (++TicksSincePlayerScan >= PlayerScanInterval || PlayerStates.Num() == 0)
	{
		TicksSincePlayerScan = 0;
		RefreshPlayers();
	}

	for (int32 Index = PlayerStates.Num() - 1; Index >= 0; --Index)
	{
		FPreviewPlayerState& State = PlayerStates[Index];
		if (!State.Player.IsValid())
		{
			DestroySpawnedComponents(State);
			PlayerStates.RemoveAtSwap(Index);
			continue;
		}

		UpdatePlayer(State);
	}

	return true;
}

void FAtlasFXPaperZDPreview::DumpNotifyDiagnostics(const UPaperZDAnimNotify_Base* Notify)
{
	if (!Notify)
	{
		return;
	}

	UE_LOG(LogAtlasFXPreview, Display, TEXT("  通知 %s：时间 %.3f 秒 / 跳过=%s"),
		*Notify->GetClass()->GetName(), Notify->Time, ShouldIgnoreNotify(Notify) ? TEXT("是（PaperZD 原生）") : TEXT("否"));

	if (const UNiagaraSystem* System = GetNotifySystem(Notify))
	{
		UE_LOG(LogAtlasFXPreview, Display, TEXT("      Niagara 系统 = %s"), *System->GetPathName());
	}
	else
	{
		UE_LOG(LogAtlasFXPreview, Display, TEXT("      Niagara 系统 = <没找到，这条通知不会生成特效>"));
	}

	UE_LOG(LogAtlasFXPreview, Display, TEXT("      Offset=%s Rotation=%s Scale=%s NotAttach=%s"),
		*GetVectorProperty(Notify, TEXT("Offset"), FVector::ZeroVector).ToString(),
		*GetRotatorProperty(Notify, TEXT("Rotation"), FRotator::ZeroRotator).ToString(),
		*GetVectorProperty(Notify, TEXT("Scale"), FVector::OneVector).ToString(),
		GetBoolProperty(Notify, TEXT("NotAttach"), false) ? TEXT("true") : TEXT("false"));

	// 属性名对不上时，把类上所有对象属性列出来好排查（比如蓝图里叫 PSTemplate 而不是 Niagara）。
	for (TFieldIterator<FObjectPropertyBase> It(Notify->GetClass()); It; ++It)
	{
		const FObjectPropertyBase* Property = *It;
		UE_LOG(LogAtlasFXPreview, Display, TEXT("      对象属性 %s : %s"),
			*Property->GetName(),
			Property->PropertyClass ? *Property->PropertyClass->GetName() : TEXT("<无类型>"));
	}
}

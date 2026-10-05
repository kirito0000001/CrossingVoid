// AtlasFXPaperZDPreview 实现。设计说明见同名 .h。

#include "AtlasFXPaperZDPreview.h"

#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/Players/PaperZDAnimPlayer.h"
#include "Components/PrimitiveComponent.h"
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
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogAtlasFXPreview, Log, All);

static TAutoConsoleVariable<int32> CVarAtlasFXPreviewFX(
	TEXT("AtlasFX.PaperZD.PreviewFX"),
	1,
	TEXT("1 = 在 PaperZD 序列编辑器预览里自动生成通知上的 Niagara 特效（默认）；0 = 关闭。"),
	ECVF_Default);

// 2026-10-05 实测结论（用户在预览里逐值试出来的）：**预览相机与游戏相机同轴**（都沿 Y 轴看，
// PaperZD 预览相机在 (0,-100,0) 朝 +Y），所以预览里生成的特效**不需要任何朝向补偿** ——
// 把 AtlasFX.PaperZD.PreviewYaw 设成 0 时，看到的画面与游戏里完全一致。
// 早先「预览沿 Y、游戏沿 X」的判断是误判，两个现象被它带偏了：
//   ① 最初「看不见」的真因是每帧重建把刚生成的组件立刻收掉了（已由 SequencePath 那版修掉）；
//   ② 网格模板里 MeshYaw = -90 本身就把面片转成了侧对镜头 —— **游戏里也一样是一条缝**，
//      已把模块默认与网格模板改回 0（见 DFX/Modules/M_SpriteAtlasSize.dfm）。
// 这个 CVar 保留作逃生开关：万一某个工程/相机的轴向确实不同，可以在编辑器控制台里临时叠加
// 一个 Yaw 做对比。默认 0 = 不叠加。
static TAutoConsoleVariable<float> CVarAtlasFXPreviewYaw(
	TEXT("AtlasFX.PaperZD.PreviewYaw"),
	0.0f,
	TEXT("预览生成特效时额外叠加的 Yaw（度，默认 0 = 不叠加）。预览相机与游戏相机同轴，正常不用改。"),
	ECVF_Default);

// 游戏里特效挂在角色的根组件上（脚底），而预览里这个渲染组件的原点在精灵中心（实测在胸口）。
// 不对齐的话同一个特效在预览里会整体高出一截（用户实测「特效跑到人头上面」）。
// 用精灵包围盒的底边当脚底，把预览的基准点压下去。
static TAutoConsoleVariable<int32> CVarAtlasFXPreviewFootAlign(
	TEXT("AtlasFX.PaperZD.PreviewFootAlign"),
	1,
	TEXT("1 = 预览生成特效时把基准点对齐到精灵包围盒底边（脚底），与游戏里挂在角色根组件上的效果一致；0 = 直接用渲染组件原点。"),
	ECVF_Default);

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

		/** 上一次「一条通知都没认出来」时补扫的时间戳（FPlatformTime::Seconds），用来限流。 */
		double LastNotifyRescanTime = 0.0;

		/** 这条序列是不是已经把「一条都没认出来」的逐条诊断打过一次了，免得刷屏。 */
		bool bDumpedNotifyDiagnostics = false;
	};

	TArray<FPreviewPlayerState> PlayerStates;
	FTSTicker::FDelegateHandle TickerHandle;
	int32 TicksSincePlayerScan = PlayerScanInterval;

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
	 */
	void SpawnForNotify(FNotifyRuntimeState& NotifyState, const UPaperZDAnimNotify_Base* Notify, UPrimitiveComponent* RenderComponent)
	{
		UNiagaraSystem* System = GetNotifySystem(Notify);
		UWorld* World = RenderComponent ? RenderComponent->GetWorld() : nullptr;
		if (!System || !World)
		{
			UE_LOG(LogAtlasFXPreview, Warning, TEXT("通知 %s 到点了但生成不了：Niagara 系统 = %s，世界 = %s"),
				*Notify->GetClass()->GetName(),
				System ? *System->GetName() : TEXT("<空>"),
				World ? TEXT("有") : TEXT("<空>"));
			return;
		}

		// 循环播放或来回拖时间轴时，同一条通知会反复触发：先收掉上一次的。
		if (UNiagaraComponent* Previous = NotifyState.SpawnedComponent.Get())
		{
			Previous->DestroyComponent();
			NotifyState.SpawnedComponent = nullptr;
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

		// 见文件头 CVarAtlasFXPreviewFootAlign：把基准点从「精灵中心」压到「精灵底边（脚底）」，
		// 与游戏里挂在角色根组件上的位置对齐。
		FVector FootOffset = FVector::ZeroVector;
		if (CVarAtlasFXPreviewFootAlign.GetValueOnGameThread() != 0)
		{
			const FBoxSphereBounds& Bounds = RenderComponent->Bounds;
			FootOffset.Z = (Bounds.Origin.Z - Bounds.BoxExtent.Z) - RenderComponent->GetComponentLocation().Z;
		}

		UNiagaraComponent* Spawned = nullptr;
		if (bNotAttach)
		{
			// 蓝图：Spawn System at Location（世界位置 + 偏移，Scale 直接给，池 = AutoRelease）。
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, System,
				RenderComponent->GetComponentLocation() + FootOffset + SignedOffset,
				Rotation,
				Scale,
				true, true, ENCPoolMethod::AutoRelease, true);
		}
		else
		{
			// 蓝图：Spawn System Attached —— 注意这个函数**没有 Scale 参数**，
			// 蓝图也是靠随后那句 SetRelativeScale3D 定缩放的，别把 scale 塞进 spawn。
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System, RenderComponent, NAME_None,
				FootOffset + Offset, Rotation,
				EAttachLocation::SnapToTargetIncludingScale,
				true, true, ENCPoolMethod::None, true);
		}

		NotifyState.SpawnedComponent = Spawned;
		if (Spawned)
		{
			Spawned->SetRelativeScale3D(Scale);

			// 见文件头 CVarAtlasFXPreviewYaw：默认 0（预览与游戏同轴，不需要补偿），
			// 只在有人手动改了这个 CVar 时才叠加，方便对比不同轴向的工程。
			const float PreviewYaw = CVarAtlasFXPreviewYaw.GetValueOnGameThread();
			if (!FMath::IsNearlyZero(PreviewYaw))
			{
				Spawned->AddLocalRotation(FRotator(0.0f, PreviewYaw, 0.0f));
			}

			UE_LOG(LogAtlasFXPreview, Log,
				TEXT("预览生成特效：%s（通知 %s，时间 %.3f 秒，%s，Scale %s，预览 Yaw %.1f，脚底对齐 %+.1f，组件 %s）"),
				*System->GetName(), *Notify->GetClass()->GetName(), Notify->Time,
				bNotAttach ? TEXT("世界生成") : TEXT("挂在渲染组件上"), *Scale.ToString(),
				PreviewYaw, FootOffset.Z, *Spawned->GetName());
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

			// 没挂 Niagara 系统的通知（语音、屏幕震动之类）不掺和。
			if (!GetNotifySystem(Notify))
			{
				continue;
			}

			FNotifyRuntimeState NotifyState;
			NotifyState.Notify = Notify;
			State.NotifyStates.Add(MoveTemp(NotifyState));
		}

		UE_LOG(LogAtlasFXPreview, Log, TEXT("跟踪预览序列 %s：%d 条通知会生成特效（序列上共 %d 条）。"),
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
		const UPaperZDAnimSequence* Sequence = Player->GetCurrentAnimSequence();
		const FString SequencePath = Sequence ? Sequence->GetPathName() : FString();
		if (SequencePath != State.SequencePath)
		{
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

		for (FNotifyRuntimeState& NotifyState : State.NotifyStates)
		{
			const UPaperZDAnimNotify_Base* Notify = NotifyState.Notify.Get();
			if (!Notify)
			{
				continue;
			}

			// 与 PaperZDAnimNotify.cpp:27-50 完全同一条规则（含回绕与反向）。
			const float NotifyTime = Notify->Time;
			bool bActive = false;
			if (DeltaTime > 0.0f)
			{
				const bool bLooped = Playtime < LastPlaybackTime;
				if (bLooped && (Playtime >= NotifyTime || LastPlaybackTime <= NotifyTime))
				{
					bActive = true;
				}
				else if (Playtime >= NotifyTime && LastPlaybackTime <= NotifyTime)
				{
					bActive = true;
				}
			}
			else
			{
				const bool bLooped = Playtime > LastPlaybackTime;
				if (bLooped && (Playtime <= NotifyTime || LastPlaybackTime >= NotifyTime))
				{
					bActive = true;
				}
				else if (Playtime <= NotifyTime && LastPlaybackTime >= NotifyTime)
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
				SpawnForNotify(NotifyState, Notify, RenderComponent);
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

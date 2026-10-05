// AtlasFXPaperZDPreview 实现。设计说明见同名 .h。

#include "AtlasFXPaperZDPreview.h"

#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/Players/PaperZDAnimPlayer.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/Ticker.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
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
		float LastPlaybackTime = 0.0f;
		TArray<FNotifyRuntimeState> NotifyStates;
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

	/** 为一条通知生成特效。语义对齐 TxSpawn 蓝图：NotAttach 勾了就世界生成，否则挂在渲染组件上。 */
	void SpawnForNotify(FNotifyRuntimeState& NotifyState, const UPaperZDAnimNotify_Base* Notify, UPrimitiveComponent* RenderComponent)
	{
		UNiagaraSystem* System = GetNotifySystem(Notify);
		UWorld* World = RenderComponent ? RenderComponent->GetWorld() : nullptr;
		if (!System || !World)
		{
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

		// 工程里的 TxSpawn 把 Scale 当「单值缩放」用：只填 X，Y/Z 留 0
		// （Misaka 的 DefAtk 上就是 1.0 / 0.0 / 0.0）。原样把 (1,0,0) 交给 Spawn 会把特效压成一条线，
		// 所以 Y/Z 同时为 0 时按等比缩放处理；三个都为 0 则当作 1。
		FVector Scale = GetVectorProperty(Notify, TEXT("Scale"), FVector::OneVector);
		if (Scale.Y == 0.0 && Scale.Z == 0.0)
		{
			Scale = FVector(Scale.X > 0.0 ? Scale.X : 1.0);
		}

		UNiagaraComponent* Spawned = nullptr;
		if (bNotAttach)
		{
			const FTransform Base = RenderComponent->GetComponentTransform();
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, System,
				Base.TransformPosition(Offset),
				Base.TransformRotation(Rotation.Quaternion()).Rotator(),
				Scale,
				true, true, ENCPoolMethod::None, true);
		}
		else
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System, RenderComponent, NAME_None,
				Offset, Rotation, Scale,
				EAttachLocation::KeepRelativeOffset,
				true, ENCPoolMethod::None, true);
		}

		NotifyState.SpawnedComponent = Spawned;
		if (Spawned)
		{
			UE_LOG(LogAtlasFXPreview, Verbose, TEXT("预览生成特效：%s（通知 %s，时间 %.3f 秒）"),
				*System->GetName(), *Notify->GetClass()->GetName(), Notify->Time);
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
		State.LastPlaybackTime = State.Player.IsValid() ? State.Player->GetCurrentPlaybackTime() : 0.0f;

		if (!Sequence)
		{
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
	}

	void UpdatePlayer(FPreviewPlayerState& State)
	{
		UPaperZDAnimPlayer* Player = State.Player.Get();
		const UPaperZDAnimSequence* Sequence = Player->GetCurrentAnimSequence();
		if (Sequence != State.Sequence.Get())
		{
			RebuildNotifyStates(State, Sequence);
			return;
		}

		if (!Sequence || State.NotifyStates.Num() == 0)
		{
			State.LastPlaybackTime = Player->GetCurrentPlaybackTime();
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

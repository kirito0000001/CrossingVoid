#include "PaperAnim.h"

UPaperAnim::UPaperAnim(const FObjectInitializer& ObjectInitializer)
{
}

UPaperAnim::~UPaperAnim()
{
}

FDelegateHandle WorldCleanupHandle;

void UPaperAnim::PapersStop(bool bCanle)
{
	switch (EndTypeLocal)
	{
	case EStopMode::CantSee:
		TargetLocal->SetVisibility(ESlateVisibility::Hidden);
		break;
	case EStopMode::Hide:
		TargetLocal->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EStopMode::NoAction:
		break;
	}
	if (LoopHandle.IsValid())
	{
		WorldV->GetTimerManager().ClearTimer(LoopHandle);
	}
	//如果取消播放
	if (bCanle)
	{
		IndexLocalNow = 0; //重置索引
		OnCanle.Broadcast(IndexLocalNow); //触发完成函数
	}
	SetReadyToDestroy(); //销毁节点
}

UPaperAnim* UPaperAnim::PapersPlay(UObject* WorldContextObject, UImage* Target, EStopMode EndType,
                                   TArray<TSoftObjectPtr<UTexture2D>> TexMaps,
                                   int Fps, bool bLoop)
{
	//安全检测
	if (TexMaps.IsEmpty() || Fps <= 0 || !Target)
	{
		return nullptr;
	}

	//创建一个自己类型的异步节点
	UPaperAnim* Node = NewObject<UPaperAnim>();

	//初始化
	Node->TargetLocal = Target;
	Node->TexMapsLocal.SetNum(TexMaps.Num());
	Node->TexMapsLocal = TexMaps;
	Node->FpsLocal = 1.0f / Fps;
	Node->bLoopLocal = bLoop;
	Node->WorldV = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	Node->EndTypeLocal = EndType;

	Target->SetVisibility(ESlateVisibility::SelfHitTestInvisible); //设置可视性

	Node->RegisterWithGameInstance(WorldContextObject); //注册到游戏实例中，异步需要让函数持续存在
	return Node;
}

void UPaperAnim::Activate()
{
	//创建定时器
	WorldV->GetTimerManager().SetTimer(LoopHandle, this, &UPaperAnim::LoopInternal, FpsLocal, true, false);
	// 注册世界清理回调
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UPaperAnim::OnWorldCleanup);
}

void UPaperAnim::LoopInternal()
{
	if (IndexLocalNow > TexMapsLocal.Num() - 1) //如果播放完一轮
	{
		if (!bLoopLocal) //不循环就在这里结束
		{
			WorldV->GetTimerManager().ClearTimer(LoopHandle);

			switch (EndTypeLocal)
			{
			case EStopMode::CantSee:
				TargetLocal->SetVisibility(ESlateVisibility::Hidden);
				break;
			case EStopMode::Hide:
				TargetLocal->SetVisibility(ESlateVisibility::Collapsed);
				break;
			case EStopMode::NoAction:
				break;
			}
			OnComplete.Broadcast(IndexLocalNow); //触发完成函数
			SetReadyToDestroy(); //销毁节点
		}
		IndexLocalNow = 0; //循环就重置索引
	}

	//设置图片
	if (TargetLocal.IsValid())
	{
		TargetLocal->SetBrushFromSoftTexture(TexMapsLocal[IndexLocalNow], false);
		OnLoop.Broadcast(IndexLocalNow); //每次循环时触发
		IndexLocalNow++; //索引加一
	}
	else
	{
		WorldV->GetTimerManager().ClearTimer(LoopHandle);
		SetReadyToDestroy(); //销毁节点
	}
}

void UPaperAnim::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// 清理定时器
	if (LoopHandle.IsValid())
	{
		if (World)
			World->GetTimerManager().ClearTimer(LoopHandle);
	}

	// 注销委托（防止悬挂回调）
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldV = nullptr;
	// 安全销毁自身
	SetReadyToDestroy();
}

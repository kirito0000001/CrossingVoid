#include "ForEachLoopDelay.h"

UForEachLoopDelay::UForEachLoopDelay(const FObjectInitializer& ObjectInitializer) //构造函数
{
}

UForEachLoopDelay::~UForEachLoopDelay() //析构函数
{
}


//请到PaperAnim里学习
UForEachLoopDelay* UForEachLoopDelay::Async_ForLoopDelay(UObject* WorldContextObject, int InStart, int InEnd,
														 float InDuration, bool bInForce)
{
	UForEachLoopDelay* Node = NewObject<UForEachLoopDelay>();
	Node->Start = InStart;
	Node->End = InEnd;
	Node->Duration = InDuration;
	Node->bForce = bInForce;
	Node->WorldContextObject = WorldContextObject;

	Node->RegisterWithGameInstance(WorldContextObject);
	return Node; //返回节点
}

void UForEachLoopDelay::Break()
{
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject.Get());
	if (World)
	{
		World->GetTimerManager().ClearTimer(Handle);
	}
}

void UForEachLoopDelay::Activate()//被动激活函数
{
	if (bForce) //如果除去第一次延迟，就直接触发一次Loop回调
		LoopInternal();
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject.Get());
	if (World)
	{
		GWorld->GetTimerManager().SetTimer(Handle, this, &UForEachLoopDelay::LoopInternal, Duration, true);
		// 设置定时器，每秒触发一次LoopInternal函数，参数1是定时器句柄，参数2是this指针，参数3是函数指针，参数4是延迟时间，参数5是循环
	}
}

void UForEachLoopDelay::LoopInternal()
{
	OnLoop.Broadcast(Start);
	Start++; //循环次数加一

	if (Start > End) //如果start参数大于结束参数，则结束循环，并触发结束函数
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject.Get());
		if (World)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
		OnFinish.Broadcast(End); //.Broadcast前面的就是创建的函数
	}
}

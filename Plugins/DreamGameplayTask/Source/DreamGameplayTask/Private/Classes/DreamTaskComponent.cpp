// Copyright 2022 - 2024 Dream Moon Team. All Rights Reserved.


#include "Classes/DreamTaskComponent.h"

#include "DreamGameplayTaskBlueprintLibrary.h"
#include "DreamGameplayTaskLog.h"
#include "DreamGameplayTaskSetting.h"
#include "Classes/DreamTask.h"
#include "Classes/DreamTaskConditionTemplate.h"

UDreamTaskComponent::UDreamTaskComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

UDreamTaskComponent::~UDreamTaskComponent()
{
	if (bTimerActive && GetOwner())
	{
		GetOwner()->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		bTimerActive = false;
	}
}

void UDreamTaskComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UDreamTaskComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	TaskData.ClearHandles();
	OnTaskListChanged.Clear();
	OnTaskListChangedDelegate.Clear();
	OnTaskUpdate.Clear();
	OnTaskRemoved.Clear();
	OnTaskReset.Clear();
	if (bTimerActive && GetOwner())
	{
		GetOwner()->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		bTimerActive = false;
	}
}

void UDreamTaskComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

FDreamTaskSpecHandle UDreamTaskComponent::GiveTaskByClass(TSubclassOf<UDreamTask> InClass, UObject* InPayload)
{
	if (!InClass || HasTaskByClass(InClass))
	{
		return FDreamTaskSpecHandle();
	}

	UDreamTask* Task = NewObject<UDreamTask>(this, InClass);
	Task->InitializeTask(this, InPayload);
	FDreamTaskSpecHandle& SpecHandle = TaskData.AddHandle(FDreamTaskSpecHandle(Task, FDateTime::Now()));

	DelegateCall_TaskListChanged(GetTaskData());

	ActiveTimer();

	return SpecHandle;
}

void UDreamTaskComponent::InitializeTaskList(FDreamTaskSpecHandleContainer NewList)
{
	TaskData.ClearHandles();
	TaskData.SetHandles(NewList.GetHandles());

	DelegateCall_TaskListChanged(GetTaskData());
}

bool UDreamTaskComponent::HasTaskByClass(TSubclassOf<UDreamTask> InCheckTaskClass)
{
	return GetTaskList().ContainsByPredicate([InCheckTaskClass](const FDreamTaskSpecHandle& Handle)
	{
		return Handle == InCheckTaskClass;
	});
}

bool UDreamTaskComponent::HasTaskByName(FName InCheckTaskName)
{
	return GetTaskList().ContainsByPredicate([InCheckTaskName](const FDreamTaskSpecHandle& Handle)
	{
		return Handle == InCheckTaskName;
	});
}

bool UDreamTaskComponent::RemoveTaskByClass(TSubclassOf<UDreamTask> InRemoveTaskClass)
{
	const FDreamTaskSpecHandle& Handle = TaskData.FindHandle(InRemoveTaskClass);
	if (!Handle.IsValid())
	{
		return false;
	}
	
	Handle.GetTask()->DelegateCall_TaskRemoved();
	
	TaskData.RemoveHandle(Handle);
	
	DelegateCall_TaskListChanged(GetTaskData());
	
	return true;
}

bool UDreamTaskComponent::RemoveTaskByName(FName InRemoveTaskName)
{
	TArray<FDreamTaskSpecHandle> ToRemove;
	for (const FDreamTaskSpecHandle& Element : TaskData.GetHandles())
	{
		if (Element == InRemoveTaskName)
		{
			ToRemove.Add(Element);
		}
	}
	bool bRemoved = false;
	for (const FDreamTaskSpecHandle& R : ToRemove)
	{
		R.GetTask()->DelegateCall_TaskRemoved();
		TaskData.RemoveHandle(R);
		bRemoved = true;
	}
	if (bRemoved)
	{
		DelegateCall_TaskListChanged(GetTaskData());
	}
	return bRemoved;
}

void UDreamTaskComponent::ClearTasks()
{
	TaskData.ClearHandles();
	
	DelegateCall_TaskListChanged(GetTaskData());
}

void UDreamTaskComponent::UpdateTask(FName TaskName, const TArray<FName>& InConditionNames)
{
	TaskData.FindHandle(TaskName).GetTask()->UpdateTaskByName(InConditionNames);
}

void UDreamTaskComponent::UpdateTaskByClass(TSubclassOf<UDreamTask> InClass, const TArray<FName>& InConditionNames)
{
	TaskData.FindHandle(InClass).GetTask()->UpdateTaskByName(InConditionNames);
}

const FDreamTaskSpecHandle& UDreamTaskComponent::GetTaskByClass(TSubclassOf<UDreamTask> InTaskClass)
{
	if (!HasTaskByClass(InTaskClass)) return FDreamTaskSpecHandle::InvalidHandle();

	return *GetTaskList().FindByPredicate([InTaskClass](const FDreamTaskSpecHandle& Handle)
	{
		return Handle == InTaskClass;
	});
}

const FDreamTaskSpecHandle& UDreamTaskComponent::GetTaskByName(FName InTaskName)
{
	if (!HasTaskByName(InTaskName)) return FDreamTaskSpecHandle::InvalidHandle();

	return *GetTaskList().FindByPredicate([InTaskName](const FDreamTaskSpecHandle& Handle)
	{
		return Handle == InTaskName;
	});
}

void UDreamTaskComponent::ResetTaskByName(FName InName)
{
	FDreamTaskSpecHandle* Handle = TaskData.FindHandleMutable(InName);
	if (Handle == nullptr)
		return;
	if (Handle->IsUseMaximumTime())
	{
		Handle->Reset();
	}
	else
	{
		Handle->GetTask()->ResetTask();
	}
}

void UDreamTaskComponent::ResetTaskByClass(TSubclassOf<UDreamTask> InClass)
{
	FDreamTaskSpecHandle* Handle = TaskData.FindHandleMutable(InClass);
	if (Handle == nullptr)
		return;
	if (Handle->IsUseMaximumTime())
	{
		Handle->Reset();
	}
	else
	{
		Handle->GetTask()->ResetTask();
	}
}

void UDreamTaskComponent::ResetTask(UDreamTask* InTask)
{
	FDreamTaskSpecHandle* Handle = TaskData.FindHandleMutable(InTask);
	if (Handle == nullptr)
		return;
	if (Handle->IsUseMaximumTime())
	{
		Handle->Reset();
	}
	else
	{
		Handle->GetTask()->ResetTask();
	}
}

TArray<UDreamTask*> UDreamTaskComponent::GetTaskArray()
{
	return UDreamGameplayTaskBlueprintLibrary::BuildContainerTaskArray(GetTaskData());
}

void UDreamTaskComponent::ActiveTimer()
{
	if (!bTimerActive && GetOwner())
	{
		DGT_UPDATER_DEBUG_LOG(Warning, TEXT("Timer Active."));
		GetOwner()->GetWorld()->GetTimerManager()
		          .SetTimer(TimerHandle, this, &UDreamTaskComponent::Updater, TimerDeltaTime, true);
		bTimerActive = true;
	}
}

void UDreamTaskComponent::StopTimer()
{
	if (TaskData.IsAllUseMaximumTimeCompleted())
	{
		if (bTimerActive && GetOwner())
		{
			DGT_UPDATER_DEBUG_LOG(Warning, TEXT("Timer Stop."));
			GetOwner()->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
			bTimerActive = false;
		}
	}
}

void UDreamTaskComponent::DelegateCall_TaskListChanged(const FDreamTaskSpecHandleContainer& InTaskData)
{
	OnTaskListChanged.Broadcast(TaskData);
	OnTaskListChangedDelegate.Broadcast(const_cast<FDreamTaskSpecHandleContainer&>(InTaskData));
}

void UDreamTaskComponent::DelegateCall_TaskUpdate(const FDreamTaskSpecHandle& InTask)
{
	OnTaskUpdate.Broadcast(InTask);
}

void UDreamTaskComponent::DelegateCall_TaskUpdate(UDreamTask* InTask)
{
	const FDreamTaskSpecHandle& Handle = TaskData.FindHandle(InTask);
	OnTaskUpdate.Broadcast(Handle);
}

void UDreamTaskComponent::DelegateCall_TaskRemoved(const FDreamTaskSpecHandle& InTask)
{
	OnTaskRemoved.Broadcast(InTask);
}

void UDreamTaskComponent::DelegateCall_TaskRemoved(UDreamTask* InTask)
{
	const FDreamTaskSpecHandle& Handle = TaskData.FindHandle(InTask);
	OnTaskRemoved.Broadcast(Handle);
}

void UDreamTaskComponent::DelegateCall_TaskReset(const FDreamTaskSpecHandle& InTask)
{
	OnTaskReset.Broadcast(InTask);
}

void UDreamTaskComponent::DelegateCall_TaskReset(UDreamTask* InTask)
{
	const FDreamTaskSpecHandle& Handle = TaskData.FindHandle(InTask);

	OnTaskReset.Broadcast(Handle);
}

void UDreamTaskComponent::Updater()
{
	if (!IsValid(this))
		return;

	TaskData.UpdateHandles(TimerDeltaTime);

	// 如果开启自动停止，且任务空了/全完成，则停止
	if (TaskData.IsAllCompleted() || TaskData.IsEmpty())
	{
		DGT_UPDATER_DEBUG_LOG(Warning, TEXT("All tasks done or empty; stopping timer automatically."));
		GetOwner()->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		bTimerActive = false;
	}
}

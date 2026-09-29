// Fill out your copyright notice in the Description page of Project Settings.


#include "Classes/DreamTask.h"

#include "DreamGameplayTaskLog.h"
#include "DreamGameplayTaskSetting.h"
#include "Classes/DreamTaskConditionTemplate.h"
#include "Classes/DreamTaskInterface.h"
#include "Classes/DreamTaskComponent.h"
#include "Kismet/GameplayStatics.h"

UDreamTask::UDreamTask(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UDreamTask::InitializeTask(UDreamTaskComponent* InOwnerComponent, UObject* InPayload)
{
	DGT_DEBUG_LOG(Log, TEXT("Task initialize. Name: %s"), *TaskName.ToString())

	// Start Pre Initialize.

	OwnerComponent = InOwnerComponent;
	Payload = InPayload;
	CachedRelatedActors = GetRelatedActors();

	for (TPair<FName, UDreamTaskConditionTemplate*>& Condition : TaskCompletedCondition.GetConditionMapping())
	{
		Condition.Value->InitializeCondition(this);
	}

	// Pre Initialized Successful. Start Post Initialize

	for (auto Element : CachedRelatedActors)
	{
		Cast<IDreamTaskInterface>(Element)->Execute_TaskInitialize(Element, this);
	}

	BP_TaskInitialize();

	// Post Initialize Successful. Start Begin

	SetTaskState(TaskState);
}

UDreamTaskConditionTemplate* UDreamTask::GetTaskCondition(FName ConditionName)
{
	return TaskCompletedCondition.GetConditionByName(ConditionName);
}

bool UDreamTask::CheckTaskCompleted()
{
	if (TaskCompletedCondition.IsConditionsCompleted())
	{
		SetTaskState(EDreamTaskState::EDTS_Completed);
		return true;
	}
	else
	{
		return false;
	}
}

TArray<AActor*> UDreamTask::GetRelatedActors()
{
	TArray<AActor*> RelatedActors;

	for (TSubclassOf<AActor> ActorClass : RelatedActorsClasses)
	{
		if (ActorClass->ImplementsInterface(UDreamTaskInterface::StaticClass()))
		{
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(GEngine->GetCurrentPlayWorld(), ActorClass, FoundActors);
			RelatedActors.Append(FoundActors);
		}
	}

	return RelatedActors;
}

void UDreamTask::UpdateTaskByName(TArray<FName> ConditionNames)
{
	if (EnumHasAnyFlags(TaskState, (EDreamTaskState::EDTS_Accept | EDreamTaskState::EDTS_Going)))
	{
		for (auto Element : ConditionNames)
		{
			if (TaskCompletedCondition.UpdateConditionByName(Element))
			{
				DelegateCall_TaskUpdate(true);

				for (auto RelatedActor : CachedRelatedActors)
				{
					Cast<IDreamTaskInterface>(RelatedActor)->Execute_TaskUpdate(RelatedActor, this);
				}
			}
		}

		CheckTaskCompleted();
	}
}

void UDreamTask::SetTaskState(EDreamTaskState NewState)
{
	UpdateTaskState_Internal(NewState);
}

void UDreamTask::SetTaskConditionProgress(const TMap<FName, int32>& InValue)
{
	for (auto Element : InValue)
	{
		if (UDreamTaskConditionTemplate* Conditon = GetTaskCondition(Element.Key))
		{
			Conditon->SetCount(Element.Value);
		}
		CheckTaskCompleted();
	}
}

TMap<FName, int32> UDreamTask::GetTaskConditionProgress() const
{
	TMap<FName, int32> Result;
	for (auto Element : TaskCompletedCondition.Conditions)
	{
		Result.Add(Element.Key, Element.Value->GetCount());
	}
	return Result;
}

UDreamTaskData* UDreamTask::GetTaskData() const
{
	return TaskData;
}

void UDreamTask::RefreshTask()
{
	DelegateCall_TaskUpdate(true);

	if (OwnerComponent)
	{
		OwnerComponent->DelegateCall_TaskUpdate(this);
	}
}

void UDreamTask::ResetTask()
{
	if (bUseMaximumCompletionTime)
	{
		OwnerComponent->ResetTask(this);
	}
	else
	{
		DelegateCall_TaskReset();
	}
}

void UDreamTask::BP_TaskCompleted_Implementation()
{
}

void UDreamTask::BP_TaskInitialize_Implementation()
{
}

void UDreamTask::BP_TaskUpdate_Implementation()
{
}

void UDreamTask::BP_TaskConditionUpdate_Implementation()
{
}

void UDreamTask::BP_TaskFailed_Implementation()
{
}

void UDreamTask::BP_TaskTimeout_Implementation()
{
}

void UDreamTask::BP_TaskAccept_Implementation()
{
}

void UDreamTask::BP_TaskGoing_Implementation()
{
}

void UDreamTask::BP_TaskRemove_Implementation()
{
}

void UDreamTask::BP_TaskReset_Implementation()
{
}

UDreamTask* UDreamTask::Create(TSubclassOf<UDreamTask> Class, TMap<FName, int32> Progress)
{
	if (!Class) return nullptr;

	UDreamTask* Task = NewObject<UDreamTask>(Class);
	Task->SetTaskConditionProgress(Progress);
	return Task;
}

UWorld* UDreamTask::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
		return nullptr;
	}
	return GetOuter()->GetWorld();
}

void UDreamTask::DelegateCall_TaskReset()
{
	TaskCompletedCondition.Reset();
	SetTaskState(EDreamTaskState::EDTS_Accept);
	OnTaskReset.Broadcast(this);
	BP_TaskReset();

	if (OwnerComponent)
		OwnerComponent->DelegateCall_TaskReset(this);
}

void UDreamTask::DelegateCall_TaskRemoved()
{
	OnTaskRemoved.Broadcast(this);
	BP_TaskRemove();

	if (OwnerComponent)
		OwnerComponent->DelegateCall_TaskRemoved(this);
}

void UDreamTask::DelegateCall_TaskUpdate(bool bCallCondition)
{
	if (bCallCondition)
	{
		OnTaskConditionUpdate.Broadcast(this);
		BP_TaskConditionUpdate();
	}
	BP_TaskUpdate();
	OnTaskUpdate.Broadcast(this);

	if (OwnerComponent)
		OwnerComponent->DelegateCall_TaskUpdate(this);
}

void UDreamTask::CompletedTask_Internal()
{
	BP_TaskCompleted();
}

void UDreamTask::AcceptTask_Internal()
{
	BP_TaskAccept();
}

void UDreamTask::GoingTask_Internal()
{
	BP_TaskGoing();
}

void UDreamTask::TimeoutTask_Internal()
{
	BP_TaskTimeout();
}

void UDreamTask::FailedTask_Internal()
{
	BP_TaskFailed();
}


void UDreamTask::UpdateTaskState_Internal(EDreamTaskState NewState)
{
	// 更新状态
	TaskState = NewState;

	// 根据不同状态执行专属逻辑，并在完成/失败/超时后停止定时
	if (OwnerComponent)
	{
		switch (TaskState)
		{
		case EDreamTaskState::EDTS_Accept:
			BP_TaskAccept();
			OwnerComponent->ActiveTimer();
			break;

		case EDreamTaskState::EDTS_Going:
			BP_TaskGoing();
			OwnerComponent->ActiveTimer();
			break;

		case EDreamTaskState::EDTS_Completed:
			if (bCompletedAutoGiveSubTask && OwnerComponent)
			{
				for (auto Element : SubTasks)
				{
					OwnerComponent->GiveTaskByClass(Element);
				}
			}
			if (OwnerComponent && bUseMaximumCompletionTime)
			{
				OwnerComponent->TaskData.FindHandleMutable(this)->SetEndTime(FDateTime::Now());
			}
			BP_TaskCompleted();
			OnTaskCompleted.Broadcast(this);
			OwnerComponent->StopTimer();
			break;

		case EDreamTaskState::EDTS_Failed:
			BP_TaskFailed();
			OwnerComponent->StopTimer();
			break;

		case EDreamTaskState::EDTS_Timeout:
			if (bUseMaximumCompletionTime)
			{
				BP_TaskTimeout();
				OwnerComponent->StopTimer();
			}
			break;

		default:
			break;
		}
	}

	DelegateCall_TaskUpdate(false);
}

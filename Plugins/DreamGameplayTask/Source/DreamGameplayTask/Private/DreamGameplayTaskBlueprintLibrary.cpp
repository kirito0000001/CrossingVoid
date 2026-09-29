// Fill out your copyright notice in the Description page of Project Settings.


#include "DreamGameplayTaskBlueprintLibrary.h"

#include "DreamGameplayTaskSetting.h"
#include "Classes/DreamTask.h"
#include "Classes/DreamTaskComponent.h"
#include "DreamGameplayTaskTypes.h"

UDreamTaskComponent* UDreamGameplayTaskBlueprintLibrary::GetDreamTaskComponent(AActor* Actor)
{
	return Actor->FindComponentByClass<UDreamTaskComponent>();
}

TArray<UDreamTaskComponent*> UDreamGameplayTaskBlueprintLibrary::GetDreamTaskComponents(AActor* Actor)
{
	TArray<UDreamTaskComponent*> Result;
	Actor->GetComponents(UDreamTaskComponent::StaticClass(), Result);
	return Result;
}

TSubclassOf<UDreamTask> UDreamGameplayTaskBlueprintLibrary::GetDreamTaskClassByGUID(FGuid Guid)
{
	for (auto Element : GetDefault<UDreamGameplayTaskSetting>()->TaskMapping)
	{
		if (Element.Value == Guid)
		{
			return Element.Key;
		}
	}

	return nullptr;
}

FGuid UDreamGameplayTaskBlueprintLibrary::GetDreamTaskGuid(TSubclassOf<UDreamTask> InTaskClass)
{
	return GetDefault<UDreamGameplayTaskSetting>()->TaskMapping.FindRef(InTaskClass);
}

FDreamTaskSaveData UDreamGameplayTaskBlueprintLibrary::ConstructDreamGameplayTaskSaveData(TArray<UDreamTask*> Tasks)
{
	return FDreamTaskSaveData(Tasks);
}

TArray<UDreamTask*> UDreamGameplayTaskBlueprintLibrary::DestructDreamGameplayTaskSaveData(const FDreamTaskSaveData& Data)
{
	TArray<UDreamTask*> Result;

	for (auto Element : Data.SaveData)
	{
		if (UDreamTask* Task = UDreamTask::Create(GetDreamTaskClassByGUID(Element.TaskGuid), Element.TaskProgress))
		{
			Result.Add(Task);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("DreamGameplayTask: Failed to create task with guid %s"), *Element.TaskGuid.ToString());
		}
	}

	return Result;
}

UDreamTaskData* UDreamGameplayTaskBlueprintLibrary::GetTaskData(UDreamTask* InTask, TSubclassOf<UDreamTaskData> InTaskClass)
{
	if (InTaskClass)
	{
		return InTask->GetTaskData();
	}
	return nullptr;
}

TArray<UDreamTask*> UDreamGameplayTaskBlueprintLibrary::FilterTasksByType(const TArray<UDreamTask*>& Tasks, UDreamTaskType* TaskType)
{
	TArray<UDreamTask*> Result;
	for (UDreamTask* Task : Tasks)
	{
		if (Task->TaskType == TaskType)
		{
			Result.Add(Task);
		}
	}
	return Result;
}

TArray<UDreamTask*> UDreamGameplayTaskBlueprintLibrary::FilterTasksByPriority(const TArray<UDreamTask*>& Tasks, EDreamTaskPriority Priority)
{
	TArray<UDreamTask*> Result;
	for (UDreamTask* Task : Tasks)
	{
		if (Task->TaskPriority == Priority)
		{
			Result.Add(Task);
		}
	}
	return Result;
}

TArray<UDreamTask*> UDreamGameplayTaskBlueprintLibrary::FilterTasksByState(const TArray<UDreamTask*>& Tasks, EDreamTaskState State)
{
	TArray<UDreamTask*> Result;
	for (UDreamTask* Task : Tasks)
	{
		if (Task->TaskState == State)
		{
			Result.Add(Task);
		}
	}
	return Result;
}

TArray<FDreamTaskSpecHandle> UDreamGameplayTaskBlueprintLibrary::FilterHandlesByType(TArray<FDreamTaskSpecHandle>& Handles, UDreamTaskType* TaskType)
{
	TArray<FDreamTaskSpecHandle> Result;
	for (FDreamTaskSpecHandle& Handle : Handles)
	{
		if (Handle == TaskType)
			Result.Add(Handle);
	}

	return Result;
}

TArray<FDreamTaskSpecHandle> UDreamGameplayTaskBlueprintLibrary::FilterHandlesByPriority(TArray<FDreamTaskSpecHandle>& Handles, EDreamTaskPriority Priority)
{
	TArray<FDreamTaskSpecHandle> Result;
	for (FDreamTaskSpecHandle& Handle : Handles)
	{
		if (Handle.GetTask()->TaskPriority == Priority)
			Result.Add(Handle);
	}

	return Result;
}

TArray<FDreamTaskSpecHandle> UDreamGameplayTaskBlueprintLibrary::FilterHandlesByState(TArray<FDreamTaskSpecHandle>& Handles, EDreamTaskState State)
{
	TArray<FDreamTaskSpecHandle> Result;
	for (FDreamTaskSpecHandle& Handle : Handles)
	{
		if (Handle.GetTask()->TaskState == State)
			Result.Add(Handle);
	}

	return Result;
}

UDreamTask* UDreamGameplayTaskBlueprintLibrary::GetHandleTask(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetTask();
}

UDreamTaskConditionTemplate* UDreamGameplayTaskBlueprintLibrary::GetHandleTaskCondition(const FDreamTaskSpecHandle& Handle, FName ConditionName)
{
	if (UDreamTask* Task = GetHandleTask(Handle))
	{
		return GetConditionByName(Task->TaskCompletedCondition, ConditionName);
	}

	return nullptr;
}

UDreamTaskComponent* UDreamGameplayTaskBlueprintLibrary::GetHandleTaskOwnerComponent(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetOwnerComponent();
}

FGuid UDreamGameplayTaskBlueprintLibrary::GetHandleGuid(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetGuid();
}

EDreamTaskState UDreamGameplayTaskBlueprintLibrary::GetHandleState(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetTaskState();
}

FTimespan UDreamGameplayTaskBlueprintLibrary::GetHandleRunningTime(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetRunningTime();
}

FDateTime UDreamGameplayTaskBlueprintLibrary::GetHandleStartTime(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetStartTime();
}

FDateTime UDreamGameplayTaskBlueprintLibrary::GetHandleEndTime(const FDreamTaskSpecHandle& Handle)
{
	return Handle.GetEndTime();
}

TMap<FName, UDreamTaskConditionTemplate*>& UDreamGameplayTaskBlueprintLibrary::GetHandleConditions(FDreamTaskSpecHandle& Handle)
{
	return Handle.GetTaskConditions();
}

bool UDreamGameplayTaskBlueprintLibrary::IsHandleUseMaximumTime(const FDreamTaskSpecHandle& Handle)
{
	return Handle.IsUseMaximumTime();
}

bool UDreamGameplayTaskBlueprintLibrary::IsHandleTimeout(const FDreamTaskSpecHandle& Handle)
{
	return Handle.IsTimeout();
}

bool UDreamGameplayTaskBlueprintLibrary::IsHandleCompleted(const FDreamTaskSpecHandle& Handle)
{
	return Handle.IsCompleted();
}

bool UDreamGameplayTaskBlueprintLibrary::IsHandleFailed(const FDreamTaskSpecHandle& Handle)
{
	return Handle.IsFailed();
}

bool UDreamGameplayTaskBlueprintLibrary::IsHandleValid(const FDreamTaskSpecHandle& Handle)
{
	return Handle.IsValid();
}

void UDreamGameplayTaskBlueprintLibrary::SetHandleRunningTime(FDreamTaskSpecHandle& Handle, FTimespan InTime)
{
	Handle.SetRunningTime(InTime);
}

void UDreamGameplayTaskBlueprintLibrary::SetHandleStartTime(FDreamTaskSpecHandle& Handle, FDateTime InTime)
{
	Handle.SetStartTime(InTime);
}

void UDreamGameplayTaskBlueprintLibrary::SetHandleEndTime(FDreamTaskSpecHandle& Handle, FDateTime InTime)
{
	Handle.SetEndTime(InTime);
}

void UDreamGameplayTaskBlueprintLibrary::AddHandleTime(FDreamTaskSpecHandle& Handle, FTimespan InTime)
{
	Handle.AddTime(InTime);
}

void UDreamGameplayTaskBlueprintLibrary::AddHandleTimeWithSeconds(FDreamTaskSpecHandle& Handle, float InSeconds)
{
	Handle.AddTime(InSeconds);
}

void UDreamGameplayTaskBlueprintLibrary::UpdateHandle(FDreamTaskSpecHandle& Handle, float DeltaTime)
{
	Handle.Update(DeltaTime);
}

bool UDreamGameplayTaskBlueprintLibrary::EqualEqual_TaskHandleTaskHandle(const FDreamTaskSpecHandle& A, const FDreamTaskSpecHandle& B)
{
	return A == B;
}

bool UDreamGameplayTaskBlueprintLibrary::NotEqual_TaskHandleTaskHandle(const FDreamTaskSpecHandle& A, const FDreamTaskSpecHandle& B)
{
	return A != B;
}

bool UDreamGameplayTaskBlueprintLibrary::EqualEqual_TaskHandleTask(const FDreamTaskSpecHandle& A, UDreamTask* B)
{
	return A == B;
}

bool UDreamGameplayTaskBlueprintLibrary::NotEqual_TaskHandleTask(const FDreamTaskSpecHandle& A, UDreamTask* B)
{
	return A != B;
}

bool UDreamGameplayTaskBlueprintLibrary::EqualEqual_TaskHandleTaskName(const FDreamTaskSpecHandle& A, FName B)
{
	return A == B;
}

bool UDreamGameplayTaskBlueprintLibrary::NotEqual_TaskHandleTaskName(const FDreamTaskSpecHandle& A, FName B)
{
	return A != B;
}

bool UDreamGameplayTaskBlueprintLibrary::EqualEqual_TaskHandleTaskClass(const FDreamTaskSpecHandle& A, TSubclassOf<UDreamTask> B)
{
	return A == B;
}

bool UDreamGameplayTaskBlueprintLibrary::NotEqual_TaskHandleTaskClass(const FDreamTaskSpecHandle& A, TSubclassOf<UDreamTask> B)
{
	return A != B;
}

TArray<FDreamTaskSpecHandle> UDreamGameplayTaskBlueprintLibrary::FilterContainerHandlesByTaskType(FDreamTaskSpecHandleContainer& Container, UDreamTaskType* InTaskType)
{
	return Container.Filter(InTaskType);
}

TArray<FDreamTaskSpecHandle> UDreamGameplayTaskBlueprintLibrary::FilterContainerHandlesByTaskPriority(FDreamTaskSpecHandleContainer& Container, EDreamTaskPriority InPriority)
{
	return Container.Filter(InPriority);
}

TArray<FDreamTaskSpecHandle> UDreamGameplayTaskBlueprintLibrary::FilterContainerHandlesByTaskState(FDreamTaskSpecHandleContainer& Container, EDreamTaskState InState)
{
	return Container.Filter(InState);
}

TArray<FDreamTaskSpecHandle>& UDreamGameplayTaskBlueprintLibrary::GetContainerHandles(FDreamTaskSpecHandleContainer& Container)
{
	return Container.GetHandles();
}

FDreamTaskSpecHandle& UDreamGameplayTaskBlueprintLibrary::AddContainerHandle(FDreamTaskSpecHandleContainer& Container, FDreamTaskSpecHandle InHandle)
{
	return Container.AddHandle(InHandle);
}

bool UDreamGameplayTaskBlueprintLibrary::RemoveContainerHandle(FDreamTaskSpecHandleContainer& Container, const FDreamTaskSpecHandle& InHandle)
{
	return Container.RemoveHandle(InHandle);
}

const FDreamTaskSpecHandle& UDreamGameplayTaskBlueprintLibrary::FindContainerHandleByClass(FDreamTaskSpecHandleContainer& Container, UDreamTask* InTask)
{
	return Container.FindHandle(InTask);
}

const FDreamTaskSpecHandle& UDreamGameplayTaskBlueprintLibrary::FindContainerHandleByTaskClass(FDreamTaskSpecHandleContainer& Container, TSubclassOf<UDreamTask> InClass)
{
	return Container.FindHandle(InClass);
}

const FDreamTaskSpecHandle& UDreamGameplayTaskBlueprintLibrary::FindContainerHandleByTaskName(FDreamTaskSpecHandleContainer& Container, FName InName)
{
	return Container.FindHandle(InName);
}

int32 UDreamGameplayTaskBlueprintLibrary::FindContainerHandleIndex(FDreamTaskSpecHandleContainer& Container, const FDreamTaskSpecHandle& InHandle)
{
	return Container.FindHandleIndex(InHandle);
}

void UDreamGameplayTaskBlueprintLibrary::ClearContainerHandles(FDreamTaskSpecHandleContainer& Container)
{
	Container.ClearHandles();
}

int UDreamGameplayTaskBlueprintLibrary::SetContainerHandles(FDreamTaskSpecHandleContainer& Container, const TArray<FDreamTaskSpecHandle>& InHandles)
{
	return Container.SetHandles(InHandles);
}

TArray<UDreamTask*> UDreamGameplayTaskBlueprintLibrary::BuildContainerTaskArray(FDreamTaskSpecHandleContainer& Container)
{
	return Container.BuildTaskArray();
}

void UDreamGameplayTaskBlueprintLibrary::UpdateContainerHandles(FDreamTaskSpecHandleContainer& Container, float DeltaTime)
{
	Container.UpdateHandles(DeltaTime);
}

bool UDreamGameplayTaskBlueprintLibrary::IsContainerAllCompleted(FDreamTaskSpecHandleContainer& Container)
{
	return Container.IsAllCompleted();
}

bool UDreamGameplayTaskBlueprintLibrary::IsContainerSomeCompleted(FDreamTaskSpecHandleContainer& Container)
{
	return Container.IsSomeCompleted();
}

bool UDreamGameplayTaskBlueprintLibrary::IsContainerNoCompleted(FDreamTaskSpecHandleContainer& Container)
{
	return Container.IsNoCompleted();
}

bool UDreamGameplayTaskBlueprintLibrary::IsContainerEmpty(FDreamTaskSpecHandleContainer& Container)
{
	return Container.IsEmpty();
}

UDreamTaskConditionTemplate* UDreamGameplayTaskBlueprintLibrary::GetConditionByName(FDreamTaskConditionContainer& Container, FName InConditionName)
{
	return Container.GetConditionByName(InConditionName);
}

TArray<UDreamTaskConditionTemplate*> UDreamGameplayTaskBlueprintLibrary::GetConditions(FDreamTaskConditionContainer& Container)
{
	return Container.GetConditions();
}

TMap<FName, UDreamTaskConditionTemplate*>& UDreamGameplayTaskBlueprintLibrary::GetConditionMapping(FDreamTaskConditionContainer& Container)
{
	return Container.GetConditionMapping();
}

bool UDreamGameplayTaskBlueprintLibrary::UpdateConditionByName(FDreamTaskConditionContainer& Container, FName InConditionName)
{
	return Container.UpdateConditionByName(InConditionName);
}

int UDreamGameplayTaskBlueprintLibrary::ConditionCompletedCount(FDreamTaskConditionContainer& Container)
{
	return Container.ConditionCompletedCount();
}

bool UDreamGameplayTaskBlueprintLibrary::IsConditionsCompleted(FDreamTaskConditionContainer& Container)
{
	return Container.IsConditionsCompleted();
}

void UDreamGameplayTaskBlueprintLibrary::ResetConditionContainer(FDreamTaskConditionContainer& Container)
{
	Container.Reset();
}

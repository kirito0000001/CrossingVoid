#include "DreamGameplayTaskSpecHandle.h"

#include "DreamGameplayTaskLog.h"
#include "DreamGameplayTaskSetting.h"
#include "Classes/DreamTaskComponent.h"
#include "Classes/DreamTask.h"

FDreamTaskSpecHandle::FDreamTaskSpecHandle()
	: Guid()
	  , Task()
	  , OwnerComponent()
	  , StartTime()
	  , RunningTime(FTimespan::Zero())
	  , EndTime()
{
	Guid.Invalidate();
}

FDreamTaskSpecHandle::FDreamTaskSpecHandle(UDreamTask* InTask, FDateTime InStartTime)
	: Guid(FGuid::NewGuid())
	  , Task(InTask)
	  , OwnerComponent(InTask ? InTask->GetOwnerComponent() : nullptr)
	  , StartTime(InStartTime)
	  , RunningTime(FTimespan::Zero())
	  , EndTime(InStartTime)
{
	if (Task == nullptr)
	{
		DGT_LOG(Warning, TEXT("FDreamTaskSpecHandle constructed with null Task"));
	}
}

const FDreamTaskSpecHandle& FDreamTaskSpecHandle::InvalidHandle()
{
	static const FDreamTaskSpecHandle InvalidHandle = FDreamTaskSpecHandle();
	return InvalidHandle;
}


EDreamTaskState FDreamTaskSpecHandle::GetTaskState() const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->GetTaskState();
	}
	DGT_LOG(Warning, TEXT("GetTaskState called on invalid handle"));
	return EDreamTaskState::EDTS_Initialized;
}

FTimespan FDreamTaskSpecHandle::GetRunningTime() const
{
	return RunningTime;
}

FDateTime FDreamTaskSpecHandle::GetStartTime() const
{
	return StartTime;
}

FDateTime FDreamTaskSpecHandle::GetEndTime() const
{
	return EndTime;
}

FDateTime FDreamTaskSpecHandle::GetMaximumTime() const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		// 使用真实时间（秒）来计算最大完成时间
		return GetStartTime() + FTimespan::FromSeconds(TaskPtr->MaximumCompletionTimeSeconds);
	}
	DGT_LOG(Warning, TEXT("GetMaximumTime called on invalid handle"));
	return GetStartTime();
}

TMap<FName, UDreamTaskConditionTemplate*>& FDreamTaskSpecHandle::GetTaskConditions()
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->TaskCompletedCondition.GetConditionMapping();
	}
	static TMap<FName, UDreamTaskConditionTemplate*> EmptyMap;
	DGT_LOG(Warning, TEXT("GetTaskConditions called on invalid handle"));
	return EmptyMap;
}

TMap<FName, UDreamTaskConditionTemplate*>& FDreamTaskSpecHandle::GetTaskConditions() const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->TaskCompletedCondition.GetConditionMapping();
	}
	static TMap<FName, UDreamTaskConditionTemplate*> EmptyMap;
	DGT_LOG(Warning, TEXT("GetTaskConditions called on invalid handle"));
	return EmptyMap;
}

int32 FDreamTaskSpecHandle::GetTaskConditionsCount() const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->TaskCompletedCondition.GetConditionMapping().Num();
	}
	return 0;
}

bool FDreamTaskSpecHandle::IsUseMaximumTime() const
{
	if (UDreamTask* TaskPtr = GetTask())
	{
		return TaskPtr->bUseMaximumCompletionTime;
	}
	return false;
}

bool FDreamTaskSpecHandle::IsTimeout() const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		if (TaskPtr->bUseMaximumCompletionTime)
		{
			return FDateTime::Now() >= GetMaximumTime();
		}
	}
	return false;
}

bool FDreamTaskSpecHandle::IsCompleted() const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->IsCompleted();
	}
	return false;
}

bool FDreamTaskSpecHandle::IsFailed() const
{
	EDreamTaskState State = GetTaskState();
	return EnumHasAnyFlags(State, EDreamTaskState::EDTS_Failed);
}

bool FDreamTaskSpecHandle::IsValid() const
{
	return Guid.IsValid();
}

void FDreamTaskSpecHandle::SetRunningTime(FTimespan InTime)
{
	RunningTime = InTime;
}

void FDreamTaskSpecHandle::SetStartTime(FDateTime InTime)
{
	StartTime = InTime;
}

void FDreamTaskSpecHandle::SetEndTime(FDateTime InTime)
{
	EndTime = InTime;
}

void FDreamTaskSpecHandle::AddTime(float InSeconds)
{
	RunningTime += FTimespan::FromSeconds(InSeconds);
}

void FDreamTaskSpecHandle::AddTime(FTimespan InTime)
{
	RunningTime += InTime;
}

void FDreamTaskSpecHandle::Update(float DeltaTime)
{
	AddTime(DeltaTime);

	if (UDreamTask* TaskPtr = Task.Get())
	{
		if (TaskPtr->IsCompleted())
		{
			TaskPtr->SetTaskState(EDreamTaskState::EDTS_Completed);
			return;
		}

		if (TaskPtr->bUseMaximumCompletionTime && IsTimeout())
		{
			TaskPtr->SetTaskState(EDreamTaskState::EDTS_Timeout);
		}
	}
}

void FDreamTaskSpecHandle::Reset()
{
	RunningTime = FTimespan::Zero();
	StartTime = FDateTime::Now();
	EndTime = FDateTime();

	if (UDreamTask* TaskPtr = Task.Get())
	{
		TaskPtr->DelegateCall_TaskReset();
	}
}

bool FDreamTaskSpecHandle::operator==(const FDreamTaskSpecHandle& Other) const
{
	return Guid == Other.Guid;
}

bool FDreamTaskSpecHandle::operator==(const UDreamTask* Other) const
{
	if (!Other)
	{
		return false;
	}

	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr == Other;
	}
	return false;
}

bool FDreamTaskSpecHandle::operator==(const FName& InName) const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->GetTaskName() == InName;
	}
	return false;
}

bool FDreamTaskSpecHandle::operator==(const TSubclassOf<UDreamTask>& Class) const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->GetClass() == Class;
	}
	return false;
}

bool FDreamTaskSpecHandle::operator==(const UDreamTaskType* InTaskType) const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->GetTaskType() == InTaskType;
	}
	return false;
}

bool FDreamTaskSpecHandle::operator==(EDreamTaskState InState) const
{
	return GetTaskState() == InState;
}

bool FDreamTaskSpecHandle::operator==(EDreamTaskPriority InPriority) const
{
	if (UDreamTask* TaskPtr = Task.Get())
	{
		return TaskPtr->GetTaskPriority() == InPriority;
	}
	return false;
}

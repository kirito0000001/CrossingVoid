#include "DreamGameplayTaskSpecHandleContainer.h"

#include "Classes/DreamTask.h"
#include "DreamGameplayTaskSpecHandle.h"

FDreamTaskSpecHandleContainer::FDreamTaskSpecHandleContainer(const FDreamTaskSpecHandleContainer& Other)
{
	Handles = Other.Handles;
}

TArray<FDreamTaskSpecHandle>& FDreamTaskSpecHandleContainer::GetHandles()
{
	return Handles;
}

FDreamTaskSpecHandle& FDreamTaskSpecHandleContainer::AddHandle(const FDreamTaskSpecHandle& InHandle)
{
	FDreamTaskSpecHandle Handle = InHandle;
    
	Handle.StartTime = FDateTime::Now();
	Handle.Guid = FGuid::NewGuid();

	int32 AddedHandleIndex = Handles.Add(Handle);

	ChangeContainerState();

	return Handles[AddedHandleIndex];
}

bool FDreamTaskSpecHandleContainer::RemoveHandle(const FDreamTaskSpecHandle& InHandle)
{
	bool bRemoved = false;
	TArray<int32> IndicesToRemove;
	for (int32 Index = 0; Index < Handles.Num(); ++Index)
	{
		if (Handles[Index] == InHandle)
		{
			IndicesToRemove.Add(Index);
		}
	}
	// 倒序删除，避免索引错位
	for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
	{
		Handles.RemoveAt(IndicesToRemove[i]);
		bRemoved = true;
	}
	return bRemoved;
}

const FDreamTaskSpecHandle& FDreamTaskSpecHandleContainer::FindHandle(UDreamTask* InTask)
{
	FDreamTaskSpecHandle* Handle = GetHandles().FindByPredicate([InTask](const FDreamTaskSpecHandle& InHandle)
	{
		return InTask == InHandle;
	});

	return Handle ? *Handle : FDreamTaskSpecHandle::InvalidHandle();
}

FDreamTaskSpecHandle* FDreamTaskSpecHandleContainer::FindHandleMutable(UDreamTask* InTask)
{
	FDreamTaskSpecHandle* Handle = GetHandles().FindByPredicate([InTask](const FDreamTaskSpecHandle& InHandle)
	{
		return InTask == InHandle.GetTask();
	});

	return Handle;
}

const FDreamTaskSpecHandle& FDreamTaskSpecHandleContainer::FindHandle(TSubclassOf<UDreamTask> InClass)
{
	FDreamTaskSpecHandle* Handle = GetHandles().FindByPredicate([InClass](const FDreamTaskSpecHandle& InHandle)
	{
		return InClass == InHandle.GetTask()->GetClass();
	});

	return Handle ? *Handle : FDreamTaskSpecHandle::InvalidHandle();
}

FDreamTaskSpecHandle* FDreamTaskSpecHandleContainer::FindHandleMutable(TSubclassOf<UDreamTask> InClass)
{
	FDreamTaskSpecHandle* Handle = GetHandles().FindByPredicate([InClass](const FDreamTaskSpecHandle& InHandle)
	{
		return InClass == InHandle.GetTask()->GetClass();
	});

	return Handle;
}

const FDreamTaskSpecHandle& FDreamTaskSpecHandleContainer::FindHandle(FName InName)
{
	FDreamTaskSpecHandle* Handle = GetHandles().FindByPredicate([InName](const FDreamTaskSpecHandle& InHandle)
	{
		return InName == InHandle.GetTask()->GetTaskName();
	});

	return Handle ? *Handle : FDreamTaskSpecHandle::InvalidHandle();
}

FDreamTaskSpecHandle* FDreamTaskSpecHandleContainer::FindHandleMutable(FName InName)
{
	FDreamTaskSpecHandle* Handle = GetHandles().FindByPredicate([InName](const FDreamTaskSpecHandle& InHandle)
	{
		return InName == InHandle.GetTask()->GetTaskName();
	});

	return Handle;
}

int32 FDreamTaskSpecHandleContainer::FindHandleIndex(const FDreamTaskSpecHandle& InHandle)
{
	for (int i = 0; i < Handles.Num(); ++i)
	{
		if (Handles[i] == InHandle)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

TArray<FDreamTaskSpecHandle*> FDreamTaskSpecHandleContainer::GetUseMaximumTimeHandles()
{
	TArray<FDreamTaskSpecHandle*> Result;

	for (FDreamTaskSpecHandle& Handle : GetHandles())
	{
		if (Handle.IsUseMaximumTime())
		{
			Result.Add(&Handle);
		}
	}

	return Result;
}

void FDreamTaskSpecHandleContainer::ClearHandles()
{
	// Pre Remove
	for (auto Element : BuildTaskArray())
	{
		Element->DelegateCall_TaskRemoved();
	}

	// Clear
	Handles.Empty(); // Clear all task handles
	ContainerState = EDreamTaskSpecHandleContainerState::Empty; // Reset state to empty
}

int FDreamTaskSpecHandleContainer::SetHandles(const TArray<FDreamTaskSpecHandle>& InHandles)
{
	Handles = InHandles;
	return Handles.Num();
}

TArray<UDreamTask*> FDreamTaskSpecHandleContainer::BuildTaskArray()
{
	TArray<UDreamTask*> Result;

	for (auto DreamTask : GetHandles())
	{
		Result.Add(DreamTask.GetTask());
	}

	return Result;
}

// Component -> TaskSpecHandleContainer -> TaskSpecHandle
void FDreamTaskSpecHandleContainer::UpdateHandles(float DeltaTime)
{
	if (GetHandles().IsEmpty())
	{
		ContainerState = EDreamTaskSpecHandleContainerState::Empty;
		return;
	}

	int CompletedNumber = 0;
	int UseMaximumHandleNum = GetUseMaximumTimeHandles().Num();

	for (FDreamTaskSpecHandle& Handle : GetHandles())
	{
		Handle.Update(DeltaTime);

		if (Handle.IsCompleted())
		{
			CompletedNumber++;
		}
	}

	if (CompletedNumber >= UseMaximumHandleNum)
	{
		ContainerState = EDreamTaskSpecHandleContainerState::AllUseMaximumTimeCompleted;
	}
	else if (CompletedNumber >= GetHandles().Num())
	{
		ContainerState = EDreamTaskSpecHandleContainerState::AllCompleted;
	}
	else if (CompletedNumber > 0)
	{
		ContainerState = EDreamTaskSpecHandleContainerState::SomeCompleted;
	}
	else if (CompletedNumber == 0)
	{
		ContainerState = EDreamTaskSpecHandleContainerState::NoCompleted;
	}
}

void FDreamTaskSpecHandleContainer::ChangeContainerState()
{
	int CompletedCount = 0;
	for (FDreamTaskSpecHandle& Handle : GetHandles())
	{
		if (Handle.IsCompleted())
			CompletedCount++;
	}

	// Reset state based on the number of completed tasks
	if (CompletedCount == 0)
		ContainerState = EDreamTaskSpecHandleContainerState::NoCompleted;
	else if (CompletedCount == Handles.Num())
		ContainerState = EDreamTaskSpecHandleContainerState::AllCompleted;
	else
		ContainerState = EDreamTaskSpecHandleContainerState::SomeCompleted;
}

bool FDreamTaskSpecHandleContainer::IsAllCompleted() const
{
	return EnumHasAnyFlags(ContainerState, EDreamTaskSpecHandleContainerState::AllCompleted);
}

bool FDreamTaskSpecHandleContainer::IsAllUseMaximumTimeCompleted() const
{
	return EnumHasAnyFlags(ContainerState, EDreamTaskSpecHandleContainerState::AllUseMaximumTimeCompleted);
}

bool FDreamTaskSpecHandleContainer::IsSomeCompleted() const
{
	return EnumHasAnyFlags(ContainerState, EDreamTaskSpecHandleContainerState::SomeCompleted);
}

bool FDreamTaskSpecHandleContainer::IsNoCompleted() const
{
	return EnumHasAnyFlags(ContainerState, EDreamTaskSpecHandleContainerState::NoCompleted);
}

bool FDreamTaskSpecHandleContainer::IsEmpty() const
{
	return EnumHasAnyFlags(ContainerState, EDreamTaskSpecHandleContainerState::Empty);
}

TArray<FDreamTaskSpecHandle> FDreamTaskSpecHandleContainer::Filter(UDreamTaskType* InType)
{
	TArray<FDreamTaskSpecHandle> Result;
	Algo::CopyIf(GetHandles(), Result, [InType](const FDreamTaskSpecHandle& InHandle)
	{
		return InHandle == InType;
	});

	return Result;
}

TArray<FDreamTaskSpecHandle> FDreamTaskSpecHandleContainer::Filter(EDreamTaskPriority InPriority)
{
	TArray<FDreamTaskSpecHandle> Result;
	Algo::CopyIf(GetHandles(), Result, [InPriority](const FDreamTaskSpecHandle& InHandle)
	{
		return InHandle == InPriority;
	});
	return Result;
}

TArray<FDreamTaskSpecHandle> FDreamTaskSpecHandleContainer::Filter(EDreamTaskState InState)
{
	TArray<FDreamTaskSpecHandle> Result;
	Algo::CopyIf(GetHandles(), Result, [InState](const FDreamTaskSpecHandle& InHandle)
	{
		return InHandle == InState;
	});
	return Result;
}

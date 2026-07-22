#include "DreamGameplayTaskConditionContainer.h"

#include "DreamGameplayTaskLog.h"
#include "DreamGameplayTaskSetting.h"
#include "DreamGameplayTaskSpecHandle.h"
#include "Classes/DreamTaskConditionTemplate.h"

UDreamTaskConditionTemplate* FDreamTaskConditionContainer::GetConditionByName(FName InConditionName) const
{
	for (auto Element : Conditions)
	{
		if (Element.Key == InConditionName)
		{
			return Element.Value;
		}
	}

	return nullptr;
}

TArray<UDreamTaskConditionTemplate*> FDreamTaskConditionContainer::GetConditions()
{
	TArray<UDreamTaskConditionTemplate*> Result;
	Conditions.GenerateValueArray(Result);
	return Result;
}

TMap<FName, UDreamTaskConditionTemplate*>& FDreamTaskConditionContainer::GetConditionMapping()
{
	return Conditions;
}

bool FDreamTaskConditionContainer::UpdateConditionByName(FName InConditionName)
{
	UDreamTaskConditionTemplate* Condition = GetConditionByName(InConditionName);
	if (Condition)
	{
		Condition->Update();
		return true;
	}
	return false;
}

int FDreamTaskConditionContainer::ConditionCompletedCount() const
{
	int Count = 0;
	for (auto Element : Conditions)
	{
		DGT_DEBUG_LOG(Log, TEXT("Element.Value->IsCompleted() >>> %d.GetCount() >>> %d"), Element.Value->IsCompleted(), Element.Value->GetCount())
		
		if (Element.Value->IsCompleted())
		{
			Count++;
		}
		else
		{
			if (Element.Value->MustTaskBeCompleted())
				return TASK_CONDITION_NOT_COMPLETED;
		}
	}

	return Count;
}

bool FDreamTaskConditionContainer::IsConditionsCompleted() const
{
	int CompletedCount = ConditionCompletedCount();

	DGT_DEBUG_LOG(Log, TEXT("Count : %d"), CompletedCount);

	if (CompletedCount == TASK_CONDITION_NOT_COMPLETED)
	{
		return false;
	}

	DGT_DEBUG_LOG(Log, TEXT("Completed Mode : %s"),
	              *StaticEnum<EDreamTaskState>()->GetDisplayValueAsText(CompletionMode).ToString())

	switch (CompletionMode)
	{
	case EDreamTaskConditionalCompletionMode::EDTCCM_All:
		DGT_LOG(Log, TEXT("All -> %d "), CompletedCount == Conditions.Num());
		return CompletedCount == Conditions.Num();
		break;
	case EDreamTaskConditionalCompletionMode::EDTCCM_Any:
		DGT_DEBUG_LOG(Log, TEXT("Any -> %d "), CompletedCount > 0)
		return CompletedCount > 0;
		break;
	case EDreamTaskConditionalCompletionMode::EDTCCM_Custom:
		DGT_DEBUG_LOG(Log, TEXT("Custom -> %d"), CompletedCount >= CustomConditionCount)
		return CompletedCount >= CustomConditionCount;
		break;
	}

	DGT_DEBUG_LOG(Error, TEXT("Not Found"))

	return false;
}

void FDreamTaskConditionContainer::Reset()
{
	for (TPair<FName, UDreamTaskConditionTemplate*>& Condition : Conditions)
	{
		Condition.Value->Reset();
	}
}

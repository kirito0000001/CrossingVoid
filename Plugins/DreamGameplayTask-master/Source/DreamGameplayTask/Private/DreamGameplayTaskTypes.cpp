#include "DreamGameplayTaskTypes.h"

#include "DreamGameplayTaskBlueprintLibrary.h"
#include "DreamGameplayTaskLog.h"
#include "Classes/DreamTask.h"
#include "Classes/DreamTaskConditionTemplate.h"

FDreamTaskSaveSingle::FDreamTaskSaveSingle(const UDreamTask* Task)
{
	TaskGuid = UDreamGameplayTaskBlueprintLibrary::GetDreamTaskGuid(Task->StaticClass());
	TaskProgress = Task->GetTaskConditionProgress();
}

FDreamTaskSaveData::FDreamTaskSaveData(const TArray<UDreamTask*>& InSaveArray)
{
	SaveData.Empty();
	for (auto Element : InSaveArray)
	{
		SaveData.Add(FDreamTaskSaveSingle(Element));
	}
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "DreamGameplayTaskSetting.h"
#include "Classes/DreamTask.h"

UDreamGameplayTaskSetting::UDreamGameplayTaskSetting(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
}

UDreamGameplayTaskSetting* UDreamGameplayTaskSetting::Get()
{
	return GetMutableDefault<UDreamGameplayTaskSetting>();
}

bool UDreamGameplayTaskSetting::MappingHasTask(TSubclassOf<UDreamTask> InTask)
{
	for (auto Element : TaskMapping)
	{
		if (InTask == Element.Key)
		{
			return true;
		}
	}

	return false;
}

bool UDreamGameplayTaskSetting::MakeTaskMapping(UDreamTask* InTask)
{
	if (!MappingHasTask(InTask->GetClass()))
	{
		TaskMapping.Add(InTask->GetClass(), FGuid::NewGuid());
		SaveConfig();
		return true;
	}
	return false;
}

bool UDreamGameplayTaskSetting::GetEnableDebug()
{
	if (Get())
	{
		return Get()->bEnableDebug;
	}
	else
	{
		return false;
	}
}

bool UDreamGameplayTaskSetting::GetEnableUpdaterDebug()
{
	if (Get())
	{
		return Get()->bEnableUpdaterDebug;
	}
	else
	{
		return false;
	}
}

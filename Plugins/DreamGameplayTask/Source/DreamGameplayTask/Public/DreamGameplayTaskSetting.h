// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DreamGameplayTaskSetting.generated.h"

class UDreamTaskData;
class UDreamTask;

/**
 * Dream Gameplay Task Setting
 */
UCLASS(Config = DreamGameplayTask, DefaultConfig)
class DREAMGAMEPLAYTASK_API UDreamGameplayTaskSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDreamGameplayTaskSetting(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	static UDreamGameplayTaskSetting* Get();

public:
	virtual FName GetContainerName() const override { return FName(TEXT("Project")); }
	virtual FName GetCategoryName() const override { return FName(TEXT("DreamPlugin")); }
	virtual FName GetSectionName() const override { return FName(TEXT("TaskPluginSetting")); }

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, DisplayName = "TaskMapping")
	TMap<TSubclassOf<UDreamTask>, FGuid> TaskMapping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, DisplayName = "DebugTaskLifeTime")
	bool bDebugTaskLifeTime = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, DisplayName = "Enable Debug")
	bool bEnableDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, DisplayName = "Enable Updater Debug")
	bool bEnableUpdaterDebug = false;

public:
	bool MappingHasTask(TSubclassOf<UDreamTask> InTask);
	bool MakeTaskMapping(UDreamTask* InTask);
	static bool GetEnableDebug();
	static bool GetEnableUpdaterDebug();
};

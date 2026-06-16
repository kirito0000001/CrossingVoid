// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Classes/DreamTask.h"
#include "Engine/DeveloperSettings.h"
#include "DreamGameplayTaskEditorSetting.generated.h"

class UDreamTaskConditionTemplate;
class UDreamTask;
/**
 * 
 */
UCLASS(Config = DreamGameplayTaskEditor, DefaultConfig)
class DREAMGAMEPLAYTASKEDITOR_API UDreamGameplayTaskEditorSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDreamGameplayTaskEditorSetting(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	static UDreamGameplayTaskEditorSetting* Get();
	static void Register();
	static void Unregistered();

public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("DreamPlugin"); }
	virtual FName GetSectionName() const override { return TEXT("TaskPluginEditorSetting"); }

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Manager")
	TSoftClassPtr<UDreamTask> CreateTaskClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Manager")
	TSoftClassPtr<UDreamTaskConditionTemplate> CreateTaskConditionTemplateClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, meta=(LongPackageName), Category = "Manager")
	TArray<FDirectoryPath> TaskLoadPaths = {FDirectoryPath(TEXT("/Game"))};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Manager")
	bool bManagerStartupRefresh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Manager|Style")
	FSlateFontInfo ManagerFont;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Config, Category = "Version")
	FName ManagerVersion = FName(TEXT("3.0.0"));

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Config, Category = "Version")
	FName DebuggerVersion = FName(TEXT("1.0.0"));

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Config, Category = "Version")
	FName PluginVersion = FName(TEXT("3.0.0"));

public:
	TSubclassOf<UDreamTask> GetCreateTaskClass() const;
	TSubclassOf<UDreamTaskConditionTemplate> GetCreateTaskConditionTemplateClass() const;
};

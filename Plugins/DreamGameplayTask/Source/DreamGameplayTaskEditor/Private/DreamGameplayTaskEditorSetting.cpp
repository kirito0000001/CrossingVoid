// Fill out your copyright notice in the Description page of Project Settings.


#include "DreamGameplayTaskEditorSetting.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif
#include "Classes/DreamTask.h"
#include "Classes/DreamTaskConditionTemplate.h"

UDreamGameplayTaskEditorSetting::UDreamGameplayTaskEditorSetting(const FObjectInitializer& ObjectInitializer)
{
	CreateTaskClass = UDreamTask::StaticClass();
	CreateTaskConditionTemplateClass = UDreamTaskConditionTemplate::StaticClass();
	ManagerFont = FAppStyle::GetFontStyle(TEXT("BoldFont"));
}

UDreamGameplayTaskEditorSetting* UDreamGameplayTaskEditorSetting::Get()
{
	return GetMutableDefault<UDreamGameplayTaskEditorSetting>();
}

void UDreamGameplayTaskEditorSetting::Register()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// Register Setting Page
		SettingsModule->RegisterSettings(
			"Editor",
			"DreamPlugin",
			"TaskPluginEditorSetting",
			FText::FromString(TEXT("Task Editor Settings")),
			FText::FromString(TEXT("Task Editor Settings")),
			GetMutableDefault<UDreamGameplayTaskEditorSetting>()
		);
	}
#endif
}

void UDreamGameplayTaskEditorSetting::Unregistered()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "DreamPlugin", "TaskPluginEditorSetting");
	}
#endif
}

TSubclassOf<UDreamTask> UDreamGameplayTaskEditorSetting::GetCreateTaskClass() const
{
	return CreateTaskClass.LoadSynchronous();
}

TSubclassOf<UDreamTaskConditionTemplate> UDreamGameplayTaskEditorSetting::GetCreateTaskConditionTemplateClass() const
{
	return CreateTaskConditionTemplateClass.LoadSynchronous();
}

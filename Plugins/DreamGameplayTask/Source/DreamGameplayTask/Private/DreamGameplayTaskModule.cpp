// Copyright Epic Games, Inc. All Rights Reserved.

#include "DreamGameplayTaskModule.h"

#if WITH_EDITOR
#include "DreamGameplayTaskSetting.h"
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FDreamGameplayTaskModule"

void FDreamGameplayTaskModule::StartupModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project",
			"DreamPlugin",
			"TaskPluginSetting",
			LOCTEXT("DreamGameplayTask", "Task Plugin Settings"),
			LOCTEXT("DreamGameplayTask", "Task Plugin Settings"),
			GetMutableDefault<UDreamGameplayTaskSetting>()
		);
	}
#endif
}

void FDreamGameplayTaskModule::ShutdownModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings(
			"Project",
			"DreamPlugin",
			"TaskPluginSetting"
		);
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDreamGameplayTaskModule, DreamGameplayTask)
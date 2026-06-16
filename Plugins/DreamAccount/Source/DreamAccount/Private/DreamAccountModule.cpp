// Copyright 2025 Dream Moon. All Rights Reserved.

#include "DreamAccountModule.h"

#include "DreamAccountSettings.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FDreamAccountModule"

void FDreamAccountModule::StartupModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project",
			"DreamPlugin",
			"DreamAccount",
			LOCTEXT("DreamAccount", "Account Plugin Settings"),
			LOCTEXT("DreamAccount", "Account Plugin Settings"),
			GetMutableDefault<UDreamAccountSettings>()
		);
	}
#endif
}

void FDreamAccountModule::ShutdownModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings(
			"Project",
			"DreamPlugin",
			"DreamAccount");
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDreamAccountModule, DreamAccount)

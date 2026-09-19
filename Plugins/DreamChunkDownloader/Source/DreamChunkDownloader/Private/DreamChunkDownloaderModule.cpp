// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownloaderModule.h"

#include "DreamChunkDownloaderSettings.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FDreamChunkDownloaderModule"

void FDreamChunkDownloaderModule::StartupModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		SettingsModule->RegisterSettings(
			TEXT("Project"),
			TEXT("DreamPlugin"),
			TEXT("ChunkDownloaderSetting"),
			LOCTEXT("Setting_DisplayName", "梦想分片下载器设置（Dream Chunk Downloader）"),
			LOCTEXT("Setting_Description", "分片（热更）的开关、清单、远程地址与缓存设置"),
			UDreamChunkDownloaderSettings::Get());
	}
#endif
}

void FDreamChunkDownloaderModule::ShutdownModule()
{
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		SettingsModule->UnregisterSettings(
			TEXT("Project"),
			TEXT("DreamPlugin"),
			TEXT("ChunkDownloaderSetting"));
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDreamChunkDownloaderModule, DreamChunkDownloader)

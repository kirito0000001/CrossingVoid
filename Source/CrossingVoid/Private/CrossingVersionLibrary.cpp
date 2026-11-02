// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrossingVersionLibrary.h"

#include "Misc/ConfigCacheIni.h"   // GConfig / GGameIni

namespace
{
	// 内容版本所在的段/键（由 CrossingChunkEditor 面板在打包时写入 DefaultGame.ini）
	const TCHAR* GContentVersionSection = TEXT("CrossingVersion");
	const TCHAR* GContentVersionKey     = TEXT("GameVersion");

	// 引擎网络兼容性用的那一个（参与 NetCL 的 CRC，别当日常发版号用）
	const TCHAR* GNetworkVersionSection = TEXT("/Script/EngineSettings.GeneralProjectSettings");
	const TCHAR* GNetworkVersionKey     = TEXT("ProjectVersion");
}

FString UCrossingVersionLibrary::GetGameVersion()
{
	FString Version;
	if (GConfig && GConfig->GetString(GContentVersionSection, GContentVersionKey, Version, GGameIni) && !Version.IsEmpty())
	{
		return Version;
	}

	// 兼容没有 [CrossingVersion] 的老包：退回读网络兼容版本
	GConfig->GetString(GNetworkVersionSection, GNetworkVersionKey, Version, GGameIni);
	return Version;
}

FString UCrossingVersionLibrary::GetNetworkCompatibleVersion()
{
	FString Version;
	GConfig->GetString(GNetworkVersionSection, GNetworkVersionKey, Version, GGameIni);
	return Version;
}

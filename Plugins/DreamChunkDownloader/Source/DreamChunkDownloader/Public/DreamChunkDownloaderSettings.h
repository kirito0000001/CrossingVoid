// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DreamChunkDownloaderSettings.generated.h"

struct FDreamChunkDownloaderDeploymentSet;
enum class EDreamChunkDownloaderCacheLocation : uint8;

/**
 * Dream Chunk Downloader Settings
 * 
 * This class provides project-wide configuration settings for the Dream Chunk Downloader plugin.
 * It inherits from UDeveloperSettings which allows these settings to be exposed in the
 * Unreal Editor's Project Settings UI under the specified category.
 * 
 * The settings control various aspects of chunk downloading including:
 * - Which chunks to download
 * - CDN configuration
 * - Cache storage locations
 * - Download concurrency limits
 * - Manifest file names
 * 
 * Settings are stored in the DreamChunkDownloader config file and can be modified
 * both in the editor and at runtime.
 */
UCLASS(DefaultConfig, Config=DreamChunkDownloader)
class DREAMCHUNKDOWNLOADER_API UDreamChunkDownloaderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Get the container name for these settings in the project settings UI
	 * @return The container name (Project)
	 */
	virtual FName GetContainerName() const override { return FName(TEXT("Project")); }

	/**
	 * Get the category name for these settings in the project settings UI
	 * @return The category name (DreamPlugin)
	 */
	virtual FName GetCategoryName() const override { return FName(TEXT("DreamPlugin")); }

	/**
	 * Get the section name for these settings in the project settings UI
	 * @return The section name (ChunkDownloaderSetting)
	 */
	virtual FName GetSectionName() const override { return FName(TEXT("ChunkDownloaderSetting")); }

public:
	/**
	 * Whether to use static remote host for getting chunk download list and build ID
	 * If enabled, the plugin will use a static remote host instead of getting these
	 * values from the manifest file.
	 * 
	 * When enabled, you need to add "download-chunk-id-list" and "client-build-id" 
	 * fields to your manifest.json file.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bUseStaticRemoteHost = false;

	/**
	 * Static remote host URL for getting build ID and download chunk IDs
	 * Used when bUseStaticRemoteHost is enabled.
	 * 
	 * Example: "https://example.com/data/"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings", Meta = (EditCondition = "bUseStaticRemoteHost"))
	FString StaticRemoteHost = "sample.com/data/";

	/**
	 * List of chunk IDs to download
	 * Used when bUseStaticRemoteHost is disabled.
	 * 
	 * These are the chunk IDs that will be downloaded and mounted by the plugin.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings", Meta = (EditCondition = "!bUseStaticRemoteHost"))
	TArray<int> DownloadChunkIds;

	/**
	 * Build ID for content versioning
	 * Used when bUseStaticRemoteHost is disabled.
	 * 
	 * This ID is used to ensure clients download the correct version of content.
	 * Format: Usually follows semantic versioning like "1.2.3" or timestamp format.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings", Meta = (EditCondition = "!bUseStaticRemoteHost"))
	FString BuildID = TEXT("0.0.0");

	/**
	 * Location for storing cached chunk files
	 * 
	 * Determines where downloaded pak files are stored on the user's device.
	 * Note: This setting currently only affects Windows platform.
	 * 
	 * Options:
	 * - User: Store in user-specific directory (e.g. Saved folder)
	 * - Game: Store in game installation directory
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDreamChunkDownloaderCacheLocation CacheFolderPath;

	/**
	 * Maximum number of concurrent downloads
	 * 
	 * Controls how many chunks can be downloaded simultaneously to avoid
	 * overwhelming the network connection or server.
	 * 
	 * Minimum value is 1. Higher values may improve download speed but
	 * increase network and system resource usage.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int MaxConcurrentDownloads = 5;

	/**
	 * Deployment-specific CDN configurations
	 * 
	 * Allows configuration of different CDN hosts for different platforms.
	 * Each deployment set contains a platform name and a list of CDN URLs.
	 * 
	 * Example configurations:
	 * - Deployment: Windows / Hosts: [ "https://cdn1.example.com/windows/", "https://cdn2.example.com/windows/" ]
	 * - Deployment: Android / Hosts: [ "https://cdn1.example.com/android/" ]
	 * 
	 * Supported platforms: Windows, Android, IOS, Mac, Linux
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FDreamChunkDownloaderDeploymentSet> DeploymentSets;

	/**
	 * Name of the embedded manifest file
	 * 
	 * This file contains information about chunks that are shipped with the
	 * game build (embedded chunks). The file is typically located in the
	 * game's content directory.
	 * 
	 * Default: "EmbeddedManifest.json"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "File")
	FString EmbeddedManifestFileName = "EmbeddedManifest.json";

	/**
	 * Name of the local manifest file
	 * 
	 * This file tracks the state of locally cached chunks on the user's device.
	 * It is updated as chunks are downloaded, mounted, or removed.
	 * 
	 * Default: "LocalManifest.json"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "File")
	FString LocalManifestFileName = "LocalManifest.json";

	/**
	 * Name of the cached build manifest file
	 * 
	 * This file contains the complete manifest for the current build downloaded
	 * from the CDN. It defines all available chunks and their properties.
	 * 
	 * Default: "CachedBuildManifest.json"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "File")
	FString CachedBuildManifestFileName = "CachedBuildManifest.json";

public:
	/**
	 * Get the singleton instance of the settings
	 * @return Pointer to the settings instance
	 */
	static UDreamChunkDownloaderSettings* Get();
};

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
UCLASS(DefaultConfig, Config=DreamChunkDownloader,
	meta = (DisplayName = "梦想分片下载器设置",
		ToolTip = "分片（热更）清单、远程地址与缓存相关的项目设置。"))
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
	 * 总开关（2026-09-20 新增，默认关闭）
	 *
	 * 关闭时：初始化阶段不读本地/内置清单、不联网下载 BuildManifest、不下载也不挂载任何分片；
	 * 蓝图入口 StartPatchGame / StartPatchGameWithDelegate 直接返回失败并广播失败事件。
	 *
	 * 之所以默认关闭：服务端启动时会跑这套逻辑，清单在 CDN/GitHub 上取不到（404）就会按
	 * 5s→10s→20s… 退避重试最多 10 次，实测把"端口开始监听"拖到 3 分钟之后。最近用不到热更，
	 * 所以先整体关掉，需要时在项目设置里打开即可。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "启用分片下载器（总开关）",
			ToolTip = "关掉后：不读清单、不联网、不下载/挂载分片，蓝图入口 StartPatchGame 直接返回失败。默认关闭。"))
	bool bEnableChunkDownloader = false;

	/**
	 * Whether to use static remote host for getting chunk download list and build ID
	 * If enabled, the plugin will use a static remote host instead of getting these
	 * values from the manifest file.
	 * 
	 * When enabled, you need to add "download-chunk-id-list" and "client-build-id" 
	 * fields to your manifest.json file.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "使用固定远程地址",
			ToolTip = "开启后不读清单里的地址，直接用下面那一项「固定远程地址」。"))
	bool bUseStaticRemoteHost = false;

	/**
	 * Static remote host URL for getting build ID and download chunk IDs
	 * Used when bUseStaticRemoteHost is enabled.
	 * 
	 * Example: "https://example.com/data/"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "固定远程地址",
			ToolTip = "示例：https://example.com/data/ 。仅在「使用固定远程地址」打开时生效。",
			EditCondition = "bUseStaticRemoteHost"))
	FString StaticRemoteHost = "sample.com/data/";

	/**
	 * List of chunk IDs to download
	 * Used when bUseStaticRemoteHost is disabled.
	 * 
	 * These are the chunk IDs that will be downloaded and mounted by the plugin.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "要下载的分片 ID 列表",
			ToolTip = "这些分片会被下载并挂载。仅在「使用固定远程地址」关闭时生效。",
			EditCondition = "!bUseStaticRemoteHost"))
	TArray<int> DownloadChunkIds;

	/**
	 * Build ID for content versioning
	 * Used when bUseStaticRemoteHost is disabled.
	 * 
	 * This ID is used to ensure clients download the correct version of content.
	 * Format: Usually follows semantic versioning like "1.2.3" or timestamp format.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "构建版本号（Build ID）",
			ToolTip = "内容版本号，用来确保客户端下载到对应版本的分片，例如 1.2.3。仅在「使用固定远程地址」关闭时生效。",
			EditCondition = "!bUseStaticRemoteHost"))
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
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "缓存目录位置",
			ToolTip = "下载下来的 pak 存哪儿：用户目录（Saved）或游戏安装目录。目前只对 Windows 生效。"))
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
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "最大同时下载数",
			ToolTip = "同时下载几个分片，最小 1。调大能更快，但更吃带宽和机器资源。"))
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
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings",
		Meta = (DisplayName = "部署集合（Deployment Sets）",
			ToolTip = "按平台配置 CDN 地址，例如 Windows 用 cdn1/cdn2，Android 用 cdn3。支持 Windows、Android、IOS、Mac、Linux。"))
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
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "File",
		Meta = (DisplayName = "内置清单文件名",
			ToolTip = "随包发布的内置分片清单，通常在 Content 目录下。默认 EmbeddedManifest.json。"))
	FString EmbeddedManifestFileName = "EmbeddedManifest.json";

	/**
	 * Name of the local manifest file
	 * 
	 * This file tracks the state of locally cached chunks on the user's device.
	 * It is updated as chunks are downloaded, mounted, or removed.
	 * 
	 * Default: "LocalManifest.json"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "File",
		Meta = (DisplayName = "本地清单文件名",
			ToolTip = "记录本机已缓存分片状态的清单，下载/挂载/删除时会更新。默认 LocalManifest.json。"))
	FString LocalManifestFileName = "LocalManifest.json";

	/**
	 * Name of the cached build manifest file
	 * 
	 * This file contains the complete manifest for the current build downloaded
	 * from the CDN. It defines all available chunks and their properties.
	 * 
	 * Default: "CachedBuildManifest.json"
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "File",
		Meta = (DisplayName = "缓存清单文件名",
			ToolTip = "从 CDN 下载回来的完整构建清单，定义了所有可用分片及其属性。默认 CachedBuildManifest.json。"))
	FString CachedBuildManifestFileName = "CachedBuildManifest.json";

public:
	/**
	 * Get the singleton instance of the settings
	 * @return Pointer to the settings instance
	 */
	static UDreamChunkDownloaderSettings* Get();
};

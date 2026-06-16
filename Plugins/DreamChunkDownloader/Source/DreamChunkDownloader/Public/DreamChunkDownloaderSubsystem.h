// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamChunkDownloaderTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "DreamChunkDownloaderSubsystem.generated.h"

class FDreamChunkDownloaderPlatformWrapper;
class FDreamChunkDownload;
class IHttpRequest;
class IFileManager;
class FJsonObject;
class FJsonValue;


// Delegate for chunk mount events - called when a chunk is mounted/unmounted
DECLARE_MULTICAST_DELEGATE_TwoParams(FDreamPlatformChunkInstallMultiDelegate, uint32, bool);

// Internal callback delegate for chunk downloader operations
DECLARE_MULTICAST_DELEGATE_OneParam(FDreamChunkDownloaderInternalCallback, bool);

// Blueprint callable delegate for chunk downloader operations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDreamChunkDownloaderCallback, bool, bSuccess);

// Blueprint dynamic delegate for event-based callbacks
DECLARE_DYNAMIC_DELEGATE_OneParam(FDreamChunkDownloaderCallbackEvent, bool, bSuccess);

/**
 * Chunk Downloader Subsystem
 * 
 * This subsystem handles downloading, caching, and mounting of game content chunks (pak files).
 * It manages the entire lifecycle of chunked content including:
 * - Downloading chunks from CDN
 * - Caching downloaded content to disk
 * - Mounting/unmounting pak files
 * - Tracking chunk status and progress
 * - Handling manifest updates
 * 
 * The system is designed to work with chunked content distribution where game content
 * is split into multiple downloadable packages that can be loaded on-demand.
 */
UCLASS()
class DREAMCHUNKDOWNLOADER_API UDreamChunkDownloaderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private:
	// Allow platform wrapper and download classes to access private members
	friend FDreamChunkDownloaderPlatformWrapper;
	friend FDreamChunkDownload;

public:
	/**
	 * Destructor - Ensures all pak files are cleaned up
	 */
	~UDreamChunkDownloaderSubsystem() override;

	/**
	 * Initialize the subsystem
	 * @param Collection The subsystem collection
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Deinitialize the subsystem
	 */
	virtual void Deinitialize() override;

public:
	/**
	 * Final cleanup of the subsystem - unmounts all chunks and cancels downloads
	 */
	void Finalize();

	/**
	 * Process local pak files found on disk
	 * @param LocalManifest The manifest entries to process
	 * @param FileManager File manager instance for file operations
	 */
	void ProcessLocalPakFiles(const TArray<FDreamPakFileEntry>& LocalManifest, IFileManager& FileManager);

	/**
	 * Setup the content build ID from either remote manifest or settings
	 * @param JsonObject The parsed manifest JSON object
	 */
	void SetupBuildId(const TSharedPtr<FJsonObject>& JsonObject);

	/**
	 * Setup the chunk download list from either remote manifest or settings
	 * @param JsonObject The parsed manifest JSON object
	 */
	void SetupChunkDownloadList(const TSharedPtr<FJsonObject>& JsonObject);

	/**
	 * Check if the system is ready for patching operations
	 * @return True if manifest is up to date and all required chunks are available
	 */
	bool IsReadyForPatching() const;

	/**
	 * Create a default local manifest file when none exists
	 */
	void CreateDefaultLocalManifest();

	/**
	 * Validate that a manifest file exists and is properly formatted
	 * @param ManifestPath Path to the manifest file
	 * @param OutErrorMessage Output error message if validation fails
	 * @return True if manifest is valid
	 */
	bool ValidateManifestFile(const FString& ManifestPath, FString& OutErrorMessage);

	/**
	 * Load a cached build manifest if it exists and matches current build ID
	 * @param DeploymentName Name of the deployment
	 * @return True if valid cached build was loaded
	 */
	bool LoadCachedBuild(const FString& DeploymentName);

	/**
	 * Update the build manifest from CDN
	 * @param InDeploymentName Name of the deployment
	 * @param InContentBuildId Build ID to update to
	 * @param OnCallback Callback to execute when update completes
	 */
	void UpdateBuild(const FString& InDeploymentName, const FString& InContentBuildId, const FDreamChunkDownloaderTypes::FDreamCallback OnCallback);

	/**
	 * Mount multiple chunks by ID
	 * @param ChunkIds Array of chunk IDs to mount
	 * @param OnCallback Callback to execute when mounting completes
	 */
	void MountChunks(const TArray<int32>& ChunkIds, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback);

	/**
	 * Mount a single chunk by ID
	 * @param ChunkId ID of the chunk to mount
	 * @param OnCallback Callback to execute when mounting completes
	 */
	void MountChunk(int32 ChunkId, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback);

	/**
	 * Download multiple chunks by ID
	 * @param ChunkIds Array of chunk IDs to download
	 * @param OnCallback Callback to execute when download completes
	 * @param Priority Download priority (higher values = higher priority)
	 */
	void DownloadChunks(const TArray<int32>& ChunkIds, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback, int32 Priority);

	/**
	 * Download a single chunk by ID
	 * @param ChunkId ID of the chunk to download
	 * @param OnCallback Callback to execute when download completes
	 * @param Priority Download priority (higher values = higher priority)
	 */
	void DownloadChunk(int32 ChunkId, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback, int32 Priority);

	/**
	 * Validate that all required chunks are available in the manifest
	 */
	void ValidateChunksAvailability();

	/**
	 * Flush (delete) all cached chunk files
	 * @return Number of files that could not be deleted
	 */
	int32 FlushCache();

	/**
	 * Validate integrity of cached files
	 * @return Number of invalid files found
	 */
	int ValidateCache();

	/**
	 * Begin loading mode to track download/mount progress
	 * @param OnCallback Callback to execute when loading completes
	 */
	void BeginLoadingMode(const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback);

	/**
	 * Delegate called when a chunk is mounted or unmounted
	 */
	FDreamPlatformChunkInstallMultiDelegate OnChunkMounted;

public:
	/**
	 * Start the patching process to download and mount required chunks
	 * @param InManifestFileDownloadHostIndex Index of CDN host to use
	 * @return True if patching started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamChunkDownloader")
	bool StartPatchGame(int InManifestFileDownloadHostIndex = 0);

	/**
	 * Start the patching process with blueprint delegates
	 * @param InManifestFileDownloadHostIndex Index of CDN host to use
	 * @param InOnPatchCompletedEvent Delegate called when patching completes
	 * @param InOnMountCompletedEvent Delegate called when mounting completes
	 * @return True if patching started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamChunkDownloader")
	bool StartPatchGameWithDelegate(int InManifestFileDownloadHostIndex,
	                                const FDreamChunkDownloaderCallbackEvent& InOnPatchCompletedEvent,
	                                const FDreamChunkDownloaderCallbackEvent& InOnMountCompletedEvent);

protected:
	/**
	 * Handle completion of download operations
	 * @param bSuccess Whether downloads completed successfully
	 */
	void HandleDownloadCompleted(bool bSuccess);

	/**
	 * Handle completion of loading mode
	 * @param bSuccess Whether loading completed successfully
	 */
	void HandleLoadingModeCompleted(bool bSuccess);

	/**
	 * Handle completion of mount operations
	 * @param bSuccess Whether mounting completed successfully
	 */
	void HandleMountCompleted(bool bSuccess);

public:
	/**
	 * Get the current status of a chunk
	 * @param ChunkId ID of the chunk to check
	 * @return Current status of the chunk
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	EDreamChunkStatus GetChunkStatus(int32 ChunkId) const;

	/**
	 * Get all known chunk IDs
	 * @param ChunkIds Output array of chunk IDs
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	void GetAllChunkIds(TArray<int32>& ChunkIds) const;

	/**
	 * Get the number of active download requests
	 * @return Number of download requests
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	int32 GetNumDownloadRequests() const
	{
		return DownloadRequests.Num();
	}

	/**
	 * Get the current patching progress as a percentage
	 * @return Progress value between 0.0 and 1.0
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	float GetPatchProgress() const;

	/**
	 * Get the cache folder path
	 * @return Path to the cache folder
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	FORCEINLINE FString GetCacheFolder() const
	{
		return CacheFolder;
	}

	/**
	 * Get the base URLs for content downloads
	 * @return Array of base URLs
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	FORCEINLINE TArray<FString>& GetBuildBaseUrls()
	{
		return BuildBaseUrls;
	}

	/**
	 * Get loading statistics
	 * @return Reference to loading stats structure
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	FORCEINLINE FDreamChunkDownloaderStats& GetStats()
	{
		return LoadingModeStats;
	}

	/**
	 * Get the current content build ID
	 * @return Current build ID
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	FORCEINLINE FString GetContentBuildId() const
	{
		return ContentBuildId;
	}

	/**
	 * Get the current deployment name
	 * @return Current deployment name
	 */
	UFUNCTION(BlueprintPure, Category = "DreamChunkDownloader")
	FORCEINLINE FString GetDeploymentName() const
	{
		return LastDeploymentName;
	}

	/**
	 * Whether the download manifest is up to date
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	bool bIsDownloadManifestUpToDate = false;

	/**
	 * List of chunk IDs that should be downloaded
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	TArray<int> ChunkDownloadList;

	/**
	 * Event called when patching completes
	 */
	UPROPERTY(BlueprintAssignable, Category = "DreamChunkDownloader")
	FDreamChunkDownloaderCallback OnPatchCompleted;

	/**
	 * Event called when mounting completes
	 */
	UPROPERTY(BlueprintAssignable, Category = "DreamChunkDownloader")
	FDreamChunkDownloaderCallback OnMountCompleted;

	/**
	 * Internal callback for patch completion
	 */
	FDreamChunkDownloaderInternalCallback OnPatchCompletedInternal;

	/**
	 * Internal callback for mount completion
	 */
	FDreamChunkDownloaderInternalCallback OnMountCompletedInternal;

	/**
	 * Analytics delegate for download events
	 */
	FDreamChunkDownloaderTypes::FDreamDownloadAnalytics OnDownloadAnalytics;

	/**
	 * Get reference to pak files map
	 * @return Reference to pak files map
	 */
	TMap<FString, TSharedRef<FDreamPakFile>>& GetPakFiles()
	{
		return PakFiles;
	}

	/**
	 * Get reference to download requests array
	 * @return Reference to download requests array
	 */
	TArray<TSharedRef<FDreamPakFile>>& GetDownloadRequests()
	{
		return DownloadRequests;
	}

protected:
	/** Loading statistics and progress tracking */
	FDreamChunkDownloaderStats LoadingModeStats;

	/** Callbacks to execute after loading completes */
	TArray<FDreamChunkDownloaderTypes::FDreamCallback> PostLoadCallbacks;

	/** Counter to ensure loading completion is stable */
	int32 LoadingCompleteLatch = 0;

	/** Callback to execute when build update completes */
	FDreamChunkDownloaderTypes::FDreamCallback UpdateBuildCallback;

	/** Platform name (determines the manifest) */
	FString PlatformName;

	/** Folder to save pak files into on disk */
	FString CacheFolder;

	/** Content folder where we can find some chunks shipped with the build */
	FString EmbeddedFolder;

	/** Last deployment name */
	FString LastDeploymentName;

	/** Current content build ID */
	FString ContentBuildId;

	/** Base URLs for content downloads */
	TArray<FString> BuildBaseUrls;

	/** Map of chunk ID to chunk record */
	TMap<int32, TSharedRef<FDreamChunk>> Chunks;

	/** Map of pak file name to pak file record */
	TMap<FString, TSharedRef<FDreamPakFile>> PakFiles;

	/** Pak files embedded in the build (immutable, compressed) */
	TMap<FString, FDreamPakFileEntry> EmbeddedPaks;

	/** Whether we need to save the manifest (done whenever new downloads have started) */
	bool bNeedsManifestSave = false;

	/** Handle for the per-frame mount ticker in the main thread */
	FTSTicker::FDelegateHandle MountTicker;

	/** Manifest download request */
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ManifestRequest;

	/** Maximum number of downloads to allow concurrently */
	int32 TargetDownloadsInFlight = 1;

	/** List of pak files that have been requested */
	TArray<TSharedRef<FDreamPakFile>> DownloadRequests;

private:
	/**
	 * Set the content build ID and update base URLs
	 * @param DeploymentName Name of the deployment
	 * @param NewContentBuildId New build ID
	 */
	void SetContentBuildId(const FString& DeploymentName, const FString& NewContentBuildId);

	/**
	 * Load a manifest of pak files
	 * @param ManifestPakFiles Array of pak file entries
	 */
	void LoadManifest(const TArray<FDreamPakFileEntry>& ManifestPakFiles);

	/**
	 * Try to load the build manifest, with retry logic
	 * @param TryNumber Current attempt number
	 */
	void TryLoadBuildManifest(int TryNumber);

	/**
	 * Try to download the build manifest from CDN
	 * @param TryNumber Current attempt number
	 */
	void TryDownloadBuildManifest(int TryNumber);

	/**
	 * Wait for all mount operations to complete
	 */
	void WaitForMounts();

	/**
	 * Save the local manifest file
	 * @param bForce Whether to force save even if not needed
	 */
	void SaveLocalManifest(bool bForce);

	/**
	 * Update loading mode state
	 * @return True if loading should continue
	 */
	bool UpdateLoadingMode();

	/**
	 * Compute loading statistics
	 */
	void ComputeLoadingStats();

	/**
	 * Unmount a pak file
	 * @param PakFile Pak file to unmount
	 */
	void UnmountPakFile(const TSharedRef<FDreamPakFile>& PakFile);

	/**
	 * Cancel a download operation
	 * @param PakFile Pak file whose download to cancel
	 * @param bResult Result to report for the cancelled download
	 */
	void CancelDownload(const TSharedRef<FDreamPakFile>& PakFile, bool bResult);

	/**
	 * Internal function to download a pak file
	 * @param PakFile Pak file to download
	 * @param Callback Callback to execute when download completes
	 * @param Priority Download priority
	 */
	void DownloadPakFileInternal(const TSharedRef<FDreamPakFile>& PakFile, const FDreamChunkDownloaderTypes::FDreamCallback& Callback, int32 Priority);

	/**
	 * Internal function to mount a chunk
	 * @param Chunk Chunk to mount
	 * @param Callback Callback to execute when mounting completes
	 */
	void MountChunkInternal(FDreamChunk& Chunk, const FDreamChunkDownloaderTypes::FDreamCallback& Callback);

	/**
	 * Internal function to download a chunk
	 * @param Chunk Chunk to download
	 * @param Callback Callback to execute when download completes
	 * @param Priority Download priority
	 */
	void DownloadChunkInternal(const FDreamChunk& Chunk, const FDreamChunkDownloaderTypes::FDreamCallback& Callback, int32 Priority);

	/**
	 * Complete a mount task
	 * @param Chunk Chunk whose mount task to complete
	 */
	void CompleteMountTask(FDreamChunk& Chunk);

	/**
	 * Update mount tasks each frame
	 * @param dts Delta time since last update
	 * @return True if mounts are still pending
	 */
	bool UpdateMountTasks(float dts);

	/**
	 * Execute a callback on the next tick
	 * @param Callback Callback to execute
	 * @param bSuccess Success status to pass to callback
	 */
	void ExecuteNextTick(const FDreamChunkDownloaderTypes::FDreamCallback& Callback, bool bSuccess);

	/**
	 * Issue pending downloads
	 */
	void IssueDownloads();
};

// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "DreamChunkDownloaderTypes.generated.h"

class FDreamChunkDownload;
class FDreamPakMountWork;

struct FDreamPakFile;
struct FDreamChunkDownloaderStats;
struct FDreamChunk;
struct FDreamPakFileEntry;

/**
 * Type Definitions for Dream Chunk Downloader
 * 
 * This namespace contains type aliases and function pointer definitions
 * used throughout the Dream Chunk Downloader system.
 */
namespace FDreamChunkDownloaderTypes
{
	/** Type alias for the async mount task */
	typedef FAsyncTask<FDreamPakMountWork> FDreamMountTask;

	/** Callback function type for async operations */
	typedef TFunction<void(bool bSuccess)> FDreamCallback;

	/** Callback function type for download analytics */
	typedef TFunction<void(
		const FString& FileName,
		const FString& Url,
		uint64 SizeBytes,
		const FTimespan& DownloadTime,
		int32 HttpStatus)> FDreamDownloadAnalytics;
}

/**
 * Static Constants for Dream Chunk Downloader
 * 
 * This namespace contains static string constants used as keys
 * in JSON manifest files and other data structures.
 */
namespace FDreamChunkDownloaderStatics
{
	/** Key for build ID in manifest files */
	static const FString BUILD_ID_KEY = TEXT("build-id");

	/** Field name for entries count in manifest files */
	static const FString ENTRIES_COUNT_FIELD = TEXT("entries-count");

	/** Field name for entries array in manifest files */
	static const FString ENTRIES_FIELD = TEXT("entries");

	/** Field name for file name in pak file entries */
	static const FString FILE_NAME_FIELD = TEXT("file-name");

	/** Field name for file size in pak file entries */
	static const FString FILE_SIZE_FIELD = TEXT("file-size");

	/** Field name for file version in pak file entries */
	static const FString FILE_VERSION_FIELD = TEXT("file-version");

	/** Field name for chunk ID in pak file entries */
	static const FString FILE_CHUNK_ID_FIELD = TEXT("chunk-id");

	/** Field name for relative URL in pak file entries */
	static const FString FILE_RELATIVE_URL_FIELD = TEXT("relative-url");

	/** Field name for download chunk ID list in manifest files */
	static const FString DOWNLOAD_CHUNK_ID_LIST_FIELD = TEXT("download-chunk-id-list");

	/** Field name for client build ID in manifest files */
	static const FString CLIENT_BUILD_ID = "client-build-id";
}

/**
 * Status of a Chunk
 * 
 * Represents the various states a chunk can be in during the download
 * and mounting process.
 */
UENUM(BlueprintType)
enum class EDreamChunkStatus : uint8
{
	/** Chunk is fully mounted and ready for use */
	Mounted UMETA(DisplayName = "Mounted"),

	/** Chunk is fully downloaded and cached locally */
	Cached UMETA(DisplayName = "Cached"),

	/** Chunk is currently being downloaded */
	Downloading UMETA(DisplayName = "Downloading"),

	/** Chunk has partial data downloaded */
	Partial UMETA(DisplayName = "Partial"),

	/** Chunk is available on CDN but not downloaded */
	Remote UMETA(DisplayName = "Remote"),

	/** Chunk status is unknown */
	Unknown UMETA(DisplayName = "Unknown")
};

/**
 * Cache Location Options
 * 
 * Specifies where downloaded chunk files should be stored on the user's device.
 */
UENUM(BlueprintType)
enum class EDreamChunkDownloaderCacheLocation : uint8
{
	/** Store in user-specific directory (e.g. Saved folder) */
	User UMETA(DisplayName = "User"),

	/** Store in game installation directory */
	Game UMETA(DisplayName = "Game"),
};

/**
 * Deployment Set Configuration
 * 
 * Defines a set of CDN hosts for a specific deployment/platform.
 * Allows configuration of multiple CDN URLs for redundancy and load balancing.
 */
USTRUCT(BlueprintType)
struct FDreamChunkDownloaderDeploymentSet
{
	GENERATED_BODY()

public:
	/** Name of the deployment (e.g. Windows, Android, IOS) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "DreamChunkDownloader")
	FString DeploymentName;

	/** List of CDN host URLs for this deployment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "DreamChunkDownloader")
	TArray<FString> Hosts;
};

/**
 * Chunk Downloader Statistics
 * 
 * Tracks various statistics related to the download and mounting process
 * for performance monitoring and progress reporting.
 */
USTRUCT(BlueprintType)
struct FDreamChunkDownloaderStats
{
public:
	GENERATED_BODY()

	/** Number of pak files that have been downloaded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int FilesDownloaded = 0;

	/** Total number of pak files that need to be downloaded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int TotalFilesToDownload = 0;

	/** Number of bytes that have been downloaded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int64 BytesDownloaded = 0;

	/** Total number of bytes that need to be downloaded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int64 TotalBytesToDownload = 0;

	/** Number of chunks that have been mounted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int ChunksMounted = 0;

	/** Total number of chunks that need to be mounted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int TotalChunksToMount = 0;

	/** UTC time when loading began (for rate calculations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FDateTime LoadingStartTime = FDateTime::MinValue();

	/** Last error that occurred during operations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FText LastError;
};

/**
 * Pak File Entry
 * 
 * Represents metadata for a single pak file as defined in manifest files.
 * Contains all information needed to download and validate a pak file.
 */
USTRUCT(BlueprintType)
struct FDreamPakFileEntry
{
	GENERATED_BODY()

	/** Unique name of the pak file (filename only, no path) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FString FileName;

	/** Final size of the file in bytes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int64 FileSize = 0;

	/** 
	 * Unique ID representing a particular version of this pak file
	 * When used for validation (if it begins with "SHA1:"), it's treated as a SHA1 hash
	 * Otherwise, it's considered just a unique ID
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FString FileVersion;

	/** Chunk ID this pak file is assigned to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int32 ChunkId = -1;

	/** URL for this pak file (relative to CDN root, includes build-specific folder) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FString RelativeUrl;
};

/**
 * Pak File Information
 * 
 * Runtime information about a pak file including its current state,
 * download progress, and associated operations.
 */
USTRUCT(BlueprintType)
struct FDreamPakFile
{
	GENERATED_BODY()

	/** Metadata entry for this pak file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FDreamPakFileEntry Entry;

	/** Whether the file is fully cached locally */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	bool bIsCached = false;

	/** Whether the file is currently mounted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	bool bIsMounted = false;

	/** Whether the file is embedded in the build */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	bool bIsEmbedded = false;

	/** Current size of the file on disk (grows during download) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int64 SizeOnDisk = 0; // grows as the file is downloaded. See Entry.FileSize for the target size

	/** Priority for download operations (higher values = higher priority) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int32 Priority = 0;

	/** Active download operation for this file */
	TSharedPtr<FDreamChunkDownload> Download;

	/** Callbacks to execute after download completes */
	TArray<FDreamChunkDownloaderTypes::FDreamCallback> PostDownloadCallbacks;
};

/**
 * Chunk Information
 * 
 * Represents a logical chunk which may consist of multiple pak files.
 * Tracks the mounting state and associated pak files for the chunk.
 */
USTRUCT(BlueprintType)
struct FDreamChunk
{
	GENERATED_BODY()

	/** ID of this chunk */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int32 ChunkId = -1;

	/** Whether this chunk is currently mounted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	bool bIsMounted = false;

	/** List of pak files that make up this chunk */
	TArray<TSharedRef<FDreamPakFile>> PakFiles;

	/**
	 * Check if all pak files in this chunk are cached
	 * @return True if all pak files are cached
	 */
	inline bool IsCached() const
	{
		for (const auto& PakFile : PakFiles)
		{
			if (!PakFile->bIsCached)
			{
				return false;
			}
		}
		return true;
	}

	/** Active mount task for this chunk */
	FDreamChunkDownloaderTypes::FDreamMountTask* MountTask = nullptr;
};

/**
 * Manifest Data
 * 
 * Represents the complete manifest data structure containing
 * build information and all pak file entries.
 */
USTRUCT(BlueprintType)
struct FDreamManifestData
{
	GENERATED_BODY()

	/** Build ID for this manifest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FString BuildId;

	/** Target platform for this manifest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	FString Platform;

	/** Manifest version */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	int32 Version = 1;

	/** List of pak file entries in this manifest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	TArray<FDreamPakFileEntry> PakFiles;

	/** Additional properties stored in the manifest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DreamChunkDownloader")
	TMap<FString, FString> Properties;
};

/**
 * Multi Callback Handler
 * 
 * Helper class for managing multiple async operations that should
 * trigger a single callback when all operations complete.
 */
class FDreamMultiCallback
{
public:
	/**
	 * Constructor
	 * @param OnCallback The callback to execute when all operations complete
	 */
	FDreamMultiCallback(const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback);

	/**
	 * Add a pending operation and get a callback for it
	 * @return Callback function for the new pending operation
	 */
	inline const FDreamChunkDownloaderTypes::FDreamCallback& AddPending()
	{
		++NumPending;
		return IndividualCb;
	}

	/**
	 * Get the number of pending operations
	 * @return Number of pending operations
	 */
	inline int GetNumPending() const
	{
		return NumPending;
	}

	/**
	 * Abort the multi callback (when no pending operations remain)
	 */
	void Abort()
	{
		check(NumPending == 0);
		delete this;
	}

private:
	/**
	 * Destructor (private to ensure proper cleanup)
	 */
	~FDreamMultiCallback()
	{
	}

	/** Number of pending operations */
	int NumPending = 0;

	/** Number of operations that succeeded */
	int NumSucceeded = 0;

	/** Number of operations that failed */
	int NumFailed = 0;

	/** Callback for individual operations */
	FDreamChunkDownloaderTypes::FDreamCallback IndividualCb;

	/** Callback to execute when all operations complete */
	FDreamChunkDownloaderTypes::FDreamCallback OuterCallback;
};

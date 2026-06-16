// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamChunkDownloaderTypes.h"

struct FDreamPakFile;

/**
 * Asynchronous Pak File Mounting Task
 * 
 * This class represents an asynchronous task for mounting pak files in a background thread.
 * It implements the FNonAbandonableTask interface which means the task must complete
 * once started and cannot be abandoned or cancelled midway through execution.
 * 
 * The task handles the mounting of multiple pak files in a specific order, which is
 * important for proper dependency resolution and content loading. All file I/O and
 * pak mounting operations are performed off the main thread to prevent blocking
 * the game's execution.
 * 
 * After completion, the task provides results indicating which pak files were
 * successfully mounted, allowing the main thread to update the chunk status accordingly.
 */
class FDreamPakMountWork : public FNonAbandonableTask
{
public:
	/** Allow the async task template to access private members */
	friend class FAsyncTask<FDreamPakMountWork>;

	/**
	 * Main work function that performs the pak mounting operations
	 * This function is executed on a background thread
	 */
	void DoWork();

	/**
	 * Get the statistics ID for this task type
	 * Used by the engine's profiling system to track performance
	 * @return Statistics ID for this task type
	 */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPakMountWork, STATGROUP_ThreadPoolAsyncTasks);
	}

public: // inputs

	/** 
	 * ID of the chunk being mounted 
	 * Used for logging and identification purposes
	 */
	int32 ChunkId;

	/** 
	 * Folder path where cached pak files are stored 
	 * Used to locate pak files that have been downloaded and cached
	 */
	FString CacheFolder;

	/** 
	 * Folder path where embedded pak files are stored 
	 * Used to locate pak files that were shipped with the game build
	 */
	FString EmbeddedFolder;

	/** 
	 * List of pak files to mount in order 
	 * The order is important for proper dependency resolution
	 * Only unmounted pak files should be included in this list
	 */
	TArray<TSharedRef<FDreamPakFile>> PakFiles;

	/** 
	 * Callbacks to execute after mounting completes 
	 * These are called on the main thread after the async task finishes
	 */
	TArray<FDreamChunkDownloaderTypes::FDreamCallback> PostMountCallbacks;

public: // results

	/** 
	 * List of pak files that were successfully mounted 
	 * This allows the main thread to update the status of individual pak files
	 */
	TArray<TSharedRef<FDreamPakFile>> MountedPakFiles;
};

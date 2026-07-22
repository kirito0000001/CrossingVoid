// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"

class UDreamChunkDownloaderSubsystem;

/**
 * Platform Wrapper for Chunk Downloader Subsystem
 * 
 * This class implements the generic platform chunk installation interface
 * to integrate the custom chunk downloader with Unreal Engine's platform abstraction layer.
 * It acts as a bridge between the engine's chunk management system and the
 * custom chunk downloader implementation.
 * 
 * The wrapper forwards all platform chunk installation requests to the
 * UDreamChunkDownloaderSubsystem for processing.
 */
class FDreamChunkDownloaderPlatformWrapper : public FGenericPlatformChunkInstall
{
public:
	/**
	 * Get the current location status of a chunk
	 * @param ChunkID The ID of the chunk to check
	 * @return The location status of the chunk (NotAvailable, LocalSlow, LocalFast, etc.)
	 */
	virtual EChunkLocation::Type GetChunkLocation(uint32 ChunkID) override;

	/**
	 * Prioritize a chunk for installation with the specified priority
	 * @param ChunkID The ID of the chunk to prioritize
	 * @param Priority The priority level to set for the chunk
	 * @return True if the priority was successfully set
	 */
	virtual bool PrioritizeChunk(uint32 ChunkID, EChunkPriority::Type Priority) override;

	/**
	 * Add a delegate to be notified of chunk installation events
	 * @param Delegate The delegate to add
	 * @return A handle to the added delegate for later removal
	 */
	virtual FDelegateHandle AddChunkInstallDelegate(FPlatformChunkInstallDelegate Delegate) override;

	/**
	 * Remove a previously added chunk installation delegate
	 * @param Delegate The handle of the delegate to remove
	 */
	virtual void RemoveChunkInstallDelegate(FDelegateHandle Delegate) override;

public:
	/**
	 * Get the current installation speed setting
	 * @return The current chunk installation speed setting
	 */
	virtual EChunkInstallSpeed::Type GetInstallSpeed() override;

	/**
	 * Set the installation speed for chunk downloads
	 * @param InstallSpeed The desired installation speed
	 * @return True if the speed was successfully set
	 */
	virtual bool SetInstallSpeed(EChunkInstallSpeed::Type InstallSpeed) override;

	/**
	 * Debug function to start the next available chunk installation
	 * @return True if a chunk installation was started
	 */
	virtual bool DebugStartNextChunk() override;

	/**
	 * Check if a specific progress reporting type is supported
	 * @param ReportType The progress reporting type to check
	 * @return True if the reporting type is supported
	 */
	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) override;

	/**
	 * Get the progress of a specific chunk installation
	 * @param ChunkID The ID of the chunk to check progress for
	 * @param ReportType The type of progress reporting to use
	 * @return The progress value (0.0 to 1.0) or -1.0 if not available
	 */
	virtual float GetChunkProgress(uint32 ChunkID, EChunkProgressReportingType::Type ReportType) override;

public:
	/**
	 * Constructor
	 * @param InChunkDownloader Reference to the chunk downloader subsystem
	 */
	FDreamChunkDownloaderPlatformWrapper(TSharedPtr<UDreamChunkDownloaderSubsystem>& InChunkDownloader) : ChunkDownloader(InChunkDownloader)
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FDreamChunkDownloaderPlatformWrapper() override
	{
	}

private:
	/** Reference to the chunk downloader subsystem that handles the actual implementation */
	TSharedPtr<UDreamChunkDownloaderSubsystem>& ChunkDownloader;
};

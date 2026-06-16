// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "DreamChunkDownloaderSubsystem.h"
#include "DreamChunkDownloaderPlatformStreamDownload.h"

/**
 * Chunk Download Manager
 * 
 * This class handles the downloading of individual pak files (chunks) from CDN.
 * It manages the entire download lifecycle including:
 * - Download initiation and progress tracking
 * - Retry logic with exponential backoff
 * - File validation and integrity checking
 * - Device space verification
 * - Completion and error handling
 * 
 * Each instance manages a single pak file download and maintains state
 * information about the download progress and status.
 */
class FDreamChunkDownload : public TSharedFromThis<FDreamChunkDownload>
{
public:
	/**
	 * Constructor
	 * @param DownloaderIn Reference to the chunk downloader subsystem
	 * @param PakFileIn Reference to the pak file to download
	 */
	FDreamChunkDownload(const TWeakObjectPtr<UDreamChunkDownloaderSubsystem>& DownloaderIn, const TSharedRef<FDreamPakFile>& PakFileIn);

	/**
	 * Destructor
	 */
	virtual ~FDreamChunkDownload();

	/**
	 * Check if the download has completed (successfully or unsuccessfully)
	 * @return True if the download has completed
	 */
	inline bool HasCompleted() const { return bHasCompleted; }

	/**
	 * Get the current download progress in bytes
	 * @return Number of bytes received so far
	 */
	inline int32 GetProgress() const { return LastBytesReceived; }

	/**
	 * Start the download process
	 */
	void Start();

	/**
	 * Cancel the download
	 * @param bResult The result to report for the cancelled download (true = success, false = failure)
	 */
	void Cancel(bool bResult);

public:
	/** Reference to the chunk downloader subsystem that owns this download */
	const TWeakObjectPtr<UDreamChunkDownloaderSubsystem> Downloader;

	/** Reference to the pak file being downloaded */
	const TSharedRef<FDreamPakFile> PakFile;

	/** Target file path where the downloaded content will be saved */
	const FString TargetFile;

protected:
	/**
	 * Update the file size information in the pak file record
	 */
	void UpdateFileSize();

	/**
	 * Validate the downloaded file's integrity
	 * @return True if the file is valid
	 */
	bool ValidateFile() const;

	/**
	 * Check if there is sufficient device space for the download
	 * @return True if there is enough space
	 */
	bool HasDeviceSpaceRequired() const;

	/**
	 * Start the actual download process with retry logic
	 * @param TryNumber The current attempt number (for retry logic)
	 */
	void StartDownload(int TryNumber);

	/**
	 * Handle download progress updates
	 * @param BytesReceived Number of bytes received in this update
	 */
	void OnDownloadProgress(int32 BytesReceived);

	/**
	 * Handle download completion
	 * @param Url The URL that was downloaded from
	 * @param TryNumber The attempt number that completed
	 * @param HttpStatus The HTTP status code of the response
	 */
	void OnDownloadComplete(const FString& Url, int TryNumber, int32 HttpStatus);

	/**
	 * Handle overall download completion (success or failure)
	 * @param bSuccess Whether the download completed successfully
	 * @param ErrorText Error message if the download failed
	 */
	void OnCompleted(bool bSuccess, const FText& ErrorText);

private:
	/** Whether the download has been cancelled */
	bool bIsCancelled = false;

	/** Callback function to execute when download is cancelled */
	FDreamDownloadCancel CancelCallback;

	/** Whether the download has completed (successfully or unsuccessfully) */
	bool bHasCompleted = false;

	/** Time when the download started */
	FDateTime BeginTime;

	/** Last reported number of bytes received */
	int32 LastBytesReceived = 0;
};

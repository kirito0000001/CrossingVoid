// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownload.h"

#include "DreamChunkDownloaderLog.h"
#include "DreamChunkDownloaderUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "Interfaces/IHttpResponse.h"

#define LOCTEXT_NAMESPACE "ChunkDownloader"

FDreamChunkDownload::FDreamChunkDownload(const TWeakObjectPtr<UDreamChunkDownloaderSubsystem>& DownloaderIn, const TSharedRef<FDreamPakFile>& PakFileIn)
	: Downloader(DownloaderIn)
	  , PakFile(PakFileIn)
	  , TargetFile(Downloader.Get()->GetCacheFolder() / PakFileIn->Entry.FileName)
{
	// couple of sanity checks for our flags
	check(!PakFile->bIsCached);
	check(!PakFile->bIsEmbedded);
	check(!PakFile->bIsMounted);
}

FDreamChunkDownload::~FDreamChunkDownload()
{
}

void FDreamChunkDownload::Start()
{
	check(!bHasCompleted);

	// check to make sure we have enough space for this download
	if (!HasDeviceSpaceRequired())
	{
		TWeakPtr<FDreamChunkDownload> WeakThisPtr = AsShared();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThisPtr](float Unused)
		{
			TSharedPtr<FDreamChunkDownload> SharedThis = WeakThisPtr.Pin();
			if (SharedThis.IsValid())
			{
				SharedThis->OnCompleted(false, LOCTEXT("NotEnoughSpace", "Not enough space on device."));
			}
			return false;
		}), 1.0);
		return;
	}

	// try to download from the CDN
	StartDownload(0);
}

void FDreamChunkDownload::Cancel(bool bResult)
{
	check(!bHasCompleted);
	DCD_LOG(Warning, TEXT("Canceling download of '%s'. result=%s"), *PakFile->Entry.FileName, bResult ? TEXT("true") : TEXT("false"));

	// cancel the platform specific file download
	if (!bIsCancelled)
	{
		bIsCancelled = true;
		CancelCallback();
	}

	// fire the completion results
	OnCompleted(bResult, FText::Format(LOCTEXT("DownloadCanceled", "Download of '%s' was canceled."), FText::FromString(PakFile->Entry.FileName)));
}

void FDreamChunkDownload::UpdateFileSize()
{
	IFileManager& FileManager = IFileManager::Get();
	int64 FileSizeOnDisk = FileManager.FileSize(*TargetFile);
	PakFile->SizeOnDisk = (FileSizeOnDisk > 0) ? (uint64)FileSizeOnDisk : 0;
}

bool FDreamChunkDownload::ValidateFile() const
{
	if (PakFile->SizeOnDisk != PakFile->Entry.FileSize)
	{
		DCD_LOG(Error, TEXT("Size mismatch. Expected %llu, got %llu"), PakFile->Entry.FileSize, PakFile->SizeOnDisk);
		return false;
	}

	if (PakFile->Entry.FileVersion.StartsWith(TEXT("SHA1:")))
	{
		// check the sha1 hash
		if (!FDreamChunkDownloaderUtils::CheckFileSha1Hash(TargetFile, PakFile->Entry.FileVersion))
		{
			DCD_LOG(Error, TEXT("Checksum mismatch. Expected %s"), *PakFile->Entry.FileVersion);
			return false;
		}
	}

	return true;
}

bool FDreamChunkDownload::HasDeviceSpaceRequired() const
{
	uint64 TotalDiskSpace = 0;
	uint64 TotalDiskFreeSpace = 0;
	if (FPlatformMisc::GetDiskTotalAndFreeSpace(Downloader.Get()->GetCacheFolder(), TotalDiskSpace, TotalDiskFreeSpace))
	{
		uint64 BytesNeeded = PakFile->Entry.FileSize - PakFile->SizeOnDisk;
		if (TotalDiskFreeSpace < BytesNeeded)
		{
			// not enough space
			DCD_LOG(Warning, TEXT("Unable to download '%s'. Needed %llu bytes had %llu bytes free (of %llu bytes)"),
			        *PakFile->Entry.FileName, PakFile->Entry.FileSize, TotalDiskFreeSpace, TotalDiskSpace);
			return false;
		}
	}
	return true;
}

void FDreamChunkDownload::StartDownload(int TryNumber)
{
	// only handle completion once
	check(!bHasCompleted);
	BeginTime = FDateTime::UtcNow();
	OnDownloadProgress(0);

	// download the next url
	check(Downloader.Get()->GetBuildBaseUrls().Num() > 0);
	FString Url = Downloader.Get()->GetBuildBaseUrls()[TryNumber % Downloader.Get()->GetBuildBaseUrls().Num()] / PakFile->Entry.RelativeUrl;
	DCD_LOG(Log, TEXT("Downloading %s from %s"), *PakFile->Entry.FileName, *Url);
	TWeakPtr<FDreamChunkDownload> WeakThisPtr = AsShared();
	CancelCallback = PlatformStreamDownload(Url, TargetFile, [WeakThisPtr](int32 BytesReceived)
	                                        {
		                                        TSharedPtr<FDreamChunkDownload> SharedThis = WeakThisPtr.Pin();
		                                        if (SharedThis.IsValid() && !SharedThis->bHasCompleted)
		                                        {
			                                        SharedThis->OnDownloadProgress(BytesReceived);
		                                        }
	                                        }, [WeakThisPtr, TryNumber, Url](int32 HttpStatus)
	                                        {
		                                        TSharedPtr<FDreamChunkDownload> SharedThis = WeakThisPtr.Pin();
		                                        if (SharedThis.IsValid() && !SharedThis->bHasCompleted)
		                                        {
			                                        SharedThis->OnDownloadComplete(Url, TryNumber, HttpStatus);
		                                        }
	                                        });
}

void FDreamChunkDownload::OnDownloadProgress(int32 BytesReceived)
{
	Downloader.Get()->GetStats().BytesDownloaded -= LastBytesReceived;
	LastBytesReceived = BytesReceived;
	Downloader.Get()->GetStats().BytesDownloaded += LastBytesReceived;
}

void FDreamChunkDownload::OnDownloadComplete(const FString& Url, int TryNumber, int32 HttpStatus)
{
	// only handle completion once
	check(!bHasCompleted);

	// update file size on disk
	UpdateFileSize();

	// report analytics
	if (Downloader.Get()->OnDownloadAnalytics)
	{
		Downloader.Get()->OnDownloadAnalytics(PakFile->Entry.FileName, Url, PakFile->SizeOnDisk, FDateTime::UtcNow() - BeginTime, HttpStatus);
	}

	// handle success
	if (EHttpResponseCodes::IsOk(HttpStatus))
	{
		// make sure the file is complete
		if (ValidateFile())
		{
			PakFile->bIsCached = true;
			OnCompleted(true, FText());
			return;
		}

		// if we fail validation, delete the file and start over
		DCD_LOG(Error, TEXT("%s from %s failed validation"), *TargetFile, *Url);
		IPlatformFile::GetPlatformPhysical().DeleteFile(*TargetFile);
	}

	// check again to make sure we have enough space for this download
	if (!HasDeviceSpaceRequired())
	{
		OnCompleted(false, LOCTEXT("NotEnoughSpace", "Not enough space on device."));
		return;
	}

	// compute delay before re-starting download
	float SecondsToDelay = (TryNumber + 1) * 5.0f;
	if (SecondsToDelay > 60)
	{
		SecondsToDelay = 60;
	}

	// set a ticker to delay
	DCD_LOG(Log, TEXT("Will re-attempt to download %s in %f seconds"), *PakFile->Entry.FileName, SecondsToDelay);
	TWeakPtr<FDreamChunkDownload> WeakThisPtr = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThisPtr, TryNumber](float Unused)
	{
		TSharedPtr<FDreamChunkDownload> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid() && !SharedThis->bHasCompleted)
		{
			SharedThis->StartDownload(TryNumber + 1);
		}
		return false;
	}), SecondsToDelay);
}

void FDreamChunkDownload::OnCompleted(bool bSuccess, const FText& ErrorText)
{
	// make sure we don't complete more than once
	check(!bHasCompleted);
	bHasCompleted = true;

	// increment files downloaded
	OnDownloadProgress(bSuccess ? PakFile->SizeOnDisk : 0);
	++Downloader.Get()->GetStats().FilesDownloaded;
	if (!bSuccess && !ErrorText.IsEmpty())
	{
		Downloader.Get()->GetStats().LastError = ErrorText;
	}

	// queue up callbacks
	for (const auto& Callback : PakFile->PostDownloadCallbacks)
	{
		Downloader.Get()->ExecuteNextTick(Callback, bSuccess);
	}
	PakFile->PostDownloadCallbacks.Empty();

	// remove from download requests
	if (ensure(Downloader.Get()->GetDownloadRequests().RemoveSingle(PakFile) > 0))
	{
		Downloader.Get()->IssueDownloads();
	}

	// unhook from pak file (this may delete us)
	if (PakFile->Download.Get() == this)
	{
		PakFile->Download.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

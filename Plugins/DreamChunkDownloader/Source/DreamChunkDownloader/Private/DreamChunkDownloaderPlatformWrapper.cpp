// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownloaderPlatformWrapper.h"

#include "DreamChunkDownloaderTypes.h"
#include "DreamChunkDownloaderSubsystem.h"

EChunkLocation::Type FDreamChunkDownloaderPlatformWrapper::GetChunkLocation(uint32 ChunkID)
{
	// chunk 0 is special, it's always shipped with the app (by definition) so don't report it missing.
	if (ChunkID == 0)
	{
		return EChunkLocation::BestLocation;
	}

	// if the chunk downloader's not initialized, report all chunks missing
	if (!ChunkDownloader.IsValid())
	{
		return EChunkLocation::DoesNotExist;
	}

	// map the downloader's status to the chunk install interface enum
	switch (ChunkDownloader->GetChunkStatus(ChunkID))
	{
	case EDreamChunkStatus::Mounted:
		return EChunkLocation::BestLocation;
	case EDreamChunkStatus::Remote:
	case EDreamChunkStatus::Partial:
	case EDreamChunkStatus::Downloading:
	case EDreamChunkStatus::Cached:
	case EDreamChunkStatus::Unknown:
		return EChunkLocation::NotAvailable;
	
	}
	return EChunkLocation::DoesNotExist;
}

bool FDreamChunkDownloaderPlatformWrapper::PrioritizeChunk(uint32 ChunkID, EChunkPriority::Type Priority)
{
	if (!ChunkDownloader.IsValid())
	{
		return false;
	}
	ChunkDownloader->MountChunk(ChunkID, FDreamChunkDownloaderTypes::FDreamCallback());
	return true;
}

FDelegateHandle FDreamChunkDownloaderPlatformWrapper::AddChunkInstallDelegate(FPlatformChunkInstallDelegate Delegate)
{
	// create if necessary
	ensureMsgf(ChunkDownloader != nullptr, TEXT("ChunkDownloader is not valid"));
	return ChunkDownloader->OnChunkMounted.Add(Delegate);
}

void FDreamChunkDownloaderPlatformWrapper::RemoveChunkInstallDelegate(FDelegateHandle Delegate)
{
	if (!ChunkDownloader.IsValid())
	{
		return;
	}
	ChunkDownloader->OnChunkMounted.Remove(Delegate);
}

EChunkInstallSpeed::Type FDreamChunkDownloaderPlatformWrapper::GetInstallSpeed()
{
	return EChunkInstallSpeed::Fast;
}

bool FDreamChunkDownloaderPlatformWrapper::SetInstallSpeed(EChunkInstallSpeed::Type InstallSpeed)
{
	return false;
}

bool FDreamChunkDownloaderPlatformWrapper::DebugStartNextChunk()
{
	return false;
}

bool FDreamChunkDownloaderPlatformWrapper::GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType)
{
	return false;
}

float FDreamChunkDownloaderPlatformWrapper::GetChunkProgress(uint32 ChunkID, EChunkProgressReportingType::Type ReportType)
{
	return 0;
}

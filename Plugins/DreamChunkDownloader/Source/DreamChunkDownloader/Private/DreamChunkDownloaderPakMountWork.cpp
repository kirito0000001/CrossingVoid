// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownloaderPakMountWork.h"

#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include "DreamChunkDownloaderLog.h"
#include "DreamChunkDownloaderTypes.h"

void FDreamPakMountWork::DoWork()
{
	// try to mount the pak file
	if (FCoreDelegates::MountPak.IsBound())
	{
		uint32 PakReadOrder = PakFiles.Num();
		for (const TSharedRef<FDreamPakFile>& PakFile : PakFiles)
		{
			FString FullPathOnDisk = (PakFile->bIsEmbedded ? EmbeddedFolder : CacheFolder) / PakFile->Entry.FileName;
			IPakFile* MountedPak = FCoreDelegates::MountPak.Execute(FullPathOnDisk, PakReadOrder);

#if !UE_BUILD_SHIPPING
			if (!MountedPak)
			{
				// This can fail because of the sandbox system - which the pak system doesn't understand.
				FString SandboxedPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FullPathOnDisk);
				MountedPak = FCoreDelegates::MountPak.Execute(SandboxedPath, PakReadOrder);
			}
#endif

			if (MountedPak)
			{
				// record that we successfully mounted this pak file
				MountedPakFiles.Add(PakFile);
				--PakReadOrder;
			}
			else
			{
				DCD_LOG(Error, TEXT("Unable to mount %s from chunk %d (mount operation failed)"), *FullPathOnDisk, ChunkId);
			}
		}
	}
	else
	{
		DCD_LOG(Error, TEXT("Unable to mount chunk %d (no FCoreDelegates::MountPak bound)"), ChunkId);
	}
}

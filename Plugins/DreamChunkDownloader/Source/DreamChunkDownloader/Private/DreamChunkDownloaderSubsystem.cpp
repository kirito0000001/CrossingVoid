// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownloaderSubsystem.h"

#include "Http.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "DreamChunkDownloaderLog.h"
#include "DreamChunkDownloaderSettings.h"
#include "DreamChunkDownloaderUtils.h"
#include "DreamChunkDownload.h"
#include "DreamChunkDownloaderPakMountWork.h"

#define LOCTEXT_NAMESPACE "DreamChunkDownloaderSubsystem"

using namespace FDreamChunkDownloaderTypes;
using namespace FDreamChunkDownloaderStatics;

UDreamChunkDownloaderSubsystem::~UDreamChunkDownloaderSubsystem()
{
	// this will be true unless we forgot to have Finalize called.
	check(PakFiles.Num() <= 0);
}

void UDreamChunkDownloaderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	OnMountCompletedInternal.AddLambda([this](bool bSuccess)
	{
		OnMountCompleted.Broadcast(bSuccess);
	});

	OnPatchCompletedInternal.AddLambda([this](bool bSuccess)
	{
		OnPatchCompleted.Broadcast(bSuccess);
	});

		PlatformName = FDreamChunkDownloaderUtils::GetTargetPlatformName();

	FString PackageBaseDir;
	switch (UDreamChunkDownloaderSettings::Get()->CacheFolderPath)
	{
	case EDreamChunkDownloaderCacheLocation::User:
		PackageBaseDir = FPaths::ProjectSavedDir();
		break;
	case EDreamChunkDownloaderCacheLocation::Game:
		PackageBaseDir = FPaths::ProjectDir();
		break;
	}

	PackageBaseDir /= TEXT("DreamChunkDownloader");

	check(!PackageBaseDir.IsEmpty())
	check(PakFiles.Num() == 0);
	check(PlatformName != TEXT("Unknown"));
	DCD_LOG(Log, TEXT("Initializing with platform = '%s' With cache Path = '%s'"), *PlatformName, *PackageBaseDir)

	FString PackageCacheDir = FPaths::Combine(PackageBaseDir, TEXT("PakCache"));
	FString PackageEmbeddedDir = FPaths::Combine(PackageBaseDir, TEXT("Embedded"));

	DCD_LOG(Log, TEXT("Initialize dirs : cache %s embedded %s"), *PackageCacheDir, *PackageEmbeddedDir);

	FPlatformMisc::AddAdditionalRootDirectory(PackageCacheDir);

	TargetDownloadsInFlight = FMath::Max(1, UDreamChunkDownloaderSettings::Get()->MaxConcurrentDownloads);
	CacheFolder = PackageCacheDir;
	EmbeddedFolder = PackageEmbeddedDir;

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.MakeDirectory(*PackageCacheDir, true))
	{
		DCD_LOG(Error, TEXT("Failed to create cache folder '%s'"), *PackageCacheDir);
	}

	// 加载embedded paks
	EmbeddedPaks.Empty();
	for (const FDreamPakFileEntry& Entry : FDreamChunkDownloaderUtils::ParseManifest(EmbeddedFolder / UDreamChunkDownloaderSettings::Get()->EmbeddedManifestFileName))
	{
		EmbeddedPaks.Add(Entry.FileName, Entry);
	}

	// 处理本地manifest
	FString LocalManifestPath = CacheFolder / UDreamChunkDownloaderSettings::Get()->LocalManifestFileName;
	if (!FPaths::FileExists(LocalManifestPath))
	{
		DCD_LOG(Warning, TEXT("Local manifest file does not exist at '%s', creating default one"), *LocalManifestPath);
		CreateDefaultLocalManifest();
	}
	else
	{
		FString TestContent;
		if (!FFileHelper::LoadFileToString(TestContent, *LocalManifestPath) || TestContent.IsEmpty())
		{
			DCD_LOG(Warning, TEXT("Local manifest file at '%s' is corrupted or empty, recreating"), *LocalManifestPath);
			CreateDefaultLocalManifest();
		}
	}

	// 解析本地manifest并设置下载列表和BuildID
	TSharedPtr<FJsonObject> JsonObject;
	TArray<FDreamPakFileEntry> LocalManifest = FDreamChunkDownloaderUtils::ParseManifest(LocalManifestPath, JsonObject);

	// 设置chunk下载列表
	SetupChunkDownloadList(JsonObject);

	// 设置Build ID
	SetupBuildId(JsonObject);

	// 验证配置有效性
	if (ChunkDownloadList.Num() == 0)
	{
		DCD_LOG(Error, TEXT("No chunks configured for download! Please check your settings."));
	}
	if (ContentBuildId.IsEmpty())
	{
		DCD_LOG(Error, TEXT("Build ID is empty! Please check your settings."));
	}

	// 处理本地已有的pak文件
	ProcessLocalPakFiles(LocalManifest, FileManager);

	SaveLocalManifest(false);

	// 尝试加载缓存的构建，只调用一次
	bool bHasValidCache = LoadCachedBuild(LastDeploymentName);

	if (!bHasValidCache)
	{
		DCD_LOG(Warning, TEXT("No valid cached build found, will download from CDN"));
		UpdateBuild(LastDeploymentName, ContentBuildId, [this](bool bSuccess)
		{
			DCD_LOG(Log, TEXT("UpdateBuild completed: %s"), bSuccess ? TEXT("success") : TEXT("failed"));
			bIsDownloadManifestUpToDate = bSuccess;

			if (bSuccess)
			{
				ValidateChunksAvailability();
			}
		});
	}
	else
	{
		DCD_LOG(Log, TEXT("Using valid cached build manifest"));
		bIsDownloadManifestUpToDate = true;
		ValidateChunksAvailability();
	}
}

void UDreamChunkDownloaderSubsystem::SetupChunkDownloadList(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (UDreamChunkDownloaderSettings::Get()->bUseStaticRemoteHost)
	{
		if (JsonObject.IsValid() && JsonObject->HasField(DOWNLOAD_CHUNK_ID_LIST_FIELD))
		{
			const TArray<TSharedPtr<FJsonValue>>* ValuesArray = nullptr;
			if (JsonObject->TryGetArrayField(DOWNLOAD_CHUNK_ID_LIST_FIELD, ValuesArray) && ValuesArray)
			{
				ChunkDownloadList.Empty();
				for (const TSharedPtr<FJsonValue>& Value : *ValuesArray)
				{
					if (Value.IsValid())
					{
						int32 AddedChunkID = 0;
						if (Value->TryGetNumber(AddedChunkID))
						{
							ChunkDownloadList.Add(AddedChunkID);
							DCD_LOG(Log, TEXT("Adding chunk %d to download list"), AddedChunkID);
						}
					}
				}

				if (ChunkDownloadList.Num() == 0)
				{
					DCD_LOG(Warning, TEXT("Remote download list is empty, falling back to settings"));
					ChunkDownloadList = UDreamChunkDownloaderSettings::Get()->DownloadChunkIds;
				}
				return;
			}
		}

		DCD_LOG(Warning, TEXT("Using settings download list (remote list not available)"));
	}

	ChunkDownloadList = UDreamChunkDownloaderSettings::Get()->DownloadChunkIds;
	DCD_LOG(Log, TEXT("Using local chunk download list from settings (%d chunks)"), ChunkDownloadList.Num());
}

void UDreamChunkDownloaderSubsystem::SetupBuildId(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (UDreamChunkDownloaderSettings::Get()->bUseStaticRemoteHost)
	{
		if (JsonObject.IsValid() && JsonObject->HasField(CLIENT_BUILD_ID))
		{
			FString RemoteBuildId;
			if (JsonObject->TryGetStringField(CLIENT_BUILD_ID, RemoteBuildId) && !RemoteBuildId.IsEmpty())
			{
				SetContentBuildId(FDreamChunkDownloaderUtils::GetTargetPlatformName(), RemoteBuildId);
				DCD_LOG(Log, TEXT("Using remote build id '%s'"), *ContentBuildId);
				return;
			}
		}

		DCD_LOG(Warning, TEXT("Using settings build ID (remote build ID not available)"));
	}

	SetContentBuildId(FDreamChunkDownloaderUtils::GetTargetPlatformName(), UDreamChunkDownloaderSettings::Get()->BuildID);
	DCD_LOG(Log, TEXT("Using local build id '%s'"), *ContentBuildId);
}

void UDreamChunkDownloaderSubsystem::ProcessLocalPakFiles(const TArray<FDreamPakFileEntry>& LocalManifest, IFileManager& FileManager)
{
	TArray<FString> StrayFiles;
	FileManager.FindFiles(StrayFiles, *CacheFolder, TEXT("*.pak"));

	if (LocalManifest.Num() > 0)
	{
		for (const FDreamPakFileEntry& Entry : LocalManifest)
		{
			TSharedRef<FDreamPakFile> FileInfo = MakeShared<FDreamPakFile>();
			FileInfo->Entry = Entry;

			FString LocalPath = CacheFolder / Entry.FileName;
			int64 SizeOnDisk = FileManager.FileSize(*LocalPath);
			if (SizeOnDisk > 0)
			{
				FileInfo->SizeOnDisk = SizeOnDisk;
				if (FileInfo->SizeOnDisk > Entry.FileSize)
				{
					DCD_LOG(Warning, TEXT("File '%s' needs update, size on disk = %lld, size in manifest = %lld"),
					        *LocalPath, FileInfo->SizeOnDisk, Entry.FileSize);
					bNeedsManifestSave = true;
					continue;
				}

				if (FileInfo->SizeOnDisk == Entry.FileSize)
				{
					FileInfo->bIsCached = true;
				}

				PakFiles.Add(Entry.FileName, FileInfo);
			}
			else
			{
				DCD_LOG(Log, TEXT("'%s' appears in LocalManifest but is not on disk"), *LocalPath);
				bNeedsManifestSave = true;
			}

			StrayFiles.RemoveSingle(Entry.FileName);
		}
	}

	// 清理孤立文件
	for (const FString& Orphan : StrayFiles)
	{
		bNeedsManifestSave = true;
		FString FullPathOnDisk = CacheFolder / Orphan;
		DCD_LOG(Log, TEXT("Deleting orphaned file '%s'"), *FullPathOnDisk);
		if (!FileManager.Delete(*FullPathOnDisk))
		{
			DCD_LOG(Error, TEXT("Unable to delete '%s'"), *FullPathOnDisk);
		}
	}
}

void UDreamChunkDownloaderSubsystem::CreateDefaultLocalManifest()
{
	FString JsonData;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonData);
	Writer->WriteObjectStart();

	// 基本字段
	Writer->WriteValue(ENTRIES_COUNT_FIELD, 0);
	Writer->WriteArrayStart(ENTRIES_FIELD);
	Writer->WriteArrayEnd();

	// 根据配置添加远程下载列表
	if (UDreamChunkDownloaderSettings::Get()->bUseStaticRemoteHost)
	{
		Writer->WriteArrayStart(DOWNLOAD_CHUNK_ID_LIST_FIELD);
		const TArray<int32>& DefaultChunks = UDreamChunkDownloaderSettings::Get()->DownloadChunkIds;
		for (int32 ChunkId : DefaultChunks)
		{
			Writer->WriteValue(ChunkId);
		}
		Writer->WriteArrayEnd();
		DCD_LOG(Log, TEXT("Added %d default chunk IDs to manifest"), DefaultChunks.Num());

		const FString& DefaultBuildId = UDreamChunkDownloaderSettings::Get()->BuildID;
		Writer->WriteValue(CLIENT_BUILD_ID, DefaultBuildId);
		DCD_LOG(Log, TEXT("Added default build ID '%s' to manifest"), *DefaultBuildId);
	}

	Writer->WriteObjectEnd();
	Writer->Close();

	// 确保目录存在
	FString ManifestPath = CacheFolder / UDreamChunkDownloaderSettings::Get()->LocalManifestFileName;
	FString ManifestDir = FPaths::GetPath(ManifestPath);

	if (!IFileManager::Get().MakeDirectory(*ManifestDir, true))
	{
		DCD_LOG(Error, TEXT("Failed to create directory for manifest: '%s'"), *ManifestDir);
		return;
	}

	if (FDreamChunkDownloaderUtils::WriteStringAsUtf8TextFile(JsonData, ManifestPath))
	{
		DCD_LOG(Log, TEXT("Created default local manifest at '%s'"), *ManifestPath);
	}
	else
	{
		DCD_LOG(Error, TEXT("Failed to write default local manifest to '%s'"), *ManifestPath);
	}
}

bool UDreamChunkDownloaderSubsystem::ValidateManifestFile(const FString& ManifestPath, FString& OutErrorMessage)
{
	// 检查文件是否存在
	if (!FPaths::FileExists(ManifestPath))
	{
		OutErrorMessage = FString::Printf(TEXT("Manifest file does not exist: %s"), *ManifestPath);
		return false;
	}

	// 检查文件是否可读
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ManifestPath))
	{
		OutErrorMessage = FString::Printf(TEXT("Cannot read manifest file: %s"), *ManifestPath);
		return false;
	}

	// 检查文件是否为空
	if (FileContent.IsEmpty())
	{
		OutErrorMessage = FString::Printf(TEXT("Manifest file is empty: %s"), *ManifestPath);
		return false;
	}

	// 检查JSON格式是否有效
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutErrorMessage = FString::Printf(TEXT("Manifest file contains invalid JSON: %s"), *ManifestPath);
		return false;
	}

	// 检查必要字段是否存在
	if (!JsonObject->HasField(ENTRIES_FIELD))
	{
		OutErrorMessage = FString::Printf(TEXT("Manifest file missing '%s' field: %s"), *ENTRIES_FIELD, *ManifestPath);
		return false;
	}

	return true;
}


void UDreamChunkDownloaderSubsystem::Deinitialize()
{
	Super::Deinitialize();

	Finalize();

	if (OnPatchCompleted.IsBound())
	{
		OnPatchCompleted.Clear();
	}
	if (OnMountCompleted.IsBound())
	{
		OnMountCompleted.Clear();
	}
	if (OnPatchCompletedInternal.IsBound())
	{
		OnPatchCompletedInternal.Clear();
	}
	if (OnMountCompletedInternal.IsBound())
	{
		OnMountCompletedInternal.Clear();
	}
}

void UDreamChunkDownloaderSubsystem::Finalize()
{
	DCD_LOG(Display, TEXT("Finalizing."));

	// 首先取消manifest请求
	if (ManifestRequest.IsValid())
	{
		DCD_LOG(Log, TEXT("Cancelling pending manifest request"));
		ManifestRequest->CancelRequest();
		ManifestRequest.Reset();
	}

	// wait for all mounts to finish
	WaitForMounts();

	// update the mount tasks (queues up callbacks)
	ensure(UpdateMountTasks(0.0f) == false);

	// cancel all downloads
	for (const auto& It : PakFiles)
	{
		const TSharedRef<FDreamPakFile>& File = It.Value;
		if (File->Download.IsValid())
		{
			CancelDownload(File, false);
		}
	}

	// unmount all mounted chunks (best effort)
	for (const auto& It : Chunks)
	{
		const TSharedRef<FDreamChunk>& Chunk = It.Value;
		if (Chunk->bIsMounted)
		{
			// unmount the paks (in reverse order)
			for (int32 i = Chunk->PakFiles.Num() - 1; i >= 0; --i)
			{
				const TSharedRef<FDreamPakFile>& PakFile = Chunk->PakFiles[i];
				UnmountPakFile(PakFile);
			}

			// clear the flag
			Chunk->bIsMounted = false;
		}
	}

	// clear pak files and chunks
	PakFiles.Empty();
	Chunks.Empty();

	// any loading mode is de-facto complete
	if (PostLoadCallbacks.Num() > 0)
	{
		TArray<FDreamChunkDownloaderTypes::FDreamCallback> Callbacks = MoveTemp(PostLoadCallbacks);
		PostLoadCallbacks.Empty();
		for (const auto& Callback : Callbacks)
		{
			ExecuteNextTick(Callback, false);
		}
	}

	// update is also de-facto complete
	if (UpdateBuildCallback)
	{
		FDreamChunkDownloaderTypes::FDreamCallback Callback = MoveTemp(UpdateBuildCallback);
		ExecuteNextTick(Callback, false);
	}

	// clear out the content build id
	ContentBuildId.Empty();
}

bool UDreamChunkDownloaderSubsystem::LoadCachedBuild(const FString& DeploymentName)
{
	TMap<FString, FString> CachedManifestProps;
	TSharedPtr<FJsonObject> JsonObject;

	FString CachedManifestPath = CacheFolder / UDreamChunkDownloaderSettings::Get()->CachedBuildManifestFileName;

	TArray<FDreamPakFileEntry> CachedManifest = FDreamChunkDownloaderUtils::ParseManifest(CachedManifestPath, JsonObject, &CachedManifestProps);

	// 检查是否有有效的缓存manifest
	if (CachedManifest.Num() == 0)
	{
		DCD_LOG(Warning, TEXT("No cached manifest entries found at '%s'"), *CachedManifestPath);
		return false;
	}

	const FString* BuildId = CachedManifestProps.Find(BUILD_ID_KEY);
	if (BuildId == nullptr || BuildId->IsEmpty())
	{
		DCD_LOG(Warning, TEXT("No cached build ID found in manifest"));
		return false;
	}

	if (*BuildId != ContentBuildId)
	{
		DCD_LOG(Warning, TEXT("Cached build ID (%s) doesn't match current (%s)"), **BuildId, *ContentBuildId);
		return false;
	}

	// 验证缓存的manifest包含我们需要的chunks
	TSet<int32> AvailableChunks;
	for (const FDreamPakFileEntry& Entry : CachedManifest)
	{
		if (Entry.ChunkId >= 0)
		{
			AvailableChunks.Add(Entry.ChunkId);
		}
	}

	bool bAllChunksAvailable = true;
	for (int32 RequiredChunk : ChunkDownloadList)
	{
		if (!AvailableChunks.Contains(RequiredChunk))
		{
			DCD_LOG(Warning, TEXT("Required chunk %d not found in cached manifest"), RequiredChunk);
			bAllChunksAvailable = false;
		}
	}

	if (!bAllChunksAvailable)
	{
		DCD_LOG(Warning, TEXT("Cached manifest doesn't contain all required chunks"));
		return false;
	}

	DCD_LOG(Log, TEXT("Using cached build manifest with %d entries for build ID: %s"), CachedManifest.Num(), **BuildId);
	SetContentBuildId(DeploymentName, *BuildId);
	LoadManifest(CachedManifest);
	return true;
}

void UDreamChunkDownloaderSubsystem::UpdateBuild(const FString& InDeploymentName, const FString& InContentBuildId, const FDreamChunkDownloaderTypes::FDreamCallback OnCallback)
{
	check(!InContentBuildId.IsEmpty());

	// 验证CDN配置
	SetContentBuildId(InDeploymentName, InContentBuildId);
	if (BuildBaseUrls.Num() <= 0)
	{
		DCD_LOG(Error, TEXT("No CDN URLs configured for deployment: %s"), *InDeploymentName);
		ExecuteNextTick(OnCallback, false);
		return;
	}

	// 如果已有UpdateBuild在进行中，处理并发调用
	if (UpdateBuildCallback)
	{
		DCD_LOG(Warning, TEXT("UpdateBuild already in progress, handling concurrent request"));

		// 取消之前的manifest请求
		if (ManifestRequest.IsValid())
		{
			ManifestRequest->CancelRequest();
			ManifestRequest.Reset();
		}

		// 执行之前的回调（失败）
		FDreamChunkDownloaderTypes::FDreamCallback PreviousCallback = MoveTemp(UpdateBuildCallback);
		ExecuteNextTick(PreviousCallback, false);
	}

	// 检查是否真的需要更新
	bool bNeedUpdate = true;

	// 检查缓存的manifest是否存在且有效
	FString CachedManifestPath = CacheFolder / UDreamChunkDownloaderSettings::Get()->CachedBuildManifestFileName;
	if (FPaths::FileExists(CachedManifestPath))
	{
		TMap<FString, FString> CachedManifestProps;
		TSharedPtr<FJsonObject> JsonObject;
		TArray<FDreamPakFileEntry> CachedManifest = FDreamChunkDownloaderUtils::ParseManifest(CachedManifestPath, JsonObject, &CachedManifestProps);

		const FString* CachedBuildId = CachedManifestProps.Find(BUILD_ID_KEY);
		if (CachedBuildId && *CachedBuildId == InContentBuildId && CachedManifest.Num() > 0)
		{
			DCD_LOG(Log, TEXT("Cached manifest is up to date for build %s"), *InContentBuildId);
			bNeedUpdate = false;
		}
	}

	if (!bNeedUpdate)
	{
		ExecuteNextTick(OnCallback, true);
		return;
	}

	// 设置新的回调
	check(OnCallback);
	UpdateBuildCallback = OnCallback;

	DCD_LOG(Log, TEXT("Starting manifest update for build %s from CDN"), *InContentBuildId);

	// start the load/download process
	TryLoadBuildManifest(0);
}

void UDreamChunkDownloaderSubsystem::ValidateChunksAvailability()
{
	TArray<int32> MissingChunks;
	TArray<int32> AvailableChunks;

	for (int32 ChunkId : ChunkDownloadList)
	{
		EDreamChunkStatus Status = GetChunkStatus(ChunkId);
		DCD_LOG(Log, TEXT("Chunk %d status: %s"), ChunkId, *UEnum::GetValueAsString(Status));

		// Remote状态也是可用的（可以下载）
		if (Status == EDreamChunkStatus::Unknown)
		{
			MissingChunks.Add(ChunkId);
		}
		else
		{
			AvailableChunks.Add(ChunkId);
		}
	}

	if (MissingChunks.Num() > 0)
	{
		auto ConvertIntArrayToStringArray = [](const TArray<int32>& IntArray) -> TArray<FString>
		{
			TArray<FString> StringArray;
			StringArray.Reserve(IntArray.Num());
			for (int32 Value : IntArray)
			{
				StringArray.Add(FString::FromInt(Value));
			}
			return StringArray;
		};

		DCD_LOG(Error, TEXT("Missing chunks in manifest: %s. Available chunks: %s"),
		        *FString::Join(ConvertIntArrayToStringArray(MissingChunks), TEXT(", ")),
		        *FString::Join(ConvertIntArrayToStringArray(AvailableChunks), TEXT(", ")));

		if (bIsDownloadManifestUpToDate)
		{
			DCD_LOG(Warning, TEXT("Manifest appears up to date but missing required chunks. Forcing manifest refresh."));
			bIsDownloadManifestUpToDate = false;

			UpdateBuild(LastDeploymentName, ContentBuildId, [this](bool bSuccess)
			{
				if (bSuccess)
				{
					ValidateChunksAvailability(); // 递归验证
				}
			});
		}
	}
	else
	{
		DCD_LOG(Log, TEXT("All required chunks (%d) are available in manifest"), AvailableChunks.Num());
	}
}

float UDreamChunkDownloaderSubsystem::GetPatchProgress() const
{
	if (ChunkDownloadList.Num() == 0)
	{
		return 1.0f;
	}

	int32 CompletedChunks = 0;
	float TotalProgress = 0.0f;

	for (int32 ChunkId : ChunkDownloadList)
	{
		EDreamChunkStatus Status = GetChunkStatus(ChunkId);
		switch (Status)
		{
		case EDreamChunkStatus::Mounted:
			TotalProgress += 1.0f;
			CompletedChunks++;
			break;

		case EDreamChunkStatus::Cached:
			TotalProgress += 0.95f; // 已下载但未挂载
			break;

		case EDreamChunkStatus::Downloading:
			// 可以获取具体的下载进度
			{
				const TSharedRef<FDreamChunk>* ChunkPtr = Chunks.Find(ChunkId);
				if (ChunkPtr != nullptr)
				{
					const FDreamChunk& Chunk = **ChunkPtr;
					for (const TSharedRef<FDreamPakFile>& PakFile : Chunk.PakFiles)
					{
						if (PakFile->Download.IsValid())
						{
							float FileProgress = static_cast<float>(PakFile->Download->GetProgress()) / PakFile->Entry.FileSize;
							TotalProgress += FMath::Clamp(FileProgress * 0.9f, 0.0f, 0.9f); // 最多90%，留10%给挂载
							break; // 假设每个chunk只有一个pak文件
						}
					}
				}
			}
			break;

		case EDreamChunkStatus::Partial:
			TotalProgress += 0.1f; // 部分完成
			break;

		case EDreamChunkStatus::Remote:
		case EDreamChunkStatus::Unknown:
		default:
			// 没有进度
			break;
		}
	}

	return TotalProgress / ChunkDownloadList.Num();
}

bool UDreamChunkDownloaderSubsystem::IsReadyForPatching() const
{
	if (!bIsDownloadManifestUpToDate)
	{
		return false;
	}

	// 检查所有需要的chunks是否都可用
	for (int32 ChunkId : ChunkDownloadList)
	{
		EDreamChunkStatus Status = GetChunkStatus(ChunkId);
		if (Status == EDreamChunkStatus::Unknown)
		{
			return false;
		}
	}

	return true;
}

void UDreamChunkDownloaderSubsystem::MountChunks(const TArray<int32>& ChunkIds, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback)
{
	// convert to chunk references
	TArray<TSharedRef<FDreamChunk>> ChunksToMount;
	for (int32 ChunkId : ChunkIds)
	{
		TSharedRef<FDreamChunk>* ChunkPtr = Chunks.Find(ChunkId);
		if (ChunkPtr != nullptr)
		{
			TSharedRef<FDreamChunk>& ChunkRef = *ChunkPtr;
			if (ChunkRef->PakFiles.Num() > 0)
			{
				if (!ChunkRef->bIsMounted)
				{
					ChunksToMount.Add(ChunkRef);
				}
				continue;
			}
		}
		DCD_LOG(Warning, TEXT("Ignoring mount request for chunk %d (no mapped pak files)."), ChunkId);
	}

	// make sure there are some chunks to mount (saves a frame)
	if (ChunksToMount.Num() <= 0)
	{
		// trivial success
		ExecuteNextTick(OnCallback, true);
		return;
	}

	// if there's no callback for some reason, avoid a bunch of boilerplate
#ifndef PVS_STUDIO // Build machine refuses to disable this warning
	if (OnCallback)
	{
		// loop over chunks and issue individual callback
		FDreamMultiCallback* MultiCallback = new FDreamMultiCallback(OnCallback);
		for (const TSharedRef<FDreamChunk>& Chunk : ChunksToMount)
		{
			MountChunkInternal(*Chunk, MultiCallback->AddPending());
		}
		check(MultiCallback->GetNumPending() > 0);
	} //-V773
	else
	{
		// no need to manage callbacks
		for (const TSharedRef<FDreamChunk>& Chunk : ChunksToMount)
		{
			MountChunkInternal(*Chunk, FDreamChunkDownloaderTypes::FDreamCallback());
		}
	}
#endif

	// resave manifest if needed
	SaveLocalManifest(false);
	ComputeLoadingStats();
}

void UDreamChunkDownloaderSubsystem::MountChunk(int32 ChunkId, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback)
{
	// look up the chunk
	TSharedRef<FDreamChunk>* ChunkPtr = Chunks.Find(ChunkId);
	if (ChunkPtr == nullptr || (*ChunkPtr)->PakFiles.Num() <= 0)
	{
		// a chunk that doesn't exist or one with no pak files are both considered "complete" for the purposes of this call
		// use GetChunkStatus to differentiate from chunks that mounted successfully
		DCD_LOG(Warning, TEXT("Ignoring mount request for chunk %d (no mapped pak files)."), ChunkId);
		ExecuteNextTick(OnCallback, true);
		return;
	}
	FDreamChunk& Chunk = **ChunkPtr;

	// see if we're mounted already
	if (Chunk.bIsMounted)
	{
		// trivial success
		ExecuteNextTick(OnCallback, true);
		return;
	}

	// mount the chunk
	MountChunkInternal(Chunk, OnCallback);

	// resave manifest if needed
	SaveLocalManifest(false);
	ComputeLoadingStats();
}

void UDreamChunkDownloaderSubsystem::DownloadChunks(const TArray<int32>& ChunkIds, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback, int32 Priority)
{
	// convert to chunk references
	TArray<TSharedRef<FDreamChunk>> ChunksToDownload;
	for (int32 ChunkId : ChunkIds)
	{
		TSharedRef<FDreamChunk>* ChunkPtr = Chunks.Find(ChunkId);
		if (ChunkPtr != nullptr)
		{
			TSharedRef<FDreamChunk>& ChunkRef = *ChunkPtr;
			if (ChunkRef->PakFiles.Num() > 0)
			{
				if (!ChunkRef->IsCached())
				{
					ChunksToDownload.Add(ChunkRef);
				}
				continue;
			}
		}
		DCD_LOG(Warning, TEXT("Ignoring download request for chunk %d (no mapped pak files)."), ChunkId);
	}

	// make sure there are some chunks to mount (saves a frame)
	if (ChunksToDownload.Num() <= 0)
	{
		// trivial success
		ExecuteNextTick(OnCallback, true);
		return;
	}

	// if there's no callback for some reason, avoid a bunch of boilerplate
#ifndef PVS_STUDIO // Build machine refuses to disable this warning
	if (OnCallback)
	{
		// loop over chunks and issue individual callback
		FDreamMultiCallback* MultiCallback = new FDreamMultiCallback(OnCallback);
		for (const TSharedRef<FDreamChunk>& Chunk : ChunksToDownload)
		{
			DownloadChunkInternal(*Chunk, MultiCallback->AddPending(), Priority);
		}
		check(MultiCallback->GetNumPending() > 0);
	} //-V773
	else
	{
		// no need to manage callbacks
		for (const TSharedRef<FDreamChunk>& Chunk : ChunksToDownload)
		{
			DownloadChunkInternal(*Chunk, FDreamChunkDownloaderTypes::FDreamCallback(), Priority);
		}
	}
#endif

	// resave manifest if needed
	SaveLocalManifest(false);
	ComputeLoadingStats();
}

void UDreamChunkDownloaderSubsystem::DownloadChunk(int32 ChunkId, const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback, int32 Priority)
{
	// look up the chunk
	TSharedRef<FDreamChunk>* ChunkPtr = Chunks.Find(ChunkId);
	if (ChunkPtr == nullptr || (*ChunkPtr)->PakFiles.Num() <= 0)
	{
		// a chunk that doesn't exist or one with no pak files are both considered "complete" for the purposes of this call
		// use GetChunkStatus to differentiate from chunks that mounted successfully
		DCD_LOG(Warning, TEXT("Ignoring download request for chunk %d (no mapped pak files)."), ChunkId);
		ExecuteNextTick(OnCallback, true);
		return;
	}
	const FDreamChunk& Chunk = **ChunkPtr;

	// if all the paks are cached, just succeed
	if (Chunk.IsCached())
	{
		ExecuteNextTick(OnCallback, true);
		return;
	}

	// queue the download
	DownloadChunkInternal(Chunk, OnCallback, Priority);

	// resave manifest if needed
	SaveLocalManifest(false);
	ComputeLoadingStats();
}

int32 UDreamChunkDownloaderSubsystem::FlushCache()
{
	IFileManager& FileManager = IFileManager::Get();

	// wait for all mounts to finish
	WaitForMounts();

	DCD_LOG(Display, TEXT("Flushing chunk caches at %s"), *CacheFolder);
	int FilesDeleted = 0, FilesSkipped = 0;
	for (const auto& It : Chunks)
	{
		const TSharedRef<FDreamChunk>& Chunk = It.Value;
		check(Chunk->MountTask == nullptr); // we waited for mounts

		// cancel background downloads
		bool bDownloadPending = false;
		for (const TSharedRef<FDreamPakFile>& PakFile : Chunk->PakFiles)
		{
			if (PakFile->Download.IsValid() && !PakFile->Download->HasCompleted())
			{
				// skip paks that are being downloaded
				bDownloadPending = true;
				break;
			}
		}

		// skip chunks that have a foreground download pending
		if (bDownloadPending)
		{
			for (const TSharedRef<FDreamPakFile>& PakFile : Chunk->PakFiles)
			{
				if (PakFile->SizeOnDisk > 0)
				{
					// log that we skipped this one
					DCD_LOG(Warning, TEXT("Could not flush %s (chunk %d) due to download in progress."), *PakFile->Entry.FileName, Chunk->ChunkId);
					++FilesSkipped;
				}
			}
		}
		else
		{
			// delete paks
			for (const TSharedRef<FDreamPakFile>& PakFile : Chunk->PakFiles)
			{
				if (PakFile->SizeOnDisk > 0 && !PakFile->bIsEmbedded)
				{
					// log that we deleted this one
					FString FullPathOnDisk = CacheFolder / PakFile->Entry.FileName;
					if (ensure(FileManager.Delete(*FullPathOnDisk)))
					{
						DCD_LOG(Log, TEXT("Deleted %s (chunk %d)."), *FullPathOnDisk, Chunk->ChunkId);
						++FilesDeleted;

						// flag uncached (may have been partial)
						PakFile->bIsCached = false;
						PakFile->SizeOnDisk = 0;
						bNeedsManifestSave = true;
					}
					else
					{
						// log an error (best we can do)
						DCD_LOG(Error, TEXT("Unable to delete %s"), *FullPathOnDisk);
						++FilesSkipped;
					}
				}
			}
		}
	}

	// resave the manifest
	SaveLocalManifest(false);

	DCD_LOG(Display, TEXT("Chunk cache flush complete. %d files deleted. %d files skipped."), FilesDeleted, FilesSkipped);
	return FilesSkipped;
}

int UDreamChunkDownloaderSubsystem::ValidateCache()
{
	IFileManager& FileManager = IFileManager::Get();

	// wait for all mounts to finish
	WaitForMounts();

	DCD_LOG(Display, TEXT("Starting inline chunk validation."));
	int ValidFiles = 0, InvalidFiles = 0, SkippedFiles = 0;
	for (const auto& It : PakFiles)
	{
		const TSharedRef<FDreamPakFile>& PakFile = It.Value;
		if (PakFile->bIsCached && !PakFile->bIsEmbedded)
		{
			// we know how to validate certain hash versions
			bool bFileIsValid = false;
			if (PakFile->Entry.FileVersion.StartsWith(TEXT("SHA1:")))
			{
				// check the sha1 hash
				bFileIsValid = FDreamChunkDownloaderUtils::CheckFileSha1Hash(CacheFolder / PakFile->Entry.FileName, PakFile->Entry.FileVersion);
			}
			else
			{
				// we don't know how to validate this version format
				DCD_LOG(Warning, TEXT("Unable to validate %s with version '%s'."), *PakFile->Entry.FileName, *PakFile->Entry.FileVersion);
				++SkippedFiles;
				continue;
			}

			// see if it's valid or not
			if (bFileIsValid)
			{
				// log valid
				DCD_LOG(Log, TEXT("%s matches hash '%s'."), *PakFile->Entry.FileName, *PakFile->Entry.FileVersion);
				++ValidFiles;
			}
			else
			{
				// log invalid
				DCD_LOG(Warning, TEXT("%s does NOT match hash '%s'."), *PakFile->Entry.FileName, *PakFile->Entry.FileVersion);
				++InvalidFiles;

				// delete invalid files
				FString FullPathOnDisk = CacheFolder / PakFile->Entry.FileName;
				if (ensure(FileManager.Delete(*FullPathOnDisk)))
				{
					DCD_LOG(Log, TEXT("Deleted invalid pak %s (chunk %d)."), *FullPathOnDisk, PakFile->Entry.ChunkId);
					PakFile->bIsCached = false;
					PakFile->SizeOnDisk = 0;
					bNeedsManifestSave = true;
				}
			}
		}
	}

	// resave the manifest
	SaveLocalManifest(false);

	DCD_LOG(Display, TEXT("Chunk validation complete. %d valid, %d invalid, %d skipped"), ValidFiles, InvalidFiles, SkippedFiles);
	return InvalidFiles;
}

void UDreamChunkDownloaderSubsystem::BeginLoadingMode(const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback)
{
	check(OnCallback); // you can't start loading mode without a valid callback

	// see if we're already in loading mode
	if (PostLoadCallbacks.Num() > 0)
	{
		DCD_LOG(Log, TEXT("JoinLoadingMode"));

		// just wait on the existing loading mode to finish
		PostLoadCallbacks.Add(OnCallback);
		return;
	}

	// start loading mode
	DCD_LOG(Log, TEXT("BeginLoadingMode"));
#if PLATFORM_ANDROID || PLATFORM_IOS
	FPlatformApplicationMisc::ControlScreensaver(FPlatformApplicationMisc::Disable);
#endif

	// reset stats
	LoadingModeStats.LastError = FText();
	LoadingModeStats.BytesDownloaded = 0;
	LoadingModeStats.FilesDownloaded = 0;
	LoadingModeStats.ChunksMounted = 0;
	LoadingModeStats.LoadingStartTime = FDateTime::UtcNow();
	ComputeLoadingStats(); // recompute before binding callback in case there's nothing queued yet

	// set the callback
	PostLoadCallbacks.Add(OnCallback);
	LoadingCompleteLatch = 0;

	// compute again next frame (if nothing's queued by then, we'll fire the callback
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float dts)
	{
		if (!IsValid(this) || this->PostLoadCallbacks.Num() <= 0)
		{
			return false; // stop ticking
		}
		return this->UpdateLoadingMode();
	}));
}

bool UDreamChunkDownloaderSubsystem::StartPatchGame(int InManifestFileDownloadHostIndex)
{
	DCD_LOG(Log, TEXT("StartPatchGame requested with host index %d"), InManifestFileDownloadHostIndex);

	if (!bIsDownloadManifestUpToDate)
	{
		DCD_LOG(Warning, TEXT("Chunk manifest is not up to date, attempting to update..."));

		UpdateBuild(LastDeploymentName, ContentBuildId, [this, InManifestFileDownloadHostIndex](bool bUpdateSuccess)
		{
			if (bUpdateSuccess)
			{
				DCD_LOG(Log, TEXT("Manifest update completed, retrying patch start"));
				StartPatchGame(InManifestFileDownloadHostIndex);
			}
			else
			{
				DCD_LOG(Error, TEXT("Failed to update manifest, cannot start patch"));
				OnPatchCompletedInternal.Broadcast(false);
			}
		});
		return true; // 异步处理中
	}

	// 检查所有需要的chunk状态
	TArray<int32> MountedChunks;
	TArray<int32> CachedChunks;
	TArray<int32> DownloadableChunks;
	TArray<int32> MissingChunks;

	for (int32 ChunkId : ChunkDownloadList)
	{
		EDreamChunkStatus Status = GetChunkStatus(ChunkId);
		DCD_LOG(Log, TEXT("Chunk %d status: %s"), ChunkId, *UEnum::GetValueAsString(Status));

		switch (Status)
		{
		case EDreamChunkStatus::Mounted:
			MountedChunks.Add(ChunkId);
			break;

		case EDreamChunkStatus::Cached:
			CachedChunks.Add(ChunkId);
			break;

		case EDreamChunkStatus::Remote:
		case EDreamChunkStatus::Downloading:
		case EDreamChunkStatus::Partial:
			DownloadableChunks.Add(ChunkId);
			break;

		case EDreamChunkStatus::Unknown:
		default:
			MissingChunks.Add(ChunkId);
			break;
		}
	}

	// 如果有缺失的chunks，无法继续
	if (MissingChunks.Num() > 0)
	{
		auto ConvertIntArrayToString = [](const TArray<int32>& IntArray) -> FString
		{
			TArray<FString> StringArray;
			for (int32 Value : IntArray)
			{
				StringArray.Add(FString::FromInt(Value));
			}
			return FString::Join(StringArray, TEXT(", "));
		};

		DCD_LOG(Error, TEXT("Some chunks are missing from manifest: %s"), *ConvertIntArrayToString(MissingChunks));
		return false;
	}

	// 如果所有chunks都已经mounted，直接完成
	if (MountedChunks.Num() == ChunkDownloadList.Num())
	{
		DCD_LOG(Log, TEXT("All chunks are already mounted, patch completed"));
		OnPatchCompletedInternal.Broadcast(true);
		return true;
	}

	DCD_LOG(Log, TEXT("Patch status: %d mounted, %d cached, %d need download"),
	        MountedChunks.Num(), CachedChunks.Num(), DownloadableChunks.Num());

	// 开始loading mode来跟踪进度
	BeginLoadingMode([this](bool bLoadingSuccess)
	{
		HandleLoadingModeCompleted(bLoadingSuccess);
	});

	// 需要处理的chunks
	TArray<int32> ChunksNeedProcessing;
	ChunksNeedProcessing.Append(CachedChunks);
	ChunksNeedProcessing.Append(DownloadableChunks);

	if (DownloadableChunks.Num() > 0)
	{
		DCD_LOG(Log, TEXT("Starting download for %d chunks"), DownloadableChunks.Num());

		DownloadChunks(DownloadableChunks, [this, ChunksNeedProcessing](bool bDownloadSuccess)
		{
			if (bDownloadSuccess)
			{
				DCD_LOG(Log, TEXT("Download completed, starting mount for all processed chunks"));
				MountChunks(ChunksNeedProcessing, [this](bool bMountSuccess)
				{
					HandleMountCompleted(bMountSuccess);
					HandleDownloadCompleted(bMountSuccess);
				});
			}
			else
			{
				DCD_LOG(Error, TEXT("Download failed"));
				HandleDownloadCompleted(false);
			}
		}, 0);
	}
	else if (CachedChunks.Num() > 0)
	{
		// 只需要mount已缓存的chunks
		DCD_LOG(Log, TEXT("No downloads needed, mounting %d cached chunks"), CachedChunks.Num());
		MountChunks(CachedChunks, [this](bool bMountSuccess)
		{
			HandleMountCompleted(bMountSuccess);
			HandleDownloadCompleted(bMountSuccess);
		});
	}
	else
	{
		// 所有chunks都已处理完毕
		HandleDownloadCompleted(true);
	}

	return true;
}

bool UDreamChunkDownloaderSubsystem::StartPatchGameWithDelegate(
	int InManifestFileDownloadHostIndex,
	const FDreamChunkDownloaderCallbackEvent& InOnPatchCompletedEvent,
	const FDreamChunkDownloaderCallbackEvent& InOnMountCompletedEvent)
{
	OnPatchCompletedInternal.AddLambda([this, InOnPatchCompletedEvent](bool bSuccess)
	{
		InOnPatchCompletedEvent.Execute(bSuccess);
	});
	OnMountCompletedInternal.AddLambda([this, InOnMountCompletedEvent](bool bSuccess)
	{
		InOnMountCompletedEvent.Execute(bSuccess);
	});
	return StartPatchGame(InManifestFileDownloadHostIndex);
}

void UDreamChunkDownloaderSubsystem::HandleDownloadCompleted(bool bSuccess)
{
	if (bSuccess)
	{
		DCD_LOG(Log, TEXT("Download completed successfully."));

		auto AllChunksIsThisState = [this](EDreamChunkStatus State) -> bool
		{
			bool bSuccess = true;

			for (int ChunkID : ChunkDownloadList)
			{
				bSuccess = GetChunkStatus(ChunkID) == State;
				if (!bSuccess)
					return false;
			}

			return bSuccess;
		};

		if (AllChunksIsThisState(EDreamChunkStatus::Mounted))
		{
			DCD_LOG(Log, TEXT("All chunks are mounted, patch completed"));
			return;
		}

		// 添加详细的chunk状态信息
		for (int ChunkID : ChunkDownloadList)
		{
			EDreamChunkStatus Status = GetChunkStatus(ChunkID);
			DCD_LOG(Log, TEXT("Chunk %d status after download: %s"), ChunkID, *UEnum::GetValueAsString(Status));
		}

		// 自动挂载下载完成的chunks
		FJsonSerializableArrayInt DownloadedChunks;
		for (int ChunkID : ChunkDownloadList)
		{
			// 只挂载已经完全下载的chunks
			EDreamChunkStatus Status = GetChunkStatus(ChunkID);
			if (Status == EDreamChunkStatus::Cached)
			{
				DownloadedChunks.Add(ChunkID);
			}
		}

		if (DownloadedChunks.Num() > 0)
		{
			auto ConvertIntArrayToString = [](const FJsonSerializableArrayInt& IntArray) -> TArray<FString>
			{
				TArray<FString> StringArray;
				for (int32 Value : IntArray)
				{
					StringArray.Add(FString::FromInt(Value));
				}
				return StringArray;
			};
			DCD_LOG(Log, TEXT("Mounting %d downloaded chunks: %s"), DownloadedChunks.Num(),
			        *FString::Join(ConvertIntArrayToString(DownloadedChunks), TEXT(", ")));

			MountChunks(DownloadedChunks, [this](bool bMountSuccess)
			{
				HandleMountCompleted(bMountSuccess);
			});
		}
		else
		{
			// 记录更详细的警告信息
			FString ChunkStatuses;
			for (int ChunkID : ChunkDownloadList)
			{
				if (!ChunkStatuses.IsEmpty()) ChunkStatuses += TEXT(", ");
				EDreamChunkStatus Status = GetChunkStatus(ChunkID);
				ChunkStatuses += FString::Printf(TEXT("%d:%s"), ChunkID, *UEnum::GetValueAsString(Status));
			}

			DCD_LOG(Warning, TEXT("No chunks ready for mounting after download. Chunk statuses: %s"), *ChunkStatuses);
			OnPatchCompletedInternal.Broadcast(false);
		}
	}
	else
	{
		DCD_LOG(Error, TEXT("Download failed."));
		OnPatchCompletedInternal.Broadcast(false);
	}
}

void UDreamChunkDownloaderSubsystem::HandleLoadingModeCompleted(bool bSuccess)
{
	OnPatchCompletedInternal.Broadcast(bSuccess);
}

void UDreamChunkDownloaderSubsystem::HandleMountCompleted(bool bSuccess)
{
	OnMountCompletedInternal.Broadcast(bSuccess);
}

EDreamChunkStatus UDreamChunkDownloaderSubsystem::GetChunkStatus(int32 ChunkId) const
{
	// do we know about this chunk at all?
	const TSharedRef<FDreamChunk>* ChunkPtr = Chunks.Find(ChunkId);
	if (ChunkPtr == nullptr)
	{
		return EDreamChunkStatus::Unknown;
	}
	const FDreamChunk& Chunk = **ChunkPtr;

	// if it has no pak files, treat it the same as not found (shouldn't happen)
	if (!ensure(Chunk.PakFiles.Num() > 0))
	{
		return EDreamChunkStatus::Unknown;
	}

	// see if it's fully mounted
	if (Chunk.bIsMounted)
	{
		return EDreamChunkStatus::Mounted;
	}

	// count the number of paks in flight vs local
	int32 NumPaks = Chunk.PakFiles.Num(), NumCached = 0, NumDownloading = 0;
	for (const TSharedRef<FDreamPakFile>& PakFile : Chunk.PakFiles)
	{
		if (PakFile->bIsCached)
		{
			++NumCached;
		}
		else if (PakFile->Download.IsValid())
		{
			++NumDownloading;
		}
	}

	if (NumCached >= NumPaks)
	{
		// all cached
		return EDreamChunkStatus::Cached;
	}
	else if (NumCached + NumDownloading >= NumPaks)
	{
		// some downloads still in progress
		return EDreamChunkStatus::Downloading;
	}
	else if (NumCached + NumDownloading > 0)
	{
		// any progress at all? (might be paused or partially preserved from manifest update)
		return EDreamChunkStatus::Partial;
	}

	// nothing
	return EDreamChunkStatus::Remote;
}

void UDreamChunkDownloaderSubsystem::GetAllChunkIds(TArray<int32>& ChunkIds) const
{
	Chunks.GetKeys(ChunkIds);
}

void UDreamChunkDownloaderSubsystem::SetContentBuildId(const FString& DeploymentName, const FString& NewContentBuildId)
{
	// save the content build id
	ContentBuildId = NewContentBuildId;
	LastDeploymentName = DeploymentName;
	DCD_LOG(Display, TEXT("Deployment = %s, ContentBuildId = %s"), *DeploymentName, *ContentBuildId);

	// read CDN urls from deployment configs
	TArray<FString> CdnBaseUrls;
	TArray<FDreamChunkDownloaderDeploymentSet> Sets;
	Sets = UDreamChunkDownloaderSettings::Get()->DeploymentSets;
	for (const FDreamChunkDownloaderDeploymentSet& Set : Sets)
	{
		if (DeploymentName == Set.DeploymentName)
		{
			CdnBaseUrls = Set.Hosts;
			break;
		}
	}

	bool bFoundDeployment = false;
	for (const FDreamChunkDownloaderDeploymentSet& Set : UDreamChunkDownloaderSettings::Get()->DeploymentSets)
	{
		if (DeploymentName == Set.DeploymentName)
		{
			bFoundDeployment = true;
			CdnBaseUrls = Set.Hosts;
			break;
		}
	}

	if (!bFoundDeployment)
	{
		DCD_LOG(Error, TEXT("Deployment '%s' not found in settings"), *DeploymentName);
	}

	if (CdnBaseUrls.Num() <= 0)
	{
		DCD_LOG(Warning, TEXT("Please see the ProjectSettings DreamPlugin/Dream the Chunk Downloader Setting - > DeploymentSets and set! Count: %d"), UDreamChunkDownloaderSettings::Get()->DeploymentSets.Num());
	}

	// combine CdnBaseUrls with ContentBuildId
	BuildBaseUrls.Empty();
	for (int32 i = 0, n = CdnBaseUrls.Num(); i < n; ++i)
	{
		const FString& BaseUrl = CdnBaseUrls[i];
		check(!BaseUrl.IsEmpty());
		FString BuildUrl = BaseUrl / ContentBuildId;
		DCD_LOG(Display, TEXT("ContentBaseUrl[%d] = %s"), i, *BuildUrl);
		BuildBaseUrls.Add(BuildUrl);
	}
}

void UDreamChunkDownloaderSubsystem::LoadManifest(const TArray<FDreamPakFileEntry>& ManifestPakFiles)
{
	DCD_LOG(Display, TEXT("Beginning manifest load."));

	// wait for all mounts to finish
	WaitForMounts();

	// trigger garbage collection (give any unmounts which are about to happen a good chance of success)
	CollectGarbage(RF_NoFlags);

	// group the manifest paks by chunk ID (maintain ordering)
	TMap<int32, TArray<FDreamPakFileEntry>> Manifest;
	for (const FDreamPakFileEntry& FileEntry : ManifestPakFiles)
	{
		check(FileEntry.ChunkId >= 0);
		Manifest.FindOrAdd(FileEntry.ChunkId).Add(FileEntry);
	}

	// copy old chunk map (we will reuse any that still exist)
	TMap<int32, TSharedRef<FDreamChunk>> OldChunks = MoveTemp(Chunks);
	TMap<FString, TSharedRef<FDreamPakFile>> OldPakFiles = MoveTemp(PakFiles);

	// loop over the new chunks
	int32 NumChunks = 0, NumPaks = 0;
	for (const auto& It : Manifest)
	{
		int32 ChunkId = It.Key;

		// keep track of new chunk and old pak files
		TSharedPtr<FDreamChunk> Chunk;
		TArray<TSharedRef<FDreamPakFile>> PrevPakList;

		// create or reuse the chunk
		TSharedRef<FDreamChunk>* OldChunk = OldChunks.Find(ChunkId);
		if (OldChunk != nullptr)
		{
			// move over the old chunk
			Chunk = *OldChunk;
			check(Chunk->ChunkId == ChunkId);

			// don't clean it up later
			OldChunks.Remove(ChunkId);

			// move out OldPakFiles
			PrevPakList = MoveTemp(Chunk->PakFiles);
		}
		else
		{
			// make a brand new chunk
			Chunk = MakeShared<FDreamChunk>();
			Chunk->ChunkId = ChunkId;
		}

		// add the chunk to the new map
		Chunks.Add(Chunk->ChunkId, Chunk.ToSharedRef());

		// find or create new pak files
		check(Chunk->PakFiles.Num() == 0);
		for (const FDreamPakFileEntry& FileEntry : It.Value)
		{
			// see if there's an existing file for this one
			const TSharedRef<FDreamPakFile>* ExistingFilePtr = OldPakFiles.Find(FileEntry.FileName);
			if (ExistingFilePtr != nullptr)
			{
				const TSharedRef<FDreamPakFile>& ExistingFile = *ExistingFilePtr;
				if (ExistingFile->Entry.FileVersion == FileEntry.FileVersion)
				{
					// if version matched, size should too
					check(ExistingFile->Entry.FileSize == FileEntry.FileSize);

					// update and add to list (may populate ChunkId and RelativeUrl if we loaded from cache)
					ExistingFile->Entry = FileEntry;
					Chunk->PakFiles.Add(ExistingFile);
					PakFiles.Add(ExistingFile->Entry.FileName, ExistingFile);

					// remove from old pak files list
					OldPakFiles.Remove(FileEntry.FileName);
					continue;
				}
			}

			// create a new entry
			TSharedRef<FDreamPakFile> NewFile = MakeShared<FDreamPakFile>();
			NewFile->Entry = FileEntry;
			Chunk->PakFiles.Add(NewFile);
			PakFiles.Add(NewFile->Entry.FileName, NewFile);

			// see if it matches an embedded pak file
			const FDreamPakFileEntry* CachedEntry = EmbeddedPaks.Find(FileEntry.FileName);
			if (CachedEntry != nullptr && CachedEntry->FileVersion == FileEntry.FileVersion)
			{
				NewFile->bIsEmbedded = true;
				NewFile->bIsCached = true;
				NewFile->SizeOnDisk = CachedEntry->FileSize;
			}
		}

		// log the chunk and pak file count
		DCD_LOG(Verbose, TEXT("Found chunk %d (%d pak files)."), ChunkId, Chunk->PakFiles.Num());
		++NumChunks;
		NumPaks += Chunk->PakFiles.Num();

		// if the chunk is already mounted, we want to unmount any invalid data
		check(Chunk->MountTask == nullptr); // we already waited for mounts to finish
		if (Chunk->bIsMounted)
		{
			// see if all the existing pak files match to the new manifest (means it can stay mounted)
			// this is a common case so we're trying to be more efficient here
			int LongestCommonPrefix = 0;
			for (int i = 0; i < PrevPakList.Num() && i < Chunk->PakFiles.Num(); ++i, ++LongestCommonPrefix)
			{
				if (Chunk->PakFiles[i]->Entry.FileVersion != PrevPakList[i]->Entry.FileVersion)
				{
					break;
				}
			}

			// if they don't all match we need to remount
			if (LongestCommonPrefix != PrevPakList.Num() || LongestCommonPrefix != Chunk->PakFiles.Num())
			{
				// this chunk is no longer fully mounted
				Chunk->bIsMounted = false;

				// unmount any old paks that didn't match (reverse order)
				for (int i = PrevPakList.Num() - 1; i >= 0; --i)
				{
					UnmountPakFile(PrevPakList[i]);
				}

				// unmount any new paks that didn't match (may have changed position) (reverse order)
				// any new pak files unmounted will be re-mounted (in the right order) if this chunk is requested again
				for (int i = Chunk->PakFiles.Num() - 1; i >= 0; --i)
				{
					UnmountPakFile(Chunk->PakFiles[i]);
				}
			}
		}
	}

	// any files still left in OldPakFiles should be cancelled, unmounted, and deleted
	IFileManager& FileManager = IFileManager::Get();
	for (const auto& It : OldPakFiles)
	{
		const TSharedRef<FDreamPakFile>& File = It.Value;
		DCD_LOG(Log, TEXT("Removing orphaned pak file %s (was chunk %d)."), *File->Entry.FileName, File->Entry.ChunkId);

		// cancel downloads of pak files that are no longer valid
		if (File->Download.IsValid())
		{
			// treat these cancellations as successful since the pak is no longer needed (we've successfully downloaded nothing)
			CancelDownload(File, true);
		}

		// if a chunk completely disappeared we may need to clean up its mounts this way (otherwise would have been taken care of above)
		if (File->bIsMounted)
		{
			UnmountPakFile(File);
		}

		// delete any locally cached file
		if (File->SizeOnDisk > 0 && !File->bIsEmbedded)
		{
			bNeedsManifestSave = true;
			FString FullPathOnDisk = CacheFolder / File->Entry.FileName;
			if (!ensure(FileManager.Delete(*FullPathOnDisk)))
			{
				DCD_LOG(Error, TEXT("Failed to delete orphaned pak %s."), *FullPathOnDisk);
			}
		}
	}

	// resave the manifest
	SaveLocalManifest(false);

	// log end
	check(ManifestPakFiles.Num() == NumPaks);
	DCD_LOG(Display, TEXT("Manifest load complete. %d chunks with %d pak files."), NumChunks, NumPaks);
}

void UDreamChunkDownloaderSubsystem::TryLoadBuildManifest(int TryNumber)
{
	// load the local build manifest
	TMap<FString, FString> CachedManifestProps;
	TSharedPtr<FJsonObject> JsonObject;
	TArray<FDreamPakFileEntry> CachedManifest = FDreamChunkDownloaderUtils::ParseManifest(
		CacheFolder / UDreamChunkDownloaderSettings::Get()->CachedBuildManifestFileName,
		JsonObject,
		&CachedManifestProps // 使用三参数版本
	);

	// 其余代码保持不变...

	// Check if we have a valid cached manifest that matches our build ID
	const FString* CachedBuildId = CachedManifestProps.Find(BUILD_ID_KEY);
	bool bManifestMatches = (CachedBuildId != nullptr && *CachedBuildId == ContentBuildId);
	bool bManifestValid = (CachedManifest.Num() > 0 && bManifestMatches);

	if (bManifestValid)
	{
		// Cached build manifest is up to date, load this one
		DCD_LOG(Log, TEXT("Using cached manifest for build ID: %s"), *ContentBuildId);
		LoadManifest(CachedManifest);

		// Execute and clear the callback - SUCCESS
		FDreamChunkDownloaderTypes::FDreamCallback Callback = MoveTemp(UpdateBuildCallback);
		ExecuteNextTick(Callback, true);
		return;
	}

	// Need to download new manifest
	if (BuildBaseUrls.Num() <= 0)
	{
		DCD_LOG(Error, TEXT("Unable to download build manifest. No CDN urls configured."));
		LoadingModeStats.LastError = LOCTEXT("UnableToDownloadManifest", "Unable to download build manifest. (NoCDN)");

		FDreamChunkDownloaderTypes::FDreamCallback Callback = MoveTemp(UpdateBuildCallback);
		ExecuteNextTick(Callback, false);
		return;
	}

	// Check retry limits to prevent infinite loops
	const int32 MAX_MANIFEST_RETRIES = 10;
	if (TryNumber >= MAX_MANIFEST_RETRIES)
	{
		DCD_LOG(Error, TEXT("Maximum manifest download retries (%d) exceeded"), MAX_MANIFEST_RETRIES);
		LoadingModeStats.LastError = LOCTEXT("ManifestMaxRetriesExceeded", "Maximum manifest download retries exceeded");

		FDreamChunkDownloaderTypes::FDreamCallback Callback = MoveTemp(UpdateBuildCallback);
		ExecuteNextTick(Callback, false);
		return;
	}

	// Fast path the first try - no delay
	if (TryNumber <= 0)
	{
		TryDownloadBuildManifest(TryNumber);
		return;
	}

	// Compute delay before re-attempting download
	float SecondsToDelay = FMath::Min(TryNumber * 5.0f, 60.0f);

	DCD_LOG(Log, TEXT("Will re-attempt manifest download in %f seconds (attempt %d/%d)"),
	        SecondsToDelay, TryNumber + 1, MAX_MANIFEST_RETRIES);

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, TryNumber](float Unused)
	{
		if (IsValid(this))
		{
			this->TryDownloadBuildManifest(TryNumber);
		}
		return false;
	}), SecondsToDelay);
}

void UDreamChunkDownloaderSubsystem::TryDownloadBuildManifest(int TryNumber)
{
	check(BuildBaseUrls.Num() > 0);

	// 修复：如果已有请求在进行中，先取消它
	if (ManifestRequest.IsValid())
	{
		DCD_LOG(Warning, TEXT("Previous manifest request still active, cancelling it"));
		ManifestRequest->CancelRequest();
		ManifestRequest.Reset();
	}

	// Download the manifest from CDN
	FString ManifestFileName = FString::Printf(TEXT("BuildManifest-%s.json"), *PlatformName);
	FString Url = BuildBaseUrls[TryNumber % BuildBaseUrls.Num()] / ManifestFileName;

	if (UDreamChunkDownloaderSettings::Get()->bUseStaticRemoteHost)
	{
		Url = UDreamChunkDownloaderSettings::Get()->StaticRemoteHost / ManifestFileName;
		DCD_LOG(Log, TEXT("Using static remote host: %s"), *Url);
	}

	DCD_LOG(Log, TEXT("Downloading build manifest (attempt #%d) from %s"), TryNumber + 1, *Url);

	// Download the manifest from the root CDN
	FHttpModule& HttpModule = FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");

	ManifestRequest = HttpModule.Get().CreateRequest();
	ManifestRequest->SetURL(Url);
	ManifestRequest->SetVerb(TEXT("GET"));

	// Set reasonable timeout
	ManifestRequest->SetTimeout(30.0f);

	FString CachedManifestFullPath = CacheFolder / UDreamChunkDownloaderSettings::Get()->CachedBuildManifestFileName;

	// 使用弱引用避免循环引用
	TWeakObjectPtr<UDreamChunkDownloaderSubsystem> WeakThis(this);

	ManifestRequest->OnProcessRequestComplete().BindLambda([WeakThis, TryNumber, CachedManifestFullPath](
		FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSuccess)
		{
			// 检查subsystem是否仍然有效
			if (!WeakThis.IsValid())
			{
				DCD_LOG(Warning, TEXT("Subsystem was destroyed while downloading manifest '%s'"), *HttpRequest->GetURL());
				return;
			}

			UDreamChunkDownloaderSubsystem* Self = WeakThis.Get();

			// 清理请求引用
			if (Self->ManifestRequest.IsValid() && Self->ManifestRequest.Get() == HttpRequest.Get())
			{
				Self->ManifestRequest.Reset();
			}

			FText LastError;
			bool bDownloadSuccess = false;

			if (bSuccess && HttpResponse.IsValid())
			{
				const int32 HttpStatus = HttpResponse->GetResponseCode();
				if (EHttpResponseCodes::IsOk(HttpStatus))
				{
					const FString ResponseContent = HttpResponse->GetContentAsString();

					// Validate that we got actual JSON content
					if (!ResponseContent.IsEmpty())
					{
						// Try to parse the JSON to make sure it's valid before saving
						TSharedPtr<FJsonObject> JsonObject;
						TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

						if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
						{
							// Add our build ID to the manifest before saving
							JsonObject->SetStringField(BUILD_ID_KEY, Self->ContentBuildId);

							// Serialize back to string with build ID
							FString OutputString;
							TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
							if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
							{
								// Save the manifest with build ID to file
								if (FDreamChunkDownloaderUtils::WriteStringAsUtf8TextFile(OutputString, CachedManifestFullPath))
								{
									DCD_LOG(Log, TEXT("Successfully downloaded and saved manifest with build ID: %s"), *Self->ContentBuildId);
									bDownloadSuccess = true;
								}
								else
								{
									DCD_LOG(Error, TEXT("Failed to write manifest to '%s'"), *CachedManifestFullPath);
									LastError = FText::Format(LOCTEXT("FailedToWriteManifest", "[Try {0}] Failed to write manifest."), FText::AsNumber(TryNumber + 1));
								}
							}
							else
							{
								DCD_LOG(Error, TEXT("Failed to serialize manifest JSON"));
								LastError = FText::Format(LOCTEXT("FailedToSerializeManifest", "[Try {0}] Failed to serialize manifest JSON."), FText::AsNumber(TryNumber + 1));
							}
						}
						else
						{
							DCD_LOG(Error, TEXT("Downloaded manifest contains invalid JSON from '%s'"), *HttpRequest->GetURL());
							LastError = FText::Format(LOCTEXT("ManifestInvalidJson", "[Try {0}] Downloaded manifest contains invalid JSON."), FText::AsNumber(TryNumber + 1));
						}
					}
					else
					{
						DCD_LOG(Error, TEXT("Downloaded manifest is empty from '%s'"), *HttpRequest->GetURL());
						LastError = FText::Format(LOCTEXT("ManifestEmpty", "[Try {0}] Downloaded manifest is empty."), FText::AsNumber(TryNumber + 1));
					}
				}
				else
				{
					DCD_LOG(Error, TEXT("HTTP %d while downloading manifest from '%s'"), HttpStatus, *HttpRequest->GetURL());
					LastError = FText::Format(LOCTEXT("ManifestHttpError_FailureCode", "[Try {0}] Manifest download failed (HTTP {1})"),
					                          FText::AsNumber(TryNumber + 1), FText::AsNumber(HttpStatus));
				}
			}
			else
			{
				DCD_LOG(Error, TEXT("HTTP connection issue while downloading manifest '%s'"), *HttpRequest->GetURL());
				LastError = FText::Format(LOCTEXT("ManifestHttpError_Generic", "[Try {0}] Connection issues downloading manifest. Check your network connection..."),
				                          FText::AsNumber(TryNumber + 1));
			}

			// Update error state
			Self->LoadingModeStats.LastError = LastError;

			if (bDownloadSuccess)
			{
				// Successfully downloaded and saved manifest, now try to load it
				DCD_LOG(Log, TEXT("Manifest download successful, attempting to load..."));
				Self->TryLoadBuildManifest(0); // Reset try count since download succeeded
			}
			else
			{
				// Download failed, retry with incremented count
				DCD_LOG(Warning, TEXT("Manifest download failed, will retry..."));
				Self->TryLoadBuildManifest(TryNumber + 1);
			}
		});


	// Start the HTTP request
	if (!ManifestRequest->ProcessRequest())
	{
		DCD_LOG(Error, TEXT("Failed to start manifest download request"));
		ManifestRequest.Reset();

		// 延迟重试
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, TryNumber](float)
		{
			if (IsValid(this))
			{
				TryLoadBuildManifest(TryNumber + 1);
			}
			return false;
		}), 1.0f);
	}
}

void UDreamChunkDownloaderSubsystem::WaitForMounts()
{
	bool bWaiting = false;

	for (const auto& It : Chunks)
	{
		const TSharedRef<FDreamChunk>& Chunk = It.Value;
		if (Chunk->MountTask != nullptr)
		{
			if (!bWaiting)
			{
				DCD_LOG(Display, TEXT("Waiting for chunk mounts to complete..."));
				bWaiting = true;
			}

			// wait for the async task to end
			Chunk->MountTask->EnsureCompletion(true);

			// complete the task on the main thread
			CompleteMountTask(*Chunk);
			check(Chunk->MountTask == nullptr);
		}
	}

	if (bWaiting)
	{
		DCD_LOG(Display, TEXT("...chunk mounts finished."));
	}
}

void UDreamChunkDownloaderSubsystem::SaveLocalManifest(bool bForce)
{
	if (!bForce && !bNeedsManifestSave)
	{
		return;
	}

	// 构建JSON数据
	int32 NumEntries = 0;
	TArray<TSharedRef<FDreamPakFile>> ValidPakFiles;

	for (const auto& It : PakFiles)
	{
		if (!It.Value->bIsEmbedded && (It.Value->SizeOnDisk > 0 || It.Value->Download.IsValid()))
		{
			ValidPakFiles.Add(It.Value);
			++NumEntries;
		}
	}

	FString JsonData;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonData);
	Writer->WriteObjectStart();

	Writer->WriteValue(ENTRIES_COUNT_FIELD, NumEntries);

	Writer->WriteArrayStart(ENTRIES_FIELD);
	for (const TSharedRef<FDreamPakFile>& PakFile : ValidPakFiles)
	{
		const FDreamPakFileEntry& Entry = PakFile->Entry;
		Writer->WriteObjectStart();
		Writer->WriteValue(FILE_NAME_FIELD, Entry.FileName);
		Writer->WriteValue(FILE_SIZE_FIELD, Entry.FileSize);
		Writer->WriteValue(FILE_VERSION_FIELD, Entry.FileVersion);
		Writer->WriteValue(FILE_CHUNK_ID_FIELD, -1); // 本地manifest中设为-1
		Writer->WriteValue(FILE_RELATIVE_URL_FIELD, TEXT("/"));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	if (UDreamChunkDownloaderSettings::Get()->bUseStaticRemoteHost)
	{
		Writer->WriteArrayStart(DOWNLOAD_CHUNK_ID_LIST_FIELD);
		for (int32 ChunkId : ChunkDownloadList)
		{
			Writer->WriteValue(ChunkId);
		}
		Writer->WriteArrayEnd();

		Writer->WriteValue(CLIENT_BUILD_ID, ContentBuildId);
	}

	Writer->WriteObjectEnd();
	Writer->Close();

	FString ManifestPath = CacheFolder / UDreamChunkDownloaderSettings::Get()->LocalManifestFileName;
	FString TempPath = ManifestPath + TEXT(".tmp");

	bool bWriteSuccess = FDreamChunkDownloaderUtils::WriteStringAsUtf8TextFile(JsonData, TempPath);
	if (bWriteSuccess)
	{
		// 验证临时文件
		FString ErrorMessage;
		if (ValidateManifestFile(TempPath, ErrorMessage))
		{
			// 原子性替换
			if (IFileManager::Get().Move(*ManifestPath, *TempPath))
			{
				bNeedsManifestSave = false;
				DCD_LOG(Log, TEXT("Successfully saved local manifest with %d entries"), NumEntries);
			}
			else
			{
				DCD_LOG(Error, TEXT("Failed to move temp manifest file from '%s' to '%s'"), *TempPath, *ManifestPath);
				IFileManager::Get().Delete(*TempPath); // 清理临时文件
			}
		}
		else
		{
			DCD_LOG(Error, TEXT("Validation failed for temp manifest: %s"), *ErrorMessage);
			IFileManager::Get().Delete(*TempPath); // 清理无效的临时文件
		}
	}
	else
	{
		DCD_LOG(Error, TEXT("Failed to write temp manifest file: '%s'"), *TempPath);
	}
}

bool UDreamChunkDownloaderSubsystem::UpdateLoadingMode()
{
	// recompute loading stats
	ComputeLoadingStats();

	// check for the end of loading mode
	if (LoadingModeStats.FilesDownloaded >= LoadingModeStats.TotalFilesToDownload &&
		LoadingModeStats.ChunksMounted >= LoadingModeStats.TotalChunksToMount)
	{
		// make sure loading's been done for at least 2 frames before firing the callback
		// this adds a negligible amount of time to the loading screen but gives dependent loads a chance to queue
		static const int32 NUM_CONSECUTIVE_IDLE_FRAMES_FOR_LOADING_COMPLETION = 5;
		if (++LoadingCompleteLatch >= NUM_CONSECUTIVE_IDLE_FRAMES_FOR_LOADING_COMPLETION)
		{
			// end loading mode
			DCD_LOG(Log, TEXT("EndLoadingMode (%d files downloaded, %d chunks mounted)"), LoadingModeStats.FilesDownloaded, LoadingModeStats.ChunksMounted);
#if PLATFORM_ANDROID || PLATFORM_IOS
			FPlatformApplicationMisc::ControlScreensaver(FPlatformApplicationMisc::Enable);
#endif

			// fire any loading mode completion callbacks
			TArray<FDreamChunkDownloaderTypes::FDreamCallback> Callbacks = MoveTemp(PostLoadCallbacks);
			if (Callbacks.Num() > 0)
			{
				PostLoadCallbacks.Empty(); // shouldn't be necessary due to MoveTemp but just in case
				for (const auto& Callback : Callbacks)
				{
					Callback(LoadingModeStats.LastError.IsEmpty());
				}
			}
			return false; // stop ticking
		}
	}
	else
	{
		// reset the latch
		LoadingCompleteLatch = 0;
	}

	return true; // keep ticking
}

void UDreamChunkDownloaderSubsystem::ComputeLoadingStats()
{
	LoadingModeStats.TotalBytesToDownload = LoadingModeStats.BytesDownloaded;
	LoadingModeStats.TotalFilesToDownload = LoadingModeStats.FilesDownloaded;
	LoadingModeStats.TotalChunksToMount = LoadingModeStats.ChunksMounted;

	// loop over all chunks
	for (const auto& It : Chunks)
	{
		const TSharedRef<FDreamChunk>& Chunk = It.Value;

		// if it's mounting, add files to mount
		if (Chunk->MountTask != nullptr)
		{
			++LoadingModeStats.TotalChunksToMount;
		}
	}

	// check downloads
	for (const TSharedRef<FDreamPakFile>& PakFile : DownloadRequests)
	{
		++LoadingModeStats.TotalFilesToDownload;
		if (PakFile->Download.IsValid())
		{
			LoadingModeStats.TotalBytesToDownload += PakFile->Entry.FileSize - PakFile->Download->GetProgress();
		}
		else
		{
			LoadingModeStats.TotalBytesToDownload += PakFile->Entry.FileSize;
		}
	}
}

void UDreamChunkDownloaderSubsystem::UnmountPakFile(const TSharedRef<FDreamPakFile>& PakFile)
{
	// if it's already unmounted, don't do anything
	if (PakFile->bIsMounted)
	{
		// unmount
		if (ensure(FCoreDelegates::OnUnmountPak.IsBound()))
		{
			FString FullPathOnDisk = (PakFile->bIsEmbedded ? EmbeddedFolder : CacheFolder) / PakFile->Entry.FileName;
			if (ensure(FCoreDelegates::OnUnmountPak.Execute(FullPathOnDisk)))
			{
				// clear the mounted flag
				PakFile->bIsMounted = false;
			}
			else
			{
				DCD_LOG(Error, TEXT("Unable to unmount %s"), *FullPathOnDisk);
			}
		}
		else
		{
			DCD_LOG(Error, TEXT("Unable to unmount %s because no OnUnmountPak is bound"), *PakFile->Entry.FileName);
		}
	}
}

void UDreamChunkDownloaderSubsystem::CancelDownload(const TSharedRef<FDreamPakFile>& PakFile, bool bResult)
{
	if (PakFile->Download.IsValid())
	{
		// cancel the download itself
		PakFile->Download->Cancel(bResult);
		check(!PakFile->Download.IsValid());
	}
}

void UDreamChunkDownloaderSubsystem::DownloadPakFileInternal(const TSharedRef<FDreamPakFile>& PakFile, const FDreamChunkDownloaderTypes::FDreamCallback& Callback, int32 Priority)
{
	check(BuildBaseUrls.Num() > 0);

	// increase priority if it's updated
	if (Priority > PakFile->Priority)
	{
		// if the download has already started this won't really change anything
		PakFile->Priority = Priority;
	}

	// just piggyback on the existing post-download callback
	if (Callback)
	{
		PakFile->PostDownloadCallbacks.Add(Callback);
	}

	// see if the download is already started
	if (PakFile->Download.IsValid())
	{
		// nothing to do then (we already added our callback)
		return;
	}

	// add it to the downloading set
	DownloadRequests.AddUnique(PakFile);
	DownloadRequests.StableSort([](const TSharedRef<FDreamPakFile>& A, const TSharedRef<FDreamPakFile>& B)
	{
		return A->Priority < B->Priority;
	});

	// start the first N pak files in flight
	IssueDownloads();
}

void UDreamChunkDownloaderSubsystem::MountChunkInternal(FDreamChunk& Chunk, const FDreamChunkDownloaderTypes::FDreamCallback& Callback)
{
	check(!Chunk.bIsMounted);

	// see if there's already a mount pending
	if (Chunk.MountTask != nullptr)
	{
		// join with the existing callbacks
		if (Callback)
		{
			Chunk.MountTask->GetTask().PostMountCallbacks.Add(Callback);
		}
		return;
	}

	// see if we need to trigger any downloads
	bool bAllPaksCached = true;
	for (const auto& PakFile : Chunk.PakFiles)
	{
		if (!PakFile->bIsCached)
		{
			bAllPaksCached = false;
			break;
		}
	}

	if (bAllPaksCached)
	{
		// if all pak files are cached, mount now
		DCD_LOG(Log, TEXT("Chunk %d mount requested (%d pak sequence)."), Chunk.ChunkId, Chunk.PakFiles.Num());

		// spin up a background task to mount the pak file
		check(Chunk.MountTask == nullptr);
		Chunk.MountTask = new FDreamChunkDownloaderTypes::FDreamMountTask();

		// configure the task
		FDreamPakMountWork& MountWork = Chunk.MountTask->GetTask();
		MountWork.ChunkId = Chunk.ChunkId;
		MountWork.CacheFolder = CacheFolder;
		MountWork.EmbeddedFolder = EmbeddedFolder;
		for (const TSharedRef<FDreamPakFile>& PakFile : Chunk.PakFiles)
		{
			if (!PakFile->bIsMounted)
			{
				MountWork.PakFiles.Add(PakFile);
			}
		}
		if (Callback)
		{
			MountWork.PostMountCallbacks.Add(Callback);
		}

		// start as a background task
		Chunk.MountTask->StartBackgroundTask();

		// start a per-frame ticker until mounts are finished
		if (!MountTicker.IsValid())
		{
			MountTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDreamChunkDownloaderSubsystem::UpdateMountTasks));
		}
	}
	else
	{
		// queue up pak file downloads
		int32 ChunkId = Chunk.ChunkId;
		DownloadChunkInternal(Chunk, [this, ChunkId, Callback](bool bDownloadSuccess)
		{
			// if the download failed, we can't mount
			if (bDownloadSuccess)
			{
				if (IsValid(this))
				{
					// if all chunks are downloaded, do the mount again (this will pick up any changes and continue downloading if needed)
					this->MountChunk(ChunkId, Callback);
					return;
				}
			}

			// if anything went wrong, fire the callback now
			if (Callback)
			{
				Callback(false);
			}
		}, MAX_int32);
	}
}

void UDreamChunkDownloaderSubsystem::DownloadChunkInternal(const FDreamChunk& Chunk, const FDreamChunkDownloaderTypes::FDreamCallback& Callback, int32 Priority)
{
	DCD_LOG(Log, TEXT("Chunk %d download requested."), Chunk.ChunkId);

	// see if we need to download anything at all
	bool bNeedsDownload = false;
	for (const auto& PakFile : Chunk.PakFiles)
	{
		if (!PakFile->bIsCached)
		{
			bNeedsDownload = true;
			break;
		}
	}
	if (!bNeedsDownload)
	{
		ExecuteNextTick(Callback, true);
		return;
	}

	// make sure we have CDN configured
	if (BuildBaseUrls.Num() <= 0)
	{
		DCD_LOG(Error, TEXT("Unable to download Chunk %d (no CDN urls)."), Chunk.ChunkId);
		ExecuteNextTick(Callback, false);
		return;
	}

	// download all pak files that aren't already cached
	FDreamMultiCallback* MultiCallback = new FDreamMultiCallback(Callback);
	for (const auto& PakFile : Chunk.PakFiles)
	{
		if (!PakFile->bIsCached)
		{
			DownloadPakFileInternal(PakFile, MultiCallback->AddPending(), Priority);
		}
	}
	check(MultiCallback->GetNumPending() > 0);
}

void UDreamChunkDownloaderSubsystem::CompleteMountTask(FDreamChunk& Chunk)
{
	check(Chunk.MountTask != nullptr);
	check(Chunk.MountTask->IsDone());

	// increment chunks mounted
	++LoadingModeStats.ChunksMounted;

	// remove the mount
	FDreamChunkDownloaderTypes::FDreamMountTask* Mount = Chunk.MountTask;
	Chunk.MountTask = nullptr;

	// get the work
	const FDreamPakMountWork& MountWork = Mount->GetTask();

	// update bIsMounted on paks that actually succeeded
	for (const TSharedRef<FDreamPakFile>& PakFile : MountWork.MountedPakFiles)
	{
		PakFile->bIsMounted = true;
	}

	// update bIsMounted on the chunk
	bool bAllPaksMounted = true;
	for (const TSharedRef<FDreamPakFile>& PakFile : Chunk.PakFiles)
	{
		if (!PakFile->bIsMounted)
		{
			LoadingModeStats.LastError = FText::Format(LOCTEXT("FailedToMount", "Failed to mount {0}."), FText::FromString(PakFile->Entry.FileName));
			bAllPaksMounted = false;
			break;
		}
	}
	Chunk.bIsMounted = bAllPaksMounted;
	if (Chunk.bIsMounted)
	{
		DCD_LOG(Log, TEXT("Chunk %d mount succeeded."), Chunk.ChunkId);
	}
	else
	{
		DCD_LOG(Error, TEXT("Chunk %d mount failed."), Chunk.ChunkId);
	}

	// trigger the post-mount callbacks
	for (const FDreamChunkDownloaderTypes::FDreamCallback& Callback : MountWork.PostMountCallbacks)
	{
		ExecuteNextTick(Callback, bAllPaksMounted);
	}

	// also trigger the multicast event
	OnChunkMounted.Broadcast(Chunk.ChunkId, bAllPaksMounted);

	// finally delete the task
	delete Mount;

	// recompute loading stats
	ComputeLoadingStats();
}

bool UDreamChunkDownloaderSubsystem::UpdateMountTasks(float dts)
{
	bool bMountsPending = false;

	for (const auto& It : Chunks)
	{
		const TSharedRef<FDreamChunk>& Chunk = It.Value;
		if (Chunk->MountTask != nullptr)
		{
			if (Chunk->MountTask->IsDone())
			{
				// complete it
				CompleteMountTask(*Chunk);
			}
			else
			{
				// mount still pending
				bMountsPending = true;
			}
		}
	}

	if (!bMountsPending)
	{
		MountTicker.Reset();
	}
	return bMountsPending; // keep ticking
}

void UDreamChunkDownloaderSubsystem::ExecuteNextTick(const FDreamChunkDownloaderTypes::FDreamCallback& Callback, bool bSuccess)
{
	if (Callback)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Callback, bSuccess](float dts)
		{
			Callback(bSuccess);
			return false;
		}));
	}
}

void UDreamChunkDownloaderSubsystem::IssueDownloads()
{
	int32 StartedDownloads = 0;

	for (int32 i = 0; i < DownloadRequests.Num() && i < TargetDownloadsInFlight; ++i)
	{
		TSharedRef<FDreamPakFile> DownloadPakFile = DownloadRequests[i];
		if (DownloadPakFile->Download.IsValid())
		{
			// already downloading
			continue;
		}

		// 检查文件是否已经存在且有效
		if (DownloadPakFile->bIsCached)
		{
			DCD_LOG(Log, TEXT("Pak file %s is already cached, skipping download"),
			        *DownloadPakFile->Entry.FileName);
			continue;
		}

		// log that we're starting a download
		DCD_LOG(Log, TEXT("Starting download: %s (%lld bytes) from %s"),
		        *DownloadPakFile->Entry.FileName,
		        DownloadPakFile->Entry.FileSize,
		        *DownloadPakFile->Entry.RelativeUrl
		);
		bNeedsManifestSave = true;

		// make a new download
		TWeakObjectPtr<UDreamChunkDownloaderSubsystem> WeakThis(this);
		DownloadPakFile->Download = MakeShared<FDreamChunkDownload>(WeakThis, DownloadPakFile);
		DownloadPakFile->Download->Start();
		StartedDownloads++;
	}

	if (StartedDownloads > 0)
	{
		DCD_LOG(Log, TEXT("Started %d new downloads"), StartedDownloads);
	}
}

#undef LOCTEXT_NAMESPACE

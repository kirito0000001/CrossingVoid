// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownloaderUtils.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "DreamChunkDownloaderLog.h"
#include "DreamChunkDownloaderSubsystem.h"

using namespace FDreamChunkDownloaderStatics;

bool FDreamChunkDownloaderUtils::CheckFileSha1Hash(const FString& FullPathOnDisk, const FString& Sha1HashString)
{
	IFileHandle* FilePtr = IPlatformFile::GetPlatformPhysical().OpenRead(*FullPathOnDisk);
	if (FilePtr == nullptr)
	{
		DCD_LOG(Error, TEXT("Unable to open %s for hash verify."), *FullPathOnDisk);
		return false;
	}

	// create a SHA1 reader
	FSHA1 HashContext;

	// read in 64K chunks to prevent raising the memory high water mark too much
	{
		static const int64 FILE_BUFFER_SIZE = 64 * 1024;
		uint8 Buffer[FILE_BUFFER_SIZE];
		int64 FileSize = FilePtr->Size();
		for (int64 Pointer = 0; Pointer < FileSize;)
		{
			// how many bytes to read in this iteration
			int64 SizeToRead = FileSize - Pointer;
			if (SizeToRead > FILE_BUFFER_SIZE)
			{
				SizeToRead = FILE_BUFFER_SIZE;
			}

			// read dem bytes
			if (!FilePtr->Read(Buffer, SizeToRead))
			{
				DCD_LOG(Error, TEXT("Read error while validating '%s' at offset %lld."), *FullPathOnDisk, Pointer);

				// don't forget to close
				delete FilePtr;
				return false;
			}
			Pointer += SizeToRead;

			// update the hash
			HashContext.Update(Buffer, SizeToRead);
		}

		// done with the file
		delete FilePtr;
	}

	// close up shop
	HashContext.Final();
	uint8 FinalHash[FSHA1::DigestSize];
	HashContext.GetHash(FinalHash);

	// build the hash string we just computed
	FString LocalHashStr = TEXT("SHA1:");
	for (int Idx = 0; Idx < 20; Idx++)
	{
		LocalHashStr += FString::Printf(TEXT("%02X"), FinalHash[Idx]);
	}
	return Sha1HashString == LocalHashStr;
}

void FDreamChunkDownloaderUtils::DumpLoadedChunks()
{
	TSharedRef<UDreamChunkDownloaderSubsystem> ChunkDownloader = MakeShareable(GWorld->GetGameInstance()->GetSubsystem<UDreamChunkDownloaderSubsystem>());

	TArray<int32> ChunkIdList;
	ChunkDownloader->GetAllChunkIds(ChunkIdList);

	DCD_LOG(Display, TEXT("Dumping loaded chunk status\n--------------------------"));
	for (int32 ChunkId : ChunkIdList)
	{
		auto ChunkStatus = ChunkDownloader->GetChunkStatus(ChunkId);
		DCD_LOG(Display, TEXT("Chunk #%d => %s"), ChunkId, ChunkStatusToString(ChunkStatus));
	}
}

const TCHAR* FDreamChunkDownloaderUtils::ChunkStatusToString(EDreamChunkStatus Status)
{
	switch (Status)
	{
	case EDreamChunkStatus::Mounted: return TEXT("Mounted");
	case EDreamChunkStatus::Cached: return TEXT("Cached");
	case EDreamChunkStatus::Downloading: return TEXT("Downloading");
	case EDreamChunkStatus::Partial: return TEXT("Partial");
	case EDreamChunkStatus::Remote: return TEXT("Remote");
	case EDreamChunkStatus::Unknown: return TEXT("Unknown");
	default: return TEXT("Invalid");
	}
}

FString FDreamChunkDownloaderUtils::GetTargetPlatformName()
{
	FString Str = TEXT("Unknown");

#if PLATFORM_ANDROID
	Str = TEXT("Android");
#elif PLATFORM_IOS
	Str = TEXT("IOS");
#elif PLATFORM_WINDOWS
	Str = TEXT("Windows");
#elif PLATFORM_LINUX
	Str = TEXT("Linux");
#elif PLATFORM_MAC
	Str = TEXT("Mac");
#endif

	return Str;
}

TArray<FDreamPakFileEntry> FDreamChunkDownloaderUtils::ParseManifest(const FString& ManifestPath, TMap<FString, FString>* Properties)
{
	TSharedPtr<FJsonObject> JsonObject;
	return ParseManifest(ManifestPath, JsonObject, Properties);
}

TArray<FDreamPakFileEntry> FDreamChunkDownloaderUtils::ParseManifest(const FString& ManifestPath, TSharedPtr<FJsonObject>& JsonObject)
{
	return ParseManifest(ManifestPath, JsonObject, nullptr);
}

TArray<FDreamPakFileEntry> FDreamChunkDownloaderUtils::ParseManifest(const FString& ManifestPath, TSharedPtr<FJsonObject>& OutJsonObject, TMap<FString, FString>* OutProperties)
{
	int32 ExpectedEntries = -1;
	TArray<FDreamPakFileEntry> Entries;
	OutJsonObject.Reset();

	if (OutProperties)
	{
		OutProperties->Empty();
	}

	FString FileStrings;
	if (!FFileHelper::LoadFileToString(FileStrings, *ManifestPath))
	{
		DCD_LOG(Log, TEXT("Unable to load manifest file %s"), *ManifestPath);
		return Entries;
	}

	if (FileStrings.IsEmpty())
	{
		DCD_LOG(Log, TEXT("Manifest file %s is empty"), *ManifestPath);
		return Entries;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileStrings);
	TSharedPtr<FJsonObject> Object;

	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		DCD_LOG(Error, TEXT("Failed to deserialize JSON from manifest file %s"), *ManifestPath);
		return Entries;
	}

	DCD_LOG(Log, TEXT("Deserialize Data : %s"), *FileStrings);

	// 设置输出JsonObject
	OutJsonObject = Object;

	for (const TPair<FString, TSharedPtr<FJsonValue>>& ValuePair : Object->Values)
	{
		if (!ValuePair.Value.IsValid())
		{
			continue;
		}

		if (ValuePair.Key == ENTRIES_COUNT_FIELD)
		{
			double NumberValue = 0;
			if (ValuePair.Value->TryGetNumber(NumberValue))
			{
				ExpectedEntries = static_cast<int32>(NumberValue);
			}
		}
		else if (ValuePair.Key == ENTRIES_FIELD)
		{
			const TArray<TSharedPtr<FJsonValue>>* EntryArray = nullptr;
			if (ValuePair.Value->TryGetArray(EntryArray) && EntryArray)
			{
				for (const TSharedPtr<FJsonValue>& EntryValue : *EntryArray)
				{
					if (!EntryValue.IsValid())
					{
						continue;
					}

					const TSharedPtr<FJsonObject> EntryObject = EntryValue->AsObject();
					if (!EntryObject.IsValid())
					{
						DCD_LOG(Warning, TEXT("Invalid entry object in manifest"));
						continue;
					}

					FDreamPakFileEntry EntryStruct;

					// 必需字段检查
					if (!EntryObject->TryGetStringField(FILE_NAME_FIELD, EntryStruct.FileName) ||
						EntryStruct.FileName.IsEmpty())
					{
						DCD_LOG(Warning, TEXT("Entry missing or empty FileName field"));
						continue;
					}

					double FileSizeDouble = 0;
					if (!EntryObject->TryGetNumberField(FILE_SIZE_FIELD, FileSizeDouble) || FileSizeDouble <= 0)
					{
						DCD_LOG(Warning, TEXT("Entry missing or invalid FileSize field for %s"), *EntryStruct.FileName);
						continue;
					}
					EntryStruct.FileSize = static_cast<uint64>(FileSizeDouble);

					if (!EntryObject->TryGetStringField(FILE_VERSION_FIELD, EntryStruct.FileVersion) ||
						EntryStruct.FileVersion.IsEmpty())
					{
						DCD_LOG(Warning, TEXT("Entry missing or empty FileVersion field for %s"), *EntryStruct.FileName);
						continue;
					}

					// 可选字段
					int32 ChunkIdValue = -1;
					EntryStruct.ChunkId = EntryObject->TryGetNumberField(FILE_CHUNK_ID_FIELD, ChunkIdValue) ? ChunkIdValue : -1;

					if (!EntryObject->TryGetStringField(FILE_RELATIVE_URL_FIELD, EntryStruct.RelativeUrl))
					{
						EntryStruct.RelativeUrl = TEXT("/");
					}

					Entries.Add(EntryStruct);
				}
			}
		}
		else
		{
			// 其他属性存储到Properties
			if (OutProperties)
			{
				FString StringValue;
				if (ValuePair.Value->TryGetString(StringValue))
				{
					OutProperties->Add(ValuePair.Key, StringValue);
				}
				// 处理BUILD_ID等特殊字段
				else if (ValuePair.Key == BUILD_ID_KEY || ValuePair.Key == CLIENT_BUILD_ID)
				{
					if (ValuePair.Value->TryGetString(StringValue))
					{
						OutProperties->Add(BUILD_ID_KEY, StringValue);
					}
				}
			}
		}
	}

	// 验证entries数量
	if (ExpectedEntries >= 0 && ExpectedEntries != Entries.Num())
	{
		DCD_LOG(Error, TEXT("Corrupt manifest at %s (expected %d entries, got %d)"), *ManifestPath, ExpectedEntries, Entries.Num());
		Entries.Empty();
		OutJsonObject.Reset();
		if (OutProperties)
		{
			OutProperties->Empty();
		}
	}

	DCD_LOG(Log, TEXT("Successfully parsed %d entries from manifest %s"), Entries.Num(), *ManifestPath);
	return Entries;
}

bool FDreamChunkDownloaderUtils::WriteStringAsUtf8TextFile(const FString& FileText, const FString& FilePath)
{
	if (FFileHelper::SaveStringToFile(FileText, *FilePath))
	{
		DCD_LOG(Log, TEXT("Wrote file %s content %s"), *FilePath, *FileText);
		return true;
	}
	else
	{
		DCD_LOG(Error, TEXT("Failed to write file %s"), *FilePath);
		return false;
	}
}

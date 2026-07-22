// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
struct FDreamPakFileEntry;
enum class EDreamChunkStatus : uint8;

/**
 * Utility Functions for Dream Chunk Downloader
 * 
 * This struct provides static utility functions used throughout the Dream Chunk Downloader system.
 * These functions handle common operations such as file validation, platform detection, 
 * manifest parsing, and string/file operations.
 * 
 * All functions are static and can be called without instantiating the struct.
 */
struct DREAMCHUNKDOWNLOADER_API FDreamChunkDownloaderUtils
{
public:
	/**
	 * Check if a file matches the specified SHA1 hash
	 * 
	 * Validates the integrity of a file by computing its SHA1 hash and comparing
	 * it with the expected hash value.
	 * 
	 * @param FullPathOnDisk Full path to the file to check
	 * @param Sha1HashString Expected SHA1 hash string (should start with "SHA1:")
	 * @return True if the file's hash matches the expected hash
	 */
	static bool CheckFileSha1Hash(const FString& FullPathOnDisk, const FString& Sha1HashString);

	/**
	 * Dump information about all loaded chunks to the log
	 * 
	 * Outputs detailed information about currently loaded chunks including
	 * their IDs, status, and associated pak files. Useful for debugging.
	 */
	static void DumpLoadedChunks();

	/**
	 * Convert a chunk status enum to a string representation
	 * 
	 * @param Status The chunk status to convert
	 * @return String representation of the status
	 */
	static const TCHAR* ChunkStatusToString(EDreamChunkStatus Status);

	/**
	 * Get the target platform name
	 * 
	 * Returns a string representing the current target platform which is used
	 * to select the appropriate manifest and CDN paths.
	 * 
	 * @return Platform name (e.g. "Windows", "Android", "IOS")
	 */
	static FString GetTargetPlatformName();

	/**
	 * Parse a manifest file and extract pak file entries
	 * 
	 * Reads and parses a JSON manifest file, extracting the list of pak file entries
	 * and optionally additional properties.
	 * 
	 * @param ManifestPath Path to the manifest file to parse
	 * @param Properties Optional output parameter to receive additional manifest properties
	 * @return Array of pak file entries parsed from the manifest
	 */
	static TArray<FDreamPakFileEntry> ParseManifest(const FString& ManifestPath, TMap<FString, FString>* Properties = nullptr);

	/**
	 * Parse a manifest file and extract pak file entries and JSON object
	 * 
	 * Reads and parses a JSON manifest file, extracting the list of pak file entries
	 * and returning the parsed JSON object.
	 * 
	 * @param ManifestPath Path to the manifest file to parse
	 * @param JsonObject Output parameter to receive the parsed JSON object
	 * @return Array of pak file entries parsed from the manifest
	 */
	static TArray<FDreamPakFileEntry> ParseManifest(const FString& ManifestPath, TSharedPtr<FJsonObject>& JsonObject);

	/**
	 * Parse a manifest file and extract pak file entries, JSON object, and properties
	 * 
	 * Reads and parses a JSON manifest file, extracting the list of pak file entries,
	 * the parsed JSON object, and additional properties.
	 * 
	 * @param ManifestPath Path to the manifest file to parse
	 * @param OutJsonObject Output parameter to receive the parsed JSON object
	 * @param OutProperties Output parameter to receive additional manifest properties
	 * @return Array of pak file entries parsed from the manifest
	 */
	static TArray<FDreamPakFileEntry> ParseManifest(const FString& ManifestPath, TSharedPtr<FJsonObject>& OutJsonObject, TMap<FString, FString>* OutProperties);

	/**
	 * Write a string to a file using UTF-8 encoding
	 * 
	 * Writes the specified text string to a file using UTF-8 encoding.
	 * Creates the file if it doesn't exist, or overwrites it if it does.
	 * 
	 * @param FileText The text content to write to the file
	 * @param FilePath The path to the file to write
	 * @return True if the file was successfully written
	 */
	static bool WriteStringAsUtf8TextFile(const FString& FileText, const FString& FilePath);
};

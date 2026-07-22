// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"

/**
 * Log category for the Dream Chunk Downloader plugin
 * 
 * This log category is used for all logging within the Dream Chunk Downloader subsystem.
 * It allows developers to filter and view logs specifically related to chunk downloading,
 * caching, and mounting operations.
 * 
 * The category is declared as DECLARE_LOG_CATEGORY_EXTERN which means the actual log
 * category object is defined in the implementation file, allowing for proper linking
 * across multiple translation units.
 * 
 * Log verbosity levels:
 * - VeryVerbose: Extremely detailed logs, typically disabled in release builds
 * - Verbose: Detailed logs useful for debugging
 * - Log: General information logs
 * - Display: Important information to display to users
 * - Warning: Warning messages about potential issues
 * - Error: Error messages about failures
 * - Fatal: Fatal errors that will terminate the application
 */
DECLARE_LOG_CATEGORY_EXTERN(LogDreamChunkDownloader, All, All);

/**
 * Convenience macro for logging messages with the Dream Chunk Downloader category
 * 
 * This macro simplifies logging by automatically using the LogDreamChunkDownloader
 * category and supports variable arguments like printf-style formatting.
 * 
 * Usage examples:
 *   DCD_LOG(Log, TEXT("Initializing chunk downloader"));
 *   DCD_LOG(Warning, TEXT("Failed to download chunk %d"), ChunkId);
 *   DCD_LOG(Error, TEXT("Critical error: %s"), *ErrorMessage);
 * 
 * @param V Log verbosity level (Log, Warning, Error, etc.)
 * @param F Format string for the log message
 * @param ... Variable arguments for the format string
 */
#define DCD_LOG(V, F, ...) UE_LOG(LogDreamChunkDownloader, V, F, ##__VA_ARGS__)

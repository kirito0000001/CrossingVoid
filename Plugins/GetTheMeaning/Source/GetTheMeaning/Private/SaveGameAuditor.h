#pragma once

#include "CoreMinimal.h"

struct FSaveGameAuditFinding
{
	FString Severity;
	FString Confidence;
	FString Category;
	FString Message;
	FString Details;
	FString FilePath;
	FString PropertyPath;
};

using FSaveGameAuditProgress = TFunction<bool(int32 FileIndex, int32 FileCount, const FString& FilePath)>;

class FSaveGameAuditor
{
public:
	static TArray<FSaveGameAuditFinding> AuditDirectory(
		const FString& Directory,
		const TArray<FString>& IdentityNamePatterns,
		FSaveGameAuditProgress Progress = FSaveGameAuditProgress());

	static bool WriteReports(
		const TArray<FSaveGameAuditFinding>& Findings,
		const FString& SourceDirectory,
		const FString& OutputDirectory);

	static void AuditGuidValue(
		const FGuid& Value,
		const FString& FilePath,
		const FString& PropertyPath,
		TArray<FSaveGameAuditFinding>& OutFindings);

	static void AuditGuidMapEntry(
		const FGuid& Key,
		const TOptional<FGuid>& ValueIdentifier,
		const FString& FilePath,
		const FString& PropertyPath,
		TArray<FSaveGameAuditFinding>& OutFindings);
};

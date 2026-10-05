#include "Misc/AutomationTest.h"

#include "SaveGameAuditor.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GetTheMeaningSaveGameAuditTests
{
	static bool HasCategory(const TArray<FSaveGameAuditFinding>& Findings, const FString& Category)
	{
		return Findings.ContainsByPredicate([&Category](const FSaveGameAuditFinding& Finding)
		{
			return Finding.Category == Category;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningInvalidGuidAuditTest,
	"GetTheMeaning.SaveGameAudit.InvalidGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningInvalidGuidAuditTest::RunTest(const FString& Parameters)
{
	TArray<FSaveGameAuditFinding> Findings;
	FSaveGameAuditor::AuditGuidValue(FGuid(), TEXT("Sample.sav"), TEXT("Root.Record.Identifier"), Findings);

	TestTrue(TEXT("invalid guid is reported"), GetTheMeaningSaveGameAuditTests::HasCategory(Findings, TEXT("InvalidGuid")));
	if (Findings.Num() > 0)
	{
		TestEqual(TEXT("invalid guid is confirmed"), Findings[0].Confidence, FString(TEXT("Confirmed")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningInvalidMapKeyAuditTest,
	"GetTheMeaning.SaveGameAudit.InvalidMapKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningInvalidMapKeyAuditTest::RunTest(const FString& Parameters)
{
	TArray<FSaveGameAuditFinding> Findings;
	FSaveGameAuditor::AuditGuidMapEntry(
		FGuid(),
		TOptional<FGuid>(),
		TEXT("Sample.sav"),
		TEXT("Root.Records[0]"),
		Findings);

	TestTrue(TEXT("invalid map key is reported"), GetTheMeaningSaveGameAuditTests::HasCategory(Findings, TEXT("InvalidMapKey")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningIdentityMismatchAuditTest,
	"GetTheMeaning.SaveGameAudit.IdentityMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningIdentityMismatchAuditTest::RunTest(const FString& Parameters)
{
	const FGuid Key(1, 2, 3, 4);
	const FGuid ValueIdentifier(5, 6, 7, 8);
	TArray<FSaveGameAuditFinding> Findings;
	FSaveGameAuditor::AuditGuidMapEntry(
		Key,
		TOptional<FGuid>(ValueIdentifier),
		TEXT("Sample.sav"),
		TEXT("Root.Records[0]"),
		Findings);

	TestTrue(TEXT("key value mismatch is reported"), GetTheMeaningSaveGameAuditTests::HasCategory(Findings, TEXT("IdentityMismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningValidIdentityMirrorAuditTest,
	"GetTheMeaning.SaveGameAudit.ValidIdentityMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningValidIdentityMirrorAuditTest::RunTest(const FString& Parameters)
{
	const FGuid Identifier(1, 2, 3, 4);
	TArray<FSaveGameAuditFinding> Findings;
	FSaveGameAuditor::AuditGuidMapEntry(
		Identifier,
		TOptional<FGuid>(Identifier),
		TEXT("Sample.sav"),
		TEXT("Root.Records[0]"),
		Findings);

	TestEqual(TEXT("valid mirror has no finding"), Findings.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningUnreadableSaveAuditTest,
	"GetTheMeaning.SaveGameAudit.UnreadableFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningUnreadableSaveAuditTest::RunTest(const FString& Parameters)
{
	const FString Directory = FPaths::ProjectIntermediateDir() / TEXT("GetTheMeaningTests/UnreadableSave");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*Directory);
	const FString SavePath = Directory / TEXT("Corrupt.sav");
	const TArray<uint8> InvalidBytes{ 0x47, 0x54, 0x4D, 0x00 };
	FFileHelper::SaveArrayToFile(InvalidBytes, *SavePath);
	const FDateTime TimestampBefore = PlatformFile.GetTimeStamp(*SavePath);

	const TArray<FSaveGameAuditFinding> Findings = FSaveGameAuditor::AuditDirectory(
		Directory,
		TArray<FString>{ TEXT("Guid"), TEXT("Uid"), TEXT("Id"), TEXT("Identifier") });

	TestTrue(TEXT("corrupt save is reported"), GetTheMeaningSaveGameAuditTests::HasCategory(Findings, TEXT("DeserializeFailed")));
	TestEqual(TEXT("audit does not modify source file timestamp"), PlatformFile.GetTimeStamp(*SavePath), TimestampBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningSaveAuditSerializationTest,
	"GetTheMeaning.SaveGameAudit.ReportSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningSaveAuditSerializationTest::RunTest(const FString& Parameters)
{
	TArray<FSaveGameAuditFinding> Findings;
	FSaveGameAuditor::AuditGuidValue(FGuid(), TEXT("Sample.sav"), TEXT("Root.Record.Identifier"), Findings);
	const FString OutputDirectory = FPaths::ProjectIntermediateDir() / TEXT("GetTheMeaningTests/SaveGameAuditReport");
	TestTrue(TEXT("audit reports are written"), FSaveGameAuditor::WriteReports(Findings, TEXT("NeutralSaveDirectory"), OutputDirectory));

	FString Json;
	TestTrue(TEXT("audit json can be read"), FFileHelper::LoadFileToString(Json, *(OutputDirectory / TEXT("SaveGameAudit.json"))));
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("audit json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid()) return false;

	TestTrue(TEXT("schema version exists"), Root->HasField(TEXT("schemaVersion")));
	TestTrue(TEXT("source directory exists"), Root->HasField(TEXT("sourceDirectory")));
	const TArray<TSharedPtr<FJsonValue>>* JsonFindings = nullptr;
	TestTrue(TEXT("findings array exists"), Root->TryGetArrayField(TEXT("findings"), JsonFindings) && JsonFindings && JsonFindings->Num() > 0);
	if (!JsonFindings || JsonFindings->Num() == 0) return false;
	const TSharedPtr<FJsonObject> Finding = (*JsonFindings)[0]->AsObject();
	TestTrue(TEXT("file path exists"), Finding.IsValid() && Finding->HasField(TEXT("filePath")));
	TestTrue(TEXT("property path exists"), Finding.IsValid() && Finding->HasField(TEXT("propertyPath")));

	FString Markdown;
	TestTrue(TEXT("audit markdown can be read"), FFileHelper::LoadFileToString(Markdown, *(OutputDirectory / TEXT("SaveGameAudit.md"))));
	TestTrue(TEXT("read-only behavior is documented"), Markdown.Contains(TEXT("No SaveGame file was modified")));
	return true;
}

#endif

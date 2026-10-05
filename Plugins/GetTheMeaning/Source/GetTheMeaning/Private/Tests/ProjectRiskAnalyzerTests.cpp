#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "BlueprintRiskAnalyzer.h"
#include "GetTheMeaningSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProjectRiskReport.h"
#include "Serialization/JsonSerializer.h"

namespace GetTheMeaningProjectRiskTests
{
	bool HasCategory(const TArray<FProjectRiskFinding>& Findings, const FString& Category)
	{
		return Findings.ContainsByPredicate([&Category](const FProjectRiskFinding& Finding)
		{
			return Finding.Category == Category;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningIdentityPatternTest,
	"GetTheMeaning.ProjectRisk.IdentityPattern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningIdentityPatternTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Patterns = { TEXT("Guid"), TEXT("Uid"), TEXT("Id"), TEXT("Identifier") };
	TestTrue(TEXT("token suffix is recognized"), FBlueprintRiskAnalyzer::IsIdentityLikeName(TEXT("EntityGuid"), Patterns));
	TestTrue(TEXT("exact short token is recognized"), FBlueprintRiskAnalyzer::IsIdentityLikeName(TEXT("ID"), Patterns));
	TestFalse(TEXT("substring inside an unrelated word is rejected"), FBlueprintRiskAnalyzer::IsIdentityLikeName(TEXT("GridSize"), Patterns));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningTypeNormalizationTest,
	"GetTheMeaning.ProjectRisk.TypeNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningTypeNormalizationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Guid pin type matches reflected FGuid"), FBlueprintRiskAnalyzer::NormalizeTypeName(TEXT("Guid")), FString(TEXT("FGuid")));
	TestEqual(TEXT("FGuid remains stable"), FBlueprintRiskAnalyzer::NormalizeTypeName(TEXT("FGuid")), FString(TEXT("FGuid")));
	TestEqual(TEXT("container type is normalized recursively"), FBlueprintRiskAnalyzer::NormalizeTypeName(TEXT("TArray<Guid>")), FString(TEXT("TArray<FGuid>")));
	TestEqual(TEXT("integer pin matches reflected int32"), FBlueprintRiskAnalyzer::NormalizeTypeName(TEXT("int")), FString(TEXT("int32")));
	TestEqual(TEXT("name pin matches reflected FName"), FBlueprintRiskAnalyzer::NormalizeTypeName(TEXT("name")), FString(TEXT("FName")));
	TestEqual(TEXT("string pin matches reflected FString"), FBlueprintRiskAnalyzer::NormalizeTypeName(TEXT("string")), FString(TEXT("FString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningCollectorGuardRulesTest,
	"GetTheMeaning.ProjectRisk.CollectorGuardRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningCollectorGuardRulesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("native guid validation is trusted"), FBlueprintRiskAnalyzer::IsTrustedValidationFunction(TEXT("IsValid_Guid"), TEXT("KismetGuidLibrary")));
	TestTrue(TEXT("native object validation is trusted"), FBlueprintRiskAnalyzer::IsTrustedValidationFunction(TEXT("IsValid"), TEXT("KismetSystemLibrary")));
	TestFalse(TEXT("custom similarly named function is not trusted"), FBlueprintRiskAnalyzer::IsTrustedValidationFunction(TEXT("IsValidationEnabled"), TEXT("ExampleLibrary")));
	TestTrue(TEXT("then-only dominated consumer is protected"), FBlueprintRiskAnalyzer::IsGuardTopologyProtected(true, true, false, false));
	TestFalse(TEXT("else path invalidates protection"), FBlueprintRiskAnalyzer::IsGuardTopologyProtected(true, true, true, false));
	TestFalse(TEXT("bypass invalidates protection"), FBlueprintRiskAnalyzer::IsGuardTopologyProtected(true, true, false, true));
	TestFalse(TEXT("unreachable branch is not protection"), FBlueprintRiskAnalyzer::IsGuardTopologyProtected(false, true, false, false));
	TestTrue(TEXT("unconnected map value is default"), FBlueprintRiskAnalyzer::IsContainerValueDefault(false, false, false));
	TestTrue(TEXT("connected make-struct field without source is default"), FBlueprintRiskAnalyzer::IsContainerValueDefault(true, true, false));
	TestFalse(TEXT("connected explicit field is not default"), FBlueprintRiskAnalyzer::IsContainerValueDefault(true, true, true));
	TestFalse(TEXT("connected whole struct is unverified rather than default"), FBlueprintRiskAnalyzer::IsContainerValueDefault(true, false, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningSettingsDefaultsTest,
	"GetTheMeaning.ProjectRisk.SettingsDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UGetTheMeaningSettings* Settings = GetDefault<UGetTheMeaningSettings>();
	TestNotNull(TEXT("settings are registered"), Settings);
	if (!Settings) return false;
	TestTrue(TEXT("Guid default exists"), Settings->IdentityNamePatterns.Contains(TEXT("Guid")));
	TestTrue(TEXT("Uid default exists"), Settings->IdentityNamePatterns.Contains(TEXT("Uid")));
	TestTrue(TEXT("Id default exists"), Settings->IdentityNamePatterns.Contains(TEXT("Id")));
	TestTrue(TEXT("Identifier default exists"), Settings->IdentityNamePatterns.Contains(TEXT("Identifier")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningMissingWriterTest,
	"GetTheMeaning.ProjectRisk.MissingWriter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningMissingWriterTest::RunTest(const FString& Parameters)
{
	FBlueprintRiskFacts Facts;
	Facts.AssetPath = TEXT("/Game/Test/BP_Consumer.BP_Consumer");

	FStructFieldAccessFact Read;
	Read.AssetPath = Facts.AssetPath;
	Read.GraphName = TEXT("EventGraph");
	Read.NodeId = TEXT("read-node");
	Read.StructPath = TEXT("/Script/Test.ExampleRecord");
	Read.FieldName = TEXT("EntityGuid");
	Read.FieldType = TEXT("FGuid");
	Read.AccessKind = EProjectRiskAccessKind::Read;
	Read.bIdentityLike = true;
	Read.bCriticalSink = true;
	Facts.StructFieldAccesses.Add(Read);

	const TArray<FProjectRiskFinding> Findings = FProjectRiskReport::Analyze({ Facts });
	TestTrue(TEXT("critical identity read without a writer is reported"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("NoKnownWriter")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningDefaultOnlyTest,
	"GetTheMeaning.ProjectRisk.DefaultOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningDefaultOnlyTest::RunTest(const FString& Parameters)
{
	FBlueprintRiskFacts Facts;
	Facts.AssetPath = TEXT("/Game/Test/BP_Default.BP_Default");

	FStructFieldAccessFact DefaultWrite;
	DefaultWrite.AssetPath = Facts.AssetPath;
	DefaultWrite.StructPath = TEXT("/Script/Test.ExampleRecord");
	DefaultWrite.FieldName = TEXT("EntityGuid");
	DefaultWrite.FieldType = TEXT("FGuid");
	DefaultWrite.AccessKind = EProjectRiskAccessKind::DefaultWrite;
	DefaultWrite.bIdentityLike = true;
	Facts.StructFieldAccesses.Add(DefaultWrite);

	FStructFieldAccessFact Read = DefaultWrite;
	Read.AccessKind = EProjectRiskAccessKind::Read;
	Read.bCriticalSink = true;
	Facts.StructFieldAccesses.Add(Read);

	const TArray<FProjectRiskFinding> Findings = FProjectRiskReport::Analyze({ Facts });
	TestTrue(TEXT("default-only critical identity is reported"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("DefaultOnly")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningIdentityMirrorTest,
	"GetTheMeaning.ProjectRisk.IdentityMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningIdentityMirrorTest::RunTest(const FString& Parameters)
{
	FBlueprintRiskFacts MatchingFacts;
	FContainerIdentityFact Matching;
	Matching.AssetPath = TEXT("/Game/Test/BP_Map.BP_Map");
	Matching.ContainerExpression = TEXT("Records");
	Matching.KeyType = TEXT("FGuid");
	Matching.ValueStructPath = TEXT("/Script/Test.ExampleRecord");
	Matching.ValueFieldName = TEXT("EntityGuid");
	Matching.ValueFieldType = TEXT("FGuid");
	Matching.KeySource = TEXT("Get StableGuid");
	Matching.ValueSource = TEXT("Get StableGuid");
	Matching.bIdentityLike = true;
	MatchingFacts.ContainerIdentities.Add(Matching);

	TArray<FProjectRiskFinding> Findings = FProjectRiskReport::Analyze({ MatchingFacts });
	TestTrue(TEXT("matching sources are intentional mirror"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("IntentionalMirror")));

	FBlueprintRiskFacts MismatchingFacts;
	FContainerIdentityFact Mismatching = Matching;
	Mismatching.KeySource = TEXT("Get StableGuid");
	Mismatching.ValueSource = TEXT("Default FGuid");
	Mismatching.bValueUsesDefault = true;
	MismatchingFacts.ContainerIdentities.Add(Mismatching);

	Findings = FProjectRiskReport::Analyze({ MismatchingFacts });
	TestTrue(TEXT("default value mirror is inconsistent"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("InconsistentIdentity")));

	FBlueprintRiskFacts UnverifiedFacts;
	FContainerIdentityFact Unverified = Matching;
	Unverified.ValueSource.Reset();
	Unverified.bValueUsesDefault = false;
	UnverifiedFacts.ContainerIdentities.Add(Unverified);
	Findings = FProjectRiskReport::Analyze({ UnverifiedFacts });
	TestTrue(TEXT("whole-struct source without field proof is unverified"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("UnverifiedMirror")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningUncheckedMapFindTest,
	"GetTheMeaning.ProjectRisk.UncheckedMapFind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningUncheckedMapFindTest::RunTest(const FString& Parameters)
{
	FBlueprintRiskFacts Facts;
	FMapFindFact MapFind;
	MapFind.AssetPath = TEXT("/Game/Test/BP_Map.BP_Map");
	MapFind.GraphName = TEXT("ResolveRecord");
	MapFind.NodeId = TEXT("map-find-node");
	MapFind.MapExpression = TEXT("Records");
	MapFind.KeyExpression = TEXT("Get EntityGuid");
	MapFind.ValueType = TEXT("TArray<FName>");
	MapFind.ValueConsumers.Add(TEXT("Send Result"));
	MapFind.bResultChecked = false;
	Facts.MapFinds.Add(MapFind);

	const TArray<FProjectRiskFinding> Findings = FProjectRiskReport::Analyze({ Facts });
	TestTrue(TEXT("unchecked map find is reported"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("UncheckedMapFind")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningIdentityRpcTest,
	"GetTheMeaning.ProjectRisk.IdentityRpc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningIdentityRpcTest::RunTest(const FString& Parameters)
{
	FBlueprintRiskFacts Facts;
	FRpcIdentityFact Rpc;
	Rpc.AssetPath = TEXT("/Game/Test/BP_Network.BP_Network");
	Rpc.GraphName = TEXT("EventGraph");
	Rpc.NodeId = TEXT("rpc-node");
	Rpc.RpcName = TEXT("ServerResolveRecord");
	Rpc.ParameterName = TEXT("EntityGuid");
	Rpc.ParameterType = TEXT("FGuid");
	Rpc.bRunOnServer = true;
	Rpc.bIdentityLike = true;
	Rpc.bCallerValidated = false;
	Rpc.bServerValidated = false;
	Facts.RpcIdentities.Add(Rpc);

	TArray<FProjectRiskFinding> Findings = FProjectRiskReport::Analyze({ Facts });
	TestTrue(TEXT("unvalidated server identity parameter is reported"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("UnvalidatedIdentityRpc")));

	Facts.RpcIdentities[0].bServerValidated = true;
	Findings = FProjectRiskReport::Analyze({ Facts });
	TestFalse(TEXT("validated identity parameter is not warned"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("UnvalidatedIdentityRpc")));
	TestTrue(TEXT("validated identity parameter remains visible as info"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("ValidatedIdentityRpc")));

	FBlueprintRiskFacts CallerFacts;
	FRpcIdentityFact CallerRpc = Rpc;
	CallerRpc.bCallSite = true;
	CallerRpc.bServerValidated = false;
	CallerRpc.bCallerValidated = false;
	CallerFacts.RpcIdentities.Add(CallerRpc);
	Findings = FProjectRiskReport::Analyze({ CallerFacts });
	TestTrue(TEXT("unverified caller is retained as hint"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("UnvalidatedIdentityRpcCallsite")));

	CallerFacts.RpcIdentities[0].bCallerValidated = true;
	Findings = FProjectRiskReport::Analyze({ CallerFacts });
	TestTrue(TEXT("guarded caller is retained as info"), GetTheMeaningProjectRiskTests::HasCategory(Findings, TEXT("CallerValidatedIdentityRpc")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGetTheMeaningProjectRiskSerializationTest,
	"GetTheMeaning.ProjectRisk.ReportSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGetTheMeaningProjectRiskSerializationTest::RunTest(const FString& Parameters)
{
	FBlueprintRiskFacts Facts;
	FMapFindFact MapFind;
	MapFind.AssetPath = TEXT("/Game/Test/BP_Neutral.BP_Neutral");
	MapFind.GraphName = TEXT("ResolveRecord");
	MapFind.NodeId = TEXT("node-1");
	MapFind.NodeTitle = TEXT("Find");
	MapFind.MapExpression = TEXT("Records");
	MapFind.KeyExpression = TEXT("Identifier");
	MapFind.ValueType = TEXT("FExampleRecord");
	MapFind.bResultChecked = false;
	Facts.MapFinds.Add(MapFind);

	const FString OutputDirectory = FPaths::ProjectIntermediateDir() / TEXT("GetTheMeaningTests/ProjectRiskReport");
	TestTrue(TEXT("reports are written"), FProjectRiskReport::WriteReports({ Facts }, OutputDirectory));

	FString Json;
	TestTrue(TEXT("json report can be read"), FFileHelper::LoadFileToString(Json, *(OutputDirectory / TEXT("ProjectRiskReport.json"))));
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("json report parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid()) return false;

	TestTrue(TEXT("schema version exists"), Root->HasField(TEXT("schemaVersion")));
	const TArray<TSharedPtr<FJsonValue>>* Findings = nullptr;
	TestTrue(TEXT("findings array exists"), Root->TryGetArrayField(TEXT("findings"), Findings) && Findings && Findings->Num() > 0);
	if (!Findings || Findings->Num() == 0) return false;
	const TSharedPtr<FJsonObject> Finding = (*Findings)[0]->AsObject();
	TestTrue(TEXT("finding category exists"), Finding.IsValid() && Finding->HasField(TEXT("category")));
	TestTrue(TEXT("finding severity exists"), Finding.IsValid() && Finding->HasField(TEXT("severity")));
	TestTrue(TEXT("finding confidence exists"), Finding.IsValid() && Finding->HasField(TEXT("confidence")));
	TestTrue(TEXT("finding asset path exists"), Finding.IsValid() && Finding->HasField(TEXT("assetPath")));
	TestTrue(TEXT("finding graph name exists"), Finding.IsValid() && Finding->HasField(TEXT("graphName")));
	TestTrue(TEXT("finding node id exists"), Finding.IsValid() && Finding->HasField(TEXT("nodeId")));
	return true;
}

#endif

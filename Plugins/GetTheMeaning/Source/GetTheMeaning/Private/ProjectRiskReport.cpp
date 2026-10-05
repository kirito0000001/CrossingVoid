#include "ProjectRiskReport.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace GetTheMeaningProjectRisk
{
	struct FFieldSummary
	{
		TArray<const FStructFieldAccessFact*> Reads;
		TArray<const FStructFieldAccessFact*> ExplicitWrites;
		TArray<const FStructFieldAccessFact*> DefaultWrites;
		TArray<const FStructFieldAccessFact*> ExternalSources;
	};

	FString FieldKey(const FStructFieldAccessFact& Fact)
	{
		return Fact.StructPath + TEXT("::") + Fact.FieldName;
	}

	FProjectRiskFinding MakeFinding(
		const FString& Severity,
		const FString& Confidence,
		const FString& Category,
		const FString& Message,
		const FString& Details,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& NodeId,
		const FString& NodeTitle)
	{
		FProjectRiskFinding Finding;
		Finding.Severity = Severity;
		Finding.Confidence = Confidence;
		Finding.Category = Category;
		Finding.Message = Message;
		Finding.Details = Details;
		Finding.AssetPath = AssetPath;
		Finding.GraphName = GraphName;
		Finding.NodeId = NodeId;
		Finding.NodeTitle = NodeTitle;
		return Finding;
	}

	FString DefaultValueDescription(const FString& ValueType)
	{
		if (ValueType.Contains(TEXT("Array")) || ValueType.Contains(TEXT("Set")) || ValueType.Contains(TEXT("Map")))
		{
			return TEXT("an empty container");
		}
		if (ValueType.Contains(TEXT("Object")) || ValueType.Contains(TEXT("Class")) || ValueType.Contains(TEXT("Interface")))
		{
			return TEXT("a null reference");
		}
		if (ValueType.Contains(TEXT("int"), ESearchCase::IgnoreCase) || ValueType.Contains(TEXT("float"), ESearchCase::IgnoreCase) || ValueType.Contains(TEXT("double"), ESearchCase::IgnoreCase))
		{
			return TEXT("zero");
		}
		return TEXT("the type default value");
	}

	void WriteFindingJson(const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>>& Writer, const FProjectRiskFinding& Finding)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("severity"), Finding.Severity);
		Writer->WriteValue(TEXT("confidence"), Finding.Confidence);
		Writer->WriteValue(TEXT("category"), Finding.Category);
		Writer->WriteValue(TEXT("message"), Finding.Message);
		Writer->WriteValue(TEXT("details"), Finding.Details);
		Writer->WriteValue(TEXT("assetPath"), Finding.AssetPath);
		Writer->WriteValue(TEXT("graphName"), Finding.GraphName);
		Writer->WriteValue(TEXT("nodeId"), Finding.NodeId);
		Writer->WriteValue(TEXT("nodeTitle"), Finding.NodeTitle);
		Writer->WriteObjectEnd();
	}
}

TArray<FProjectRiskFinding> FProjectRiskReport::Analyze(const TArray<FBlueprintRiskFacts>& Facts)
{
	using namespace GetTheMeaningProjectRisk;

	TArray<FProjectRiskFinding> Findings;
	TMap<FString, FFieldSummary> FieldSummaries;

	for (const FBlueprintRiskFacts& BlueprintFacts : Facts)
	{
		for (const FStructFieldAccessFact& Fact : BlueprintFacts.StructFieldAccesses)
		{
			FFieldSummary& Summary = FieldSummaries.FindOrAdd(FieldKey(Fact));
			switch (Fact.AccessKind)
			{
			case EProjectRiskAccessKind::Read:
				Summary.Reads.Add(&Fact);
				break;
			case EProjectRiskAccessKind::ExplicitWrite:
				Summary.ExplicitWrites.Add(&Fact);
				break;
			case EProjectRiskAccessKind::DefaultWrite:
				Summary.DefaultWrites.Add(&Fact);
				break;
			case EProjectRiskAccessKind::ExternalSource:
				Summary.ExternalSources.Add(&Fact);
				break;
			}
		}

		for (const FContainerIdentityFact& Fact : BlueprintFacts.ContainerIdentities)
		{
			if (!Fact.bIdentityLike || Fact.KeyType != Fact.ValueFieldType)
			{
				continue;
			}

			if (Fact.bValueUsesDefault || (!Fact.KeySource.IsEmpty() && !Fact.ValueSource.IsEmpty() && Fact.KeySource != Fact.ValueSource))
			{
				Findings.Add(MakeFinding(
					TEXT("Warning"), TEXT("High"), TEXT("InconsistentIdentity"),
					TEXT("Container key and value identifier do not have the same proven source."),
					FString::Printf(TEXT("Container=%s; KeySource=%s; ValueField=%s; ValueSource=%s"), *Fact.ContainerExpression, *Fact.KeySource, *Fact.ValueFieldName, *Fact.ValueSource),
					Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
			}
			else if (!Fact.KeySource.IsEmpty() && Fact.KeySource == Fact.ValueSource)
			{
				Findings.Add(MakeFinding(
					TEXT("Info"), TEXT("High"), TEXT("IntentionalMirror"),
					TEXT("Container key and value identifier share the same source."),
					FString::Printf(TEXT("Container=%s; Source=%s; ValueField=%s"), *Fact.ContainerExpression, *Fact.KeySource, *Fact.ValueFieldName),
					Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
			}
			else
			{
				Findings.Add(MakeFinding(
					TEXT("Hint"), TEXT("Medium"), TEXT("UnverifiedMirror"),
					TEXT("Container key and value contain same-typed identifiers, but source consistency is unknown."),
					FString::Printf(TEXT("Container=%s; ValueField=%s"), *Fact.ContainerExpression, *Fact.ValueFieldName),
					Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
			}
		}

		for (const FMapFindFact& Fact : BlueprintFacts.MapFinds)
		{
			if (Fact.bResultChecked)
			{
				continue;
			}
			Findings.Add(MakeFinding(
				TEXT("Warning"), TEXT("High"), TEXT("UncheckedMapFind"),
				TEXT("Map lookup result is consumed without checking whether the key was found."),
				FString::Printf(TEXT("Map=%s; Key=%s; failure continues with %s; Consumers=%s"), *Fact.MapExpression, *Fact.KeyExpression, *DefaultValueDescription(Fact.ValueType), *FString::Join(Fact.ValueConsumers, TEXT(", "))),
				Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
		}

		for (const FRpcIdentityFact& Fact : BlueprintFacts.RpcIdentities)
		{
			if (!Fact.bRunOnServer || !Fact.bIdentityLike)
			{
				continue;
			}
			if (Fact.bCallSite)
			{
				Findings.Add(MakeFinding(
					Fact.bCallerValidated ? TEXT("Info") : TEXT("Hint"),
					Fact.bCallerValidated ? TEXT("Medium") : TEXT("Low"),
					Fact.bCallerValidated ? TEXT("CallerValidatedIdentityRpc") : TEXT("UnvalidatedIdentityRpcCallsite"),
					Fact.bCallerValidated
						? TEXT("Server RPC callsite identity input is guarded before the call.")
						: TEXT("Server RPC callsite identity input has no detected caller-side guard; server-side validation may still exist."),
					FString::Printf(TEXT("RPC=%s; Parameter=%s; Type=%s; Source=%s"), *Fact.RpcName, *Fact.ParameterName, *Fact.ParameterType, *Fact.SourceExpression),
					Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
				continue;
			}
			if (Fact.bCallerValidated || Fact.bServerValidated)
			{
				Findings.Add(MakeFinding(
					TEXT("Info"), TEXT("Medium"), TEXT("ValidatedIdentityRpc"),
					TEXT("Server RPC identity parameter has a detected validation guard."),
					FString::Printf(TEXT("RPC=%s; Parameter=%s; CallerValidated=%s; ServerValidated=%s"), *Fact.RpcName, *Fact.ParameterName, Fact.bCallerValidated ? TEXT("true") : TEXT("false"), Fact.bServerValidated ? TEXT("true") : TEXT("false")),
					Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
				continue;
			}
			Findings.Add(MakeFinding(
				TEXT("Warning"), TEXT("Medium"), TEXT("UnvalidatedIdentityRpc"),
				TEXT("Server RPC receives an identity-like parameter without a detected validity check."),
				FString::Printf(TEXT("RPC=%s; Parameter=%s; Type=%s; Source=%s"), *Fact.RpcName, *Fact.ParameterName, *Fact.ParameterType, *Fact.SourceExpression),
				Fact.AssetPath, Fact.GraphName, Fact.NodeId, Fact.NodeTitle));
		}
	}

	for (const TPair<FString, FFieldSummary>& Pair : FieldSummaries)
	{
		const FFieldSummary& Summary = Pair.Value;
		const FStructFieldAccessFact* CriticalRead = nullptr;
		for (const FStructFieldAccessFact* Read : Summary.Reads)
		{
			if (Read && Read->bIdentityLike && Read->bCriticalSink)
			{
				CriticalRead = Read;
				break;
			}
		}
		if (!CriticalRead || Summary.ExplicitWrites.Num() > 0 || Summary.ExternalSources.Num() > 0)
		{
			continue;
		}

		if (Summary.DefaultWrites.Num() > 0)
		{
			Findings.Add(MakeFinding(
				TEXT("Warning"), TEXT("High"), TEXT("DefaultOnly"),
				TEXT("Identity-like struct field reaches a critical sink but only default construction was found."),
				Pair.Key,
				CriticalRead->AssetPath, CriticalRead->GraphName, CriticalRead->NodeId, CriticalRead->NodeTitle));
		}
		else
		{
			Findings.Add(MakeFinding(
				TEXT("Warning"), TEXT("Medium"), TEXT("NoKnownWriter"),
				TEXT("No known Blueprint write was found for an identity-like struct field used by a critical sink."),
				Pair.Key,
				CriticalRead->AssetPath, CriticalRead->GraphName, CriticalRead->NodeId, CriticalRead->NodeTitle));
		}
	}

	Findings.Sort([](const FProjectRiskFinding& A, const FProjectRiskFinding& B)
	{
		if (A.Severity != B.Severity) return A.Severity < B.Severity;
		if (A.Category != B.Category) return A.Category < B.Category;
		if (A.AssetPath != B.AssetPath) return A.AssetPath < B.AssetPath;
		return A.NodeId < B.NodeId;
	});
	return Findings;
}

bool FProjectRiskReport::WriteReports(const TArray<FBlueprintRiskFacts>& Facts, const FString& OutputDirectory)
{
	using namespace GetTheMeaningProjectRisk;

	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	const TArray<FProjectRiskFinding> Findings = Analyze(Facts);

	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), 1);
	Writer->WriteValue(TEXT("generatedBy"), TEXT("GetTheMeaning"));
	Writer->WriteValue(TEXT("findingCount"), Findings.Num());
	Writer->WriteArrayStart(TEXT("findings"));
	for (const FProjectRiskFinding& Finding : Findings)
	{
		WriteFindingJson(Writer, Finding);
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	FString Markdown = TEXT("# GetTheMeaning Project Risk Report\n\n");
	Markdown += FString::Printf(TEXT("- Findings: %d\n\n"), Findings.Num());
	for (const FProjectRiskFinding& Finding : Findings)
	{
		Markdown += FString::Printf(TEXT("## [%s/%s] %s\n\n%s\n\n"), *Finding.Severity, *Finding.Confidence, *Finding.Category, *Finding.Message);
		Markdown += FString::Printf(TEXT("- Asset: %s\n- Graph: %s\n- Node: %s (%s)\n- Details: %s\n\n"), *Finding.AssetPath, *Finding.GraphName, *Finding.NodeTitle, *Finding.NodeId, *Finding.Details);
	}

	const bool bJsonSaved = FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("ProjectRiskReport.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bMarkdownSaved = FFileHelper::SaveStringToFile(Markdown, *(OutputDirectory / TEXT("ProjectRiskReport.md")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return bJsonSaved && bMarkdownSaved;
}

#include "SaveGameAuditor.h"

#include "BlueprintRiskAnalyzer.h"
#include "Dom/JsonObject.h"
#include "GameFramework/SaveGame.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

namespace GetTheMeaningSaveGameAudit
{
	constexpr int32 MaxRecursionDepth = 32;

	struct FAuditContext
	{
		FString FilePath;
		TArray<FString> IdentityNamePatterns;
		TArray<FSaveGameAuditFinding>* Findings = nullptr;
	};

	FSaveGameAuditFinding MakeFinding(
		const FString& Severity,
		const FString& Category,
		const FString& Message,
		const FString& Details,
		const FString& FilePath,
		const FString& PropertyPath)
	{
		FSaveGameAuditFinding Finding;
		Finding.Severity = Severity;
		Finding.Confidence = TEXT("Confirmed");
		Finding.Category = Category;
		Finding.Message = Message;
		Finding.Details = Details;
		Finding.FilePath = FilePath;
		Finding.PropertyPath = PropertyPath;
		return Finding;
	}

	bool IsGuidProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty && StructProperty->Struct == TBaseStructure<FGuid>::Get();
	}

	FString AppendPropertyPath(const FString& ParentPath, const FString& Name)
	{
		return ParentPath.IsEmpty() ? Name : ParentPath + TEXT(".") + Name;
	}

	void AuditPropertyValue(
		const FProperty* Property,
		const void* ValuePtr,
		const FString& PropertyPath,
		FAuditContext& Context,
		int32 Depth);

	void AuditStructValue(
		const UStruct* Struct,
		const void* ContainerPtr,
		const FString& ParentPath,
		FAuditContext& Context,
		int32 Depth)
	{
		if (!Struct || !ContainerPtr || !Context.Findings || Depth > MaxRecursionDepth)
		{
			return;
		}

		for (TFieldIterator<const FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			{
				continue;
			}

			for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
			{
				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr, ArrayIndex);
				FString PropertyPath = AppendPropertyPath(ParentPath, Property->GetName());
				if (Property->ArrayDim > 1)
				{
					PropertyPath += FString::Printf(TEXT("[%d]"), ArrayIndex);
				}
				AuditPropertyValue(Property, ValuePtr, PropertyPath, Context, Depth + 1);
			}
		}
	}

	TArray<TPair<FString, FGuid>> FindIdentityGuidMembers(
		const FStructProperty* StructProperty,
		const void* StructValuePtr,
		const TArray<FString>& IdentityNamePatterns)
	{
		TArray<TPair<FString, FGuid>> Members;
		if (!StructProperty || !StructProperty->Struct || !StructValuePtr)
		{
			return Members;
		}

		for (TFieldIterator<const FProperty> It(StructProperty->Struct); It; ++It)
		{
			const FProperty* Member = *It;
			if (!Member || !IsGuidProperty(Member)
				|| Member->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				|| !FBlueprintRiskAnalyzer::IsIdentityLikeName(Member->GetName(), IdentityNamePatterns))
			{
				continue;
			}

			for (int32 ArrayIndex = 0; ArrayIndex < Member->ArrayDim; ++ArrayIndex)
			{
				const FGuid* Value = Member->ContainerPtrToValuePtr<FGuid>(StructValuePtr, ArrayIndex);
				if (Value)
				{
					FString MemberPath = Member->GetName();
					if (Member->ArrayDim > 1)
					{
						MemberPath += FString::Printf(TEXT("[%d]"), ArrayIndex);
					}
					Members.Emplace(MoveTemp(MemberPath), *Value);
				}
			}
		}
		return Members;
	}

	void AuditMapValue(
		const FMapProperty* MapProperty,
		const void* MapPtr,
		const FString& PropertyPath,
		FAuditContext& Context,
		int32 Depth)
	{
		FScriptMapHelper MapHelper(MapProperty, MapPtr);
		int32 LogicalIndex = 0;
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It, ++LogicalIndex)
		{
			const void* KeyPtr = MapHelper.GetKeyPtr(It);
			const void* ValuePtr = MapHelper.GetValuePtr(It);
			const FString EntryPath = FString::Printf(TEXT("%s[%d]"), *PropertyPath, LogicalIndex);

			if (IsGuidProperty(MapProperty->KeyProp))
			{
				const FGuid& Key = *static_cast<const FGuid*>(KeyPtr);
				FSaveGameAuditor::AuditGuidMapEntry(
					Key,
					TOptional<FGuid>(),
					Context.FilePath,
					EntryPath,
					*Context.Findings);

				if (const FStructProperty* ValueStruct = CastField<FStructProperty>(MapProperty->ValueProp))
				{
					const TArray<TPair<FString, FGuid>> IdentityMembers = FindIdentityGuidMembers(
						ValueStruct,
						ValuePtr,
						Context.IdentityNamePatterns);
					for (const TPair<FString, FGuid>& IdentityMember : IdentityMembers)
					{
						FSaveGameAuditor::AuditGuidMapEntry(
							Key,
							TOptional<FGuid>(IdentityMember.Value),
							Context.FilePath,
							EntryPath + TEXT(".Value.") + IdentityMember.Key,
							*Context.Findings);
					}
					if (IdentityMembers.Num() > 1)
					{
						TArray<FString> MemberNames;
						for (const TPair<FString, FGuid>& Member : IdentityMembers)
						{
							MemberNames.Add(Member.Key);
						}
						Context.Findings->Add(MakeFinding(
							TEXT("Info"),
							TEXT("DuplicateIdentityFields"),
							TEXT("Map value contains multiple identity-like GUID fields."),
							FString::Join(MemberNames, TEXT(", ")),
							Context.FilePath,
							EntryPath));
					}
				}
			}

			if (!IsGuidProperty(MapProperty->KeyProp))
			{
				AuditPropertyValue(MapProperty->KeyProp, KeyPtr, EntryPath + TEXT(".Key"), Context, Depth + 1);
			}
			AuditPropertyValue(MapProperty->ValueProp, ValuePtr, EntryPath + TEXT(".Value"), Context, Depth + 1);
		}
	}

	void AuditPropertyValue(
		const FProperty* Property,
		const void* ValuePtr,
		const FString& PropertyPath,
		FAuditContext& Context,
		int32 Depth)
	{
		if (!Property || !ValuePtr || !Context.Findings || Depth > MaxRecursionDepth)
		{
			return;
		}

		if (IsGuidProperty(Property))
		{
			FSaveGameAuditor::AuditGuidValue(*static_cast<const FGuid*>(ValuePtr), Context.FilePath, PropertyPath, *Context.Findings);
			return;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			AuditStructValue(StructProperty->Struct, ValuePtr, PropertyPath, Context, Depth + 1);
			return;
		}

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			{
				AuditPropertyValue(
					ArrayProperty->Inner,
					ArrayHelper.GetRawPtr(Index),
					FString::Printf(TEXT("%s[%d]"), *PropertyPath, Index),
					Context,
					Depth + 1);
			}
			return;
		}

		if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper SetHelper(SetProperty, ValuePtr);
			int32 LogicalIndex = 0;
			for (FScriptSetHelper::FIterator It(SetHelper); It; ++It, ++LogicalIndex)
			{
				AuditPropertyValue(
					SetProperty->ElementProp,
					SetHelper.GetElementPtr(It),
					FString::Printf(TEXT("%s[%d]"), *PropertyPath, LogicalIndex),
					Context,
					Depth + 1);
			}
			return;
		}

		if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			AuditMapValue(MapProperty, ValuePtr, PropertyPath, Context, Depth + 1);
			return;
		}

	}

	void WriteFindingJson(
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>>& Writer,
		const FSaveGameAuditFinding& Finding)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("severity"), Finding.Severity);
		Writer->WriteValue(TEXT("confidence"), Finding.Confidence);
		Writer->WriteValue(TEXT("category"), Finding.Category);
		Writer->WriteValue(TEXT("message"), Finding.Message);
		Writer->WriteValue(TEXT("details"), Finding.Details);
		Writer->WriteValue(TEXT("filePath"), Finding.FilePath);
		Writer->WriteValue(TEXT("propertyPath"), Finding.PropertyPath);
		Writer->WriteObjectEnd();
	}
}

void FSaveGameAuditor::AuditGuidValue(
	const FGuid& Value,
	const FString& FilePath,
	const FString& PropertyPath,
	TArray<FSaveGameAuditFinding>& OutFindings)
{
	if (Value.IsValid())
	{
		return;
	}

	OutFindings.Add(GetTheMeaningSaveGameAudit::MakeFinding(
		TEXT("Warning"),
		TEXT("InvalidGuid"),
		TEXT("GUID value is invalid."),
		TEXT("The stored value is the zero/default GUID."),
		FilePath,
		PropertyPath));
}

void FSaveGameAuditor::AuditGuidMapEntry(
	const FGuid& Key,
	const TOptional<FGuid>& ValueIdentifier,
	const FString& FilePath,
	const FString& PropertyPath,
	TArray<FSaveGameAuditFinding>& OutFindings)
{
	using namespace GetTheMeaningSaveGameAudit;

	if (!Key.IsValid())
	{
		OutFindings.Add(MakeFinding(
			TEXT("Error"),
			TEXT("InvalidMapKey"),
			TEXT("Map contains an invalid GUID key."),
			TEXT("The key is the zero/default GUID."),
			FilePath,
			PropertyPath + TEXT(".Key")));
	}

	if (ValueIdentifier.IsSet() && Key.IsValid() && Key != ValueIdentifier.GetValue())
	{
		OutFindings.Add(MakeFinding(
			TEXT("Warning"),
			TEXT("IdentityMismatch"),
			TEXT("Map key and value identifier do not match."),
			FString::Printf(TEXT("Key=%s; ValueIdentifier=%s"), *Key.ToString(), *ValueIdentifier.GetValue().ToString()),
			FilePath,
			PropertyPath));
	}
}

TArray<FSaveGameAuditFinding> FSaveGameAuditor::AuditDirectory(
	const FString& Directory,
	const TArray<FString>& IdentityNamePatterns,
	FSaveGameAuditProgress Progress)
{
	using namespace GetTheMeaningSaveGameAudit;

	TArray<FSaveGameAuditFinding> Findings;
	TArray<FString> SaveFiles;
	IFileManager::Get().FindFilesRecursive(SaveFiles, *Directory, TEXT("*.sav"), true, false);
	SaveFiles.Sort();

	for (int32 FileIndex = 0; FileIndex < SaveFiles.Num(); ++FileIndex)
	{
		const FString& SaveFile = SaveFiles[FileIndex];
		if (Progress && !Progress(FileIndex, SaveFiles.Num(), SaveFile))
		{
			break;
		}

		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *SaveFile))
		{
			Findings.Add(MakeFinding(
				TEXT("Error"), TEXT("UnreadableFile"), TEXT("SaveGame file could not be read."),
				TEXT("File loading failed; no other files were skipped."), SaveFile, FString()));
			continue;
		}

		USaveGame* SaveGame = UGameplayStatics::LoadGameFromMemory(Bytes);
		if (!SaveGame)
		{
			Findings.Add(MakeFinding(
				TEXT("Error"), TEXT("DeserializeFailed"), TEXT("SaveGame file could not be deserialized."),
				TEXT("The file may be corrupt, incompatible, or require unavailable classes."), SaveFile, FString()));
			continue;
		}

		FAuditContext Context;
		Context.FilePath = SaveFile;
		Context.IdentityNamePatterns = IdentityNamePatterns;
		Context.Findings = &Findings;
		AuditStructValue(SaveGame->GetClass(), SaveGame, SaveGame->GetClass()->GetName(), Context, 0);
	}

	Findings.Sort([](const FSaveGameAuditFinding& A, const FSaveGameAuditFinding& B)
	{
		if (A.FilePath != B.FilePath) return A.FilePath < B.FilePath;
		if (A.PropertyPath != B.PropertyPath) return A.PropertyPath < B.PropertyPath;
		return A.Category < B.Category;
	});
	return Findings;
}

bool FSaveGameAuditor::WriteReports(
	const TArray<FSaveGameAuditFinding>& Findings,
	const FString& SourceDirectory,
	const FString& OutputDirectory)
{
	using namespace GetTheMeaningSaveGameAudit;

	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), 1);
	Writer->WriteValue(TEXT("generatedBy"), TEXT("GetTheMeaning"));
	Writer->WriteValue(TEXT("sourceDirectory"), SourceDirectory);
	Writer->WriteValue(TEXT("findingCount"), Findings.Num());
	Writer->WriteArrayStart(TEXT("findings"));
	for (const FSaveGameAuditFinding& Finding : Findings)
	{
		WriteFindingJson(Writer, Finding);
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	FString Markdown = TEXT("# GetTheMeaning SaveGame Audit\n\n");
	Markdown += TEXT("This report is generated by a read-only reflection audit. No SaveGame file was modified.\n\n");
	Markdown += FString::Printf(TEXT("- Source directory: %s\n- Findings: %d\n\n"), *SourceDirectory, Findings.Num());
	for (const FSaveGameAuditFinding& Finding : Findings)
	{
		Markdown += FString::Printf(
			TEXT("## [%s/%s] %s\n\n%s\n\n- File: %s\n- Property: %s\n- Details: %s\n\n"),
			*Finding.Severity,
			*Finding.Confidence,
			*Finding.Category,
			*Finding.Message,
			*Finding.FilePath,
			*Finding.PropertyPath,
			*Finding.Details);
	}

	const bool bJsonSaved = FFileHelper::SaveStringToFile(
		Json,
		*(OutputDirectory / TEXT("SaveGameAudit.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bMarkdownSaved = FFileHelper::SaveStringToFile(
		Markdown,
		*(OutputDirectory / TEXT("SaveGameAudit.md")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return bJsonSaved && bMarkdownSaved;
}

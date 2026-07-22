// Copyright Epic Games, Inc. All Rights Reserved.

#include "GetTheMeaning.h"
#include "BlueprintToTextExporter.h"
#include "MaterialToTextExporter.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallDataTableFunction.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_SpawnActor.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_StructOperation.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialFunctionInterface.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DataTableUtils.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "PlayerMappableKeySettings.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserMenuContexts.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
// 引用查看器（Reference Viewer）集成所需
#include "GraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "ReferenceViewer/EdGraphNode_Reference.h"
#include "ReferenceViewer/EdGraph_ReferenceViewer.h"

#define LOCTEXT_NAMESPACE "FGetTheMeaningModule"

namespace GetTheMeaningExportImpl
{
	// 所有 AI 可读文档的统一导出根目录
	static FString GetExportRootDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("GetTheMeaningExports");
	}

	static bool IsSupportedAssetData(const FAssetData& AssetData)
	{
		return AssetData.IsValid()
			&& (AssetData.IsInstanceOf(UBlueprint::StaticClass())
				|| AssetData.IsInstanceOf(UMaterialInterface::StaticClass())
				|| AssetData.IsInstanceOf(UMaterialFunctionInterface::StaticClass())
				|| AssetData.IsInstanceOf(UDataTable::StaticClass())
				|| AssetData.IsInstanceOf(UUserDefinedStruct::StaticClass())
				|| AssetData.IsInstanceOf(UUserDefinedEnum::StaticClass())
				|| AssetData.IsInstanceOf(UWorld::StaticClass())
				|| AssetData.IsInstanceOf(UInputAction::StaticClass())
				|| AssetData.IsInstanceOf(UInputMappingContext::StaticClass()));
	}

	static FString SanitizePathSegment(const FString& Segment)
	{
		FString Out = FPaths::MakeValidFileName(Segment);
		return Out.IsEmpty() ? TEXT("_") : Out;
	}

	static bool EnsureDir(const FString& Dir)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*Dir))
		{
			return PlatformFile.CreateDirectoryTree(*Dir);
		}
		return true;
	}

	static FString GetExportDirForAsset(const FAssetData& AssetData)
	{
		const FString PackagePath = AssetData.PackagePath.ToString();
		TArray<FString> Segments;
		PackagePath.ParseIntoArray(Segments, TEXT("/"), true);

		FString Dir = GetExportRootDir();
		for (const FString& Segment : Segments)
		{
			Dir /= SanitizePathSegment(Segment);
		}
		return Dir;
	}

	// 保存文本到指定目录/文件；返回实际保存路径，失败返回空串
	static FString SaveText(const FString& Text, const FString& Dir, const FString& AssetName, const FString& FileSuffix)
	{
		if (Text.IsEmpty() || AssetName.IsEmpty()) return FString();
		if (!EnsureDir(Dir)) return FString();

		const FString SavePath = Dir / (AssetName + FileSuffix);
		if (FFileHelper::SaveStringToFile(Text, *SavePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return SavePath;
		}
		return FString();
	}

	static FString GetAssetExportDirOrRoot(const FAssetData& AssetData)
	{
		const FString Dir = GetExportDirForAsset(AssetData);
		return Dir.IsEmpty() ? GetExportRootDir() : Dir;
	}

	static FString GetStandaloneExportObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	static FString GetExportedPropertyType(const FProperty* Property)
	{
		if (!Property)
		{
			return FString();
		}

		FString ExtendedType;
		const FString CppType = Property->GetCPPType(&ExtendedType);
		return ExtendedType.IsEmpty() ? CppType : CppType + ExtendedType;
	}

	static FString GetPinContainerTypeString(EPinContainerType ContainerType)
	{
		switch (ContainerType)
		{
		case EPinContainerType::Array:
			return TEXT("Array");
		case EPinContainerType::Set:
			return TEXT("Set");
		case EPinContainerType::Map:
			return TEXT("Map");
		case EPinContainerType::None:
		default:
			return TEXT("None");
		}
	}

	static FString GetPinTypeSummary(const FEdGraphPinType& PinType)
	{
		FString Type = PinType.PinCategory.ToString();
		if (!PinType.PinSubCategory.IsNone())
		{
			Type += TEXT(":") + PinType.PinSubCategory.ToString();
		}
		if (const UObject* SubCategoryObject = PinType.PinSubCategoryObject.Get())
		{
			Type += TEXT("<") + GetStandaloneExportObjectPath(SubCategoryObject) + TEXT(">");
		}
		if (PinType.ContainerType != EPinContainerType::None)
		{
			Type = GetPinContainerTypeString(PinType.ContainerType) + TEXT("<") + Type + TEXT(">");
		}
		if (PinType.PinValueType.TerminalCategory != NAME_None)
		{
			Type += TEXT(" -> ") + PinType.PinValueType.TerminalCategory.ToString();
		}
		return Type;
	}

	template<typename EnumType>
	static FString GetEnumValueName(EnumType Value)
	{
		if (const UEnum* Enum = StaticEnum<EnumType>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}
		return FString::FromInt(static_cast<int32>(Value));
	}

	static FString GetWorldTypeName(EWorldType::Type WorldType)
	{
		return FString(LexToString(WorldType));
	}

	static FString GetObjectClassNameSafe(const UObject* Object)
	{
		return Object && Object->GetClass() ? Object->GetClass()->GetName() : FString();
	}

	static FString GetObjectAssetPathSafe(const UObject* Object)
	{
		if (!Object)
		{
			return FString();
		}
		if (const UClass* Class = Cast<UClass>(Object))
		{
			if (Class->ClassGeneratedBy)
			{
				return Class->ClassGeneratedBy->GetPathName();
			}
		}
		return Object->GetPathName();
	}

	static FString GetBlueprintSourcePathFromClass(const UClass* Class)
	{
		if (!Class)
		{
			return FString();
		}
		if (Class->ClassGeneratedBy)
		{
			return Class->ClassGeneratedBy->GetPathName();
		}
		if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class))
		{
			return BlueprintClass->ClassGeneratedBy ? BlueprintClass->ClassGeneratedBy->GetPathName() : FString();
		}
		return FString();
	}

	static FString JoinNames(const TArray<FName>& Names)
	{
		TArray<FString> Parts;
		for (const FName& Name : Names)
		{
			if (!Name.IsNone())
			{
				Parts.Add(Name.ToString());
			}
		}
		return FString::Join(Parts, TEXT(", "));
	}

	static void WriteNameArrayJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<FName>& Names)
	{
		Writer->WriteArrayStart(FieldName);
		for (const FName& Name : Names)
		{
			if (!Name.IsNone())
			{
				Writer->WriteValue(Name.ToString());
			}
		}
		Writer->WriteArrayEnd();
	}

	static FString ExportObjectPropertyValue(const UObject* Object, const FProperty* Property)
	{
		if (!Object || !Property)
		{
			return FString();
		}

		FString Value;
		Property->ExportText_InContainer(0, Value, Object, Object, const_cast<UObject*>(Object), PPF_None);
		return Value;
	}

	static void WriteSimpleObjectArrayJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<TObjectPtr<UInputTrigger>>& Objects);
	static void WriteSimpleObjectArrayJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<TObjectPtr<UInputModifier>>& Objects);

	static void WriteObjectSummaryJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const UObject* Object)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Object ? Object->GetName() : FString());
		Writer->WriteValue(TEXT("class"), GetObjectClassNameSafe(Object));
		Writer->WriteValue(TEXT("path"), GetObjectAssetPathSafe(Object));

		Writer->WriteObjectStart(TEXT("properties"));
		if (Object)
		{
			for (TFieldIterator<const FProperty> It(Object->GetClass()); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_DisableEditOnInstance))
				{
					continue;
				}

				const FString Value = ExportObjectPropertyValue(Object, Property);
				if (!Value.IsEmpty())
				{
					Writer->WriteValue(Property->GetName(), Value);
				}
			}
		}
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
	}

	static void WriteObjectSummaryJsonField(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const UObject* Object)
	{
		Writer->WriteObjectStart(FieldName);
		Writer->WriteValue(TEXT("name"), Object ? Object->GetName() : FString());
		Writer->WriteValue(TEXT("class"), GetObjectClassNameSafe(Object));
		Writer->WriteValue(TEXT("path"), GetObjectAssetPathSafe(Object));

		Writer->WriteObjectStart(TEXT("properties"));
		if (Object)
		{
			for (TFieldIterator<const FProperty> It(Object->GetClass()); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_DisableEditOnInstance))
				{
					continue;
				}

				const FString Value = ExportObjectPropertyValue(Object, Property);
				if (!Value.IsEmpty())
				{
					Writer->WriteValue(Property->GetName(), Value);
				}
			}
		}
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
	}

	template<typename ObjectType>
	static void WriteSimpleObjectArrayJsonImpl(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<TObjectPtr<ObjectType>>& Objects)
	{
		Writer->WriteArrayStart(FieldName);
		for (const TObjectPtr<ObjectType>& Object : Objects)
		{
			WriteObjectSummaryJson(Writer, Object.Get());
		}
		Writer->WriteArrayEnd();
	}

	static void WriteSimpleObjectArrayJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<TObjectPtr<UInputTrigger>>& Objects)
	{
		WriteSimpleObjectArrayJsonImpl(Writer, FieldName, Objects);
	}

	static void WriteSimpleObjectArrayJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<TObjectPtr<UInputModifier>>& Objects)
	{
		WriteSimpleObjectArrayJsonImpl(Writer, FieldName, Objects);
	}

	static void AppendInputObjectListMarkdown(FString& Out, const FString& Heading, const TArray<TObjectPtr<UInputTrigger>>& Objects);
	static void AppendInputObjectListMarkdown(FString& Out, const FString& Heading, const TArray<TObjectPtr<UInputModifier>>& Objects);

	template<typename ObjectType>
	static void AppendInputObjectListMarkdownImpl(FString& Out, const FString& Heading, const TArray<TObjectPtr<ObjectType>>& Objects)
	{
		if (Objects.Num() == 0)
		{
			return;
		}

		Out += TEXT("\n") + Heading + TEXT(":\n");
		for (const TObjectPtr<ObjectType>& Object : Objects)
		{
			if (!Object)
			{
				continue;
			}
			Out += TEXT("- ") + Object->GetClass()->GetName();
			Out += TEXT("\n");
		}
	}

	static void AppendInputObjectListMarkdown(FString& Out, const FString& Heading, const TArray<TObjectPtr<UInputTrigger>>& Objects)
	{
		AppendInputObjectListMarkdownImpl(Out, Heading, Objects);
	}

	static void AppendInputObjectListMarkdown(FString& Out, const FString& Heading, const TArray<TObjectPtr<UInputModifier>>& Objects)
	{
		AppendInputObjectListMarkdownImpl(Out, Heading, Objects);
	}

	static void WriteAssetHeaderJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const UObject* Asset, const FString& Type)
	{
		Writer->WriteObjectStart(TEXT("asset"));
		Writer->WriteValue(TEXT("name"), Asset ? Asset->GetName() : FString());
		Writer->WriteValue(TEXT("path"), GetStandaloneExportObjectPath(Asset));
		Writer->WriteValue(TEXT("package"), Asset && Asset->GetOutermost() ? Asset->GetOutermost()->GetName() : FString());
		Writer->WriteValue(TEXT("type"), Type);
		Writer->WriteObjectEnd();
	}

	static void WritePropertyDefinitionJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const FProperty* Property)
	{
		if (!Property)
		{
			return;
		}

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Property->GetName());
		Writer->WriteValue(TEXT("displayName"), Property->GetDisplayNameText().ToString());
		Writer->WriteValue(TEXT("cppType"), GetExportedPropertyType(Property));
		Writer->WriteValue(TEXT("propertyClass"), Property->GetClass() ? Property->GetClass()->GetName() : FString());
		Writer->WriteValue(TEXT("arrayDim"), Property->ArrayDim);
		Writer->WriteValue(TEXT("flags"), FString::Printf(TEXT("0x%016llx"), static_cast<unsigned long long>(Property->PropertyFlags)));
		Writer->WriteValue(TEXT("category"), Property->GetMetaData(TEXT("Category")));
		Writer->WriteValue(TEXT("tooltip"), Property->GetToolTipText().ToString());
		Writer->WriteObjectEnd();
	}

	static void WriteStructFieldsJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const UScriptStruct* Struct)
	{
		Writer->WriteArrayStart(TEXT("fields"));
		if (Struct)
		{
			for (TFieldIterator<const FProperty> It(Struct); It; ++It)
			{
				WritePropertyDefinitionJson(Writer, *It);
			}
		}
		Writer->WriteArrayEnd();
	}

	static FString ExportDataTableToJson(UDataTable* DataTable)
	{
		if (!DataTable)
		{
			return FString();
		}

		const EDataTableExportFlags ExportFlags = EDataTableExportFlags::UseJsonObjectsForStructs | EDataTableExportFlags::UseSimpleText;
		const FString RowsJson = DataTable->GetTableAsJSON(ExportFlags);
		if (RowsJson.IsEmpty() || RowsJson == TEXT("Missing RowStruct!\n"))
		{
			return FString();
		}

		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		WriteAssetHeaderJson(Writer, DataTable, TEXT("DataTable"));
		Writer->WriteObjectStart(TEXT("table"));
		Writer->WriteValue(TEXT("rowStruct"), DataTable->GetRowStruct() ? DataTable->GetRowStruct()->GetName() : FString());
		Writer->WriteValue(TEXT("rowStructPath"), GetStandaloneExportObjectPath(DataTable->GetRowStruct()));
		Writer->WriteValue(TEXT("rowCount"), DataTable->GetRowMap().Num());
		Writer->WriteValue(TEXT("importKeyField"), DataTable->ImportKeyField);
		Writer->WriteValue(TEXT("stripFromClientBuilds"), !!DataTable->bStripFromClientBuilds);
		Writer->WriteValue(TEXT("ignoreExtraFields"), !!DataTable->bIgnoreExtraFields);
		Writer->WriteValue(TEXT("ignoreMissingFields"), !!DataTable->bIgnoreMissingFields);
		Writer->WriteValue(TEXT("preserveExistingValues"), !!DataTable->bPreserveExistingValues);
		Writer->WriteObjectEnd();
		WriteStructFieldsJson(Writer, DataTable->GetRowStruct());
		Writer->WriteRawJSONValue(TEXT("rows"), RowsJson);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Out;
	}

	static FString ExportDataTableToMarkdown(UDataTable* DataTable)
	{
		if (!DataTable)
		{
			return FString();
		}

		const EDataTableExportFlags ExportFlags = EDataTableExportFlags::UseJsonObjectsForStructs | EDataTableExportFlags::UseSimpleText;
		FString Out;
		Out += TEXT("# DataTable Export\n\n");
		Out += TEXT("- Name: ") + DataTable->GetName() + TEXT("\n");
		Out += TEXT("- AssetPath: ") + GetStandaloneExportObjectPath(DataTable) + TEXT("\n");
		Out += TEXT("- RowStruct: ") + GetStandaloneExportObjectPath(DataTable->GetRowStruct()) + TEXT("\n");
		Out += FString::Printf(TEXT("- RowCount: %d\n\n"), DataTable->GetRowMap().Num());

		Out += TEXT("## Columns\n\n");
		if (const UScriptStruct* RowStruct = DataTable->GetRowStruct())
		{
			for (TFieldIterator<const FProperty> It(RowStruct); It; ++It)
			{
				const FProperty* Property = *It;
				Out += FString::Printf(TEXT("- %s: %s\n"), *Property->GetName(), *GetExportedPropertyType(Property));
			}
		}
		else
		{
			Out += TEXT("- (missing row struct)\n");
		}

		Out += TEXT("\n## Rows (CSV)\n\n```csv\n");
		Out += DataTable->GetTableAsCSV(ExportFlags);
		Out += TEXT("\n```\n");
		return Out;
	}

	static FString GetUserDefinedStructStatusString(const UUserDefinedStruct* Struct)
	{
		if (!Struct)
		{
			return FString();
		}

		switch (Struct->Status)
		{
		case UDSS_UpToDate:
			return TEXT("UpToDate");
		case UDSS_Dirty:
			return TEXT("Dirty");
		case UDSS_Error:
			return TEXT("Error");
		case UDSS_Duplicate:
			return TEXT("Duplicate");
		default:
			return TEXT("Unknown");
		}
	}

	static FString ExportStructToJson(UUserDefinedStruct* Struct)
	{
		if (!Struct)
		{
			return FString();
		}

		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		WriteAssetHeaderJson(Writer, Struct, TEXT("UserDefinedStruct"));
		Writer->WriteObjectStart(TEXT("struct"));
		Writer->WriteValue(TEXT("name"), Struct->GetName());
		Writer->WriteValue(TEXT("cppName"), Struct->GetStructCPPName(0));
		Writer->WriteValue(TEXT("path"), GetStandaloneExportObjectPath(Struct));
		Writer->WriteValue(TEXT("guid"), Struct->Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
		Writer->WriteValue(TEXT("status"), GetUserDefinedStructStatusString(Struct));
		Writer->WriteValue(TEXT("errorMessage"), Struct->ErrorMessage);
		Writer->WriteObjectEnd();
		WriteStructFieldsJson(Writer, Struct);

		Writer->WriteArrayStart(TEXT("userVariables"));
		const TArray<FStructVariableDescription>& VarDescriptions = FStructureEditorUtils::GetVarDesc(Struct);
		for (const FStructVariableDescription& Var : VarDescriptions)
		{
			const FEdGraphPinType PinType = Var.ToPinType();
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Var.VarName.ToString());
			Writer->WriteValue(TEXT("guid"), Var.VarGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
			Writer->WriteValue(TEXT("friendlyName"), Var.FriendlyName);
			Writer->WriteValue(TEXT("defaultValue"), Var.DefaultValue);
			Writer->WriteValue(TEXT("currentDefaultValue"), Var.CurrentDefaultValue);
			Writer->WriteValue(TEXT("tooltip"), Var.ToolTip);
			Writer->WriteValue(TEXT("pinType"), GetPinTypeSummary(PinType));
			Writer->WriteValue(TEXT("category"), Var.Category.ToString());
			Writer->WriteValue(TEXT("subCategory"), Var.SubCategory.ToString());
			Writer->WriteValue(TEXT("subCategoryObject"), GetStandaloneExportObjectPath(Var.SubCategoryObject.Get()));
			Writer->WriteValue(TEXT("containerType"), GetPinContainerTypeString(Var.ContainerType));
			Writer->WriteValue(TEXT("saveGame"), !!Var.bEnableSaveGame);
			Writer->WriteValue(TEXT("multiLineText"), !!Var.bEnableMultiLineText);
			Writer->WriteValue(TEXT("widget3d"), !!Var.bEnable3dWidget);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteObjectEnd();
		Writer->Close();
		return Out;
	}

	static FString ExportStructToMarkdown(UUserDefinedStruct* Struct)
	{
		if (!Struct)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("# Struct Export\n\n");
		Out += TEXT("- Name: ") + Struct->GetName() + TEXT("\n");
		Out += TEXT("- AssetPath: ") + GetStandaloneExportObjectPath(Struct) + TEXT("\n");
		Out += TEXT("- Guid: ") + Struct->Guid.ToString(EGuidFormats::DigitsWithHyphensLower) + TEXT("\n");
		Out += TEXT("- Status: ") + GetUserDefinedStructStatusString(Struct) + TEXT("\n\n");
		Out += TEXT("## Fields\n\n");

		const TArray<FStructVariableDescription>& VarDescriptions = FStructureEditorUtils::GetVarDesc(Struct);
		if (VarDescriptions.Num() > 0)
		{
			for (const FStructVariableDescription& Var : VarDescriptions)
			{
				Out += FString::Printf(TEXT("- %s: %s"), *Var.FriendlyName, *GetPinTypeSummary(Var.ToPinType()));
				const FString DefaultValue = Var.CurrentDefaultValue.IsEmpty() ? Var.DefaultValue : Var.CurrentDefaultValue;
				if (!DefaultValue.IsEmpty())
				{
					Out += TEXT(" = ") + DefaultValue;
				}
				Out += TEXT("\n");
			}
		}
		else
		{
			for (TFieldIterator<const FProperty> It(Struct); It; ++It)
			{
				const FProperty* Property = *It;
				Out += FString::Printf(TEXT("- %s: %s\n"), *Property->GetName(), *GetExportedPropertyType(Property));
			}
		}
		return Out;
	}

	static FString ExportEnumToJson(UUserDefinedEnum* Enum)
	{
		if (!Enum)
		{
			return FString();
		}

		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		WriteAssetHeaderJson(Writer, Enum, TEXT("UserDefinedEnum"));
		Writer->WriteObjectStart(TEXT("enum"));
		Writer->WriteValue(TEXT("name"), Enum->GetName());
		Writer->WriteValue(TEXT("path"), GetStandaloneExportObjectPath(Enum));
		Writer->WriteValue(TEXT("cppForm"), static_cast<int32>(Enum->GetCppForm()));
		Writer->WriteObjectEnd();

		Writer->WriteArrayStart(TEXT("entries"));
		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			const FString Name = Enum->GetNameStringByIndex(Index);
			if (Name.EndsWith(TEXT("_MAX")) || Name.Equals(TEXT("MAX"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("index"), Index);
			Writer->WriteValue(TEXT("name"), Name);
			Writer->WriteValue(TEXT("authoredName"), Enum->GetAuthoredNameStringByIndex(Index));
			Writer->WriteValue(TEXT("fullName"), Enum->GetNameByIndex(Index).ToString());
			Writer->WriteValue(TEXT("displayName"), Enum->GetDisplayNameTextByIndex(Index).ToString());
			Writer->WriteValue(TEXT("value"), Enum->GetValueByIndex(Index));
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteObjectEnd();
		Writer->Close();
		return Out;
	}

	static FString ExportEnumToMarkdown(UUserDefinedEnum* Enum)
	{
		if (!Enum)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("# Enum Export\n\n");
		Out += TEXT("- Name: ") + Enum->GetName() + TEXT("\n");
		Out += TEXT("- AssetPath: ") + GetStandaloneExportObjectPath(Enum) + TEXT("\n\n");
		Out += TEXT("## Entries\n\n");
		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			const FString Name = Enum->GetNameStringByIndex(Index);
			if (Name.EndsWith(TEXT("_MAX")) || Name.Equals(TEXT("MAX"), ESearchCase::IgnoreCase))
			{
				continue;
			}
			Out += FString::Printf(TEXT("- %lld: %s (%s)\n"), Enum->GetValueByIndex(Index), *Name, *Enum->GetDisplayNameTextByIndex(Index).ToString());
		}
		return Out;
	}

	static void WriteTransformJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		const FRotator Rotation = Transform.Rotator();
		const FVector Scale = Transform.GetScale3D();

		Writer->WriteObjectStart(FieldName);
		Writer->WriteObjectStart(TEXT("location"));
		Writer->WriteValue(TEXT("x"), Location.X);
		Writer->WriteValue(TEXT("y"), Location.Y);
		Writer->WriteValue(TEXT("z"), Location.Z);
		Writer->WriteObjectEnd();
		Writer->WriteObjectStart(TEXT("rotation"));
		Writer->WriteValue(TEXT("pitch"), Rotation.Pitch);
		Writer->WriteValue(TEXT("yaw"), Rotation.Yaw);
		Writer->WriteValue(TEXT("roll"), Rotation.Roll);
		Writer->WriteObjectEnd();
		Writer->WriteObjectStart(TEXT("scale"));
		Writer->WriteValue(TEXT("x"), Scale.X);
		Writer->WriteValue(TEXT("y"), Scale.Y);
		Writer->WriteValue(TEXT("z"), Scale.Z);
		Writer->WriteObjectEnd();
		Writer->WriteValue(TEXT("text"), Transform.ToHumanReadableString());
		Writer->WriteObjectEnd();
	}

	static FString TransformToShortText(const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		const FRotator Rotation = Transform.Rotator();
		const FVector Scale = Transform.GetScale3D();
		return FString::Printf(
			TEXT("Loc(%.1f, %.1f, %.1f) Rot(%.1f, %.1f, %.1f) Scale(%.2f, %.2f, %.2f)"),
			Location.X, Location.Y, Location.Z,
			Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
			Scale.X, Scale.Y, Scale.Z
		);
	}

	static FString ExportWorldSummaryToJson(UWorld* World)
	{
		if (!World)
		{
			return FString();
		}

		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		WriteAssetHeaderJson(Writer, World, TEXT("World"));
		Writer->WriteValue(TEXT("mapName"), World->GetMapName());
		Writer->WriteValue(TEXT("worldType"), GetWorldTypeName(World->WorldType));

		AWorldSettings* WorldSettings = World->GetWorldSettings(false, false);
		Writer->WriteObjectStart(TEXT("worldSettings"));
		Writer->WriteValue(TEXT("path"), GetObjectAssetPathSafe(WorldSettings));
		Writer->WriteValue(TEXT("defaultGameModeClass"), WorldSettings && WorldSettings->DefaultGameMode ? WorldSettings->DefaultGameMode->GetPathName() : FString());
		Writer->WriteValue(TEXT("defaultGameModeBlueprint"), WorldSettings ? GetBlueprintSourcePathFromClass(WorldSettings->DefaultGameMode) : FString());
		Writer->WriteObjectEnd();

		TArray<AActor*> Actors;
		if (ULevel* PersistentLevel = World->PersistentLevel)
		{
			for (AActor* Actor : PersistentLevel->Actors)
			{
				if (Actor && !Actor->IsA<AWorldSettings>())
				{
					Actors.Add(Actor);
				}
			}
		}
		Actors.Sort([](const AActor& A, const AActor& B)
		{
			return A.GetName() < B.GetName();
		});

		Writer->WriteValue(TEXT("persistentActorCount"), Actors.Num());
		Writer->WriteArrayStart(TEXT("actors"));
		for (const AActor* Actor : Actors)
		{
			const UClass* ActorClass = Actor ? Actor->GetClass() : nullptr;
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Actor ? Actor->GetName() : FString());
			Writer->WriteValue(TEXT("label"), Actor ? Actor->GetActorLabel(false) : FString());
			Writer->WriteValue(TEXT("class"), ActorClass ? ActorClass->GetPathName() : FString());
			Writer->WriteValue(TEXT("blueprint"), GetBlueprintSourcePathFromClass(ActorClass));
			Writer->WriteValue(TEXT("folder"), Actor ? Actor->GetFolderPath().ToString() : FString());
			WriteNameArrayJson(Writer, TEXT("tags"), Actor ? Actor->Tags : TArray<FName>());
			WriteTransformJson(Writer, TEXT("transform"), Actor ? Actor->GetActorTransform() : FTransform::Identity);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("streamingLevels"));
		for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if (!StreamingLevel)
			{
				continue;
			}
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("package"), StreamingLevel->GetWorldAssetPackageName());
			Writer->WriteValue(TEXT("loadedLevel"), GetObjectAssetPathSafe(StreamingLevel->GetLoadedLevel()));
			Writer->WriteValue(TEXT("shouldBeLoaded"), StreamingLevel->ShouldBeLoaded());
			Writer->WriteValue(TEXT("shouldBeVisible"), StreamingLevel->ShouldBeVisible());
			WriteTransformJson(Writer, TEXT("transform"), StreamingLevel->LevelTransform);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteObjectEnd();
		Writer->Close();
		return Json;
	}

	static FString ExportWorldSummaryToMarkdown(UWorld* World)
	{
		if (!World)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("# Level Summary Export\n\n");
		Out += TEXT("- Name: ") + World->GetName() + TEXT("\n");
		Out += TEXT("- AssetPath: ") + GetStandaloneExportObjectPath(World) + TEXT("\n");
		Out += TEXT("- MapName: ") + World->GetMapName() + TEXT("\n");

		AWorldSettings* WorldSettings = World->GetWorldSettings(false, false);
		if (WorldSettings)
		{
			Out += TEXT("- GameModeOverride: ") + GetObjectAssetPathSafe(WorldSettings->DefaultGameMode) + TEXT("\n");
			const FString BlueprintSource = GetBlueprintSourcePathFromClass(WorldSettings->DefaultGameMode);
			if (!BlueprintSource.IsEmpty())
			{
				Out += TEXT("- GameModeBlueprint: ") + BlueprintSource + TEXT("\n");
			}
		}

		TArray<AActor*> Actors;
		if (ULevel* PersistentLevel = World->PersistentLevel)
		{
			for (AActor* Actor : PersistentLevel->Actors)
			{
				if (Actor && !Actor->IsA<AWorldSettings>())
				{
					Actors.Add(Actor);
				}
			}
		}
		Actors.Sort([](const AActor& A, const AActor& B)
		{
			return A.GetName() < B.GetName();
		});

		Out += FString::Printf(TEXT("- PersistentActors: %d\n"), Actors.Num());
		Out += FString::Printf(TEXT("- StreamingLevels: %d\n\n"), World->GetStreamingLevels().Num());

		Out += TEXT("## Actors\n\n");
		for (const AActor* Actor : Actors)
		{
			if (!Actor)
			{
				continue;
			}
			const UClass* ActorClass = Actor->GetClass();
			Out += TEXT("- ") + Actor->GetActorLabel(false);
			Out += TEXT(" | Name: ") + Actor->GetName();
			Out += TEXT(" | Class: ") + (ActorClass ? ActorClass->GetName() : FString());
			const FString BlueprintSource = GetBlueprintSourcePathFromClass(ActorClass);
			if (!BlueprintSource.IsEmpty())
			{
				Out += TEXT(" | Blueprint: ") + BlueprintSource;
			}
			if (Actor->Tags.Num() > 0)
			{
				Out += TEXT(" | Tags: ") + JoinNames(Actor->Tags);
			}
			Out += TEXT(" | ") + TransformToShortText(Actor->GetActorTransform()) + TEXT("\n");
		}

		if (World->GetStreamingLevels().Num() > 0)
		{
			Out += TEXT("\n## Streaming Levels\n\n");
			for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
			{
				if (!StreamingLevel)
				{
					continue;
				}
				Out += TEXT("- ") + StreamingLevel->GetWorldAssetPackageName();
				Out += FString::Printf(TEXT(" | Loaded: %s | Visible: %s"), StreamingLevel->ShouldBeLoaded() ? TEXT("true") : TEXT("false"), StreamingLevel->ShouldBeVisible() ? TEXT("true") : TEXT("false"));
				Out += TEXT(" | ") + TransformToShortText(StreamingLevel->LevelTransform) + TEXT("\n");
			}
		}
		return Out;
	}

	static void WriteInputActionSummaryJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const UInputAction* Action)
	{
		Writer->WriteValue(TEXT("description"), Action ? Action->ActionDescription.ToString() : FString());
		Writer->WriteValue(TEXT("valueType"), Action ? GetEnumValueName(Action->ValueType) : FString());
		Writer->WriteValue(TEXT("triggerWhenPaused"), Action ? Action->bTriggerWhenPaused : false);
		Writer->WriteValue(TEXT("consumeInput"), Action ? Action->bConsumeInput : false);
		Writer->WriteValue(TEXT("consumesActionAndAxisMappings"), Action ? Action->bConsumesActionAndAxisMappings : false);
		Writer->WriteValue(TEXT("reserveAllMappings"), Action ? Action->bReserveAllMappings : false);
		Writer->WriteValue(TEXT("accumulationBehavior"), Action ? GetEnumValueName(Action->AccumulationBehavior) : FString());
		if (Action)
		{
			WriteSimpleObjectArrayJson(Writer, TEXT("triggers"), Action->Triggers);
			WriteSimpleObjectArrayJson(Writer, TEXT("modifiers"), Action->Modifiers);
			WriteObjectSummaryJsonField(Writer, TEXT("playerMappableKeySettings"), Action->GetPlayerMappableKeySettings().Get());
		}
	}

	static FString ExportInputActionToJson(UInputAction* Action)
	{
		if (!Action)
		{
			return FString();
		}

		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		WriteAssetHeaderJson(Writer, Action, TEXT("InputAction"));
		WriteInputActionSummaryJson(Writer, Action);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Json;
	}

	static FString ExportInputActionToMarkdown(UInputAction* Action)
	{
		if (!Action)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("# Enhanced Input Action Export\n\n");
		Out += TEXT("- Name: ") + Action->GetName() + TEXT("\n");
		Out += TEXT("- AssetPath: ") + GetStandaloneExportObjectPath(Action) + TEXT("\n");
		Out += TEXT("- Description: ") + Action->ActionDescription.ToString() + TEXT("\n");
		Out += TEXT("- ValueType: ") + GetEnumValueName(Action->ValueType) + TEXT("\n");
		Out += TEXT("- Accumulation: ") + GetEnumValueName(Action->AccumulationBehavior) + TEXT("\n");
		Out += FString::Printf(TEXT("- TriggerWhenPaused: %s\n"), Action->bTriggerWhenPaused ? TEXT("true") : TEXT("false"));
		Out += FString::Printf(TEXT("- ConsumeInput: %s\n"), Action->bConsumeInput ? TEXT("true") : TEXT("false"));
		AppendInputObjectListMarkdown(Out, TEXT("Triggers"), Action->Triggers);
		AppendInputObjectListMarkdown(Out, TEXT("Modifiers"), Action->Modifiers);
		return Out;
	}

	static void WriteEnhancedMappingJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const FEnhancedActionKeyMapping& Mapping)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("action"), GetObjectAssetPathSafe(Mapping.Action.Get()));
		Writer->WriteValue(TEXT("actionName"), Mapping.Action ? Mapping.Action->GetName() : FString());
		Writer->WriteValue(TEXT("key"), Mapping.Key.GetFName().ToString());
		Writer->WriteValue(TEXT("keyDisplayName"), Mapping.Key.GetDisplayName().ToString());
		Writer->WriteValue(TEXT("mappingName"), Mapping.GetMappingName().ToString());
		Writer->WriteValue(TEXT("displayName"), Mapping.GetDisplayName().ToString());
		Writer->WriteValue(TEXT("displayCategory"), Mapping.GetDisplayCategory().ToString());
		Writer->WriteValue(TEXT("playerMappable"), Mapping.IsPlayerMappable());
		if (const UInputAction* Action = Mapping.Action.Get())
		{
			Writer->WriteObjectStart(TEXT("actionSummary"));
			WriteInputActionSummaryJson(Writer, Action);
			Writer->WriteObjectEnd();
		}
		WriteSimpleObjectArrayJson(Writer, TEXT("triggers"), Mapping.Triggers);
		WriteSimpleObjectArrayJson(Writer, TEXT("modifiers"), Mapping.Modifiers);
		Writer->WriteObjectEnd();
	}

	static FString ExportInputMappingContextToJson(UInputMappingContext* MappingContext)
	{
		if (!MappingContext)
		{
			return FString();
		}

		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		WriteAssetHeaderJson(Writer, MappingContext, TEXT("InputMappingContext"));
		Writer->WriteValue(TEXT("description"), MappingContext->ContextDescription.ToString());
		Writer->WriteValue(TEXT("registrationTrackingMode"), GetEnumValueName(MappingContext->GetRegistrationTrackingMode()));
		Writer->WriteValue(TEXT("shouldFilterByInputMode"), MappingContext->ShouldFilterMappingByInputMode());
		Writer->WriteValue(TEXT("mappingCount"), MappingContext->GetMappings().Num());
		Writer->WriteArrayStart(TEXT("mappings"));
		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			WriteEnhancedMappingJson(Writer, Mapping);
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
		return Json;
	}

	static FString ExportInputMappingContextToMarkdown(UInputMappingContext* MappingContext)
	{
		if (!MappingContext)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("# Enhanced Input Mapping Context Export\n\n");
		Out += TEXT("- Name: ") + MappingContext->GetName() + TEXT("\n");
		Out += TEXT("- AssetPath: ") + GetStandaloneExportObjectPath(MappingContext) + TEXT("\n");
		Out += TEXT("- Description: ") + MappingContext->ContextDescription.ToString() + TEXT("\n");
		Out += TEXT("- RegistrationTracking: ") + GetEnumValueName(MappingContext->GetRegistrationTrackingMode()) + TEXT("\n");
		Out += FString::Printf(TEXT("- MappingCount: %d\n\n"), MappingContext->GetMappings().Num());

		Out += TEXT("## Mappings\n\n");
		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			Out += TEXT("- ") + (Mapping.Action ? Mapping.Action->GetName() : TEXT("<None>"));
			Out += TEXT(" <- ") + Mapping.Key.GetDisplayName().ToString();
			if (Mapping.IsPlayerMappable())
			{
				Out += TEXT(" | PlayerMappable: ") + Mapping.GetMappingName().ToString();
			}
			if (Mapping.Triggers.Num() > 0)
			{
				TArray<FString> TriggerNames;
				for (const TObjectPtr<UInputTrigger>& Trigger : Mapping.Triggers)
				{
					if (Trigger)
					{
						TriggerNames.Add(Trigger->GetClass()->GetName());
					}
				}
				Out += TEXT(" | Triggers: ") + FString::Join(TriggerNames, TEXT(", "));
			}
			if (Mapping.Modifiers.Num() > 0)
			{
				TArray<FString> ModifierNames;
				for (const TObjectPtr<UInputModifier>& Modifier : Mapping.Modifiers)
				{
					if (Modifier)
					{
						ModifierNames.Add(Modifier->GetClass()->GetName());
					}
				}
				Out += TEXT(" | Modifiers: ") + FString::Join(ModifierNames, TEXT(", "));
			}
			Out += TEXT("\n");
		}
		return Out;
	}

	static bool IsSensitiveConfigKey(const FString& Key)
	{
		FString Lower = Key.ToLower();
		return Lower.Contains(TEXT("password"))
			|| Lower.Contains(TEXT("secret"))
			|| Lower.Contains(TEXT("token"))
			|| Lower.Contains(TEXT("apikey"))
			|| Lower.Contains(TEXT("api_key"))
			|| Lower.Contains(TEXT("credential"));
	}

	static FString SanitizeConfigValue(const FString& Key, const FString& Value)
	{
		return IsSensitiveConfigKey(Key) ? TEXT("<redacted>") : Value;
	}

	static bool IsConfigSectionInteresting(const FString& Section)
	{
		return Section.Contains(TEXT("MapsSettings"))
			|| Section.Contains(TEXT("GameMapsSettings"))
			|| Section.Contains(TEXT("AssetManagerSettings"))
			|| Section.Contains(TEXT("EnhancedInput"))
			|| Section.Contains(TEXT("InputSettings"))
			|| Section.Contains(TEXT("Engine.Player"))
			|| Section.Contains(TEXT("NetDriver"))
			|| Section.Contains(TEXT("OnlineSubsystem"))
			|| Section.Contains(TEXT("PacketHandler"))
			|| Section.Contains(TEXT("URL"))
			|| Section.Contains(TEXT("Redirects"))
			|| Section.Contains(TEXT("Plugins"));
	}

	static void WriteConfigSectionJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const FConfigSection& Section)
	{
		TArray<FName> Keys;
		Section.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());

		for (const FName& KeyName : Keys)
		{
			TArray<FConfigValue> Values;
			Section.MultiFind(KeyName, Values, true);
			const FString Key = KeyName.ToString();
			if (Values.Num() == 1)
			{
				Writer->WriteValue(Key, SanitizeConfigValue(Key, Values[0].GetSavedValue()));
				continue;
			}

			Writer->WriteArrayStart(Key);
			for (const FConfigValue& Value : Values)
			{
				Writer->WriteValue(SanitizeConfigValue(Key, Value.GetSavedValue()));
			}
			Writer->WriteArrayEnd();
		}
	}

	static void AppendConfigSectionMarkdown(FString& Out, const FString& SectionName, const FConfigSection& Section)
	{
		Out += TEXT("### [") + SectionName + TEXT("]\n\n");
		TArray<FName> Keys;
		Section.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());

		for (const FName& KeyName : Keys)
		{
			TArray<FConfigValue> Values;
			Section.MultiFind(KeyName, Values, true);
			const FString Key = KeyName.ToString();
			for (const FConfigValue& Value : Values)
			{
				Out += TEXT("- ") + Key + TEXT(" = ") + SanitizeConfigValue(Key, Value.GetSavedValue()) + TEXT("\n");
			}
		}
		Out += TEXT("\n");
	}

	static bool LoadConfigFileFromProject(const FString& FileName, FConfigFile& OutConfigFile, FString& OutFullPath)
	{
		OutFullPath = FPaths::ProjectConfigDir() / FileName;
		if (!FPaths::FileExists(OutFullPath))
		{
			return false;
		}
		OutConfigFile.Read(OutFullPath);
		return true;
	}

	static void WriteProjectDescriptorJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer)
	{
		const FString ProjectFilePath = FPaths::GetProjectFilePath();
		Writer->WriteObjectStart(TEXT("uproject"));
		Writer->WriteValue(TEXT("path"), ProjectFilePath);

		FString RawJson;
		TSharedPtr<FJsonObject> ProjectJson;
		if (FPaths::FileExists(ProjectFilePath)
			&& FFileHelper::LoadFileToString(RawJson, *ProjectFilePath)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(RawJson), ProjectJson)
			&& ProjectJson.IsValid())
		{
			FString EngineAssociation;
			ProjectJson->TryGetStringField(TEXT("EngineAssociation"), EngineAssociation);
			Writer->WriteValue(TEXT("engineAssociation"), EngineAssociation);
			Writer->WriteArrayStart(TEXT("plugins"));
			const TArray<TSharedPtr<FJsonValue>>* Plugins = nullptr;
			if (ProjectJson->TryGetArrayField(TEXT("Plugins"), Plugins) && Plugins)
			{
				for (const TSharedPtr<FJsonValue>& PluginValue : *Plugins)
				{
					const TSharedPtr<FJsonObject> PluginObject = PluginValue.IsValid() ? PluginValue->AsObject() : nullptr;
					if (PluginObject.IsValid())
					{
						Writer->WriteObjectStart();
						FString Name;
						bool bEnabled = false;
						PluginObject->TryGetStringField(TEXT("Name"), Name);
						PluginObject->TryGetBoolField(TEXT("Enabled"), bEnabled);
						Writer->WriteValue(TEXT("name"), Name);
						Writer->WriteValue(TEXT("enabled"), bEnabled);
						Writer->WriteObjectEnd();
					}
				}
			}
			Writer->WriteArrayEnd();
		}

		Writer->WriteObjectEnd();
	}

	static void AppendProjectDescriptorMarkdown(FString& Out)
	{
		const FString ProjectFilePath = FPaths::GetProjectFilePath();
		Out += TEXT("## Project Descriptor\n\n");
		Out += TEXT("- Path: ") + ProjectFilePath + TEXT("\n");

		FString RawJson;
		TSharedPtr<FJsonObject> ProjectJson;
		if (FPaths::FileExists(ProjectFilePath)
			&& FFileHelper::LoadFileToString(RawJson, *ProjectFilePath)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(RawJson), ProjectJson)
			&& ProjectJson.IsValid())
		{
			FString EngineAssociation;
			if (ProjectJson->TryGetStringField(TEXT("EngineAssociation"), EngineAssociation))
			{
				Out += TEXT("- EngineAssociation: ") + EngineAssociation + TEXT("\n");
			}

			const TArray<TSharedPtr<FJsonValue>>* Plugins = nullptr;
			if (ProjectJson->TryGetArrayField(TEXT("Plugins"), Plugins) && Plugins && Plugins->Num() > 0)
			{
				Out += TEXT("\nPlugins:\n");
				for (const TSharedPtr<FJsonValue>& PluginValue : *Plugins)
				{
					const TSharedPtr<FJsonObject> PluginObject = PluginValue.IsValid() ? PluginValue->AsObject() : nullptr;
					if (PluginObject.IsValid())
					{
						FString Name;
						bool bEnabled = false;
						PluginObject->TryGetStringField(TEXT("Name"), Name);
						PluginObject->TryGetBoolField(TEXT("Enabled"), bEnabled);
						Out += FString::Printf(TEXT("- %s: %s\n"), *Name, bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
					}
				}
			}
		}
		Out += TEXT("\n");
	}

	static FString ExportProjectConfigToJson()
	{
		static const TArray<FString> ConfigFiles = {
			TEXT("DefaultGame.ini"),
			TEXT("DefaultEngine.ini"),
			TEXT("DefaultInput.ini")
		};

		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		Writer->WriteValue(TEXT("generatedBy"), TEXT("GetTheMeaning"));
		Writer->WriteValue(TEXT("projectName"), FApp::GetProjectName());
		Writer->WriteValue(TEXT("projectDir"), FPaths::ProjectDir());
		WriteProjectDescriptorJson(Writer);

		Writer->WriteArrayStart(TEXT("configFiles"));
		for (const FString& FileName : ConfigFiles)
		{
			FConfigFile ConfigFile;
			FString FullPath;
			if (!LoadConfigFileFromProject(FileName, ConfigFile, FullPath))
			{
				continue;
			}

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("file"), FileName);
			Writer->WriteValue(TEXT("path"), FullPath);
			Writer->WriteArrayStart(TEXT("sections"));

			TArray<FString> SectionNames;
			ConfigFile.GetKeys(SectionNames);
			SectionNames.Sort();
			for (const FString& SectionName : SectionNames)
			{
				if (!IsConfigSectionInteresting(SectionName))
				{
					continue;
				}

				if (const FConfigSection* Section = ConfigFile.FindSection(SectionName))
				{
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("name"), SectionName);
					Writer->WriteObjectStart(TEXT("values"));
					WriteConfigSectionJson(Writer, *Section);
					Writer->WriteObjectEnd();
					Writer->WriteObjectEnd();
				}
			}

			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
		return Json;
	}

	static FString ExportProjectConfigToMarkdown()
	{
		static const TArray<FString> ConfigFiles = {
			TEXT("DefaultGame.ini"),
			TEXT("DefaultEngine.ini"),
			TEXT("DefaultInput.ini")
		};

		FString Out;
		Out += TEXT("# Project Config Export\n\n");
		Out += TEXT("- ProjectName: ") + FString(FApp::GetProjectName()) + TEXT("\n");
		Out += TEXT("- ProjectDir: ") + FPaths::ProjectDir() + TEXT("\n\n");
		AppendProjectDescriptorMarkdown(Out);

		for (const FString& FileName : ConfigFiles)
		{
			FConfigFile ConfigFile;
			FString FullPath;
			if (!LoadConfigFileFromProject(FileName, ConfigFile, FullPath))
			{
				continue;
			}

			Out += TEXT("## ") + FileName + TEXT("\n\n");
			Out += TEXT("- Path: ") + FullPath + TEXT("\n\n");

			TArray<FString> SectionNames;
			ConfigFile.GetKeys(SectionNames);
			SectionNames.Sort();
			for (const FString& SectionName : SectionNames)
			{
				if (!IsConfigSectionInteresting(SectionName))
				{
					continue;
				}

				if (const FConfigSection* Section = ConfigFile.FindSection(SectionName))
				{
					AppendConfigSectionMarkdown(Out, SectionName, *Section);
				}
			}
		}
		return Out;
	}

	static void WriteProjectConfigFiles()
	{
		const FString Json = ExportProjectConfigToJson();
		const FString Markdown = ExportProjectConfigToMarkdown();
		SaveText(Json, GetExportRootDir(), TEXT("ProjectConfig"), TEXT(".json"));
		SaveText(Markdown, GetExportRootDir(), TEXT("ProjectConfig"), TEXT(".md"));
	}

	struct FCppSourcePropertyInfo
	{
		FString Name;
		FString Type;
		FString Declaration;
		FString Specifiers;
		FString File;
		int32 Line = 0;
		bool bBlueprintVisible = false;
		bool bBlueprintAssignable = false;
		bool bEditable = false;
		bool bReplicated = false;
		bool bSaveGame = false;
		FString RepNotify;
	};

	struct FCppSourceFunctionInfo
	{
		FString Name;
		FString ReturnType;
		FString Signature;
		FString Specifiers;
		FString File;
		int32 Line = 0;
		bool bBlueprintCallable = false;
		bool bBlueprintPure = false;
		bool bBlueprintEvent = false;
		bool bBlueprintNativeEvent = false;
		bool bBlueprintImplementableEvent = false;
		bool bServer = false;
		bool bClient = false;
		bool bNetMulticast = false;
		bool bReliable = false;
		bool bUnreliable = false;
	};

	struct FCppSourceTypeInfo
	{
		FString Kind;
		FString Name;
		FString Declaration;
		FString Specifiers;
		FString File;
		int32 Line = 0;
		TArray<FString> BaseClasses;
		TArray<FString> EnumValues;
		TArray<FCppSourcePropertyInfo> Properties;
		TArray<FCppSourceFunctionInfo> Functions;
		bool bHasGetLifetimeReplicatedProps = false;
		TArray<FString> ReplicationLines;
	};

	struct FCppSourceDefinitionInfo
	{
		FString Owner;
		FString Name;
		FString Signature;
		FString File;
		int32 Line = 0;
	};

	struct FCppSourceFileInfo
	{
		FString Path;
		FString RelativePath;
		int32 LineCount = 0;
		bool bIsProjectSource = false;
		bool bIsProjectPluginSource = false;
	};

	struct FCppSourceIndexInfo
	{
		TArray<FString> SourceRoots;
		TArray<FCppSourceFileInfo> Files;
		TArray<FCppSourceTypeInfo> Types;
		TArray<FCppSourceDefinitionInfo> Definitions;
		TArray<FString> Warnings;
	};

	static FString NormalizeCppWhitespace(FString Text)
	{
		Text.ReplaceInline(TEXT("\r"), TEXT(" "));
		Text.ReplaceInline(TEXT("\n"), TEXT(" "));
		Text.ReplaceInline(TEXT("\t"), TEXT(" "));
		while (Text.Contains(TEXT("  ")))
		{
			Text.ReplaceInline(TEXT("  "), TEXT(" "));
		}
		return Text.TrimStartAndEnd();
	}

	static FString StripCppLineComment(const FString& Line)
	{
		bool bInString = false;
		TCHAR QuoteChar = 0;
		for (int32 Index = 0; Index < Line.Len() - 1; ++Index)
		{
			const TCHAR Char = Line[Index];
			const TCHAR Next = Line[Index + 1];
			if ((Char == TEXT('"') || Char == TEXT('\'')) && (Index == 0 || Line[Index - 1] != TEXT('\\')))
			{
				if (!bInString)
				{
					bInString = true;
					QuoteChar = Char;
				}
				else if (QuoteChar == Char)
				{
					bInString = false;
					QuoteChar = 0;
				}
			}

			if (!bInString && Char == TEXT('/') && Next == TEXT('/'))
			{
				return Line.Left(Index);
			}
		}
		return Line;
	}

	static TArray<FString> SplitCppLinesWithoutBlockComments(const FString& Text)
	{
		TArray<FString> RawLines;
		Text.ParseIntoArrayLines(RawLines, false);

		TArray<FString> Lines;
		bool bInBlockComment = false;
		for (const FString& RawLine : RawLines)
		{
			FString OutLine;
			for (int32 Index = 0; Index < RawLine.Len();)
			{
				if (bInBlockComment)
				{
					const int32 EndIndex = RawLine.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
					if (EndIndex == INDEX_NONE)
					{
						Index = RawLine.Len();
					}
					else
					{
						bInBlockComment = false;
						Index = EndIndex + 2;
					}
					continue;
				}

				const int32 BlockStart = RawLine.Find(TEXT("/*"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
				if (BlockStart == INDEX_NONE)
				{
					OutLine += RawLine.Mid(Index);
					break;
				}

				OutLine += RawLine.Mid(Index, BlockStart - Index);
				const int32 BlockEnd = RawLine.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BlockStart + 2);
				if (BlockEnd == INDEX_NONE)
				{
					bInBlockComment = true;
					break;
				}
				Index = BlockEnd + 2;
			}
			Lines.Add(StripCppLineComment(OutLine));
		}

		return Lines;
	}

	static int32 CountCharOutsideQuotes(const FString& Text, TCHAR Target)
	{
		int32 Count = 0;
		bool bInString = false;
		TCHAR QuoteChar = 0;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			if ((Char == TEXT('"') || Char == TEXT('\'')) && (Index == 0 || Text[Index - 1] != TEXT('\\')))
			{
				if (!bInString)
				{
					bInString = true;
					QuoteChar = Char;
				}
				else if (QuoteChar == Char)
				{
					bInString = false;
					QuoteChar = 0;
				}
			}
			else if (!bInString && Char == Target)
			{
				++Count;
			}
		}
		return Count;
	}

	static FString ExtractMacroSpecifiers(const FString& MacroText)
	{
		const int32 OpenIndex = MacroText.Find(TEXT("("));
		const int32 CloseIndex = MacroText.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (OpenIndex != INDEX_NONE && CloseIndex != INDEX_NONE && CloseIndex > OpenIndex)
		{
			return NormalizeCppWhitespace(MacroText.Mid(OpenIndex + 1, CloseIndex - OpenIndex - 1));
		}
		return FString();
	}

	static bool ContainsCppSpecifier(const FString& Specifiers, const FString& Token)
	{
		return Specifiers.Contains(Token, ESearchCase::IgnoreCase);
	}

	static FString ExtractSpecifierValue(const FString& Specifiers, const FString& Prefix)
	{
		const int32 PrefixIndex = Specifiers.Find(Prefix, ESearchCase::IgnoreCase);
		if (PrefixIndex == INDEX_NONE)
		{
			return FString();
		}

		int32 ValueStart = PrefixIndex + Prefix.Len();
		while (ValueStart < Specifiers.Len() && FChar::IsWhitespace(Specifiers[ValueStart]))
		{
			++ValueStart;
		}
		if (ValueStart < Specifiers.Len() && Specifiers[ValueStart] == TEXT('='))
		{
			++ValueStart;
		}
		while (ValueStart < Specifiers.Len() && FChar::IsWhitespace(Specifiers[ValueStart]))
		{
			++ValueStart;
		}

		int32 ValueEnd = ValueStart;
		int32 NestedDepth = 0;
		while (ValueEnd < Specifiers.Len())
		{
			const TCHAR Char = Specifiers[ValueEnd];
			if (Char == TEXT('('))
			{
				++NestedDepth;
			}
			else if (Char == TEXT(')'))
			{
				NestedDepth = FMath::Max(0, NestedDepth - 1);
			}
			else if (NestedDepth == 0 && (Char == TEXT(',') || FChar::IsWhitespace(Char)))
			{
				break;
			}
			++ValueEnd;
		}

		return Specifiers.Mid(ValueStart, ValueEnd - ValueStart).TrimQuotes();
	}

	static bool IsLikelyCppSourceFile(const FString& Path)
	{
		const FString Extension = FPaths::GetExtension(Path, true).ToLower();
		return Extension == TEXT(".h") || Extension == TEXT(".hpp") || Extension == TEXT(".cpp");
	}

	static FString GetPathRelativeToProject(const FString& FullPath)
	{
		FString Relative = FullPath;
		if (FPaths::MakePathRelativeTo(Relative, *FPaths::ProjectDir()))
		{
			return Relative.Replace(TEXT("\\"), TEXT("/"));
		}
		return FullPath.Replace(TEXT("\\"), TEXT("/"));
	}

	static void AddCppSourceRootIfExists(TArray<FString>& Roots, const FString& Root)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString NormalizedRoot = FPaths::ConvertRelativePathToFull(Root);
		FPaths::NormalizeDirectoryName(NormalizedRoot);
		if (PlatformFile.DirectoryExists(*NormalizedRoot))
		{
			Roots.AddUnique(NormalizedRoot);
		}
	}

	static TArray<FString> GetCppSourceRoots()
	{
		TArray<FString> Roots;
		AddCppSourceRootIfExists(Roots, FPaths::ProjectDir() / TEXT("Source"));

		const FString PluginsDir = FPaths::ProjectPluginsDir();
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.DirectoryExists(*PluginsDir))
		{
			PlatformFile.IterateDirectory(*PluginsDir, [&Roots](const TCHAR* Path, bool bIsDirectory)
			{
				if (bIsDirectory)
				{
					const FString PluginName = FPaths::GetCleanFilename(FString(Path));
					if (!PluginName.Equals(TEXT("GetTheMeaning"), ESearchCase::IgnoreCase))
					{
						AddCppSourceRootIfExists(Roots, FString(Path) / TEXT("Source"));
					}
				}
				return true;
			});
		}
		return Roots;
	}

	static TArray<FString> GetCppSourceFiles(const TArray<FString>& Roots)
	{
		TMap<FString, FString> UniqueFiles;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		for (const FString& Root : Roots)
		{
			if (!PlatformFile.DirectoryExists(*Root))
			{
				continue;
			}

			PlatformFile.IterateDirectoryRecursively(*Root, [&UniqueFiles](const TCHAR* Path, bool bIsDirectory)
			{
				if (!bIsDirectory)
				{
					FString FilePath(Path);
					if (IsLikelyCppSourceFile(FilePath))
					{
						FPaths::NormalizeFilename(FilePath);
						UniqueFiles.FindOrAdd(FilePath, FilePath);
					}
				}
				return true;
			});
		}

		TArray<FString> Files;
		UniqueFiles.GenerateValueArray(Files);
		Files.Sort();
		return Files;
	}

	static FString CollectCppMacroFromLines(const TArray<FString>& Lines, int32& Index)
	{
		FString MacroText = Lines.IsValidIndex(Index) ? Lines[Index].TrimStartAndEnd() : FString();
		int32 ParenDepth = CountCharOutsideQuotes(MacroText, TEXT('(')) - CountCharOutsideQuotes(MacroText, TEXT(')'));
		while (ParenDepth > 0 && Index + 1 < Lines.Num())
		{
			++Index;
			const FString NextLine = Lines[Index].TrimStartAndEnd();
			MacroText += TEXT(" ") + NextLine;
			ParenDepth += CountCharOutsideQuotes(NextLine, TEXT('(')) - CountCharOutsideQuotes(NextLine, TEXT(')'));
		}
		return NormalizeCppWhitespace(MacroText);
	}

	static bool CollectCppDeclarationFromLines(const TArray<FString>& Lines, int32 StartIndex, FString& OutDeclaration, int32& OutDeclarationLine, bool bStopAtBrace)
	{
		OutDeclaration.Empty();
		OutDeclarationLine = 0;

		for (int32 Index = StartIndex + 1; Index < Lines.Num(); ++Index)
		{
			FString Line = Lines[Index].TrimStartAndEnd();
			if (Line.IsEmpty())
			{
				continue;
			}
			if (Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			if (Line.StartsWith(TEXT("GENERATED_BODY")) || Line.StartsWith(TEXT("GENERATED_UCLASS_BODY")))
			{
				continue;
			}

			if (OutDeclarationLine == 0)
			{
				OutDeclarationLine = Index + 1;
			}

			OutDeclaration += OutDeclaration.IsEmpty() ? Line : TEXT(" ") + Line;
			OutDeclaration = NormalizeCppWhitespace(OutDeclaration);

			if (OutDeclaration.Contains(TEXT(";")) || (bStopAtBrace && OutDeclaration.Contains(TEXT("{"))))
			{
				return true;
			}
		}

		return !OutDeclaration.IsEmpty();
	}

	static void ParseCppBaseClasses(const FString& Declaration, TArray<FString>& OutBaseClasses)
	{
		OutBaseClasses.Empty();
		const int32 ColonIndex = Declaration.Find(TEXT(":"));
		if (ColonIndex == INDEX_NONE)
		{
			return;
		}

		int32 EndIndex = Declaration.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ColonIndex);
		if (EndIndex == INDEX_NONE)
		{
			EndIndex = Declaration.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ColonIndex);
		}
		if (EndIndex == INDEX_NONE)
		{
			EndIndex = Declaration.Len();
		}

		FString BaseText = Declaration.Mid(ColonIndex + 1, EndIndex - ColonIndex - 1);
		TArray<FString> Parts;
		BaseText.ParseIntoArray(Parts, TEXT(","), true);
		for (FString Part : Parts)
		{
			Part = NormalizeCppWhitespace(Part);
			Part.ReplaceInline(TEXT("public "), TEXT(""));
			Part.ReplaceInline(TEXT("protected "), TEXT(""));
			Part.ReplaceInline(TEXT("private "), TEXT(""));
			Part = Part.TrimStartAndEnd();
			if (!Part.IsEmpty())
			{
				OutBaseClasses.AddUnique(Part);
			}
		}
		OutBaseClasses.Sort();
	}

	static FString ExtractCppTypeNameFromDeclaration(const FString& Kind, const FString& Declaration)
	{
		FString Work = Declaration;
		Work.ReplaceInline(TEXT("{"), TEXT(" { "));
		Work.ReplaceInline(TEXT(":"), TEXT(" : "));
		Work.ReplaceInline(TEXT(";"), TEXT(" ; "));
		TArray<FString> Tokens;
		Work.ParseIntoArrayWS(Tokens);

		for (int32 Index = 0; Index < Tokens.Num(); ++Index)
		{
			if (Kind == TEXT("Enum") && Tokens[Index] == TEXT("enum"))
			{
				if (Tokens.IsValidIndex(Index + 1) && Tokens[Index + 1] == TEXT("class") && Tokens.IsValidIndex(Index + 2))
				{
					return Tokens[Index + 2];
				}
				if (Tokens.IsValidIndex(Index + 1))
				{
					return Tokens[Index + 1];
				}
			}

			if ((Kind == TEXT("Class") && Tokens[Index] == TEXT("class"))
				|| (Kind == TEXT("Struct") && Tokens[Index] == TEXT("struct")))
			{
				for (int32 CandidateIndex = Index + 1; CandidateIndex < Tokens.Num(); ++CandidateIndex)
				{
					const FString& Token = Tokens[CandidateIndex];
					if (Token == TEXT(":") || Token == TEXT("{") || Token == TEXT(";"))
					{
						break;
					}
					if (Token.EndsWith(TEXT("_API")))
					{
						continue;
					}
					return Token;
				}
			}
		}

		return FString();
	}

	static FString CleanCppIdentifier(FString Identifier)
	{
		Identifier = Identifier.TrimStartAndEnd();
		Identifier.RemoveFromEnd(TEXT(";"));
		Identifier.RemoveFromEnd(TEXT(","));
		Identifier.RemoveFromEnd(TEXT("="));
		Identifier.RemoveFromEnd(TEXT("{"));
		Identifier.RemoveFromEnd(TEXT("}"));
		return Identifier.TrimStartAndEnd();
	}

	static void ParseCppPropertyDeclaration(const FString& Declaration, FCppSourcePropertyInfo& OutProperty)
	{
		OutProperty.Declaration = Declaration;
		FString Work = Declaration;
		const int32 SemicolonIndex = Work.Find(TEXT(";"));
		if (SemicolonIndex != INDEX_NONE)
		{
			Work = Work.Left(SemicolonIndex);
		}
		const int32 EqualsIndex = Work.Find(TEXT("="));
		if (EqualsIndex != INDEX_NONE)
		{
			Work = Work.Left(EqualsIndex);
		}
		Work = NormalizeCppWhitespace(Work);

		TArray<FString> Tokens;
		Work.ParseIntoArrayWS(Tokens);
		if (Tokens.Num() == 0)
		{
			return;
		}

		FString Name = Tokens.Last();
		const int32 BracketIndex = Name.Find(TEXT("["));
		if (BracketIndex != INDEX_NONE)
		{
			Name = Name.Left(BracketIndex);
		}
		OutProperty.Name = CleanCppIdentifier(Name);

		Tokens.Pop(EAllowShrinking::No);
		OutProperty.Type = NormalizeCppWhitespace(FString::Join(Tokens, TEXT(" ")));
	}

	static void ParseCppFunctionDeclaration(const FString& Declaration, FCppSourceFunctionInfo& OutFunction)
	{
		OutFunction.Signature = Declaration;
		FString Work = Declaration;
		const int32 SemicolonIndex = Work.Find(TEXT(";"));
		const int32 BraceIndex = Work.Find(TEXT("{"));
		int32 EndIndex = INDEX_NONE;
		if (SemicolonIndex != INDEX_NONE && BraceIndex != INDEX_NONE)
		{
			EndIndex = FMath::Min(SemicolonIndex, BraceIndex);
		}
		else
		{
			EndIndex = SemicolonIndex != INDEX_NONE ? SemicolonIndex : BraceIndex;
		}
		if (EndIndex != INDEX_NONE)
		{
			Work = Work.Left(EndIndex);
		}
		Work = NormalizeCppWhitespace(Work);

		const int32 ParenIndex = Work.Find(TEXT("("));
		if (ParenIndex == INDEX_NONE)
		{
			return;
		}

		FString BeforeParen = NormalizeCppWhitespace(Work.Left(ParenIndex));
		TArray<FString> Tokens;
		BeforeParen.ParseIntoArrayWS(Tokens);
		if (Tokens.Num() == 0)
		{
			return;
		}

		OutFunction.Name = CleanCppIdentifier(Tokens.Last());
		if (OutFunction.Name.Contains(TEXT("::")))
		{
			FString Left;
			FString Right;
			if (OutFunction.Name.Split(TEXT("::"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				OutFunction.Name = Right;
			}
		}

		Tokens.Pop(EAllowShrinking::No);
		OutFunction.ReturnType = NormalizeCppWhitespace(FString::Join(Tokens, TEXT(" ")));
	}

	static void ApplyCppPropertyFlags(FCppSourcePropertyInfo& Property)
	{
		Property.bBlueprintVisible = ContainsCppSpecifier(Property.Specifiers, TEXT("BlueprintReadWrite"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("BlueprintReadOnly"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("BlueprintAssignable"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("BlueprintAuthorityOnly"));
		Property.bBlueprintAssignable = ContainsCppSpecifier(Property.Specifiers, TEXT("BlueprintAssignable"));
		Property.bEditable = ContainsCppSpecifier(Property.Specifiers, TEXT("EditAnywhere"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("EditDefaultsOnly"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("EditInstanceOnly"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("VisibleAnywhere"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("VisibleDefaultsOnly"))
			|| ContainsCppSpecifier(Property.Specifiers, TEXT("VisibleInstanceOnly"));
		Property.bReplicated = ContainsCppSpecifier(Property.Specifiers, TEXT("Replicated"));
		Property.bSaveGame = ContainsCppSpecifier(Property.Specifiers, TEXT("SaveGame"));
		Property.RepNotify = ExtractSpecifierValue(Property.Specifiers, TEXT("ReplicatedUsing"));
	}

	static void ApplyCppFunctionFlags(FCppSourceFunctionInfo& Function)
	{
		Function.bBlueprintCallable = ContainsCppSpecifier(Function.Specifiers, TEXT("BlueprintCallable"));
		Function.bBlueprintPure = ContainsCppSpecifier(Function.Specifiers, TEXT("BlueprintPure"));
		Function.bBlueprintNativeEvent = ContainsCppSpecifier(Function.Specifiers, TEXT("BlueprintNativeEvent"));
		Function.bBlueprintImplementableEvent = ContainsCppSpecifier(Function.Specifiers, TEXT("BlueprintImplementableEvent"));
		Function.bBlueprintEvent = Function.bBlueprintNativeEvent || Function.bBlueprintImplementableEvent;
		Function.bServer = ContainsCppSpecifier(Function.Specifiers, TEXT("Server"));
		Function.bClient = ContainsCppSpecifier(Function.Specifiers, TEXT("Client"));
		Function.bNetMulticast = ContainsCppSpecifier(Function.Specifiers, TEXT("NetMulticast"));
		Function.bReliable = ContainsCppSpecifier(Function.Specifiers, TEXT("Reliable"));
		Function.bUnreliable = ContainsCppSpecifier(Function.Specifiers, TEXT("Unreliable"));
	}

	static void ParseCppEnumValues(const TArray<FString>& Lines, int32 DeclarationLineIndex, FCppSourceTypeInfo& TypeInfo)
	{
		int32 BraceDepth = 0;
		bool bSawOpenBrace = false;
		for (int32 Index = DeclarationLineIndex; Index < Lines.Num(); ++Index)
		{
			const FString OriginalLine = Lines[Index].TrimStartAndEnd();
			if (OriginalLine.IsEmpty())
			{
				continue;
			}

			FString ValueText = OriginalLine;
			if (!bSawOpenBrace)
			{
				const int32 OpenIndex = OriginalLine.Find(TEXT("{"));
				if (OpenIndex == INDEX_NONE)
				{
					continue;
				}
				bSawOpenBrace = true;
				ValueText = OriginalLine.Mid(OpenIndex + 1);
			}

			if (bSawOpenBrace)
			{
				if (ValueText.Contains(TEXT("}")))
				{
					ValueText = ValueText.Left(ValueText.Find(TEXT("}")));
				}

				TArray<FString> Parts;
				ValueText.ParseIntoArray(Parts, TEXT(","), true);
				for (FString Part : Parts)
				{
					Part = NormalizeCppWhitespace(Part);
					if (Part.IsEmpty())
					{
						continue;
					}
					const int32 EqualsIndex = Part.Find(TEXT("="));
					if (EqualsIndex != INDEX_NONE)
					{
						Part = Part.Left(EqualsIndex).TrimStartAndEnd();
					}
					const int32 MetaIndex = Part.Find(TEXT("UMETA"));
					if (MetaIndex != INDEX_NONE)
					{
						Part = Part.Left(MetaIndex).TrimStartAndEnd();
					}
					Part = CleanCppIdentifier(Part);
					if (!Part.IsEmpty())
					{
						TypeInfo.EnumValues.AddUnique(Part);
					}
				}

				BraceDepth += CountCharOutsideQuotes(OriginalLine, TEXT('{')) - CountCharOutsideQuotes(OriginalLine, TEXT('}'));
				if (BraceDepth <= 0)
				{
					break;
				}
			}
		}
	}

	static int32 FindTypeScopeEndLine(const TArray<FString>& Lines, int32 DeclarationLineIndex)
	{
		int32 BraceDepth = 0;
		bool bSawOpenBrace = false;
		for (int32 Index = DeclarationLineIndex; Index < Lines.Num(); ++Index)
		{
			const FString& Line = Lines[Index];
			if (Line.Contains(TEXT("{")))
			{
				bSawOpenBrace = true;
			}
			if (bSawOpenBrace)
			{
				BraceDepth += CountCharOutsideQuotes(Line, TEXT('{')) - CountCharOutsideQuotes(Line, TEXT('}'));
				if (BraceDepth <= 0 && Index > DeclarationLineIndex)
				{
					return Index + 1;
				}
			}
		}
		return Lines.Num();
	}

	static FCppSourceTypeInfo* FindCppTypeByName(TArray<FCppSourceTypeInfo>& Types, const FString& TypeName)
	{
		if (TypeName.IsEmpty())
		{
			return nullptr;
		}

		for (FCppSourceTypeInfo& Type : Types)
		{
			if (Type.Name == TypeName)
			{
				return &Type;
			}
		}
		return nullptr;
	}

	static FCppSourceTypeInfo* FindCppTypeForLine(TArray<FCppSourceTypeInfo>& Types, const FString& File, int32 Line)
	{
		FCppSourceTypeInfo* Best = nullptr;
		for (FCppSourceTypeInfo& Type : Types)
		{
			if (Type.File == File && Type.Line <= Line)
			{
				if (!Best || Type.Line > Best->Line)
				{
					Best = &Type;
				}
			}
		}
		return Best;
	}

	static FString ExtractCppOwnerFromQualifiedFunctionLine(const FString& Line, const FString& FunctionName)
	{
		const int32 FunctionIndex = Line.Find(TEXT("::") + FunctionName, ESearchCase::CaseSensitive);
		if (FunctionIndex == INDEX_NONE)
		{
			return FString();
		}

		const FString BeforeQualifier = NormalizeCppWhitespace(Line.Left(FunctionIndex));
		TArray<FString> Tokens;
		BeforeQualifier.ParseIntoArrayWS(Tokens);
		if (Tokens.Num() == 0)
		{
			return FString();
		}
		return CleanCppIdentifier(Tokens.Last());
	}

	static FString ExtractFirstCppMacroArgument(const FString& Line)
	{
		const int32 OpenIndex = Line.Find(TEXT("("));
		if (OpenIndex == INDEX_NONE)
		{
			return FString();
		}

		const int32 CommaIndex = Line.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenIndex + 1);
		const int32 CloseIndex = Line.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenIndex + 1);
		int32 EndIndex = INDEX_NONE;
		if (CommaIndex != INDEX_NONE && CloseIndex != INDEX_NONE)
		{
			EndIndex = FMath::Min(CommaIndex, CloseIndex);
		}
		else
		{
			EndIndex = CommaIndex != INDEX_NONE ? CommaIndex : CloseIndex;
		}
		if (EndIndex == INDEX_NONE || EndIndex <= OpenIndex)
		{
			return FString();
		}

		return CleanCppIdentifier(Line.Mid(OpenIndex + 1, EndIndex - OpenIndex - 1));
	}

	static void ParseCppReflectedFile(const FString& FilePath, const FString& RelativePath, const TArray<FString>& Lines, FCppSourceIndexInfo& Index)
	{
		TMap<int32, int32> ScopeEndByTypeLine;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString TrimmedLine = Lines[LineIndex].TrimStartAndEnd();
			if (TrimmedLine.StartsWith(TEXT("UCLASS")) || TrimmedLine.StartsWith(TEXT("USTRUCT")) || TrimmedLine.StartsWith(TEXT("UENUM")))
			{
				const int32 MacroLine = LineIndex + 1;
				FString MacroText = CollectCppMacroFromLines(Lines, LineIndex);
				FString Declaration;
				int32 DeclarationLine = 0;
				const bool bIsEnum = MacroText.StartsWith(TEXT("UENUM"));
				if (!CollectCppDeclarationFromLines(Lines, LineIndex, Declaration, DeclarationLine, true))
				{
					Index.Warnings.Add(FString::Printf(TEXT("%s:%d reflected macro has no declaration"), *RelativePath, MacroLine));
					continue;
				}

				FCppSourceTypeInfo TypeInfo;
				TypeInfo.Kind = MacroText.StartsWith(TEXT("UCLASS")) ? TEXT("Class") : (MacroText.StartsWith(TEXT("USTRUCT")) ? TEXT("Struct") : TEXT("Enum"));
				TypeInfo.Name = ExtractCppTypeNameFromDeclaration(TypeInfo.Kind, Declaration);
				TypeInfo.Declaration = Declaration;
				TypeInfo.Specifiers = ExtractMacroSpecifiers(MacroText);
				TypeInfo.File = RelativePath;
				TypeInfo.Line = DeclarationLine > 0 ? DeclarationLine : MacroLine;
				if (TypeInfo.Name.IsEmpty())
				{
					TypeInfo.Name = FString::Printf(TEXT("<unnamed:%d>"), MacroLine);
					Index.Warnings.Add(FString::Printf(TEXT("%s:%d unable to parse reflected type name"), *RelativePath, MacroLine));
				}
				if (TypeInfo.Kind != TEXT("Enum"))
				{
					ParseCppBaseClasses(Declaration, TypeInfo.BaseClasses);
				}
				else
				{
					ParseCppEnumValues(Lines, DeclarationLine > 0 ? DeclarationLine - 1 : LineIndex, TypeInfo);
				}

				const int32 ScopeEndLine = FindTypeScopeEndLine(Lines, DeclarationLine > 0 ? DeclarationLine - 1 : LineIndex);
				ScopeEndByTypeLine.Add(TypeInfo.Line, ScopeEndLine);
				Index.Types.Add(MoveTemp(TypeInfo));
			}
		}

		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString TrimmedLine = Lines[LineIndex].TrimStartAndEnd();
			if (TrimmedLine.StartsWith(TEXT("UPROPERTY")))
			{
				const int32 MacroLine = LineIndex + 1;
				FString MacroText = CollectCppMacroFromLines(Lines, LineIndex);
				FString Declaration;
				int32 DeclarationLine = 0;
				if (!CollectCppDeclarationFromLines(Lines, LineIndex, Declaration, DeclarationLine, false))
				{
					Index.Warnings.Add(FString::Printf(TEXT("%s:%d UPROPERTY has no declaration"), *RelativePath, MacroLine));
					continue;
				}

				FCppSourceTypeInfo* OwnerType = FindCppTypeForLine(Index.Types, RelativePath, DeclarationLine > 0 ? DeclarationLine : MacroLine);
				if (!OwnerType)
				{
					Index.Warnings.Add(FString::Printf(TEXT("%s:%d UPROPERTY has no reflected owner"), *RelativePath, MacroLine));
					continue;
				}
				if (const int32* ScopeEndLine = ScopeEndByTypeLine.Find(OwnerType->Line))
				{
					if (DeclarationLine > *ScopeEndLine)
					{
						Index.Warnings.Add(FString::Printf(TEXT("%s:%d UPROPERTY outside parsed owner scope"), *RelativePath, MacroLine));
						continue;
					}
				}

				FCppSourcePropertyInfo Property;
				Property.Specifiers = ExtractMacroSpecifiers(MacroText);
				Property.File = RelativePath;
				Property.Line = DeclarationLine > 0 ? DeclarationLine : MacroLine;
				ParseCppPropertyDeclaration(Declaration, Property);
				ApplyCppPropertyFlags(Property);
				OwnerType->Properties.Add(MoveTemp(Property));
			}
			else if (TrimmedLine.StartsWith(TEXT("UFUNCTION")))
			{
				const int32 MacroLine = LineIndex + 1;
				FString MacroText = CollectCppMacroFromLines(Lines, LineIndex);
				FString Declaration;
				int32 DeclarationLine = 0;
				if (!CollectCppDeclarationFromLines(Lines, LineIndex, Declaration, DeclarationLine, true))
				{
					Index.Warnings.Add(FString::Printf(TEXT("%s:%d UFUNCTION has no declaration"), *RelativePath, MacroLine));
					continue;
				}

				FCppSourceTypeInfo* OwnerType = FindCppTypeForLine(Index.Types, RelativePath, DeclarationLine > 0 ? DeclarationLine : MacroLine);
				if (!OwnerType)
				{
					Index.Warnings.Add(FString::Printf(TEXT("%s:%d UFUNCTION has no reflected owner"), *RelativePath, MacroLine));
					continue;
				}
				if (const int32* ScopeEndLine = ScopeEndByTypeLine.Find(OwnerType->Line))
				{
					if (DeclarationLine > *ScopeEndLine)
					{
						Index.Warnings.Add(FString::Printf(TEXT("%s:%d UFUNCTION outside parsed owner scope"), *RelativePath, MacroLine));
						continue;
					}
				}

				FCppSourceFunctionInfo Function;
				Function.Specifiers = ExtractMacroSpecifiers(MacroText);
				Function.File = RelativePath;
				Function.Line = DeclarationLine > 0 ? DeclarationLine : MacroLine;
				ParseCppFunctionDeclaration(Declaration, Function);
				ApplyCppFunctionFlags(Function);
				OwnerType->Functions.Add(MoveTemp(Function));
			}
			else if (TrimmedLine.Contains(TEXT("DOREPLIFETIME")) || TrimmedLine.Contains(TEXT("DOREPLIFETIME_CONDITION")) || TrimmedLine.Contains(TEXT("DOREPLIFETIME_ACTIVE_OVERRIDE")))
			{
				FCppSourceTypeInfo* OwnerType = Index.Types.Num() > 0 ? FindCppTypeForLine(Index.Types, RelativePath, LineIndex + 1) : nullptr;
				if (!OwnerType)
				{
					OwnerType = FindCppTypeByName(Index.Types, ExtractFirstCppMacroArgument(TrimmedLine));
				}
				if (OwnerType)
				{
					OwnerType->ReplicationLines.AddUnique(NormalizeCppWhitespace(TrimmedLine));
				}
			}
			else if (TrimmedLine.Contains(TEXT("GetLifetimeReplicatedProps")))
			{
				FCppSourceTypeInfo* OwnerType = FindCppTypeForLine(Index.Types, RelativePath, LineIndex + 1);
				if (!OwnerType)
				{
					OwnerType = FindCppTypeByName(Index.Types, ExtractCppOwnerFromQualifiedFunctionLine(TrimmedLine, TEXT("GetLifetimeReplicatedProps")));
				}
				if (OwnerType)
				{
					OwnerType->bHasGetLifetimeReplicatedProps = true;
				}
			}
		}
	}

	static void ParseCppFunctionDefinitions(const FString& RelativePath, const TArray<FString>& Lines, FCppSourceIndexInfo& Index)
	{
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString Line = NormalizeCppWhitespace(Lines[LineIndex]);
			if (!Line.Contains(TEXT("::")) || !Line.Contains(TEXT("(")) || Line.StartsWith(TEXT("//")) || Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			if (Line.Contains(TEXT(";")) && !Line.Contains(TEXT("{")))
			{
				continue;
			}

			int32 SearchLine = LineIndex;
			FString Signature = Line;
			while (!Signature.Contains(TEXT("{")) && !Signature.Contains(TEXT(";")) && SearchLine + 1 < Lines.Num())
			{
				++SearchLine;
				Signature = NormalizeCppWhitespace(Signature + TEXT(" ") + Lines[SearchLine]);
			}
			if (!Signature.Contains(TEXT("{")))
			{
				continue;
			}

			const int32 ParenIndex = Signature.Find(TEXT("("));
			const FString BeforeParen = NormalizeCppWhitespace(Signature.Left(ParenIndex));
			TArray<FString> Tokens;
			BeforeParen.ParseIntoArrayWS(Tokens);
			if (Tokens.Num() == 0)
			{
				continue;
			}

			FString QualifiedName = Tokens.Last();
			FString Owner;
			FString Name;
			if (!QualifiedName.Split(TEXT("::"), &Owner, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				continue;
			}

			FCppSourceDefinitionInfo Definition;
			Definition.Owner = Owner;
			Definition.Name = CleanCppIdentifier(Name);
			Definition.Signature = Signature.Left(Signature.Find(TEXT("{"))).TrimStartAndEnd();
			Definition.File = RelativePath;
			Definition.Line = LineIndex + 1;
			Index.Definitions.Add(MoveTemp(Definition));
			LineIndex = SearchLine;
		}
	}

	static FCppSourceIndexInfo BuildCppSourceIndex()
	{
		FCppSourceIndexInfo Index;
		Index.SourceRoots = GetCppSourceRoots();
		const TArray<FString> Files = GetCppSourceFiles(Index.SourceRoots);

		const FString ProjectSourceRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source")).Replace(TEXT("\\"), TEXT("/"));
		const FString ProjectPluginsRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir()).Replace(TEXT("\\"), TEXT("/"));

		for (const FString& FilePath : Files)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *FilePath))
			{
				Index.Warnings.Add(TEXT("Failed to read ") + GetPathRelativeToProject(FilePath));
				continue;
			}

			const FString RelativePath = GetPathRelativeToProject(FilePath);
			const TArray<FString> Lines = SplitCppLinesWithoutBlockComments(Text);

			FCppSourceFileInfo FileInfo;
			FileInfo.Path = FilePath;
			FileInfo.RelativePath = RelativePath;
			FileInfo.LineCount = Lines.Num();
			const FString NormalizedFilePath = FilePath.Replace(TEXT("\\"), TEXT("/"));
			FileInfo.bIsProjectSource = NormalizedFilePath.StartsWith(ProjectSourceRoot, ESearchCase::IgnoreCase);
			FileInfo.bIsProjectPluginSource = NormalizedFilePath.StartsWith(ProjectPluginsRoot, ESearchCase::IgnoreCase);
			Index.Files.Add(MoveTemp(FileInfo));

			ParseCppReflectedFile(FilePath, RelativePath, Lines, Index);
			if (FPaths::GetExtension(FilePath, true).Equals(TEXT(".cpp"), ESearchCase::IgnoreCase))
			{
				ParseCppFunctionDefinitions(RelativePath, Lines, Index);
			}
		}

		Index.Types.Sort([](const FCppSourceTypeInfo& A, const FCppSourceTypeInfo& B)
		{
			if (A.File == B.File)
			{
				return A.Line < B.Line;
			}
			return A.File < B.File;
		});
		Index.Definitions.Sort([](const FCppSourceDefinitionInfo& A, const FCppSourceDefinitionInfo& B)
		{
			if (A.Owner == B.Owner)
			{
				return A.Name < B.Name;
			}
			return A.Owner < B.Owner;
		});
		Index.Warnings.Sort();
		return Index;
	}

	static FString GetCppFunctionNetworkRole(const FCppSourceFunctionInfo& Function)
	{
		if (Function.bServer)
		{
			return TEXT("Server");
		}
		if (Function.bClient)
		{
			return TEXT("Client");
		}
		if (Function.bNetMulticast)
		{
			return TEXT("NetMulticast");
		}
		return TEXT("Local");
	}

	static void WriteCppPropertyJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const FCppSourcePropertyInfo& Property)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Property.Name);
		Writer->WriteValue(TEXT("type"), Property.Type);
		Writer->WriteValue(TEXT("declaration"), Property.Declaration);
		Writer->WriteValue(TEXT("specifiers"), Property.Specifiers);
		Writer->WriteValue(TEXT("file"), Property.File);
		Writer->WriteValue(TEXT("line"), Property.Line);
		Writer->WriteValue(TEXT("blueprintVisible"), Property.bBlueprintVisible);
		Writer->WriteValue(TEXT("blueprintAssignable"), Property.bBlueprintAssignable);
		Writer->WriteValue(TEXT("editable"), Property.bEditable);
		Writer->WriteValue(TEXT("replicated"), Property.bReplicated);
		Writer->WriteValue(TEXT("repNotify"), Property.RepNotify);
		Writer->WriteValue(TEXT("saveGame"), Property.bSaveGame);
		Writer->WriteObjectEnd();
	}

	static void WriteCppFunctionJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const FCppSourceFunctionInfo& Function)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Function.Name);
		Writer->WriteValue(TEXT("returnType"), Function.ReturnType);
		Writer->WriteValue(TEXT("signature"), Function.Signature);
		Writer->WriteValue(TEXT("specifiers"), Function.Specifiers);
		Writer->WriteValue(TEXT("file"), Function.File);
		Writer->WriteValue(TEXT("line"), Function.Line);
		Writer->WriteValue(TEXT("blueprintCallable"), Function.bBlueprintCallable);
		Writer->WriteValue(TEXT("blueprintPure"), Function.bBlueprintPure);
		Writer->WriteValue(TEXT("blueprintEvent"), Function.bBlueprintEvent);
		Writer->WriteValue(TEXT("blueprintNativeEvent"), Function.bBlueprintNativeEvent);
		Writer->WriteValue(TEXT("blueprintImplementableEvent"), Function.bBlueprintImplementableEvent);
		Writer->WriteValue(TEXT("networkRole"), GetCppFunctionNetworkRole(Function));
		Writer->WriteValue(TEXT("reliable"), Function.bReliable);
		Writer->WriteValue(TEXT("unreliable"), Function.bUnreliable);
		Writer->WriteObjectEnd();
	}

	static FString ExportCppSourceIndexToJson(const FCppSourceIndexInfo& Index)
	{
		int32 ClassCount = 0;
		int32 StructCount = 0;
		int32 EnumCount = 0;
		int32 PropertyCount = 0;
		int32 FunctionCount = 0;
		int32 BlueprintFunctionCount = 0;
		int32 RpcFunctionCount = 0;
		for (const FCppSourceTypeInfo& Type : Index.Types)
		{
			ClassCount += Type.Kind == TEXT("Class") ? 1 : 0;
			StructCount += Type.Kind == TEXT("Struct") ? 1 : 0;
			EnumCount += Type.Kind == TEXT("Enum") ? 1 : 0;
			PropertyCount += Type.Properties.Num();
			FunctionCount += Type.Functions.Num();
			for (const FCppSourceFunctionInfo& Function : Type.Functions)
			{
				BlueprintFunctionCount += (Function.bBlueprintCallable || Function.bBlueprintPure || Function.bBlueprintEvent) ? 1 : 0;
				RpcFunctionCount += (Function.bServer || Function.bClient || Function.bNetMulticast) ? 1 : 0;
			}
		}

		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), 1);
		Writer->WriteValue(TEXT("generatedBy"), TEXT("GetTheMeaning"));
		Writer->WriteValue(TEXT("projectName"), FApp::GetProjectName());
		Writer->WriteValue(TEXT("exportTime"), FDateTime::Now().ToIso8601());
		Writer->WriteValue(TEXT("sourceFileCount"), Index.Files.Num());
		Writer->WriteValue(TEXT("classCount"), ClassCount);
		Writer->WriteValue(TEXT("structCount"), StructCount);
		Writer->WriteValue(TEXT("enumCount"), EnumCount);
		Writer->WriteValue(TEXT("propertyCount"), PropertyCount);
		Writer->WriteValue(TEXT("functionCount"), FunctionCount);
		Writer->WriteValue(TEXT("blueprintExposedFunctionCount"), BlueprintFunctionCount);
		Writer->WriteValue(TEXT("rpcFunctionCount"), RpcFunctionCount);

		Writer->WriteArrayStart(TEXT("sourceRoots"));
		for (const FString& Root : Index.SourceRoots)
		{
			Writer->WriteValue(Root);
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("files"));
		for (const FCppSourceFileInfo& File : Index.Files)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("path"), File.Path);
			Writer->WriteValue(TEXT("relativePath"), File.RelativePath);
			Writer->WriteValue(TEXT("lineCount"), File.LineCount);
			Writer->WriteValue(TEXT("projectSource"), File.bIsProjectSource);
			Writer->WriteValue(TEXT("projectPluginSource"), File.bIsProjectPluginSource);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("types"));
		for (const FCppSourceTypeInfo& Type : Index.Types)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("kind"), Type.Kind);
			Writer->WriteValue(TEXT("name"), Type.Name);
			Writer->WriteValue(TEXT("declaration"), Type.Declaration);
			Writer->WriteValue(TEXT("specifiers"), Type.Specifiers);
			Writer->WriteValue(TEXT("file"), Type.File);
			Writer->WriteValue(TEXT("line"), Type.Line);
			Writer->WriteArrayStart(TEXT("baseClasses"));
			for (const FString& BaseClass : Type.BaseClasses)
			{
				Writer->WriteValue(BaseClass);
			}
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("enumValues"));
			for (const FString& Value : Type.EnumValues)
			{
				Writer->WriteValue(Value);
			}
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("properties"));
			for (const FCppSourcePropertyInfo& Property : Type.Properties)
			{
				WriteCppPropertyJson(Writer, Property);
			}
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("functions"));
			for (const FCppSourceFunctionInfo& Function : Type.Functions)
			{
				WriteCppFunctionJson(Writer, Function);
			}
			Writer->WriteArrayEnd();
			Writer->WriteValue(TEXT("hasGetLifetimeReplicatedProps"), Type.bHasGetLifetimeReplicatedProps);
			Writer->WriteArrayStart(TEXT("replicationLines"));
			for (const FString& Line : Type.ReplicationLines)
			{
				Writer->WriteValue(Line);
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("definitions"));
		for (const FCppSourceDefinitionInfo& Definition : Index.Definitions)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("owner"), Definition.Owner);
			Writer->WriteValue(TEXT("name"), Definition.Name);
			Writer->WriteValue(TEXT("signature"), Definition.Signature);
			Writer->WriteValue(TEXT("file"), Definition.File);
			Writer->WriteValue(TEXT("line"), Definition.Line);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("warnings"));
		for (const FString& Warning : Index.Warnings)
		{
			Writer->WriteValue(Warning);
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
		return Json;
	}

	static void AppendCppSourceFlagList(FString& Out, const FCppSourceFunctionInfo& Function)
	{
		TArray<FString> Flags;
		if (Function.bBlueprintCallable) Flags.Add(TEXT("BlueprintCallable"));
		if (Function.bBlueprintPure) Flags.Add(TEXT("BlueprintPure"));
		if (Function.bBlueprintNativeEvent) Flags.Add(TEXT("BlueprintNativeEvent"));
		if (Function.bBlueprintImplementableEvent) Flags.Add(TEXT("BlueprintImplementableEvent"));
		if (Function.bServer) Flags.Add(TEXT("Server"));
		if (Function.bClient) Flags.Add(TEXT("Client"));
		if (Function.bNetMulticast) Flags.Add(TEXT("NetMulticast"));
		if (Function.bReliable) Flags.Add(TEXT("Reliable"));
		if (Function.bUnreliable) Flags.Add(TEXT("Unreliable"));
		if (Flags.Num() > 0)
		{
			Out += TEXT(" [") + FString::Join(Flags, TEXT(", ")) + TEXT("]");
		}
	}

	static void AppendCppMarkdownList(FString& Out, const FString& Heading, const TArray<FString>& Values)
	{
		if (Values.Num() == 0)
		{
			return;
		}

		Out += TEXT("\n") + Heading + TEXT(":\n");
		const int32 MaxItems = 40;
		for (int32 Index = 0; Index < Values.Num() && Index < MaxItems; ++Index)
		{
			Out += TEXT("- ") + Values[Index] + TEXT("\n");
		}
		if (Values.Num() > MaxItems)
		{
			Out += FString::Printf(TEXT("- ... %d more\n"), Values.Num() - MaxItems);
		}
	}

	static FString ExportCppSourceIndexToMarkdown(const FCppSourceIndexInfo& Index)
	{
		int32 ClassCount = 0;
		int32 StructCount = 0;
		int32 EnumCount = 0;
		int32 PropertyCount = 0;
		int32 FunctionCount = 0;
		TArray<FString> BlueprintFunctions;
		TArray<FString> RpcFunctions;
		TArray<FString> ReplicatedProperties;
		for (const FCppSourceTypeInfo& Type : Index.Types)
		{
			ClassCount += Type.Kind == TEXT("Class") ? 1 : 0;
			StructCount += Type.Kind == TEXT("Struct") ? 1 : 0;
			EnumCount += Type.Kind == TEXT("Enum") ? 1 : 0;
			PropertyCount += Type.Properties.Num();
			FunctionCount += Type.Functions.Num();
			for (const FCppSourceFunctionInfo& Function : Type.Functions)
			{
				if (Function.bBlueprintCallable || Function.bBlueprintPure || Function.bBlueprintEvent)
				{
					FString Summary = FString::Printf(TEXT("%s::%s (%s:%d)"), *Type.Name, *Function.Name, *Function.File, Function.Line);
					if (Function.bBlueprintCallable) Summary += TEXT(" BlueprintCallable");
					if (Function.bBlueprintPure) Summary += TEXT(" BlueprintPure");
					if (Function.bBlueprintEvent) Summary += TEXT(" BlueprintEvent");
					BlueprintFunctions.Add(Summary);
				}
				if (Function.bServer || Function.bClient || Function.bNetMulticast)
				{
					FString Summary = FString::Printf(TEXT("%s::%s -> %s%s (%s:%d)"),
						*Type.Name,
						*Function.Name,
						*GetCppFunctionNetworkRole(Function),
						Function.bReliable ? TEXT(" Reliable") : TEXT(""),
						*Function.File,
						Function.Line);
					RpcFunctions.Add(Summary);
				}
			}
			for (const FCppSourcePropertyInfo& Property : Type.Properties)
			{
				if (Property.bReplicated)
				{
					ReplicatedProperties.Add(FString::Printf(TEXT("%s::%s : %s%s (%s:%d)"),
						*Type.Name,
						*Property.Name,
						*Property.Type,
						Property.RepNotify.IsEmpty() ? TEXT("") : *FString(TEXT(" RepNotify=") + Property.RepNotify),
						*Property.File,
						Property.Line));
				}
			}
		}
		BlueprintFunctions.Sort();
		RpcFunctions.Sort();
		ReplicatedProperties.Sort();

		FString Out;
		Out += TEXT("# C++ Source Index\n\n");
		Out += TEXT("- ProjectName: ") + FString(FApp::GetProjectName()) + TEXT("\n");
		Out += TEXT("- ExportTime: ") + FDateTime::Now().ToIso8601() + TEXT("\n");
		Out += FString::Printf(TEXT("- SourceFiles: %d\n"), Index.Files.Num());
		Out += FString::Printf(TEXT("- ReflectedTypes: %d (Classes %d, Structs %d, Enums %d)\n"), Index.Types.Num(), ClassCount, StructCount, EnumCount);
		Out += FString::Printf(TEXT("- ReflectedProperties: %d\n"), PropertyCount);
		Out += FString::Printf(TEXT("- ReflectedFunctions: %d\n\n"), FunctionCount);

		Out += TEXT("## Source Roots\n\n");
		if (Index.SourceRoots.Num() == 0)
		{
			Out += TEXT("- <none>\n");
		}
		for (const FString& Root : Index.SourceRoots)
		{
			Out += TEXT("- ") + Root + TEXT("\n");
		}

		AppendCppMarkdownList(Out, TEXT("Blueprint Exposed Functions"), BlueprintFunctions);
		AppendCppMarkdownList(Out, TEXT("RPC Functions"), RpcFunctions);
		AppendCppMarkdownList(Out, TEXT("Replicated Properties"), ReplicatedProperties);

		Out += TEXT("\n## Reflected Types\n\n");
		for (const FCppSourceTypeInfo& Type : Index.Types)
		{
			Out += FString::Printf(TEXT("### %s %s\n\n"), *Type.Kind, *Type.Name);
			Out += FString::Printf(TEXT("- File: %s:%d\n"), *Type.File, Type.Line);
			if (!Type.Specifiers.IsEmpty())
			{
				Out += TEXT("- Specifiers: ") + Type.Specifiers + TEXT("\n");
			}
			if (Type.BaseClasses.Num() > 0)
			{
				Out += TEXT("- BaseClasses: ") + FString::Join(Type.BaseClasses, TEXT(", ")) + TEXT("\n");
			}
			if (Type.EnumValues.Num() > 0)
			{
				Out += TEXT("- Values: ") + FString::Join(Type.EnumValues, TEXT(", ")) + TEXT("\n");
			}
			if (Type.bHasGetLifetimeReplicatedProps)
			{
				Out += TEXT("- Replication: Has GetLifetimeReplicatedProps\n");
			}

			if (Type.Properties.Num() > 0)
			{
				Out += TEXT("\nProperties:\n");
				for (const FCppSourcePropertyInfo& Property : Type.Properties)
				{
					Out += FString::Printf(TEXT("- %s: %s"), *Property.Name, *Property.Type);
					TArray<FString> Flags;
					if (Property.bBlueprintVisible) Flags.Add(TEXT("BlueprintVisible"));
					if (Property.bBlueprintAssignable) Flags.Add(TEXT("BlueprintAssignable"));
					if (Property.bEditable) Flags.Add(TEXT("Editable"));
					if (Property.bReplicated) Flags.Add(Property.RepNotify.IsEmpty() ? TEXT("Replicated") : TEXT("ReplicatedUsing=") + Property.RepNotify);
					if (Property.bSaveGame) Flags.Add(TEXT("SaveGame"));
					if (Flags.Num() > 0)
					{
						Out += TEXT(" [") + FString::Join(Flags, TEXT(", ")) + TEXT("]");
					}
					Out += FString::Printf(TEXT(" (%s:%d)\n"), *Property.File, Property.Line);
				}
			}

			if (Type.Functions.Num() > 0)
			{
				Out += TEXT("\nFunctions:\n");
				for (const FCppSourceFunctionInfo& Function : Type.Functions)
				{
					Out += FString::Printf(TEXT("- %s"), *Function.Signature);
					AppendCppSourceFlagList(Out, Function);
					Out += FString::Printf(TEXT(" (%s:%d)\n"), *Function.File, Function.Line);
				}
			}

			if (Type.ReplicationLines.Num() > 0)
			{
				Out += TEXT("\nReplication Lines:\n");
				for (const FString& Line : Type.ReplicationLines)
				{
					Out += TEXT("- ") + Line + TEXT("\n");
				}
			}
			Out += TEXT("\n");
		}

		if (Index.Definitions.Num() > 0)
		{
			Out += TEXT("## C++ Function Definitions\n\n");
			const int32 MaxDefinitions = 200;
			for (int32 IndexValue = 0; IndexValue < Index.Definitions.Num() && IndexValue < MaxDefinitions; ++IndexValue)
			{
				const FCppSourceDefinitionInfo& Definition = Index.Definitions[IndexValue];
				Out += FString::Printf(TEXT("- %s::%s (%s:%d)\n"), *Definition.Owner, *Definition.Name, *Definition.File, Definition.Line);
			}
			if (Index.Definitions.Num() > MaxDefinitions)
			{
				Out += FString::Printf(TEXT("- ... %d more\n"), Index.Definitions.Num() - MaxDefinitions);
			}
			Out += TEXT("\n");
		}

		AppendCppMarkdownList(Out, TEXT("Warnings"), Index.Warnings);
		return Out;
	}

	static void WriteCppSourceIndexFiles()
	{
		const FCppSourceIndexInfo Index = BuildCppSourceIndex();
		const FString Json = ExportCppSourceIndexToJson(Index);
		const FString Markdown = ExportCppSourceIndexToMarkdown(Index);
		SaveText(Json, GetExportRootDir(), TEXT("CppSourceIndex"), TEXT(".json"));
		SaveText(Markdown, GetExportRootDir(), TEXT("CppSourceIndex"), TEXT(".md"));
	}

	// 尝试把单个资产按类型导出；返回：0=成功, 1=失败（类型支持但导出空或保存失败）, 2=跳过（类型不支持）
	enum class EExportResult : uint8 { Success, Failed, Skipped };

	static EExportResult ExportOneAsset(const FAssetData& AssetData, FString& OutSavedPath)
	{
		UObject* Obj = AssetData.GetAsset();
		if (!Obj) return EExportResult::Failed;

		// 蓝图
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Obj))
		{
			const FString Text = FBlueprintToTextExporter::ExportBlueprintToText(Blueprint);
			const FString Json = FBlueprintToTextExporter::ExportBlueprintToJson(Blueprint);
			if (Text.IsEmpty() && Json.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, Blueprint->GetName(), TEXT("_ReadableCode.txt"));
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, Blueprint->GetName(), TEXT("_Logic.json"));
			if ((Text.IsEmpty() || !SavedText.IsEmpty()) && (Json.IsEmpty() || !SavedJson.IsEmpty()))
			{
				OutSavedPath = !SavedText.IsEmpty() ? SavedText : SavedJson;
				return EExportResult::Success;
			}
			if (!SavedText.IsEmpty() || !SavedJson.IsEmpty())
			{
				OutSavedPath = !SavedText.IsEmpty() ? SavedText : SavedJson;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		// 材质 / 材质实例（注意：UMaterialInstance 也是 UMaterialInterface）
		if (UMaterialInterface* MatIface = Cast<UMaterialInterface>(Obj))
		{
			const FString Text = FMaterialToTextExporter::ExportMaterialToText(MatIface);
			if (Text.IsEmpty()) return EExportResult::Failed;
			const FString Saved = SaveText(Text, GetAssetExportDirOrRoot(AssetData), MatIface->GetName(), TEXT("_ReadableMaterial.md"));
			if (Saved.IsEmpty()) return EExportResult::Failed;
			OutSavedPath = Saved;
			return EExportResult::Success;
		}

		// 材质函数
		if (UMaterialFunctionInterface* MatFunc = Cast<UMaterialFunctionInterface>(Obj))
		{
			const FString Text = FMaterialToTextExporter::ExportMaterialFunctionToText(MatFunc);
			if (Text.IsEmpty()) return EExportResult::Failed;
			const FString Saved = SaveText(Text, GetAssetExportDirOrRoot(AssetData), MatFunc->GetName(), TEXT("_ReadableMaterialFunction.md"));
			if (Saved.IsEmpty()) return EExportResult::Failed;
			OutSavedPath = Saved;
			return EExportResult::Success;
		}

		if (UDataTable* DataTable = Cast<UDataTable>(Obj))
		{
			const FString Json = ExportDataTableToJson(DataTable);
			const FString Text = ExportDataTableToMarkdown(DataTable);
			if (Json.IsEmpty() && Text.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, DataTable->GetName(), TEXT("_Table.json"));
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, DataTable->GetName(), TEXT("_ReadableTable.md"));
			if (!SavedJson.IsEmpty() || !SavedText.IsEmpty())
			{
				OutSavedPath = !SavedJson.IsEmpty() ? SavedJson : SavedText;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Obj))
		{
			const FString Json = ExportStructToJson(Struct);
			const FString Text = ExportStructToMarkdown(Struct);
			if (Json.IsEmpty() && Text.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, Struct->GetName(), TEXT("_Struct.json"));
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, Struct->GetName(), TEXT("_ReadableStruct.md"));
			if (!SavedJson.IsEmpty() || !SavedText.IsEmpty())
			{
				OutSavedPath = !SavedJson.IsEmpty() ? SavedJson : SavedText;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		if (UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(Obj))
		{
			const FString Json = ExportEnumToJson(Enum);
			const FString Text = ExportEnumToMarkdown(Enum);
			if (Json.IsEmpty() && Text.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, Enum->GetName(), TEXT("_Enum.json"));
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, Enum->GetName(), TEXT("_ReadableEnum.md"));
			if (!SavedJson.IsEmpty() || !SavedText.IsEmpty())
			{
				OutSavedPath = !SavedJson.IsEmpty() ? SavedJson : SavedText;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		if (UWorld* World = Cast<UWorld>(Obj))
		{
			const FString Json = ExportWorldSummaryToJson(World);
			const FString Text = ExportWorldSummaryToMarkdown(World);
			if (Json.IsEmpty() && Text.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, World->GetName(), TEXT("_LevelSummary.json"));
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, World->GetName(), TEXT("_ReadableLevelSummary.md"));
			if (!SavedJson.IsEmpty() || !SavedText.IsEmpty())
			{
				OutSavedPath = !SavedJson.IsEmpty() ? SavedJson : SavedText;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		if (UInputAction* InputAction = Cast<UInputAction>(Obj))
		{
			const FString Json = ExportInputActionToJson(InputAction);
			const FString Text = ExportInputActionToMarkdown(InputAction);
			if (Json.IsEmpty() && Text.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, InputAction->GetName(), TEXT("_InputAction.json"));
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, InputAction->GetName(), TEXT("_ReadableInputAction.md"));
			if (!SavedJson.IsEmpty() || !SavedText.IsEmpty())
			{
				OutSavedPath = !SavedJson.IsEmpty() ? SavedJson : SavedText;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		if (UInputMappingContext* MappingContext = Cast<UInputMappingContext>(Obj))
		{
			const FString Json = ExportInputMappingContextToJson(MappingContext);
			const FString Text = ExportInputMappingContextToMarkdown(MappingContext);
			if (Json.IsEmpty() && Text.IsEmpty()) return EExportResult::Failed;

			const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
			const FString SavedJson = Json.IsEmpty() ? FString() : SaveText(Json, ExportDir, MappingContext->GetName(), TEXT("_InputMappingContext.json"));
			const FString SavedText = Text.IsEmpty() ? FString() : SaveText(Text, ExportDir, MappingContext->GetName(), TEXT("_ReadableInputMappingContext.md"));
			if (!SavedJson.IsEmpty() || !SavedText.IsEmpty())
			{
				OutSavedPath = !SavedJson.IsEmpty() ? SavedJson : SavedText;
				return EExportResult::Success;
			}
			return EExportResult::Failed;
		}

		return EExportResult::Skipped;
	}

	static TArray<FAssetData> FilterSupportedAssets(const TArray<FAssetData>& AssetList)
	{
		TMap<FName, FAssetData> UniqueAssets;
		for (const FAssetData& AssetData : AssetList)
		{
			if (!IsSupportedAssetData(AssetData))
			{
				continue;
			}
			UniqueAssets.FindOrAdd(AssetData.PackageName, AssetData);
		}

		TArray<FAssetData> Result;
		UniqueAssets.GenerateValueArray(Result);
		Result.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});
		return Result;
	}

	static TArray<FAssetData> GetSupportedAssetsInFolders(const TArray<FString>& SelectedPackagePaths)
	{
		TArray<FName> PackagePaths;
		for (const FString& Path : SelectedPackagePaths)
		{
			if (!Path.IsEmpty())
			{
				PackagePaths.AddUnique(FName(*Path));
			}
		}

		TArray<FAssetData> AssetList;
		if (PackagePaths.Num() == 0)
		{
			return AssetList;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().GetAssetsByPaths(PackagePaths, AssetList, true);
		return FilterSupportedAssets(AssetList);
	}

	static TArray<FAssetData> GetSelectedAssetsFromContentBrowser()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> SelectedAssets;
		ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
		return SelectedAssets;
	}

	static void NotifyEmpty(const FText& Message)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	static void CopyExportPathToClipboard(const FString& Path)
	{
		if (!Path.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*FPaths::ConvertRelativePathToFull(Path));
		}
	}

	static bool PromptSaveDirtyPackagesBeforeExport()
	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyPackages(DirtyPackages);
		if (DirtyPackages.Num() == 0)
		{
			return true;
		}

		TArray<UPackage*> FailedPackages;
		FEditorFileUtils::FPromptForCheckoutAndSaveParams SaveParams;
		SaveParams.bCheckDirty = true;
		SaveParams.bPromptToSave = true;
		SaveParams.bCanBeDeclined = true;
		SaveParams.bIsExplicitSave = true;
		SaveParams.Title = LOCTEXT("SaveBeforeExportTitle", "导出前保存");
		SaveParams.Message = LOCTEXT("SaveBeforeExportMessage", "导出前请保存需要写入本次 AI 可读文档快照的资产。");
		SaveParams.OutFailedPackages = &FailedPackages;

		const FEditorFileUtils::EPromptReturnCode ReturnCode = FEditorFileUtils::PromptForCheckoutAndSave(DirtyPackages, SaveParams);
		if (ReturnCode == FEditorFileUtils::PR_Success)
		{
			return true;
		}

		const FText Message = FailedPackages.Num() > 0
			? FText::Format(LOCTEXT("SaveBeforeExportFailed", "导出已取消：有 {0} 个包未能保存。"), FText::AsNumber(FailedPackages.Num()))
			: LOCTEXT("SaveBeforeExportCancelled", "导出已取消：未保存当前修改。");
		NotifyEmpty(Message);
		return false;
	}

	// 打开导出根目录
	static void OpenExportRootDir()
	{
		const FString Dir = GetExportRootDir();
		EnsureDir(Dir);
		FPlatformProcess::ExploreFolder(*Dir);
	}
}

using namespace GetTheMeaningExportImpl;

struct FExportIndexEntry
{
	FString Name;
	FString Type;
	FString AssetPath;
	FString PackagePath;
	FString ParentClass;
	FString ReadablePath;
	FString LogicJsonPath;
	TArray<FString> Variables;
	TArray<FString> Events;
	TArray<FString> RPCs;
	TArray<FString> Functions;
	TArray<FString> Calls;
	TArray<FString> VariableReads;
	TArray<FString> VariableWrites;
	TArray<FString> DataTables;
	TArray<FString> Structs;
	TArray<FString> AssetReferences;
};

static FString GetObjectPathForIndex(const UObject* Obj)
{
	return Obj ? Obj->GetPathName() : FString();
}

static FString GetRelativeExportPath(const FString& FullPath)
{
	if (FullPath.IsEmpty())
	{
		return FString();
	}

	const FString RootDir = GetExportRootDir();
	FString RelativePath = FullPath;
	if (FPaths::MakePathRelativeTo(RelativePath, *RootDir))
	{
		return RelativePath.Replace(TEXT("\\"), TEXT("/"));
	}
	return FullPath.Replace(TEXT("\\"), TEXT("/"));
}

static FString GetReplicationForIndex(uint32 FunctionFlags)
{
	if ((FunctionFlags & FUNC_Net) == 0)
	{
		return TEXT("Local");
	}
	if ((FunctionFlags & FUNC_NetServer) != 0)
	{
		return TEXT("RunOnServer");
	}
	if ((FunctionFlags & FUNC_NetClient) != 0)
	{
		return TEXT("RunOnOwningClient");
	}
	if ((FunctionFlags & FUNC_NetMulticast) != 0)
	{
		return TEXT("Multicast");
	}
	return TEXT("Replicated");
}

static uint32 GetEventNetFlagsForIndex(const UK2Node_Event* EventNode)
{
	if (!EventNode)
	{
		return 0;
	}
	if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(EventNode))
	{
		return CustomEvent->GetNetFlags();
	}
	if (UFunction* Function = EventNode->FindEventSignatureFunction())
	{
		return Function->FunctionFlags & FUNC_NetFuncFlags;
	}
	return 0;
}

static void AddUniqueSorted(TArray<FString>& Values, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Values.AddUnique(Value);
		Values.Sort();
	}
}

static FString DescribeDataTablePinForIndex(const UEdGraphPin* DataTablePin)
{
	if (!DataTablePin)
	{
		return FString();
	}
	if (const UDataTable* DataTable = Cast<UDataTable>(DataTablePin->DefaultObject))
	{
		return GetObjectPathForIndex(DataTable);
	}
	if (DataTablePin->LinkedTo.Num() > 0 && DataTablePin->LinkedTo[0])
	{
		if (const UEdGraphNode* SourceNode = DataTablePin->LinkedTo[0]->GetOwningNode())
		{
			return TEXT("<linked> ") + SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString() + TEXT(".") + DataTablePin->LinkedTo[0]->PinName.ToString();
		}
	}
	return FString();
}

static FString DescribeRowNamePinForIndex(const UEdGraphPin* RowNamePin)
{
	if (!RowNamePin)
	{
		return FString();
	}
	if (RowNamePin->LinkedTo.Num() > 0 && RowNamePin->LinkedTo[0])
	{
		if (const UEdGraphNode* SourceNode = RowNamePin->LinkedTo[0]->GetOwningNode())
		{
			return SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString() + TEXT(".") + RowNamePin->LinkedTo[0]->PinName.ToString();
		}
	}
	if (RowNamePin->DefaultObject)
	{
		return GetObjectPathForIndex(RowNamePin->DefaultObject);
	}
	return RowNamePin->DefaultValue;
}

static void AddStructFromPinForIndex(TArray<FString>& Structs, const FEdGraphPinType& PinType)
{
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
		{
			AddUniqueSorted(Structs, GetObjectPathForIndex(Struct));
		}
	}
	if (PinType.ContainerType == EPinContainerType::Map && PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinValueType.TerminalSubCategoryObject.Get()))
		{
			AddUniqueSorted(Structs, GetObjectPathForIndex(Struct));
		}
	}
}

static void AddObjectReferenceForIndex(TArray<FString>& References, const FString& Purpose, const UObject* Object)
{
	if (!Object)
	{
		return;
	}

	const UClass* Class = Cast<UClass>(Object);
	const UObject* RefObject = Class && Class->ClassGeneratedBy ? Class->ClassGeneratedBy : Object;
	AddUniqueSorted(References, Purpose + TEXT(": ") + GetObjectPathForIndex(RefObject));
}

static void AddPinReferenceForIndex(TArray<FString>& References, const FString& Purpose, const UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return;
	}

	if (Pin->DefaultObject)
	{
		AddObjectReferenceForIndex(References, Purpose, Pin->DefaultObject);
	}

	if (UObject* SubCategoryObject = Pin->PinType.PinSubCategoryObject.Get())
	{
		if (SubCategoryObject->IsA<UClass>())
		{
			AddObjectReferenceForIndex(References, Purpose + TEXT(" Type"), SubCategoryObject);
		}
	}
}

static void CollectBlueprintIndexDetails(UBlueprint* Blueprint, FExportIndexEntry& Entry)
{
	if (!Blueprint)
	{
		return;
	}

	Entry.ParentClass = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		AddUniqueSorted(Entry.Variables, Var.VarName.ToString());
		AddStructFromPinForIndex(Entry.Structs, Var.VarType);
	}

	auto VisitGraph = [&Entry](UEdGraph* Graph)
	{
		if (!Graph) return;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				const FString EventName = EventNode->GetFunctionName().ToString();
				AddUniqueSorted(Entry.Events, EventName);

				const uint32 NetFlags = GetEventNetFlagsForIndex(EventNode);
				const FString Replication = GetReplicationForIndex(NetFlags);
				if (Replication != TEXT("Local"))
				{
					AddUniqueSorted(Entry.RPCs, EventName + TEXT(" [") + Replication + ((NetFlags & FUNC_NetReliable) ? TEXT(", Reliable]") : TEXT("]")));
				}
			}

			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				AddUniqueSorted(Entry.Functions, EntryNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			}

			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (UFunction* Function = CallNode->GetTargetFunction())
				{
					const FString OwnerName = Function->GetOwnerClass() ? Function->GetOwnerClass()->GetName() : FString();
					AddUniqueSorted(Entry.Calls, OwnerName.IsEmpty() ? Function->GetName() : OwnerName + TEXT("::") + Function->GetName());
				}
			}

			if (UK2Node_ConstructObjectFromClass* ConstructNode = Cast<UK2Node_ConstructObjectFromClass>(Node))
			{
				AddPinReferenceForIndex(
					Entry.AssetReferences,
					Cast<UK2Node_SpawnActorFromClass>(Node) ? TEXT("SpawnActorClass") : TEXT("ConstructObjectClass"),
					ConstructNode->GetClassPin()
				);
			}
			else if (UK2Node_SpawnActor* SpawnActorNode = Cast<UK2Node_SpawnActor>(Node))
			{
				AddPinReferenceForIndex(Entry.AssetReferences, TEXT("SpawnActorBlueprint"), SpawnActorNode->GetBlueprintPin());
			}

			if (UK2Node_GetDataTableRow* GetRowNode = Cast<UK2Node_GetDataTableRow>(Node))
			{
				const FString DataTable = DescribeDataTablePinForIndex(GetRowNode->GetDataTablePin());
				const FString RowName = DescribeRowNamePinForIndex(GetRowNode->GetRowNamePin());
				const UScriptStruct* RowStruct = GetRowNode->GetDataTableRowStructType();
				AddUniqueSorted(Entry.DataTables, DataTable + TEXT(" Row=") + RowName + TEXT(" Struct=") + GetObjectPathForIndex(RowStruct));
				AddUniqueSorted(Entry.Structs, GetObjectPathForIndex(RowStruct));
			}
			else if (UK2Node_CallDataTableFunction* DataTableFunctionNode = Cast<UK2Node_CallDataTableFunction>(Node))
			{
				if (UFunction* Function = DataTableFunctionNode->GetTargetFunction())
				{
					const FString DataTablePinName = Function->GetMetaData(FBlueprintMetadata::MD_DataTablePin);
					const UEdGraphPin* DataTablePin = DataTablePinName.IsEmpty() ? nullptr : DataTableFunctionNode->FindPin(FName(*DataTablePinName));
					const UEdGraphPin* RowNamePin = DataTableFunctionNode->FindPin(TEXT("RowName"));
					AddUniqueSorted(Entry.DataTables, DescribeDataTablePinForIndex(DataTablePin) + TEXT(" Row=") + DescribeRowNamePinForIndex(RowNamePin));
				}
			}

			if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node))
			{
				AddUniqueSorted(Entry.Structs, GetObjectPathForIndex(StructNode->StructType));
			}

			if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
			{
				const FString VarName = VariableNode->GetVarNameString();
				if (Cast<UK2Node_VariableSet>(Node))
				{
					AddUniqueSorted(Entry.VariableWrites, VarName);
				}
				else if (Cast<UK2Node_VariableGet>(Node))
				{
					AddUniqueSorted(Entry.VariableReads, VarName);
				}
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin)
				{
					AddStructFromPinForIndex(Entry.Structs, Pin->PinType);
					if (Pin->Direction == EGPD_Input)
					{
						AddPinReferenceForIndex(Entry.AssetReferences, TEXT("InputAsset"), Pin);
					}
				}
			}
		}
	};

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		VisitGraph(Graph);
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		VisitGraph(Graph);
	}
}

static void CollectWorldIndexDetails(UWorld* World, FExportIndexEntry& Entry)
{
	if (!World)
	{
		return;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings(false, false))
	{
		AddObjectReferenceForIndex(Entry.AssetReferences, TEXT("GameModeOverride"), WorldSettings->DefaultGameMode);
	}

	if (ULevel* PersistentLevel = World->PersistentLevel)
	{
		for (AActor* Actor : PersistentLevel->Actors)
		{
			if (!Actor || Actor->IsA<AWorldSettings>())
			{
				continue;
			}

			if (const FString BlueprintSource = GetBlueprintSourcePathFromClass(Actor->GetClass()); !BlueprintSource.IsEmpty())
			{
				AddUniqueSorted(Entry.AssetReferences, TEXT("PlacedActorBlueprint: ") + BlueprintSource);
			}
		}
	}
}

static void CollectInputActionIndexDetails(UInputAction* InputAction, FExportIndexEntry& Entry)
{
	if (!InputAction)
	{
		return;
	}

	AddUniqueSorted(Entry.Events, TEXT("InputAction ") + InputAction->GetName() + TEXT(" [") + GetEnumValueName(InputAction->ValueType) + TEXT("]"));
}

static void CollectInputMappingContextIndexDetails(UInputMappingContext* MappingContext, FExportIndexEntry& Entry)
{
	if (!MappingContext)
	{
		return;
	}

	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action)
		{
			AddObjectReferenceForIndex(Entry.AssetReferences, TEXT("InputAction"), Mapping.Action.Get());
			AddUniqueSorted(Entry.Events, Mapping.Key.GetFName().ToString() + TEXT(" -> ") + Mapping.Action->GetName());
		}
	}
}

static void WriteStringArrayJson(TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer, const TCHAR* FieldName, const TArray<FString>& Values)
{
	Writer->WriteArrayStart(FieldName);
	for (const FString& Value : Values)
	{
		Writer->WriteValue(Value);
	}
	Writer->WriteArrayEnd();
}

static FString GetExportIndexDisplayName(const FExportIndexEntry& Entry)
{
	if (!Entry.PackagePath.IsEmpty())
	{
		return Entry.Name + TEXT(" (") + Entry.PackagePath + TEXT(")");
	}
	return Entry.Name;
}

static void WriteExportIndexJson(const TArray<FExportIndexEntry>& Entries)
{
	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), 1);
	Writer->WriteValue(TEXT("generatedBy"), TEXT("GetTheMeaning"));
	Writer->WriteValue(TEXT("assetCount"), Entries.Num());
	Writer->WriteArrayStart(TEXT("projectLevelFiles"));
	Writer->WriteValue(TEXT("ProjectConfig.md"));
	Writer->WriteValue(TEXT("ProjectConfig.json"));
	Writer->WriteValue(TEXT("CppSourceIndex.md"));
	Writer->WriteValue(TEXT("CppSourceIndex.json"));
	Writer->WriteValue(TEXT("ExportGraph.json"));
	Writer->WriteArrayEnd();
	Writer->WriteArrayStart(TEXT("assets"));
	for (const FExportIndexEntry& Entry : Entries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Entry.Name);
		Writer->WriteValue(TEXT("displayName"), GetExportIndexDisplayName(Entry));
		Writer->WriteValue(TEXT("type"), Entry.Type);
		Writer->WriteValue(TEXT("assetPath"), Entry.AssetPath);
		Writer->WriteValue(TEXT("packagePath"), Entry.PackagePath);
		Writer->WriteValue(TEXT("parentClass"), Entry.ParentClass);
		Writer->WriteValue(TEXT("readablePath"), Entry.ReadablePath);
		Writer->WriteValue(TEXT("logicJsonPath"), Entry.LogicJsonPath);
		WriteStringArrayJson(Writer, TEXT("variables"), Entry.Variables);
		WriteStringArrayJson(Writer, TEXT("events"), Entry.Events);
		WriteStringArrayJson(Writer, TEXT("rpcs"), Entry.RPCs);
		WriteStringArrayJson(Writer, TEXT("functions"), Entry.Functions);
		WriteStringArrayJson(Writer, TEXT("calls"), Entry.Calls);
		WriteStringArrayJson(Writer, TEXT("variableReads"), Entry.VariableReads);
		WriteStringArrayJson(Writer, TEXT("variableWrites"), Entry.VariableWrites);
		WriteStringArrayJson(Writer, TEXT("dataTables"), Entry.DataTables);
		WriteStringArrayJson(Writer, TEXT("structs"), Entry.Structs);
		WriteStringArrayJson(Writer, TEXT("assetReferences"), Entry.AssetReferences);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	SaveText(Json, GetExportRootDir(), TEXT("ExportIndex"), TEXT(".json"));
}

static void AppendMarkdownList(FString& Out, const FString& Heading, const TArray<FString>& Values)
{
	if (Values.Num() == 0)
	{
		return;
	}

	Out += TEXT("\n") + Heading + TEXT(":\n");
	const int32 MaxItems = 30;
	for (int32 Index = 0; Index < Values.Num() && Index < MaxItems; ++Index)
	{
		Out += TEXT("- ") + Values[Index] + TEXT("\n");
	}
	if (Values.Num() > MaxItems)
	{
		Out += FString::Printf(TEXT("- ... %d more\n"), Values.Num() - MaxItems);
	}
}

static void WriteExportIndexMarkdown(const TArray<FExportIndexEntry>& Entries)
{
	FString Md;
	Md += TEXT("# GetTheMeaning Export Index\n\n");
	Md += FString::Printf(TEXT("- Assets: %d\n"), Entries.Num());
	Md += TEXT("- Root: Saved/GetTheMeaningExports\n\n");
	Md += TEXT("Project-level files:\n");
	Md += TEXT("- ProjectConfig.md / ProjectConfig.json\n");
	Md += TEXT("- CppSourceIndex.md / CppSourceIndex.json\n");
	Md += TEXT("- ExportGraph.json\n\n");

	for (const FExportIndexEntry& Entry : Entries)
	{
		Md += TEXT("---\n\n");
		Md += TEXT("## ") + GetExportIndexDisplayName(Entry) + TEXT("\n\n");
		Md += TEXT("- Type: ") + Entry.Type + TEXT("\n");
		if (!Entry.PackagePath.IsEmpty())
		{
			Md += TEXT("- PackagePath: ") + Entry.PackagePath + TEXT("\n");
		}
		Md += TEXT("- AssetPath: ") + Entry.AssetPath + TEXT("\n");
		if (!Entry.ParentClass.IsEmpty())
		{
			Md += TEXT("- ParentClass: ") + Entry.ParentClass + TEXT("\n");
		}
		if (!Entry.ReadablePath.IsEmpty())
		{
			Md += TEXT("- Readable: ") + Entry.ReadablePath + TEXT("\n");
		}
		if (!Entry.LogicJsonPath.IsEmpty())
		{
			Md += TEXT("- LogicJson: ") + Entry.LogicJsonPath + TEXT("\n");
		}

		AppendMarkdownList(Md, TEXT("RPCs"), Entry.RPCs);
		AppendMarkdownList(Md, TEXT("Events"), Entry.Events);
		AppendMarkdownList(Md, TEXT("Functions"), Entry.Functions);
		AppendMarkdownList(Md, TEXT("Variables"), Entry.Variables);
		AppendMarkdownList(Md, TEXT("Calls"), Entry.Calls);
		AppendMarkdownList(Md, TEXT("Variable Reads"), Entry.VariableReads);
		AppendMarkdownList(Md, TEXT("Variable Writes"), Entry.VariableWrites);
		AppendMarkdownList(Md, TEXT("Data Tables"), Entry.DataTables);
		AppendMarkdownList(Md, TEXT("Structs"), Entry.Structs);
		AppendMarkdownList(Md, TEXT("Asset References"), Entry.AssetReferences);
		Md += TEXT("\n");
	}

	SaveText(Md, GetExportRootDir(), TEXT("ExportIndex"), TEXT(".md"));
}

struct FExportGraphNode
{
	FString Id;
	FString Name;
	FString Type;
	FString PackageName;
	bool bExported = false;
	FString ParentClass;
	FString ReadablePath;
	FString LogicJsonPath;
};

struct FExportGraphEdge
{
	FString Source;
	FString Target;
	FString Purpose;
	FString SourceKind;
	FString Category;
	FString Properties;
	FString RawReference;
};

static FString GetGraphNameFromPath(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return FString();
	}

	int32 DotIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), DotIndex) && DotIndex + 1 < Path.Len())
	{
		return Path.Mid(DotIndex + 1);
	}

	int32 SlashIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('/'), SlashIndex) && SlashIndex + 1 < Path.Len())
	{
		return Path.Mid(SlashIndex + 1);
	}

	return Path;
}

static bool ParseAssetReferenceForGraph(const FString& Reference, FString& OutPurpose, FString& OutTarget)
{
	const int32 SeparatorIndex = Reference.Find(TEXT(": "));
	if (SeparatorIndex == INDEX_NONE)
	{
		return false;
	}

	OutPurpose = Reference.Left(SeparatorIndex).TrimStartAndEnd();
	OutTarget = Reference.Mid(SeparatorIndex + 2).TrimStartAndEnd();
	return !OutPurpose.IsEmpty() && !OutTarget.IsEmpty();
}

static FString GetPackageNameFromGraphId(const FString& Id)
{
	FString PackageName = Id;
	const int32 ValueSeparatorIndex = PackageName.Find(TEXT("::"));
	if (ValueSeparatorIndex != INDEX_NONE)
	{
		PackageName = PackageName.Left(ValueSeparatorIndex);
	}

	int32 DotIndex = INDEX_NONE;
	if (PackageName.FindChar(TEXT('.'), DotIndex))
	{
		PackageName = PackageName.Left(DotIndex);
	}
	return PackageName;
}

static void AddGraphNode(TMap<FString, FExportGraphNode>& Nodes, const FExportGraphNode& Node)
{
	if (Node.Id.IsEmpty())
	{
		return;
	}

	if (FExportGraphNode* Existing = Nodes.Find(Node.Id))
	{
		if (Node.bExported)
		{
			*Existing = Node;
		}
		return;
	}

	Nodes.Add(Node.Id, Node);
}

static FString DependencyCategoryToString(UE::AssetRegistry::EDependencyCategory Category)
{
	using UE::AssetRegistry::EDependencyCategory;

	TArray<FString> Parts;
	if (EnumHasAnyFlags(Category, EDependencyCategory::Package))
	{
		Parts.Add(TEXT("Package"));
	}
	if (EnumHasAnyFlags(Category, EDependencyCategory::Manage))
	{
		Parts.Add(TEXT("Manage"));
	}
	if (EnumHasAnyFlags(Category, EDependencyCategory::SearchableName))
	{
		Parts.Add(TEXT("SearchableName"));
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT("|")) : TEXT("None");
}

static FString DependencyPropertiesToString(UE::AssetRegistry::EDependencyProperty Properties, UE::AssetRegistry::EDependencyCategory Category)
{
	using UE::AssetRegistry::EDependencyCategory;
	using UE::AssetRegistry::EDependencyProperty;

	TArray<FString> Parts;
	if (EnumHasAnyFlags(Category, EDependencyCategory::Package))
	{
		Parts.Add(EnumHasAnyFlags(Properties, EDependencyProperty::Hard) ? TEXT("Hard") : TEXT("Soft"));
		Parts.Add(EnumHasAnyFlags(Properties, EDependencyProperty::Game) ? TEXT("Game") : TEXT("EditorOnly"));
		Parts.Add(EnumHasAnyFlags(Properties, EDependencyProperty::Build) ? TEXT("Build") : TEXT("NotBuild"));
	}
	if (EnumHasAnyFlags(Category, EDependencyCategory::Manage))
	{
		Parts.Add(EnumHasAnyFlags(Properties, EDependencyProperty::Direct) ? TEXT("Direct") : TEXT("Indirect"));
	}
	if (EnumHasAnyFlags(Category, EDependencyCategory::SearchableName))
	{
		Parts.Add(TEXT("SearchableName"));
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT("|")) : TEXT("None");
}

static void AddGraphEdge(TArray<FExportGraphEdge>& Edges, TSet<FString>& EdgeKeys, const FExportGraphEdge& Edge)
{
	if (Edge.Source.IsEmpty() || Edge.Target.IsEmpty())
	{
		return;
	}

	const FString EdgeKey = Edge.Source + TEXT("|") + Edge.Target + TEXT("|") + Edge.Purpose + TEXT("|") + Edge.SourceKind + TEXT("|") + Edge.Category + TEXT("|") + Edge.Properties;
	if (EdgeKeys.Contains(EdgeKey))
	{
		return;
	}

	EdgeKeys.Add(EdgeKey);
	Edges.Add(Edge);
}

static FExportGraphNode MakeExternalGraphNode(const FString& Id, const FString& Type)
{
	FExportGraphNode Node;
	Node.Id = Id;
	Node.Name = GetGraphNameFromPath(Id);
	Node.Type = Type;
	Node.PackageName = GetPackageNameFromGraphId(Id);
	Node.bExported = false;
	return Node;
}

static void AddAssetRegistryDependenciesToGraph(const FExportIndexEntry& Entry, TMap<FString, FExportGraphNode>& Nodes, TArray<FExportGraphEdge>& Edges, TSet<FString>& EdgeKeys)
{
	if (Entry.AssetPath.IsEmpty())
	{
		return;
	}

	FString SourcePackage = Entry.AssetPath;
	int32 DotIndex = INDEX_NONE;
	if (SourcePackage.FindChar(TEXT('.'), DotIndex))
	{
		SourcePackage = SourcePackage.Left(DotIndex);
	}
	if (SourcePackage.IsEmpty())
	{
		return;
	}

	AddGraphNode(Nodes, MakeExternalGraphNode(SourcePackage, TEXT("Package")));

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetDependency> Dependencies;
	if (!AssetRegistry.GetDependencies(FAssetIdentifier(FName(*SourcePackage)), Dependencies, UE::AssetRegistry::EDependencyCategory::Package))
	{
		return;
	}

	for (const FAssetDependency& Dependency : Dependencies)
	{
		const FString Target = Dependency.AssetId.ToString();
		if (Target.IsEmpty())
		{
			continue;
		}

		AddGraphNode(Nodes, MakeExternalGraphNode(Target, TEXT("Package")));

		FExportGraphEdge Edge;
		Edge.Source = SourcePackage;
		Edge.Target = Target;
		Edge.Purpose = TEXT("PackageDependency");
		Edge.SourceKind = TEXT("AssetRegistry");
		Edge.Category = DependencyCategoryToString(Dependency.Category);
		Edge.Properties = DependencyPropertiesToString(Dependency.Properties, Dependency.Category);
		Edge.RawReference = Target;
		AddGraphEdge(Edges, EdgeKeys, Edge);
	}
}

static void WriteExportGraphJson(const TArray<FExportIndexEntry>& Entries)
{
	TMap<FString, FExportGraphNode> Nodes;
	TArray<FExportGraphEdge> Edges;
	TSet<FString> EdgeKeys;

	for (const FExportIndexEntry& Entry : Entries)
	{
		FExportGraphNode Node;
		Node.Id = Entry.AssetPath;
		Node.Name = Entry.Name;
		Node.Type = Entry.Type;
		Node.PackageName = GetPackageNameFromGraphId(Entry.AssetPath);
		Node.bExported = true;
		Node.ParentClass = Entry.ParentClass;
		Node.ReadablePath = Entry.ReadablePath;
		Node.LogicJsonPath = Entry.LogicJsonPath;
		AddGraphNode(Nodes, Node);

		for (const FString& Reference : Entry.AssetReferences)
		{
			FString Purpose;
			FString Target;
			if (!ParseAssetReferenceForGraph(Reference, Purpose, Target))
			{
				continue;
			}

			AddGraphNode(Nodes, MakeExternalGraphNode(Target, TEXT("ExternalAsset")));

			FExportGraphEdge Edge;
			Edge.Source = Entry.AssetPath;
			Edge.Target = Target;
			Edge.Purpose = Purpose;
			Edge.SourceKind = TEXT("BlueprintNode");
			Edge.Category = TEXT("NodePin");
			Edge.Properties = TEXT("Explicit");
			Edge.RawReference = Reference;
			AddGraphEdge(Edges, EdgeKeys, Edge);
		}

		AddAssetRegistryDependenciesToGraph(Entry, Nodes, Edges, EdgeKeys);
	}

	TArray<FExportGraphNode> NodeList;
	Nodes.GenerateValueArray(NodeList);
	NodeList.Sort([](const FExportGraphNode& A, const FExportGraphNode& B)
	{
		return A.Id < B.Id;
	});
	Edges.Sort([](const FExportGraphEdge& A, const FExportGraphEdge& B)
	{
		if (A.Source != B.Source)
		{
			return A.Source < B.Source;
		}
		if (A.Target != B.Target)
		{
			return A.Target < B.Target;
		}
		if (A.Purpose != B.Purpose)
		{
			return A.Purpose < B.Purpose;
		}
		return A.SourceKind < B.SourceKind;
	});

	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), 2);
	Writer->WriteValue(TEXT("generatedBy"), TEXT("GetTheMeaning"));
	Writer->WriteValue(TEXT("exportedAssetCount"), Entries.Num());
	Writer->WriteValue(TEXT("nodeCount"), NodeList.Num());
	Writer->WriteValue(TEXT("edgeCount"), Edges.Num());

	Writer->WriteArrayStart(TEXT("nodes"));
	for (const FExportGraphNode& Node : NodeList)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("id"), Node.Id);
		Writer->WriteValue(TEXT("name"), Node.Name);
		Writer->WriteValue(TEXT("type"), Node.Type);
		Writer->WriteValue(TEXT("packageName"), Node.PackageName);
		Writer->WriteValue(TEXT("exported"), Node.bExported);
		Writer->WriteValue(TEXT("parentClass"), Node.ParentClass);
		Writer->WriteValue(TEXT("readablePath"), Node.ReadablePath);
		Writer->WriteValue(TEXT("logicJsonPath"), Node.LogicJsonPath);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("edges"));
	for (const FExportGraphEdge& Edge : Edges)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("source"), Edge.Source);
		Writer->WriteValue(TEXT("target"), Edge.Target);
		Writer->WriteValue(TEXT("purpose"), Edge.Purpose);
		Writer->WriteValue(TEXT("sourceKind"), Edge.SourceKind);
		Writer->WriteValue(TEXT("category"), Edge.Category);
		Writer->WriteValue(TEXT("properties"), Edge.Properties);
		Writer->WriteValue(TEXT("rawReference"), Edge.RawReference);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	Writer->Close();

	SaveText(Json, GetExportRootDir(), TEXT("ExportGraph"), TEXT(".json"));
}

static void WriteExportIndexFiles(const TArray<FExportIndexEntry>& Entries)
{
	if (Entries.Num() == 0)
	{
		return;
	}
	WriteExportIndexJson(Entries);
	WriteExportIndexMarkdown(Entries);
	WriteExportGraphJson(Entries);
}

// 批量导出：从一组 FAssetData 路由到对应导出器，返回结果与可打开路径
struct FBatchExportResult
{
	int32 SuccessCount = 0;
	int32 FailedCount = 0;
	int32 SkippedCount = 0;
	FString FirstSavedPath;
	TArray<FExportIndexEntry> IndexEntries;
};

static FBatchExportResult ExportAssetDataList(const TArray<FAssetData>& AssetList)
{
	FBatchExportResult R;
	for (const FAssetData& AssetData : AssetList)
	{
		FString SavedPath;
		const GetTheMeaningExportImpl::EExportResult Result = GetTheMeaningExportImpl::ExportOneAsset(AssetData, SavedPath);
		switch (Result)
		{
		case GetTheMeaningExportImpl::EExportResult::Success:
		{
			++R.SuccessCount;
			if (R.FirstSavedPath.IsEmpty()) R.FirstSavedPath = SavedPath;

			FExportIndexEntry Entry;
			Entry.Name = AssetData.AssetName.ToString();
			Entry.AssetPath = AssetData.GetSoftObjectPath().ToString();
			Entry.PackagePath = AssetData.PackagePath.ToString();
			Entry.ReadablePath = GetRelativeExportPath(SavedPath);

			if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("Blueprint");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (Blueprint->GetName() + TEXT("_ReadableCode.txt")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (Blueprint->GetName() + TEXT("_Logic.json")));
				CollectBlueprintIndexDetails(Blueprint, Entry);
			}
			else if (AssetData.IsInstanceOf(UMaterialFunctionInterface::StaticClass()))
			{
				Entry.Type = TEXT("MaterialFunction");
			}
			else if (AssetData.IsInstanceOf(UMaterialInterface::StaticClass()))
			{
				Entry.Type = TEXT("Material");
			}
			else if (UDataTable* DataTable = Cast<UDataTable>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("DataTable");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (DataTable->GetName() + TEXT("_ReadableTable.md")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (DataTable->GetName() + TEXT("_Table.json")));
				AddUniqueSorted(Entry.DataTables, Entry.AssetPath);
				if (DataTable->GetRowStruct())
				{
					AddUniqueSorted(Entry.Structs, GetObjectPathForIndex(DataTable->GetRowStruct()));
				}
			}
			else if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("UserDefinedStruct");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (Struct->GetName() + TEXT("_ReadableStruct.md")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (Struct->GetName() + TEXT("_Struct.json")));
				AddUniqueSorted(Entry.Structs, Entry.AssetPath);
			}
			else if (UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("UserDefinedEnum");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (Enum->GetName() + TEXT("_ReadableEnum.md")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (Enum->GetName() + TEXT("_Enum.json")));
			}
			else if (UWorld* World = Cast<UWorld>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("World");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (World->GetName() + TEXT("_ReadableLevelSummary.md")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (World->GetName() + TEXT("_LevelSummary.json")));
				CollectWorldIndexDetails(World, Entry);
			}
			else if (UInputAction* InputAction = Cast<UInputAction>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("InputAction");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (InputAction->GetName() + TEXT("_ReadableInputAction.md")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (InputAction->GetName() + TEXT("_InputAction.json")));
				CollectInputActionIndexDetails(InputAction, Entry);
			}
			else if (UInputMappingContext* MappingContext = Cast<UInputMappingContext>(AssetData.GetAsset()))
			{
				Entry.Type = TEXT("InputMappingContext");
				const FString ExportDir = GetAssetExportDirOrRoot(AssetData);
				Entry.ReadablePath = GetRelativeExportPath(ExportDir / (MappingContext->GetName() + TEXT("_ReadableInputMappingContext.md")));
				Entry.LogicJsonPath = GetRelativeExportPath(ExportDir / (MappingContext->GetName() + TEXT("_InputMappingContext.json")));
				CollectInputMappingContextIndexDetails(MappingContext, Entry);
			}
			else
			{
				Entry.Type = AssetData.AssetClassPath.ToString();
			}
			R.IndexEntries.Add(MoveTemp(Entry));
			break;
		}
		case GetTheMeaningExportImpl::EExportResult::Failed:
			++R.FailedCount;
			break;
		case GetTheMeaningExportImpl::EExportResult::Skipped:
		default:
			++R.SkippedCount;
			break;
		}
	}
	WriteExportIndexFiles(R.IndexEntries);
	if (R.SuccessCount > 0)
	{
		GetTheMeaningExportImpl::WriteProjectConfigFiles();
		GetTheMeaningExportImpl::WriteCppSourceIndexFiles();
	}
	return R;
}

static void NotifyBatchResult(const FBatchExportResult& R)
{
	const FString RootDir = GetExportRootDir();
	const FString ClipboardPath = R.SuccessCount == 1 && !R.FirstSavedPath.IsEmpty() ? R.FirstSavedPath : RootDir;
	if (R.SuccessCount > 0)
	{
		CopyExportPathToClipboard(ClipboardPath);
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("BatchExportSummary", "AI 可读文档导出完成\n成功 {0}，失败 {1}，跳过 {2}\n路径已复制到剪贴板"),
		FText::AsNumber(R.SuccessCount),
		FText::AsNumber(R.FailedCount),
		FText::AsNumber(R.SkippedCount)
	));
	Info.ExpireDuration = (R.FailedCount > 0) ? 8.0f : 5.0f;
	Info.Hyperlink = FSimpleDelegate::CreateLambda([]() { OpenExportRootDir(); });
	Info.HyperlinkText = LOCTEXT("OpenExportDir", "打开导出目录");
	FSlateNotificationManager::Get().AddNotification(Info);
}

static bool ConfirmFolderBatchExport(const TArray<FString>& SelectedPackagePaths, int32 AssetCount)
{
	FString PathSummary;
	const int32 MaxShownPaths = 5;
	for (int32 Index = 0; Index < SelectedPackagePaths.Num() && Index < MaxShownPaths; ++Index)
	{
		PathSummary += TEXT("\n- ") + SelectedPackagePaths[Index];
	}
	if (SelectedPackagePaths.Num() > MaxShownPaths)
	{
		PathSummary += FString::Printf(TEXT("\n- ... 另有 %d 个目录"), SelectedPackagePaths.Num() - MaxShownPaths);
	}

	const FText Message = FText::Format(
		LOCTEXT(
			"ConfirmFolderBatchExport",
			"即将从选中文件夹递归导出 {0} 个支持的资产。\n\n导出会加载这些蓝图/材质/数据表/结构体/枚举/关卡/Enhanced Input，刷新项目 C++ 源码索引，并写入 Saved/GetTheMeaningExports 下对应的项目目录：{1}\n\n是否继续？"
		),
		FText::AsNumber(AssetCount),
		FText::FromString(PathSummary)
	);

	return FMessageDialog::Open(EAppMsgType::YesNo, Message, LOCTEXT("ConfirmFolderBatchExportTitle", "GetTheMeaning 批量导出确认")) == EAppReturnType::Yes;
}

// 批量导出（内容浏览器入口）：遍历所有选中资产，按类型路由到对应导出器
static void ExportAllSelectedToAIDocs()
{
	if (!PromptSaveDirtyPackagesBeforeExport())
	{
		return;
	}

	const TArray<FAssetData> SelectedAssets = FilterSupportedAssets(GetSelectedAssetsFromContentBrowser());
	if (SelectedAssets.Num() == 0)
	{
		NotifyEmpty(LOCTEXT("NoAssetSelected", "请先在内容浏览器中选中若干支持资产（蓝图/材质/数据表/结构体/枚举/关卡/Enhanced Input）。"));
		return;
	}

	const FBatchExportResult R = ExportAssetDataList(SelectedAssets);
	NotifyBatchResult(R);
}

static void ExportFoldersToAIDocs(TArray<FString> SelectedPackagePaths)
{
	if (!PromptSaveDirtyPackagesBeforeExport())
	{
		return;
	}

	const TArray<FAssetData> AssetList = GetSupportedAssetsInFolders(SelectedPackagePaths);
	if (AssetList.Num() == 0)
	{
		NotifyEmpty(LOCTEXT("NoSupportedAssetInFolders", "选中文件夹中没有找到可导出的蓝图、材质、数据表、结构体、枚举、关卡或 Enhanced Input 资产。"));
		return;
	}

	if (!ConfirmFolderBatchExport(SelectedPackagePaths, AssetList.Num()))
	{
		return;
	}

	const FBatchExportResult R = ExportAssetDataList(AssetList);
	NotifyBatchResult(R);
}

// 批量导出（引用查看器入口）：从 UGraphNodeContextMenuContext 拿到选中的 Reference 节点 → FAssetData → 路由
static void ExportReferenceViewerSelectionToAIDocs(const UEdGraph* OwnerGraph)
{
	if (!PromptSaveDirtyPackagesBeforeExport())
	{
		return;
	}

	if (!OwnerGraph)
	{
		NotifyEmpty(LOCTEXT("NoRVGraph", "未能获取引用查看器图表。"));
		return;
	}

	TSharedPtr<SGraphEditor> GraphEditor = SGraphEditor::FindGraphEditorForGraph(OwnerGraph);
	if (!GraphEditor.IsValid())
	{
		NotifyEmpty(LOCTEXT("NoRVGraphEditor", "未能找到引用查看器窗口。"));
		return;
	}

	TArray<FAssetData> AssetList;
	const TSet<UObject*>& SelectedNodes = GraphEditor->GetSelectedNodes();
	for (UObject* SelectedObject : SelectedNodes)
	{
		UEdGraphNode_Reference* RefNode = Cast<UEdGraphNode_Reference>(SelectedObject);
		if (!RefNode) continue;
		if (RefNode->IsCollapsed()) continue;

		const FAssetData& AssetData = RefNode->GetAssetData();
		if (AssetData.IsValid())
		{
			AssetList.Add(AssetData);
		}
	}

	if (AssetList.Num() == 0)
	{
		NotifyEmpty(LOCTEXT("NoRVAssetSelected", "请先在引用查看器中框选支持资产节点（蓝图/材质/数据表/结构体/枚举/关卡/Enhanced Input）。"));
		return;
	}

	const FBatchExportResult R = ExportAssetDataList(AssetList);
	NotifyBatchResult(R);
}

static void ExportCppSourceIndexOnly()
{
	if (!PromptSaveDirtyPackagesBeforeExport())
	{
		return;
	}

	GetTheMeaningExportImpl::WriteCppSourceIndexFiles();

	const FString RootDir = GetExportRootDir();
	CopyExportPathToClipboard(RootDir);

	FNotificationInfo Info(LOCTEXT("CppSourceIndexExported", "C++ 源码索引已刷新\n路径已复制到剪贴板"));
	Info.ExpireDuration = 5.0f;
	Info.Hyperlink = FSimpleDelegate::CreateLambda([]() { OpenExportRootDir(); });
	Info.HyperlinkText = LOCTEXT("OpenExportDirForCppSourceIndex", "打开导出目录");
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FGetTheMeaningModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGetTheMeaningModule::RegisterMenus));
}

void FGetTheMeaningModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FGetTheMeaningModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Window 菜单：唯一入口「导出所有选中为 AI 可读文档」
	if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
	{
		FToolMenuSection& Section = WindowMenu->FindOrAddSection("GetTheMeaning");
		Section.Label = LOCTEXT("GetTheMeaningMenu", "GetTheMeaning");

		Section.AddMenuEntry(
			"ExportAllSelectedToAIDocs",
			LOCTEXT("ExportAllSelectedToAIDocs", "导出所有选中为 AI 可读文档"),
			LOCTEXT("ExportAllSelectedToAIDocsTooltip", "遍历内容浏览器中所有选中的支持资产：蓝图、材质、数据表、结构体、枚举、关卡、Enhanced Input；同时刷新项目 C++ 源码索引，输出到 Saved/GetTheMeaningExports。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&ExportAllSelectedToAIDocs))
		);

		Section.AddMenuEntry(
			"ExportCppSourceIndexOnly",
			LOCTEXT("ExportCppSourceIndexOnly", "刷新 C++ 源码索引"),
			LOCTEXT("ExportCppSourceIndexOnlyTooltip", "扫描项目 Source 和项目插件 Source 中的 UCLASS/USTRUCT/UENUM/UPROPERTY/UFUNCTION，输出 CppSourceIndex.md/json。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&ExportCppSourceIndexOnly))
		);
	}

	// 内容浏览器右键：唯一入口「导出所有选中为 AI 可读文档」
	if (UToolMenu* AssetContextMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
	{
		FToolMenuSection& Section = AssetContextMenu->FindOrAddSection("GetTheMeaningAssetActions");
		Section.Label = LOCTEXT("GetTheMeaningSection", "GetTheMeaning");

		// 选中资产中至少有一个支持类型才显示
		Section.AddDynamicEntry(
			"ExportAllSelectedToAIDocsContext",
			FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context) return;

				bool bHasSupported = false;
				for (const FAssetData& AssetData : Context->SelectedAssets)
				{
					if (IsSupportedAssetData(AssetData))
					{
						bHasSupported = true;
						break;
					}
				}
				if (!bHasSupported) return;

				InSection.AddMenuEntry(
					"ExportAllSelectedToAIDocs",
					LOCTEXT("ExportAllSelectedToAIDocs_CB", "导出所有选中为 AI 可读文档"),
					LOCTEXT("ExportAllSelectedToAIDocsCtxTooltip", "遍历所有选中的支持资产：蓝图、材质、数据表、结构体、枚举、关卡、Enhanced Input；同时刷新项目 C++ 源码索引，输出到 Saved/GetTheMeaningExports。"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&ExportAllSelectedToAIDocs))
				);
			})
		);
	}

	// 内容浏览器文件夹右键：递归收集文件夹中的支持资产，确认后批量导出
	if (UToolMenu* FolderContextMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
	{
		FolderContextMenu->AddDynamicSection(
			"GetTheMeaningFolderExport",
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				if (!InMenu) return;

				UContentBrowserFolderContext* Context = InMenu->FindContext<UContentBrowserFolderContext>();
				if (!Context || Context->SelectedPackagePaths.Num() == 0) return;

				const TArray<FAssetData> AssetList = GetSupportedAssetsInFolders(Context->SelectedPackagePaths);
				if (AssetList.Num() == 0) return;

				FToolMenuSection& Section = InMenu->AddSection(
					"GetTheMeaningFolderActions",
					LOCTEXT("GetTheMeaningFolderSection", "GetTheMeaning")
				);

				const TArray<FString> SelectedPackagePaths = Context->SelectedPackagePaths;
				Section.AddMenuEntry(
					"ExportFolderToAIDocs",
					LOCTEXT("ExportFolderToAIDocs", "递归导出文件夹为 AI 可读文档..."),
					FText::Format(
						LOCTEXT("ExportFolderToAIDocsTooltip", "递归导出选中文件夹中的 {0} 个支持资产；同时刷新项目 C++ 源码索引，输出到 Saved/GetTheMeaningExports 下对应项目目录。"),
						FText::AsNumber(AssetList.Num())
					),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([SelectedPackagePaths]()
					{
						ExportFoldersToAIDocs(SelectedPackagePaths);
					}))
				);
			})
		);
	}

	// 引用查看器（Reference Viewer）：在图"节点"右键菜单里增加批量导出。
	//
	// 关键根因：SGraphEditorImpl::GenerateContextMenu 里有
	//     if (Context->Node) MenuName = GetNodeContextMenuName(Context->Node->GetClass());
	//     else               MenuName = Schema->GetContextMenuName();
	// 用户实际操作是在节点上右键，所以菜单名是
	//     "GraphEditor.GraphNodeContextMenu.EdGraphNode_Reference"
	// 而不是 Schema 菜单 "GraphEditor.GraphContextMenu.ReferenceViewerSchema"
	// （后者只在图空白处右键时生效，导致我们之前的 DynamicSection 根本不被执行）。
	//
	// ExtendMenu 返回占位 UToolMenu（bRegistered=false）；等引擎第一次在节点上右键时，
	// SGraphEditorImpl::RegisterContextMenu 会以正确 Parent 注册它，我们这里的 DynamicSection
	// 会在后续每次生成菜单时执行。
	auto AddRVExportDynamicSection = [](UToolMenu* InMenu)
	{
		if (!InMenu) return;

		UGraphNodeContextMenuContext* Context = InMenu->FindContext<UGraphNodeContextMenuContext>();
		if (!Context || !Context->Graph) return;

		// 双保险：仅对引用查看器的图生效
		if (!Context->Graph->IsA<UEdGraph_ReferenceViewer>()) return;

		// 找到对应的 SGraphEditor，判断当前选中里是否至少一个可用资产节点
		TSharedPtr<SGraphEditor> GraphEditor = SGraphEditor::FindGraphEditorForGraph(Context->Graph);
		if (!GraphEditor.IsValid()) return;

		bool bHasSupported = false;
		for (UObject* SelectedObject : GraphEditor->GetSelectedNodes())
		{
			UEdGraphNode_Reference* RefNode = Cast<UEdGraphNode_Reference>(SelectedObject);
			if (!RefNode || RefNode->IsCollapsed()) continue;
			const FAssetData& AssetData = RefNode->GetAssetData();
			if (!AssetData.IsValid()) continue;

			if (IsSupportedAssetData(AssetData))
			{
				bHasSupported = true;
				break;
			}
		}
		if (!bHasSupported) return;

		FToolMenuSection& Section = InMenu->AddSection(
			"GetTheMeaningRVSection",
			LOCTEXT("GetTheMeaningRVSectionLabel", "GetTheMeaning")
		);

		TWeakObjectPtr<const UEdGraph> WeakGraph(Context->Graph);
		Section.AddMenuEntry(
			"ExportAllSelectedToAIDocs_RV",
			LOCTEXT("ExportAllSelectedToAIDocs_RV", "导出所有选中为 AI 可读文档"),
			LOCTEXT("ExportAllSelectedToAIDocs_RVTooltip", "遍历引用查看器中所有选中的支持资产节点：蓝图、材质、数据表、结构体、枚举、关卡、Enhanced Input；同时刷新项目 C++ 源码索引，输出到 Saved/GetTheMeaningExports。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([WeakGraph]()
			{
				ExportReferenceViewerSelectionToAIDocs(WeakGraph.Get());
			}))
		);
	};

	// (1) 节点上右键的菜单（用户主要入口）
	if (UToolMenu* RVNodeMenu = UToolMenus::Get()->ExtendMenu("GraphEditor.GraphNodeContextMenu.EdGraphNode_Reference"))
	{
		RVNodeMenu->AddDynamicSection(
			"GetTheMeaningRVExport_Node",
			FNewToolMenuDelegate::CreateLambda(AddRVExportDynamicSection)
		);
	}

	// (2) 图空白处右键的菜单（作为保险入口，仅对 Reference Viewer Schema 生效）
	if (UToolMenu* RVSchemaMenu = UToolMenus::Get()->ExtendMenu("GraphEditor.GraphContextMenu.ReferenceViewerSchema"))
	{
		RVSchemaMenu->AddDynamicSection(
			"GetTheMeaningRVExport_Schema",
			FNewToolMenuDelegate::CreateLambda(AddRVExportDynamicSection)
		);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGetTheMeaningModule, GetTheMeaning)


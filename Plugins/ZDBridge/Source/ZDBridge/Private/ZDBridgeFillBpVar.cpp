// Copyright Epic Games, Inc. All Rights Reserved.
//
// ZDBridgeFillBpVar.cpp —— 把角色蓝图里的存档数据字段值批量读出来。
//
// 为什么用「静态注册 + 引擎初始化完成回调」而不是 UCommandlet：
//   本文件是新增文件，不改 ZDBridge 任何已有文件。UCommandlet 需要 UCLASS 反射
//   （要 UHT 生成 .generated.h + 一个公有头），而 UHT 对本模块私有目录里新加的
//   头文件不保证当次就发现。这里是纯 C++ 静态注册，零反射。
//
// 为什么要逐字段读、而不是整段 ExportText：
//   这批角色蓝图 CDO 里 FItemSaveSingleData::Class（TSubclassOf）是未解析的坏指针，
//   ExportText 内部的 FClassProperty::Identical 会去解引用它，直接
//   EXCEPTION_ACCESS_VIOLATION reading address 0x3a（实测三次）。
//   所以这里只按反射逐个字段取**值**，绕开一切对象指针，Class 由调用方自己填。
//
// 输出：每个角色一段 key = value，直接照抄进蓝图变量的默认值即可。
//
// 用法：
//   UnrealEditor-Cmd.exe <Project>.uproject -unattended -nopause -nosplash -nullrhi \
//       -ZDBpVar [-ZDBpVarLog=<绝对路径，默认 <Project>/Saved/Logs/ZDBpVar.log>] \
//       [-ZDBpVarItems=/Game/ITems/CharItemS] \
//       [-ZDBpVarStruct=/Script/CrossVoidInventory.ItemSaveSingleData]
//
// 跑完自动退出编辑器。

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogZDBpVar, Log, All);

namespace ZDBpVar
{
	/** 同时进 UE 日志和报告文件，方便不开编辑器也能核对。 */
	static void Report(FCriticalSection& Lock, FString& Sink, const FString& Line)
	{
		FScopeLock ScopeLock(&Lock);
		Sink += Line;
		Sink += TEXT("\n");
		UE_LOG(LogZDBpVar, Display, TEXT("%s"), *Line);
	}

	static FString FormatArray(const FArrayProperty* ArrayProperty, const void* Value)
	{
		if (ArrayProperty == nullptr || Value == nullptr)
		{
			return TEXT("()");
		}
		FScriptArrayHelper Helper(ArrayProperty, Value);
		TArray<FString> Parts;
		Parts.Reserve(Helper.Num());

		FProperty* Inner = ArrayProperty->Inner;
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const void* Element = Helper.GetRawPtr(Index);
			if (Element == nullptr || Inner == nullptr)
			{
				Parts.Add(TEXT("?"));
				continue;
			}
			if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Inner))
			{
				Parts.Add(BoolProp->GetPropertyValue(Element) ? TEXT("True") : TEXT("False"));
			}
			else if (const FIntProperty* IntProp = CastField<FIntProperty>(Inner))
			{
				Parts.Add(FString::FromInt(IntProp->GetPropertyValue(Element)));
			}
			else if (const FStrProperty* StrProp = CastField<FStrProperty>(Inner))
			{
				Parts.Add(FString::Printf(TEXT("\"%s\""), *StrProp->GetPropertyValue(Element)));
			}
			else if (const FTextProperty* TextProp = CastField<FTextProperty>(Inner))
			{
				Parts.Add(FString::Printf(TEXT("\"%s\""), *TextProp->GetPropertyValue(Element).ToString()));
			}
			else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Inner))
			{
				Parts.Add(FString::SanitizeFloat(FloatProp->GetPropertyValue(Element)));
			}
			else
			{
				Parts.Add(TEXT("?"));
			}
		}
		return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(",")));
	}

	/** 按反射把一个字段的值转成可读文本。只认标量/字符串/文本/数组，不碰任何对象指针。 */
	static FString FormatPropertyValue(const FProperty* Property, const void* Value)
	{
		if (Value == nullptr)
		{
			return TEXT("<null>");
		}
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			return BoolProp->GetPropertyValue(Value) ? TEXT("True") : TEXT("False");
		}
		if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			return FString::FromInt(IntProp->GetPropertyValue(Value));
		}
		if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			return FString::SanitizeFloat(FloatProp->GetPropertyValue(Value));
		}
		if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			return FString::Printf(TEXT("\"%s\""), *StrProp->GetPropertyValue(Value));
		}
		if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			// 用引擎自己的导出形态：能拿到 NSLOCTEXT("ns","key","text") 或 "字面量"。
			// 贴进变量默认值时，正是这种形态才被认。
			FString Text;
			TextProp->ExportTextItem_Direct(Text, Value, nullptr, nullptr, PPF_None);
			return Text;
		}
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			return FormatArray(ArrayProp, Value);
		}
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			// 只展开 FIntPoint 这类要写进默认值的简单结构。
			if (StructProp->Struct == TBaseStructure<FIntPoint>::Get())
			{
				const FIntPoint* Point = static_cast<const FIntPoint*>(Value);
				return FString::Printf(TEXT("(%d,%d)"), Point->X, Point->Y);
			}
			return FString::Printf(TEXT("<%s>"), *StructProp->Struct->GetName());
		}
		if (CastField<FClassProperty>(Property) != nullptr || CastField<FObjectProperty>(Property) != nullptr)
		{
			// 对象指针一律不解引用：这批数据里就有坏的。
			return TEXT("<对象引用，跳过>");
		}
		return FString::Printf(TEXT("<%s>"), *Property->GetCPPType());
	}

	/**
	 * 统一数值：全角色共用一组（用户指定按原 ALLChar 那条模板的数值）。
	 * 要改统一值就改这里。
	 */
	static const TCHAR* UnifiedStats =
		TEXT("SkillLevel=(5,5,5,5),CharShapeHas=(True,False),")
		TEXT("Health=6000,Attack=800,PhyDefense=600,MagDefense=600,Critical=600,CriticalC=600");

	/** 在结构体（含嵌套子结构体）里按名字找字段，返回字段和它的取值地址。 */
	static bool FindFieldDeep(const UScriptStruct* Struct, const void* StructValue,
		const FName FieldName, const FProperty*& OutProperty, const void*& OutValue)
	{
		if (Struct == nullptr || StructValue == nullptr)
		{
			return false;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const void* Value = Property->ContainerPtrToValuePtr<void>(StructValue);
			if (Property->GetFName() == FieldName && Value != nullptr)
			{
				OutProperty = Property;
				OutValue = Value;
				return true;
			}
			// 下探一层嵌套结构体（角色数据在 ItemData.CharData 里）。
			if (const FStructProperty* Nested = CastField<FStructProperty>(Property))
			{
				if (FindFieldDeep(Nested->Struct, Value, FieldName, OutProperty, OutValue))
				{
					return true;
				}
			}
		}
		return false;
	}

	/** 取字段文本；找不到就把名字记进 Missing 供报告用。 */
	static FString FieldText(const UScriptStruct* Struct, const void* StructValue,
		const FName FieldName, TArray<FString>& Missing)
	{
		const FProperty* Property = nullptr;
		const void* Value = nullptr;
		if (!FindFieldDeep(Struct, StructValue, FieldName, Property, Value))
		{
			Missing.Add(FieldName.ToString());
			return FString();
		}
		return FormatPropertyValue(Property, Value);
	}

	/**
	 * 按出现顺序抽出所有 NSLOCTEXT 的**显示文本**（第三个引号参数）。
	 *
	 * 用途：引擎保存时会重新给 FText 生成命名空间/键（实测把资产的
	 * 7D063492.../08012575... 换成 59528C6F.../7A0D0349...），但显示文本一字不改。
	 * 所以落盘验证要比的是显示文本序列，逐字节比会被这种正常重键误判成失败。
	 */
	static TArray<FString> ExtractNsLocDisplayTexts(const FString& In)
	{
		TArray<FString> Out;
		int32 Cursor = 0;
		while (true)
		{
			const int32 Marker = In.Find(TEXT("NSLOCTEXT("), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (Marker < 0)
			{
				break;
			}
			Cursor = Marker + 10;

			FString Display;
			bool bOk = true;
			for (int32 Part = 0; Part < 3; ++Part)
			{
				// 跳到本段的开引号
				while (Cursor < In.Len() && In[Cursor] != TEXT('"'))
				{
					++Cursor;
				}
				if (Cursor >= In.Len())
				{
					bOk = false;
					break;
				}
				++Cursor; // 越过开引号

				FString Segment;
				bool bEscaped = false;
				bool bClosed = false;
				while (Cursor < In.Len())
				{
					const TCHAR Ch = In[Cursor];
					if (bEscaped)
					{
						Segment.AppendChar(Ch);
						bEscaped = false;
						++Cursor;
						continue;
					}
					if (Ch == TEXT('\\'))
					{
						bEscaped = true;
						++Cursor;
						continue;
					}
					if (Ch == TEXT('"'))
					{
						++Cursor; // 越过闭引号
						bClosed = true;
						break;
					}
					Segment.AppendChar(Ch);
					++Cursor;
				}
				if (!bClosed)
				{
					bOk = false;
					break;
				}
				if (Part == 2)
				{
					Display = Segment;
				}
			}

			if (!bOk)
			{
				break;
			}
			Out.Add(Display);
		}
		return Out;
	}

	/** 数数组文本里的条目数（结构体之间的 "),(" 分隔符）。 */
	static int32 CountEntries(const FString& In)
	{
		int32 Count = 0;
		int32 Pos = 0;
		while ((Pos = In.Find(TEXT("),("), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos)) >= 0)
		{
			++Count;
			Pos += 3;
		}
		return Count + 1;
	}

	static void Run(FCriticalSection& Lock, FString& Sink)
	{
		FString ItemsPath = TEXT("/Game/ITems/CharItemS");
		FString TargetPath = TEXT("/Game/BaseC/Mode/GM_Main");
		FString GraphHint = TEXT("Test_ALLcharGet");
		FString VarName = TEXT("ALLChar");
		const bool bWrite = FParse::Param(FCommandLine::Get(), TEXT("ZDBpVarWrite"));
		const bool bDryRun = FParse::Param(FCommandLine::Get(), TEXT("ZDBpVarDryRun"));
		FParse::Value(FCommandLine::Get(), TEXT("ZDBpVarItems="), ItemsPath);
		FParse::Value(FCommandLine::Get(), TEXT("ZDBpVarTarget="), TargetPath);
		FParse::Value(FCommandLine::Get(), TEXT("ZDBpVarGraph="), GraphHint);
		FParse::Value(FCommandLine::Get(), TEXT("ZDBpVarName="), VarName);

		Report(Lock, Sink, TEXT("==================== ZDBpVar 全角色数值写入 ===================="));
		Report(Lock, Sink, FString::Printf(TEXT("引擎版本    : %s"), *FEngineVersion::Current().ToString()));
		Report(Lock, Sink, FString::Printf(TEXT("角色目录    : %s"), *ItemsPath));
		Report(Lock, Sink, FString::Printf(TEXT("目标变量    : %s.%s.%s"), *TargetPath, *GraphHint, *VarName));
		Report(Lock, Sink, FString::Printf(TEXT("模式        : %s"), bWrite && !bDryRun
			? TEXT("WRITE（会保存资产）") : TEXT("DRY RUN（不写入资产）")));

		// ---------------------------------------------------------- 扫磁盘
		const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		FString NativeItemsPath = ItemsPath;
		if (NativeItemsPath.StartsWith(TEXT("/Game/")))
		{
			NativeItemsPath = ContentDir / NativeItemsPath.RightChop(6);
		}
		NativeItemsPath = FPaths::ConvertRelativePathToFull(NativeItemsPath);

		TArray<FString> AssetFiles;
		IFileManager::Get().FindFilesRecursive(
			AssetFiles, *NativeItemsPath, TEXT("*.uasset"), /*Files=*/true, /*Directories=*/false);
		AssetFiles.Sort();
		Report(Lock, Sink, FString::Printf(TEXT("目录下资产  : %d 个 → %s"), AssetFiles.Num(), *NativeItemsPath));

		// ---------------------------------------------------------- 逐角色拼条目
		TArray<FString> Entries;
		TArray<FString> Skipped;

		for (const FString& AssetFile : AssetFiles)
		{
			const FString AssetLabel = FPaths::GetBaseFilename(AssetFile);

			FString RelativePath = AssetFile;
			FPaths::MakePathRelativeTo(RelativePath, *ContentDir);
			RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			// FullPackagePath 形如 ITems/CharItemS/Item_ALO_Yuki（不带扩展名、不带 /Game/）
			const FString FullPackagePath = FPaths::GetBaseFilename(RelativePath, false);
			const FString PackageName = TEXT("/Game/") + FullPackagePath;

			UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
			UBlueprint* Blueprint = Package != nullptr ? Cast<UBlueprint>(Package->FindAssetInPackage()) : nullptr;
			if (Blueprint == nullptr || Blueprint->GeneratedClass == nullptr)
			{
				Skipped.Add(FString::Printf(TEXT("%s —— 不是蓝图或载入失败（%s）"), *AssetLabel, *PackageName));
				continue;
			}

			UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			const FStructProperty* ItemDataProperty =
				FindFProperty<FStructProperty>(Blueprint->GeneratedClass, TEXT("ItemData"));
			if (DefaultObject == nullptr || ItemDataProperty == nullptr)
			{
				Skipped.Add(FString::Printf(TEXT("%s —— CDO 或 ItemData 缺失"), *AssetLabel));
				continue;
			}

			const void* ItemDataValue = ItemDataProperty->ContainerPtrToValuePtr<void>(DefaultObject);
			if (ItemDataValue == nullptr)
			{
				Skipped.Add(FString::Printf(TEXT("%s —— ItemData 取址失败"), *AssetLabel));
				continue;
			}

			// 只按名字取值，绝不按 SaveStruct 的偏移去套（类型不同），也绝不碰对象指针。
			const UScriptStruct* ItemStruct = ItemDataProperty->Struct;
			TArray<FString> Missing;
			const FString Name = FieldText(ItemStruct, ItemDataValue, TEXT("Name"), Missing);
			FString Description = FieldText(ItemStruct, ItemDataValue, TEXT("Description"), Missing);
			const FString KeyWords = FieldText(ItemStruct, ItemDataValue, TEXT("KeyWords"), Missing);
			const FString Speed = FieldText(ItemStruct, ItemDataValue, TEXT("Speed"), Missing);

			if (!Missing.IsEmpty())
			{
				Skipped.Add(FString::Printf(TEXT("%s —— 缺少字段 %s"), *AssetLabel, *FString::Join(Missing, TEXT(","))));
				continue;
			}
			if (Description.IsEmpty())
			{
				Description = TEXT("\"\"");
			}

			// Class 由资产路径推出：/Game/<相对路径>.<资产名>_C
			const FString Entry = FString::Printf(
				TEXT("(Class=\"/Script/Engine.BlueprintGeneratedClass'/Game/%s.%s_C'\",Name=%s,Description=%s,KeyWords=%s,%s,Speed=%s)"),
				*FullPackagePath, *AssetLabel, *Name, *Description, *KeyWords, UnifiedStats, *Speed);

			Entries.Add(Entry);
			Report(Lock, Sink, FString::Printf(TEXT("  [%2d] %-24s Speed=%-5s %s"),
				Entries.Num() - 1, *AssetLabel, *Speed, *FPaths::GetBaseFilename(Name, false).Left(24)));
		}

		for (const FString& Skip : Skipped)
		{
			Report(Lock, Sink, FString::Printf(TEXT("  跳过: %s"), *Skip));
		}
		if (Entries.Num() == 0)
		{
			Report(Lock, Sink, TEXT("FATAL: 一个角色都没拼出来，放弃。"));
			return;
		}

		const FString ArrayText = TEXT("(") + FString::Join(Entries, TEXT(",")) + TEXT(")");
		Report(Lock, Sink, TEXT("-----------------------------------------------------------"));
		Report(Lock, Sink, FString::Printf(TEXT("条目数      : %d（跳过 %d）"), Entries.Num(), Skipped.Num()));
		Report(Lock, Sink, FString::Printf(TEXT("数组长度    : %d 字符"), ArrayText.Len()));

		const FString OutFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Saved/ZDBpVar_ALLChar.txt"));
		if (FFileHelper::SaveStringToFile(ArrayText, *OutFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Report(Lock, Sink, FString::Printf(TEXT("数组已备份  : %s"), *OutFile));
		}

		if (!bWrite || bDryRun)
		{
			Report(Lock, Sink, TEXT("=== DRY RUN 结束，未修改资产 ==="));
			return;
		}

		// ---------------------------------------------------------- 写入局部变量
		// 目标变量在 GM_Main 的 Test_ALLcharGet 函数图里，是 UK2Node_FunctionEntry
		// 的局部变量（不是成员变量）。它和成员变量一样是 FBPVariableDescription，
		// DefaultValue 就是要覆盖的那份权威副本。
		UBlueprint* TargetBlueprint = LoadObject<UBlueprint>(nullptr, *TargetPath);
		if (TargetBlueprint == nullptr)
		{
			Report(Lock, Sink, FString::Printf(TEXT("FATAL: 目标蓝图载入失败 %s"), *TargetPath));
			return;
		}

		UK2Node_FunctionEntry* OwnerEntry = nullptr;
		FBPVariableDescription* Variable = nullptr;
		for (UEdGraph* Graph : TargetBlueprint->FunctionGraphs)
		{
			if (Graph == nullptr || !Graph->GetName().Equals(GraphHint, ESearchCase::IgnoreCase))
			{
				continue;
			}
			OwnerEntry = Cast<UK2Node_FunctionEntry>(FBlueprintEditorUtils::GetEntryNode(Graph));
			if (OwnerEntry == nullptr)
			{
				continue;
			}
			for (FBPVariableDescription& Candidate : OwnerEntry->LocalVariables)
			{
				if (Candidate.VarName == FName(*VarName))
				{
					Variable = &Candidate;
					break;
				}
			}
			if (Variable != nullptr)
			{
				break;
			}
		}

		if (Variable == nullptr)
		{
			Report(Lock, Sink, FString::Printf(
				TEXT("FATAL: 在函数图 %s 里找不到局部变量 %s"), *GraphHint, *VarName));
			return;
		}

		const FString OriginalValue = Variable->DefaultValue;
		Report(Lock, Sink, FString::Printf(TEXT("原默认值    : %d 字符"), OriginalValue.Len()));

		UPackage* TargetPackage = TargetBlueprint->GetOutermost();
		TargetPackage->Modify();
		TargetBlueprint->Modify();
		OwnerEntry->Modify();
		Variable->DefaultValue = ArrayText;

		// 局部变量挂在函数签名上，得让结构变脏才会重建函数。
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(TargetBlueprint);

		// 回读校验：逐字比。
		FBPVariableDescription* Verify = nullptr;
		for (UEdGraph* Graph : TargetBlueprint->FunctionGraphs)
		{
			if (Graph == nullptr || !Graph->GetName().Equals(GraphHint, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(FBlueprintEditorUtils::GetEntryNode(Graph)))
			{
				for (FBPVariableDescription& Candidate : Entry->LocalVariables)
				{
					if (Candidate.VarName == FName(*VarName))
					{
						Verify = &Candidate;
						break;
					}
				}
			}
		}

		if (Verify == nullptr || Verify->DefaultValue != ArrayText)
		{
			Report(Lock, Sink, TEXT("FATAL: 写入后回读不一致，回滚且不保存。"));
			Variable->DefaultValue = OriginalValue;
			return;
		}
		Report(Lock, Sink, FString::Printf(TEXT("回读校验    : 一致（%d 字符）"), Verify->DefaultValue.Len()));

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(
			TargetPackage->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(TargetPackage, nullptr, *PackageFileName, SaveArgs))
		{
			Report(Lock, Sink, FString::Printf(TEXT("FATAL: 保存失败 %s"), *PackageFileName));
			return;
		}

		Report(Lock, Sink, FString::Printf(TEXT("已保存      : %s"), *PackageFileName));

		// ---------------------------------------------------------- 6. 重新加载验证
		// 前面那次回读比的是内存对象，证明不了磁盘上的东西。这里把包整个踢掉重新读，
		// 再逐字段拼一遍跟写入内容对比 —— 这才是真正验证落盘结果的测试。
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		TargetBlueprint = nullptr;
		OwnerEntry = nullptr;
		Variable = nullptr;

		UBlueprint* Reloaded = LoadObject<UBlueprint>(nullptr, *TargetPath);
		if (Reloaded == nullptr || Reloaded->GeneratedClass == nullptr)
		{
			Report(Lock, Sink, TEXT("警告: 重新加载失败，无法做落盘验证。"));
			return;
		}

		FBPVariableDescription* ReloadedVar = nullptr;
		for (UEdGraph* Graph : Reloaded->FunctionGraphs)
		{
			if (Graph == nullptr || !Graph->GetName().Equals(GraphHint, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(FBlueprintEditorUtils::GetEntryNode(Graph)))
			{
				for (FBPVariableDescription& Candidate : Entry->LocalVariables)
				{
					if (Candidate.VarName == FName(*VarName))
					{
						ReloadedVar = &Candidate;
						break;
					}
				}
			}
		}

		if (ReloadedVar == nullptr)
		{
			Report(Lock, Sink, FString::Printf(TEXT("FATAL: 落盘后重新加载，函数图 %s 里找不到变量 %s！"), *GraphHint, *VarName));
			return;
		}

		// 引擎的行为：保存时会重新给 FText 生成命名空间/键（实测把资产的
		// 7D063492.../08012575... 换成 59528C6F.../7A0D0349...），但显示文本
		// 一字不改。所以这里比的是「显示文本序列」+「条目数」，不是逐字节相同。
		const TArray<FString> GotTexts = ExtractNsLocDisplayTexts(ReloadedVar->DefaultValue);
		const TArray<FString> WantTexts = ExtractNsLocDisplayTexts(ArrayText);
		const int32 GotEntries = CountEntries(ReloadedVar->DefaultValue);
		const int32 WantEntries = CountEntries(ArrayText);
		const bool bEquivalent = (GotTexts == WantTexts) && (GotEntries == WantEntries);

		Report(Lock, Sink, FString::Printf(
			TEXT("落盘后重载  : %s（读回 %d 字符，写入时 %d 字符）"),
			bEquivalent ? TEXT("内容等价 ✓（仅 FText 键被引擎重生成）") : TEXT("内容不一致 ✗"),
			ReloadedVar->DefaultValue.Len(), ArrayText.Len()));
		Report(Lock, Sink, FString::Printf(TEXT("  条目数    : 读回 %d，写入 %d"), GotEntries, WantEntries));
		Report(Lock, Sink, FString::Printf(TEXT("  角色名数  : 读回 %d，写入 %d"), GotTexts.Num(), WantTexts.Num()));

		if (!bEquivalent)
		{
			const int32 MaxCheck = FMath::Min(GotTexts.Num(), WantTexts.Num());
			for (int32 Index = 0; Index < MaxCheck; ++Index)
			{
				if (GotTexts[Index] != WantTexts[Index])
				{
					Report(Lock, Sink, FString::Printf(TEXT("  首个不同的角色名 #%d：读回 %s / 期望 %s"),
						Index, *GotTexts[Index], *WantTexts[Index]));
					break;
				}
			}
			return;
		}

		Report(Lock, Sink, FString::Printf(TEXT("  首尾抽查  : %s ... %s"),
			*GotTexts[0], *GotTexts[GotTexts.Num() - 1]));

		// 顺带确认编译产物里也在（函数作用域内按名字查得到）。
		if (const FArrayProperty* CompiledProperty = FindFProperty<FArrayProperty>(Reloaded->GeneratedClass, FName(*VarName)))
		{
			const void* CompiledValue = CompiledProperty->ContainerPtrToValuePtr<void>(Reloaded->GeneratedClass->GetDefaultObject());
			FScriptArrayHelper Helper(CompiledProperty, CompiledValue);
			Report(Lock, Sink, FString::Printf(TEXT("  编译产物  : 生成类上的 %s 有 %d 个元素"),
				*CompiledProperty->GetName(), Helper.Num()));
		}

		Report(Lock, Sink, TEXT("==================== 完成 ===================="));
	}

	/**
	 * 真正干活的时机：**引擎初始化完成之后**。
	 * 放在模块静态初始化阶段的话，资产系统还没就位，LoadObject 会白跑。
	 * 没带 -ZDBpVar / -ZDBpVarLog 就什么都不做，不影响正常编辑器启动。
	 */
	static void RunFromCommandLine()
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		FString LogFile;
		FParse::Value(CommandLine, TEXT("ZDBpVarLog="), LogFile);
		if (!FParse::Param(CommandLine, TEXT("ZDBpVar")) && LogFile.IsEmpty())
		{
			return;
		}

		if (LogFile.IsEmpty())
		{
			LogFile = FPaths::ProjectDir() / TEXT("Saved/Logs/ZDBpVar.log");
		}
		LogFile = FPaths::ConvertRelativePathToFull(LogFile);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(LogFile), true);

		UE_LOG(LogZDBpVar, Display, TEXT("ZDBpVar 开始执行，报告将写到 %s"), *LogFile);

		FCriticalSection Lock;
		FString Sink;
		Run(Lock, Sink);

		FFileHelper::SaveStringToFile(Sink, *LogFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogZDBpVar, Display, TEXT("ZDBpVar 报告已写出: %s"), *LogFile);

		if (FParse::Param(CommandLine, TEXT("unattended")))
		{
			FPlatformMisc::RequestExit(false);
		}
	}

	// 纯 C++ 静态注册，零反射，不给编辑器添加任何控制台命令。
	static FDelegateHandle GPostEngineInitHandle =
		FCoreDelegates::GetOnPostEngineInit().AddStatic(&RunFromCommandLine);
}

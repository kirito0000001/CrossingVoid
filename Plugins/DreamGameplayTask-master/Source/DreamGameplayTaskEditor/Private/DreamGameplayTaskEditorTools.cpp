#include "DreamGameplayTaskEditorTools.h"

#include "AssetToolsModule.h"
#include "DreamGameplayTaskEditorLog.h"
#include "DreamGameplayTaskEditorModule.h"
#include "DreamGameplayTaskEditorSetting.h"
#include "DreamGameplayTaskSetting.h"
#include "EditorUtilityLibrary.h"
#include "KismetCompilerModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Classes/DreamTask.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Dialogs/DlgPickPath.h"
#include "Engine/ObjectLibrary.h"
#include "Factories/DataAssetFactory.h"
#include "Kismet2/KismetEditorUtilities.h"

UBlueprint* FDreamGameplayTaskEditorTools::CreateObjectBlueprintByClass(UClass* Class, EBlueprintType BlueprintType, bool bInCurrentPath, bool bOpenInEditor, FString Name)
{
	// 验证目标类是否支持创建 Blueprint
    if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(Class))
    {
        DGT_ED_FLOG(Error, TEXT("Cannot create blueprint for class %s"), *Class->GetName());
        return nullptr;
    }

    // 确定基础名称
    const FString BaseName = Name.IsEmpty() ? TEXT("NewBlueprint") : Name;

    // 加载模块
    IKismetCompilerInterface& KismetCompiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
    FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    // 获取 Blueprint 类型
    UClass* BlueprintClass = nullptr;
    UClass* GeneratedClass = nullptr;
    KismetCompiler.GetBlueprintTypesForClass(Class, BlueprintClass, GeneratedClass);

    // 获取当前 Content Browser 路径，并构造包名
    FString ContentPath;
    UEditorUtilityLibrary::GetCurrentContentBrowserPath(ContentPath);
    // 确保以 /Game 开头
    if (!ContentPath.StartsWith(TEXT("/Game")))
    {
        ContentPath = TEXT("/Game") + ContentPath;
    }
    FString PackageName = ContentPath / BaseName;
    FString UniqueName;
    AssetTools.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, UniqueName);

    // 默认使用当前目录
    FString FinalPackage = PackageName;
    FName AssetName = *UniqueName;

    // 如果不使用当前路径，则弹出对话框
    if (!bInCurrentPath)
    {
        TSharedRef<SDlgPickAssetPath> PickAsset = SNew(SDlgPickAssetPath)
            .Title(NSLOCTEXT("BlueprintCreator", "CreateNewAsset", "Create New Asset"))
            .DefaultAssetPath(FText::FromString(PackageName));

        if (PickAsset->ShowModal() != EAppReturnType::Ok)
        {
            return nullptr;
        }

        const FString SelectedPath = PickAsset->GetFullAssetPath().ToString();
        FName SelectedName = FName(*FPackageName::GetLongPackageAssetName(SelectedPath));
        if (!SelectedName.IsNone())
        {
            FinalPackage = SelectedPath;
            AssetName = SelectedName;
        }
    }

    // 创建包
    UPackage* Package = CreatePackage(*FinalPackage);
    check(Package);

    // 创建 Blueprint
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        Class,
        Package,
        AssetName,
        BlueprintType,
        BlueprintClass,
        GeneratedClass,
        FName("LevelEditorActions")
    );
    if (!Blueprint)
    {
        return nullptr;
    }

    // 注册并标记脏包
    FAssetRegistryModule::AssetCreated(Blueprint);
    Package->MarkPackageDirty();
    DGT_ED_FLOG(Log, TEXT("Created Blueprint: %s"), *FinalPackage);

    // 可选的 DreamTask 映射
    if (Class->IsChildOf(UDreamTask::StaticClass()))
    {
        if (auto* Setting = UDreamGameplayTaskSetting::Get())
        {
            Setting->MakeTaskMapping(Blueprint->GeneratedClass->GetDefaultObject<UDreamTask>());
        }
    }

    // 打开编辑器
    if (GEditor && bOpenInEditor)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);
    }

    return Blueprint;
}

UBlueprint* FDreamGameplayTaskEditorTools::CreateObjectBlueprintByClass(UClass* Class, FString Name, EBlueprintType BlueprintType, bool bInCurrentPath, bool bOpenInEditor)
{
	return CreateObjectBlueprintByClass(Class, BlueprintType, bInCurrentPath, bOpenInEditor, Name);
}

UObject* FDreamGameplayTaskEditorTools::CreateDataAssetByClass(TSubclassOf<UPrimaryDataAsset> Class, FString Name)
{
	if (!Class)
	{
		DGT_ED_FLOG(Error, TEXT("Class is null."));
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	if (!Factory)
	{
		DGT_ED_FLOG(Error, TEXT("Factory is null."));
		return nullptr;
	}

	Factory->SupportedClass = Class;

	FString CurrentPath;
	UEditorUtilityLibrary::GetCurrentContentBrowserPath(CurrentPath);
	const FString DefaultPath = CurrentPath.Right(CurrentPath.Len() - 4) / Name;

	DGT_ED_LOG(Log, TEXT("%s"), *DefaultPath);

	TSharedPtr<SDlgPickPath> PickAssetPathWidget =
		SNew(SDlgPickPath)
		.Title(FText::FromString(TEXT("Create New Data Asset")))
		.DefaultPath(FText::FromString(DefaultPath));

	if (EAppReturnType::Ok == PickAssetPathWidget->ShowModal())
	{
		FString SelectedPath = PickAssetPathWidget->GetPath().ToString();
		FString UName, UPath;
		AssetToolsModule.Get().CreateUniqueAssetName(SelectedPath / Name, TEXT(""), UPath, UName);

		FString AssetName = UName;

		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(UName, SelectedPath, Class, Factory);

		if (!NewAsset)
		{
			DGT_ED_FLOG(Error, TEXT("Create Failed."))
			return nullptr;
		}

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewAsset);
		return NewAsset;
	}
	else
	{
		DGT_ED_LOG(Warning, TEXT("User Canceled."))
		return nullptr;
	}
}

TArray<FString> FDreamGameplayTaskEditorTools::Conv_DirectoryToStrings(const TArray<FDirectoryPath>& Paths)
{
	TArray<FString> Out;
	for (auto& Path : Paths)
	{
		Out.Add(Path.Path);
	}
	return Out;
}

bool FDreamGameplayTaskEditorTools::bIsLoadedMemory = false;

void FDreamGameplayTaskEditorTools::LoadAssetToMemory(UClass* Class)
{
	if (bIsLoadedMemory) return;

	bIsLoadedMemory = true;

	UObjectLibrary* ObjectLibrary = UObjectLibrary::CreateLibrary(Class, true, GIsEditor);
	ObjectLibrary->AddToRoot();

	TArray<FString> Paths = Conv_DirectoryToStrings(UDreamGameplayTaskEditorSetting::Get()->TaskLoadPaths);

	int32 AssetNum = ObjectLibrary->LoadAssetDataFromPaths(Paths, false);
	int32 BlueprintNum = ObjectLibrary->LoadBlueprintAssetDataFromPaths(Paths, false);
	// int32 BlueprintNum = ObjectLibrary->LoadBlueprintsFromPaths(Paths);

	ObjectLibrary->LoadAssetsFromAssetData();

	for (auto Path : Paths)
	{
		DGT_ED_FLOG(Log, TEXT("Load Result: Find Path: %s"), *Path);
	}
	DGT_ED_FLOG(Log, TEXT("Load Result: Class: %s Asset: %d Blueprint: %d"), *Class->GetName(), AssetNum, BlueprintNum);

	ObjectLibrary->RemoveFromRoot();
	return;
}

TArray<FAssetData> FDreamGameplayTaskEditorTools::GetAssetData(UClass* Class)
{
	UObjectLibrary* Library = UObjectLibrary::CreateLibrary(Class, true, GIsEditor);
	Library->AddToRoot();

	TArray<FString> Paths = Conv_DirectoryToStrings(UDreamGameplayTaskEditorSetting::Get()->TaskLoadPaths);

	Library->LoadBlueprintAssetDataFromPaths(Paths);

	TArray<FAssetData> Result;

	Library->GetAssetDataList(Result);

	return Result;
}

void FDreamGameplayTaskEditorTools::ForceLoadAssetToMemory(UClass* Class)
{
	bIsLoadedMemory = false;
	LoadAssetToMemory(Class);
}

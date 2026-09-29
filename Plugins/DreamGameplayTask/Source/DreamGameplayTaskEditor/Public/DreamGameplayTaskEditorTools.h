#pragma once
#include "ClassViewerModule.h"
#include "Editor/ClassViewer/Private/UnloadedBlueprintData.h"

class DREAMGAMEPLAYTASKEDITOR_API FDreamGameplayTaskEditorTools
{
public:
	static UBlueprint* CreateObjectBlueprintByClass(UClass* Class, EBlueprintType BlueprintType = BPTYPE_Normal, bool bInCurrentPath = false, bool bOpenInEditor = true, FString Name = TEXT(""));
	static UBlueprint* CreateObjectBlueprintByClass(UClass* Class, FString Name = TEXT(""), EBlueprintType BlueprintType = BPTYPE_Normal, bool bInCurrentPath = false, bool bOpenInEditor = true);
	static UObject* CreateDataAssetByClass(TSubclassOf<UPrimaryDataAsset> Class, FString Name);

public:
	static TArray<FString> Conv_DirectoryToStrings(const TArray<FDirectoryPath>& Paths);

public:
	static bool bIsLoadedMemory;
	static void LoadAssetToMemory(UClass* Class);
	static TArray<FAssetData> GetAssetData(UClass* Class);
	static void ForceLoadAssetToMemory(UClass* Class);
};

class FDreamGameplayTaskManagerTaskClassFiler : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet<const UClass*> AllowedChildrenOfClasses;

	/** Disallowed class flags. */
	EClassFlags DisallowedClassFlags = CLASS_None;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass,
								TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return !InClass->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
										const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData,
										TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) !=
			EFilterReturn::Failed;
	}
};

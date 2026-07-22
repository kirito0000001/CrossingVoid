#pragma once

#include "CoreMinimal.h"
#include "DreamTaskManagerTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DreamTaskManageWidget_ItemRow.h"

class DREAMGAMEPLAYTASKEDITOR_API SDreamTaskManagerPage_Manager : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDreamTaskManagerPage_Manager)
		{
		}

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual ~SDreamTaskManagerPage_Manager() override;

	void Refresh();
	void Clear();
	void Check();

	FReply Action_Refresh();
	FReply Action_ForceLoadMemory();
	FReply Action_MakeTaskInCurrentPath();
	FReply Action_MakeTask();

	void ListerAssetRemoved(const FAssetData& Data);
	void ListerAssetSaved(const FString& PackageName, UPackage* Package, FObjectPostSaveContext ObjectSavedContext);

	void RegisterAutoRefreshList();
	void UnregisteredAutoRefreshList();

protected:
	TSharedRef<ITableRow> OnGenerateRow(FDreamTaskManagerRowDataPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SHeaderRow> MakeHeaderRow();

	TSharedPtr<SListView<FDreamTaskManagerRowDataPtr>> ListView;
	TSharedPtr<SWidgetSwitcher> Switcher;

	TArray<FDreamTaskManagerRowDataPtr> List;

	FAssetRegistryModule* AssetRegistryModule = nullptr;

	bool bIsSort = false;
};

// Copyright 2022 - 2025 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Manager/DreamTaskManagerTypes.h"
#include "Widgets/Views/STableRow.h"

class UDreamTask;

struct FDreamTaskManagerRowData
{
public:
	FDreamTaskManagerRowData()
	{
	}

	FDreamTaskManagerRowData(UDreamTask* InTask, UBlueprint* InBlueprint)
		: Task(MakeShared<UDreamTask*>(InTask)), Blueprint(MakeShared<UBlueprint*>(InBlueprint))
	{
	}

public:
	TSharedRef<UDreamTask*> Task;
	TSharedRef<UBlueprint*> Blueprint;
};

/**
 * 
 */
class DREAMGAMEPLAYTASKEDITOR_API SDreamTaskManageWidget_ItemRow : public SMultiColumnTableRow<FDreamTaskManagerRowDataPtr>
{
public:
	SLATE_BEGIN_ARGS(SDreamTaskManageWidget_ItemRow) : _ItemShow()
		{
		}

		SLATE_ARGUMENT(FDreamTaskManagerRowDataPtr, ItemShow)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase> InOwnerTableView);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

public:
	FDreamTaskManagerRowDataPtr ItemShow;
};

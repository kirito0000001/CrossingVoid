// Copyright 2022 - 2025 Dream Moon Team. All Rights Reserved.


#include "Manager/Widgets/DreamTaskManageWidget_ItemRow.h"

#include "SlateOptMacros.h"

#include "Classes/DreamTask.h"
#include "Manager/DreamTaskManager_Util.h"

#define MEMBER_IS_VALID(Item) (ItemShow.IsValid() ? ItemShow->Item.Get() != nullptr : false)
#define GET_MEMBER(Item) ItemShow->Item.Get()

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDreamTaskManageWidget_ItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase> InOwnerTableView)
{
	ItemShow = InArgs._ItemShow;
	FSuperRowType::Construct(FSuperRowType::FArguments().Padding(2.f), InOwnerTableView);
}

void SDreamTaskManageWidget_ItemRow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SMultiColumnTableRow<TSharedPtr<FDreamTaskManagerRowData>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

TSharedRef<SWidget> SDreamTaskManageWidget_ItemRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!ItemShow.IsValid() || ItemShow->Task.Get() == nullptr || ItemShow->Blueprint.Get() == nullptr)
		return SNew(SErrorText).ErrorText(MAKE_TEXT(TEXT("Error")));
	
	if (ColumnName == "TaskName")
	{
		return SNew(SOverlay)
				OSLOT()
				.Padding(2.f)
				VA(VCENTER)
				[
					FDreamTaskManagerUtil::MakeText(SELECT(MEMBER_IS_VALID(Task),
					                                       GET_MEMBER(Task)->GetTaskName().ToString(), "Error"))
				];
	}
	else if (ColumnName == "TaskDisplayName")
	{
		return SNew(SOverlay)
				OSLOT()
				.Padding(2.f)
				VA(VCENTER)
				[
					FDreamTaskManagerUtil::MakeText(SELECT(MEMBER_IS_VALID(Task),
					                                       GET_MEMBER(Task)->GetTaskDisplayName().ToString(), "Error"))
				];
	}
	else if (ColumnName == "TaskAction")
	{
		auto OpenEditorAction = [this]()
		{
			if (MEMBER_IS_VALID(Blueprint))
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(
					GET_MEMBER(Blueprint));
			}
		};

		return SNew(SOverlay)

				OSLOT()
				.Padding(2.f)
				HA(HFILL)
				VA(VFILL)
				[
					SNew(SButton)
					.OnClicked_Lambda([OpenEditorAction]()
					{
						OpenEditorAction();
						return FReply::Handled();
					})
					[
						FDreamTaskManagerUtil::MakeIconAndTextWidget(
							MAKE_TEXT(TEXT("Edit %s"),
							          SELECT(MEMBER_IS_VALID(Blueprint), *GET_MEMBER(Blueprint)->GetName(), *FString("Error"))),
							GET_STYLE.Get()->GetBrush("DreamGameplayTaskEditor.OpenEditor")
						)
					]
				];
	}

	return SNew(STextBlock)
		.Text(MAKE_TEXT(TEXT("Error")));
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef MEMBER_IS_VALID

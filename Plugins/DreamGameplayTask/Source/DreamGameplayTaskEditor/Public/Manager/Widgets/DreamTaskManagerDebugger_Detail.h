#pragma once

#include "CoreMinimal.h"
#include "Manager/DreamTaskManagerTypes.h"
#include "Widgets/SCompoundWidget.h"

struct FDreamTaskSpecHandleContainer;
struct FDreamTaskConditionContainer;

class DREAMGAMEPLAYTASKEDITOR_API SDreamTaskManagerDebugger_Detail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDreamTaskManagerDebugger_Detail)
		{
		}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	void SetComponent(FDreamManagerTaskComponent InComponent);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	void HandleTaskListChanged(FDreamTaskSpecHandleContainer& InTaskList);
	
	TSharedRef<ITableRow> OnGenerateRow(FDreamManagerAccessKey InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SHeaderRow> MakeHeaderRow();
	void Refresh();

	void BeginPIE();
	void EndPIE();

private:
	FDreamManagerTaskComponent Component;
	TArray<FDreamManagerAccessKey> Handles;

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SListView<FDreamManagerAccessKey>> ListView;
	FDelegateHandle ComponentDelegateHandle;
	
};

#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskSpecHandle.h"
#include "Manager/DreamTaskManagerTypes.h"
#include "Widgets/SCompoundWidget.h"


class SDreamTaskManagerDebugger_Detail;

class DREAMGAMEPLAYTASKEDITOR_API SDreamTaskManagerDebugger_Detail_SpecRow : public SMultiColumnTableRow<FDreamManagerAccessKey>
{
public:
	SLATE_BEGIN_ARGS(SDreamTaskManagerDebugger_Detail_SpecRow): _AccessKey(), _Detail()
		{
		}

		SLATE_ARGUMENT(FDreamManagerAccessKey, AccessKey)
		SLATE_ARGUMENT(TSharedPtr<SDreamTaskManagerDebugger_Detail>, Detail)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase> InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

public:
	FDreamManagerAccessKey AccessKey;
	TSharedPtr<SDreamTaskManagerDebugger_Detail> Detail;
	TSharedPtr<STextBlock> EndTimeTB;
};

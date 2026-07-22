// Copyright Qibo Pang 2023. All Rights Reserved.

#include "PostProcessWidgetSlot.h"
#include "ObjectEditorUtils.h"
#include "SPostProcessWidget.h"
#include "PostProcessWidget.h"

/////////////////////////////////////////////////////
// UPostProcessWidgetSlot

UPostProcessWidgetSlot::UPostProcessWidgetSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(4, 2);

	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;
}

void UPostProcessWidgetSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	PostProcess.Reset();
}

void UPostProcessWidgetSlot::BuildSlot(TSharedRef<SPostProcessWidget> InPostProcess)
{
	PostProcess = InPostProcess;

	/*PostProcess->SetPadding(Padding);
	PostProcess->SetHAlign(HorizontalAlignment);
	PostProcess->SetVAlign(VerticalAlignment);*/

	PostProcess->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UPostProcessWidgetSlot::SetPadding(FMargin InPadding)
{
	//CastChecked<UPostProcessWidget>(Parent)->SetPadding(InPadding);
}

void UPostProcessWidgetSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	//CastChecked<UPostProcessWidget>(Parent)->SetHorizontalAlignment(InHorizontalAlignment);
}

void UPostProcessWidgetSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	//CastChecked<UPostProcessWidget>(Parent)->SetVerticalAlignment(InVerticalAlignment);
}

void UPostProcessWidgetSlot::SynchronizeProperties()
{
	if ( PostProcess.IsValid() )
	{
		SetPadding(Padding);
		SetHorizontalAlignment(HorizontalAlignment);
		SetVerticalAlignment(VerticalAlignment);
	}
}

#if WITH_EDITOR

void UPostProcessWidgetSlot::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static bool IsReentrant = false;

	if ( !IsReentrant )
	{
		IsReentrant = true;

		if ( PropertyChangedEvent.Property )
		{
			static const FName PaddingName("Padding");
			static const FName HorizontalAlignmentName("HorizontalAlignment");
			static const FName VerticalAlignmentName("VerticalAlignment");

			FName PropertyName = PropertyChangedEvent.Property->GetFName();

			if ( UPostProcessWidget* ParentPostProcess = CastChecked<UPostProcessWidget>(Parent) )
			{
				if (PropertyName == PaddingName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, PaddingName, ParentPostProcess, PaddingName);
				}
				else if (PropertyName == HorizontalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, HorizontalAlignmentName, ParentPostProcess, HorizontalAlignmentName);
				}
				else if (PropertyName == VerticalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, VerticalAlignmentName, ParentPostProcess, VerticalAlignmentName);
				}
			}
		}

		IsReentrant = false;
	}
}

#endif

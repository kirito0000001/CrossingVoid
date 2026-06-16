// Copyright Qibo Pang 2023. All Rights Reserved.

#pragma once

#include "UObject/ScriptMacros.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/PanelSlot.h"
#include "Layout/Margin.h"
#include "PostProcessWidgetSlot.generated.h"


class SPostProcessWidget;
class UPostProcessWidget;

/**
 * The Slot for the UPostProcessWidgetSlot, contains the widget displayed in a PostProcess's single slot
 */
UCLASS()
class POSTPROCESSWIDGET_API UPostProcessWidgetSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Layout|PostProcessWidget Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|PostProcessWidget Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Layout|PostProcessWidget Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

protected:
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, Category="Layout|PostProcessWidget Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, Category="Layout|PostProcessWidget Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, Category="Layout|PostProcessWidget Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying slot for the slate PostProcess. */
	void BuildSlot(TSharedRef<SPostProcessWidget> InPostProcess);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

public:

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

#endif

private:

	/** A pointer to the PostProcess to allow us to adjust the size, padding...etc at runtime. */
	TSharedPtr<SPostProcessWidget> PostProcess;

	friend UPostProcessWidget;
};

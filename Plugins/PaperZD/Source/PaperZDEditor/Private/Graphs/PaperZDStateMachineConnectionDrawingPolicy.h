// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "ConnectionDrawingPolicy.h"
#include "Editors/Util/PaperZDSlateSupport.h"

class FSlateWindowElementList;
class UEdGraph;
/////////////////////////////////////////////////////
// FPaperZDStateMachineConnectionDrawingPolicy

// This class draws the connections for an UEdGraph with a state machine schema
class FPaperZDStateMachineConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
protected:
	TMap<UEdGraphNode*, int32> NodeWidgetMap;

public:
	//
	FPaperZDStateMachineConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	// FConnectionDrawingPolicy interface
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;
	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& PinGeometries, FArrangedChildren& ArrangedNodes) override;
	virtual void DetermineLinkGeometry(
		FArrangedChildren& ArrangedNodes,
		TSharedRef<SWidget>& OutputPinWidget,
		UEdGraphPin* OutputPin,
		UEdGraphPin* InputPin,
		/*out*/ FArrangedWidget*& StartWidgetGeometry,
		/*out*/ FArrangedWidget*& EndWidgetGeometry
	) override;
	virtual void DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params) override;
	virtual void DrawSplineWithArrow(const ZD::Slate::FVector2& StartPoint, const ZD::Slate::FVector2& EndPoint, const FConnectionParams& Params) override;
	virtual void DrawPreviewConnector(const FGeometry& PinGeometry, const ZD::Slate::FVector2& StartPoint, const ZD::Slate::FVector2& EndPoint, UEdGraphPin* Pin) override;
	virtual ZD::Slate::FVector2 ComputeSplineTangent(const ZD::Slate::FVector2& Start, const ZD::Slate::FVector2& End) const override;
	// End of FConnectionDrawingPolicy interface

protected:
	void Internal_DrawLineWithArrow(const ZD::Slate::FVector2& StartAnchorPoint, const ZD::Slate::FVector2& EndAnchorPoint, const FConnectionParams& Params);
};

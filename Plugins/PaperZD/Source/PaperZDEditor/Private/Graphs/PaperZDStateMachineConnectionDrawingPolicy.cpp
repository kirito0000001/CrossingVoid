// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#include "Graphs/PaperZDStateMachineConnectionDrawingPolicy.h"
#include "Graphs/Nodes/PaperZDStateGraphNode_Transition.h"
#include "Graphs/Nodes/PaperZDStateGraphNode_Root.h"
#include "Graphs/Nodes/PaperZDStateGraphNode_State.h"
#include "Graphs/Nodes/PaperZDStateGraphNode_Jump.h"
#include "Graphs/Nodes/Slate/SPaperZDStateGraphNode_Transition.h"
#include "Rendering/DrawElements.h"

FPaperZDStateMachineConnectionDrawingPolicy::FPaperZDStateMachineConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
{
}

void FPaperZDStateMachineConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	Params.AssociatedPin1 = OutputPin;
	Params.AssociatedPin2 = InputPin;
	Params.WireThickness = 1.5f;

	if (InputPin)
	{
		if (UPaperZDStateGraphNode_Transition* TransNode = Cast<UPaperZDStateGraphNode_Transition>(InputPin->GetOwningNode()))
		{
			Params.WireColor = SPaperZDStateGraphNode_Transition::StaticGetTransitionColor(TransNode, HoveredPins.Contains(InputPin));
		}
	}

	const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
	}
}

void FPaperZDStateMachineConnectionDrawingPolicy::DetermineLinkGeometry(
	FArrangedChildren& ArrangedNodes,
	TSharedRef<SWidget>& OutputPinWidget,
	UEdGraphPin* OutputPin,
	UEdGraphPin* InputPin,
	/*out*/ FArrangedWidget*& StartWidgetGeometry,
	/*out*/ FArrangedWidget*& EndWidgetGeometry
)
{
	if (UPaperZDStateGraphNode_Root* EntryNode = Cast<UPaperZDStateGraphNode_Root>(OutputPin->GetOwningNode()))
	{
		StartWidgetGeometry = PinGeometries->Find(OutputPinWidget);

		UPaperZDStateGraphNode* State = CastChecked<UPaperZDStateGraphNode>(InputPin->GetOwningNode());
		int32 StateIndex = NodeWidgetMap.FindChecked(State);
		EndWidgetGeometry = &(ArrangedNodes[StateIndex]);
	}
	else if (UPaperZDStateGraphNode_Jump* CastNode = Cast<UPaperZDStateGraphNode_Jump>(OutputPin->GetOwningNode()))
	{
		StartWidgetGeometry = PinGeometries->Find(OutputPinWidget);

		UPaperZDStateGraphNode* State = CastChecked<UPaperZDStateGraphNode>(InputPin->GetOwningNode());
		int32 StateIndex = NodeWidgetMap.FindChecked(State);
		EndWidgetGeometry = &(ArrangedNodes[StateIndex]);
	}
	else if (UPaperZDStateGraphNode_Transition* TransNode = Cast<UPaperZDStateGraphNode_Transition>(InputPin->GetOwningNode()))
	{
		UPaperZDStateGraphNode* PrevState = TransNode->GetFromNode();
		UPaperZDStateGraphNode* NextState = TransNode->GetToNode();
		if ((PrevState != NULL) && (NextState != NULL))
		{
			int32* PrevNodeIndex = NodeWidgetMap.Find(PrevState);
			int32* NextNodeIndex = NodeWidgetMap.Find(NextState);
			if ((PrevNodeIndex != NULL) && (NextNodeIndex != NULL))
			{
				StartWidgetGeometry = &(ArrangedNodes[*PrevNodeIndex]);
				EndWidgetGeometry = &(ArrangedNodes[*NextNodeIndex]);
			}
		}
	}
	else
	{
		StartWidgetGeometry = PinGeometries->Find(OutputPinWidget);

		if (TSharedPtr<SGraphPin>* pTargetWidget = PinToPinWidgetMap.Find(InputPin))
		{
			TSharedPtr<SGraphPin> InputWidget = *pTargetWidget;
			EndWidgetGeometry = PinGeometries->Find(InputWidget.ToSharedRef());
		}
	}
}

void FPaperZDStateMachineConnectionDrawingPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes)
{
	// Build an acceleration structure to quickly find geometry for the nodes
	NodeWidgetMap.Empty();
	for (int32 NodeIndex = 0; NodeIndex < ArrangedNodes.Num(); ++NodeIndex)
	{
		FArrangedWidget& CurWidget = ArrangedNodes[NodeIndex];
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
		NodeWidgetMap.Add(ChildNode->GetNodeObj(), NodeIndex);
	}

	// Now draw
	FConnectionDrawingPolicy::Draw(InPinGeometries, ArrangedNodes);
}

void FPaperZDStateMachineConnectionDrawingPolicy::DrawPreviewConnector(const FGeometry& PinGeometry, const ZD::Slate::FVector2& StartPoint, const ZD::Slate::FVector2& EndPoint, UEdGraphPin* Pin)
{
	FConnectionParams Params;
	DetermineWiringStyle(Pin, nullptr, /*inout*/ Params);

	const ZD::Slate::FVector2 SeedPoint = EndPoint;
	const ZD::Slate::FVector2 AdjustedStartPoint = FGeometryHelper::FindClosestPointOnGeom(PinGeometry, SeedPoint);

	DrawSplineWithArrow(AdjustedStartPoint, EndPoint, Params);
}


void FPaperZDStateMachineConnectionDrawingPolicy::DrawSplineWithArrow(const ZD::Slate::FVector2& StartAnchorPoint, const ZD::Slate::FVector2& EndAnchorPoint, const FConnectionParams& Params)
{
	Internal_DrawLineWithArrow(StartAnchorPoint, EndAnchorPoint, Params);

	// Is the connection bidirectional?
	if (Params.bUserFlag1)
	{
		Internal_DrawLineWithArrow(EndAnchorPoint, StartAnchorPoint, Params);
	}
}

void FPaperZDStateMachineConnectionDrawingPolicy::Internal_DrawLineWithArrow(const ZD::Slate::FVector2& StartAnchorPoint, const ZD::Slate::FVector2& EndAnchorPoint, const FConnectionParams& Params)
{
	const float LineSeparationAmount = 4.5f;

	const ZD::Slate::FVector2 DeltaPos = EndAnchorPoint - StartAnchorPoint;
	const ZD::Slate::FVector2 UnitDelta = DeltaPos.GetSafeNormal();
	const ZD::Slate::FVector2 Normal = ZD::Slate::FVector2(DeltaPos.Y, -DeltaPos.X).GetSafeNormal();

	// Come up with the final start/end points
	const ZD::Slate::FVector2 DirectionBias = Normal * LineSeparationAmount;
	const ZD::Slate::FVector2 LengthBias = ArrowRadius.X * UnitDelta;
	const ZD::Slate::FVector2 StartPoint = StartAnchorPoint + DirectionBias + LengthBias;
	const ZD::Slate::FVector2 EndPoint = EndAnchorPoint + DirectionBias - LengthBias;

	// Draw a line/spline
	DrawConnection(WireLayerID, StartPoint, EndPoint, Params);

	// Draw the arrow
	const ZD::Slate::FVector2 ArrowDrawPos = EndPoint - ArrowRadius;
	const float AngleInRadians = FMath::Atan2(DeltaPos.Y, DeltaPos.X);

	FSlateDrawElement::MakeRotatedBox(
		DrawElementsList,
		ArrowLayerID,
		FPaintGeometry(ArrowDrawPos, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
		ArrowImage,
		ESlateDrawEffect::None,
		AngleInRadians,
		TOptional<ZD::Slate::FVector2>(),
		FSlateDrawElement::RelativeToElement,
		Params.WireColor
	);
}

void FPaperZDStateMachineConnectionDrawingPolicy::DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params)
{
	// Get a reasonable seed point (halfway between the boxes)
	const ZD::Slate::FVector2 StartCenter = FGeometryHelper::CenterOf(StartGeom);
	const ZD::Slate::FVector2 EndCenter = FGeometryHelper::CenterOf(EndGeom);
	const ZD::Slate::FVector2 SeedPoint = (StartCenter + EndCenter) * 0.5f;

	// Find the (approximate) closest points between the two boxes
	const ZD::Slate::FVector2 StartAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(StartGeom, SeedPoint);
	const ZD::Slate::FVector2 EndAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(EndGeom, SeedPoint);

	DrawSplineWithArrow(StartAnchorPoint, EndAnchorPoint, Params);
}

ZD::Slate::FVector2 FPaperZDStateMachineConnectionDrawingPolicy::ComputeSplineTangent(const ZD::Slate::FVector2& Start, const ZD::Slate::FVector2& End) const
{
	const ZD::Slate::FVector2 Delta = End - Start;
	const ZD::Slate::FVector2 NormDelta = Delta.GetSafeNormal();

	return NormDelta;
}

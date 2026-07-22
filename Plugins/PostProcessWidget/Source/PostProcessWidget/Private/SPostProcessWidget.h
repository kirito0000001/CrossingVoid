// Copyright Qibo Pang 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Layout/Visibility.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Input/Events.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Children.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"
#include "Misc/FrameValue.h"

class FArrangedChildren;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextureRenderTarget2D;
class FPostProcessWidgetRenderingResources;

DECLARE_MULTICAST_DELEGATE( FOnPostProcessModeChanged );



/**
 * The SPostProcessWidget renders children widgets to a render target first before
 * later rendering that render target to the screen.  This allows both frequency
 * and phase to be controlled so that the UI can actually render less often than the
 * frequency of the main game render.  It also has the side benefit of allow materials
 * to be applied to the render target after drawing the widgets to apply a simple post process.
 */
class SPostProcessWidget : public SCompoundWidget
{
public:
	static int32 Shared_MaxRetainerWorkPerFrame;

public:
	SLATE_BEGIN_ARGS(SPostProcessWidget)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
		_Phase = 0;
		_PhaseCount = 1;
		_RenderOnPhase = true;
		_RenderOnInvalidation = false;
		_RenderWithLocalTransform = true;
	}
	SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(bool, RenderOnPhase)
		SLATE_ARGUMENT(bool, RenderOnInvalidation)
		SLATE_ARGUMENT(bool, RenderWithLocalTransform)
		SLATE_ARGUMENT(int32, Phase)
		SLATE_ARGUMENT(int32, PhaseCount)
		SLATE_ARGUMENT(FName, StatId)
	SLATE_END_ARGS()

	SPostProcessWidget();
	~SPostProcessWidget();

	/** Constructor */
	void Construct(const FArguments& Args);

	void SetRenderingPhase(int32 Phase, int32 PhaseCount);

	/** Requests that the retainer redraw the hosted content next time it's painted. */
	void RequestRender();

	void SetPostProcessRendering(bool bPostProcessRendering);

	void SetContent(const TSharedRef< SWidget >& InContent);

	UMaterialInstanceDynamic* GetEffectMaterial() const;

	void SetEffectMaterial(UMaterialInterface* EffectMaterial);

	void SetTextureParameter(FName TextureParameter);

	/** SWidget interface */
	virtual FChildren* GetChildren() override;
	virtual FChildren* GetAllChildren() override;

	UTexture* GetTexture() const;

	void SetWorld(UWorld* World);

protected:
	/** SCompoundWidget interface */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float Scale) const override;
	virtual bool CustomPrepass(float LayoutScaleMultiplier) override;

	enum class EPaintBackgroundResult
	{
		NotPainted,
		Painted,
		Queued,
		InvalidSize,
	};
	EPaintBackgroundResult PaintBackgroundImpl(const FSlateInvalidationContext& Context, const FGeometry& AllottedGeometry);
	//~ End FSlateInvalidationRoot interface

	void RefreshRenderingMode();
	bool ShouldBeRenderingOffscreen() const;
	bool IsAnythingVisibleToRender() const;
	void OnPostProcessModeChanged();
	void OnRootInvalidated();

private:

#if !UE_BUILD_SHIPPING
	static void OnPostProcessModeCVarChanged( IConsoleVariable* CVar );
	static FOnPostProcessModeChanged OnPostProcessModeChangedDelegate;
#endif
	//FSimpleSlot EmptyChildSlot;

	mutable FSlateBrush SurfaceBrush;

	FVector2D PreviousRenderSize;
	FGeometry PreviousAllottedGeometry;
	FVector2D PreviousClipRectSize;
	TOptional<FSlateClippingState> PreviousClippingState;
	FLinearColor PreviousColorAndOpacity;

	void UpdateWidgetRenderer();

	TSharedPtr<SWidget> MyWidget;
	TSharedRef<SVirtualWindow> VirtualWindow;

	TSharedRef<FHittestGrid> HittestGrid;

	int32 Phase;
	int32 PhaseCount;

	bool bEnablePostProcessRenderingDesire;
	bool bEnablePostProcessRendering;
	bool bEnableRenderWithLocalTransform;

	bool RenderOnPhase;
	bool RenderOnInvalidation;

	bool bRenderRequested;
	bool bInvalidSizeLogged;

	double LastDrawTime;
	int64 LastTickedFrame;

	TWeakObjectPtr<UWorld> OuterWorld;

	FPostProcessWidgetRenderingResources* RenderingResources;

	STAT(TStatId MyStatId;)

	FSlateBrush DynamicBrush;

	FName DynamicEffectTextureParameter;

	static TArray<SPostProcessWidget*, TInlineAllocator<3>> Shared_WaitingToRender;
	static TFrameValue<int32> Shared_RetainerWorkThisFrame;

	/** Draws the background blur in Slate */
	TSharedPtr<class FPostProcessDrawer, ESPMode::ThreadSafe> PostProcessDrawer;
};

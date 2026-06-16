// Copyright Qibo Pang 2023. All Rights Reserved.

#include "SPostProcessWidget.h"
#include "PostProcessWidgetDefine.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Render/PostProcessDrawer.h"
//#include "UMGPrivate.h"

DECLARE_CYCLE_STAT(TEXT("PostProcess Widget Tick"), STAT_SlatePostProcessWidgetTick, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("PostProcess Widget Paint"), STAT_SlatePostProcessWidgetPaint, STATGROUP_Slate);

#if !UE_BUILD_SHIPPING
FOnPostProcessModeChanged SPostProcessWidget::OnPostProcessModeChangedDelegate;
#endif

/** Whether or not the platform should have deferred PostProcess Widget render target updating enabled by default */
#define PLATFORM_REQUIRES_DEFERRED_RETAINER_UPDATE PLATFORM_IOS || PLATFORM_ANDROID;


class FPostProcessWidgetRenderingResources : public FDeferredCleanupInterface, public FGCObject
{
public:
	FPostProcessWidgetRenderingResources()
		: WidgetRenderer(nullptr)
		, RenderTarget(nullptr)
		, DynamicEffect(nullptr)
	{}

	~FPostProcessWidgetRenderingResources()
	{
		// Note not using deferred cleanup for widget renderer here as it is already in deferred cleanup
		if (WidgetRenderer)
		{
			delete WidgetRenderer;
		}
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(RenderTarget);
		Collector.AddReferencedObject(DynamicEffect);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FPostProcessWidgetRenderingResources");
	}
	
public:
	FWidgetRenderer* WidgetRenderer;
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TObjectPtr<UMaterialInstanceDynamic> DynamicEffect;
};

TArray<SPostProcessWidget*, TInlineAllocator<3>> SPostProcessWidget::Shared_WaitingToRender;
int32 SPostProcessWidget::Shared_MaxRetainerWorkPerFrame(0);
TFrameValue<int32> SPostProcessWidget::Shared_RetainerWorkThisFrame(0);


SPostProcessWidget::SPostProcessWidget()
	: /*EmptyChildSlot(this)
	, */VirtualWindow(SNew(SVirtualWindow))
	, HittestGrid(MakeShared<FHittestGrid>())
	, RenderingResources(new FPostProcessWidgetRenderingResources)
	, PostProcessDrawer(new FPostProcessDrawer())
{
	if (FSlateApplication::IsInitialized())
	{
#if !UE_BUILD_SHIPPING
		OnPostProcessModeChangedDelegate.RemoveAll(this);
#endif
	}
	bHasCustomPrepass = true;
	SetCanTick(false);
}

SPostProcessWidget::~SPostProcessWidget()
{

	// Begin deferred cleanup of rendering resources.  DO NOT delete here.  Will be deleted when safe
	BeginCleanup(RenderingResources);

	Shared_WaitingToRender.Remove(this);

	ENQUEUE_RENDER_COMMAND(SafeDeletePostProcessDrawer)(
		[PostProcessDrawer = PostProcessDrawer](FRHICommandListImmediate& RHICmdList) mutable
		{
			PostProcessDrawer.Reset();
		}
	);
}

void SPostProcessWidget::UpdateWidgetRenderer()
{
	// We can't write out linear.  If we write out linear, then we end up with premultiplied alpha
	// in linear space, which blending with gamma space later is difficult...impossible? to get right
	// since the rest of slate does blending in gamma space.
	const bool bWriteContentInGammaSpace = true;

	if (!RenderingResources->WidgetRenderer)
	{
		RenderingResources->WidgetRenderer = new FWidgetRenderer(bWriteContentInGammaSpace);
	}

	UTextureRenderTarget2D* RenderTarget = RenderingResources->RenderTarget;
	FWidgetRenderer* WidgetRenderer = RenderingResources->WidgetRenderer;

	WidgetRenderer->SetUseGammaCorrection(bWriteContentInGammaSpace);

	// This will be handled by the main slate rendering pass
	WidgetRenderer->SetApplyColorDeficiencyCorrection(false);

	WidgetRenderer->SetIsPrepassNeeded(false);
	WidgetRenderer->SetClearHitTestGrid(false);

	// Update the render target to match the current gamma rendering preferences.
	if (RenderTarget && RenderTarget->SRGB != !bWriteContentInGammaSpace)
	{
		// Note, we do the opposite here of whatever write is, if we we're writing out gamma,
		// then sRGB writes were not supported, so it won't be an sRGB texture.
		RenderTarget->TargetGamma = !bWriteContentInGammaSpace ? 0.0f : 1.0;
		RenderTarget->SRGB = !bWriteContentInGammaSpace;

		RenderTarget->UpdateResource();
	}
}

void SPostProcessWidget::Construct(const FArguments& InArgs)
{
	STAT(MyStatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_Slate>(InArgs._StatId);)

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->ClearColor = FLinearColor::Transparent;
	RenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	
	RenderingResources->RenderTarget = RenderTarget;
	SurfaceBrush.SetResourceObject(RenderTarget);

	VirtualWindow->SetVisibility(EVisibility::SelfHitTestInvisible);  
	VirtualWindow->SetShouldResolveDeferred(false);

	UpdateWidgetRenderer();

	MyWidget = InArgs._Content.Widget;

	RenderOnPhase = InArgs._RenderOnPhase;
	RenderOnInvalidation = InArgs._RenderOnInvalidation;

	Phase = InArgs._Phase;
	PhaseCount = InArgs._PhaseCount;

	LastDrawTime = FApp::GetCurrentTime();
	LastTickedFrame = 0;

	bEnablePostProcessRenderingDesire = true;
	bEnablePostProcessRendering = false;
	bEnableRenderWithLocalTransform = InArgs._RenderWithLocalTransform;

	RefreshRenderingMode();
	bRenderRequested = true;
	bInvalidSizeLogged = false;

	ChildSlot
	[
		MyWidget.ToSharedRef()
	];

	if ( FSlateApplication::IsInitialized() )
	{
#if !UE_BUILD_SHIPPING
		OnPostProcessModeChangedDelegate.AddRaw(this, &SPostProcessWidget::OnPostProcessModeChanged);

		static bool bStaticInit = false;

		if ( !bStaticInit )
		{
			bStaticInit = true;
		}
#endif
	}
}

bool SPostProcessWidget::ShouldBeRenderingOffscreen() const
{
	return bEnablePostProcessRenderingDesire;
}

bool SPostProcessWidget::IsAnythingVisibleToRender() const
{
	return MyWidget.IsValid() && MyWidget->GetVisibility().IsVisible();
}

void SPostProcessWidget::OnPostProcessModeChanged()
{
	RefreshRenderingMode();

	bRenderRequested = true;
}

void SPostProcessWidget::OnRootInvalidated()
{
	RequestRender();
}

#if !UE_BUILD_SHIPPING

void SPostProcessWidget::OnPostProcessModeCVarChanged( IConsoleVariable* CVar )
{
	OnPostProcessModeChangedDelegate.Broadcast();
}

#endif

void SPostProcessWidget::SetPostProcessRendering(bool bRetainRendering)
{
	if (bEnablePostProcessRenderingDesire != bRetainRendering)
	{
		bEnablePostProcessRenderingDesire = bRetainRendering;
		OnPostProcessModeChanged();
	}
}

void SPostProcessWidget::RefreshRenderingMode()
{
	const bool bShouldBeRenderingOffscreen = ShouldBeRenderingOffscreen();

	if ( bEnablePostProcessRendering != bShouldBeRenderingOffscreen )
	{
		bEnablePostProcessRendering = bShouldBeRenderingOffscreen;
	}
}

void SPostProcessWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	MyWidget = InContent;
	ChildSlot
	[
		InContent
	];
}

UMaterialInstanceDynamic* SPostProcessWidget::GetEffectMaterial() const
{
	return RenderingResources->DynamicEffect;
}

void SPostProcessWidget::SetEffectMaterial(UMaterialInterface* EffectMaterial)
{
	if ( EffectMaterial )
	{
		UMaterialInstanceDynamic* DynamicEffect = Cast<UMaterialInstanceDynamic>(EffectMaterial);
		if ( !DynamicEffect )
		{
			DynamicEffect = UMaterialInstanceDynamic::Create(EffectMaterial, GetTransientPackage());
		}
		RenderingResources->DynamicEffect = DynamicEffect;

		SurfaceBrush.SetResourceObject(RenderingResources->DynamicEffect);
	}
	else
	{
		RenderingResources->DynamicEffect = nullptr;
		SurfaceBrush.SetResourceObject(RenderingResources->RenderTarget);
	}

	UpdateWidgetRenderer();
}

void SPostProcessWidget::SetTextureParameter(FName TextureParameter)
{
	DynamicEffectTextureParameter = TextureParameter;
}
 
void SPostProcessWidget::SetWorld(UWorld* World)
{
	OuterWorld = World;
}

FChildren* SPostProcessWidget::GetChildren()
{
	return SCompoundWidget::GetChildren();
}

FChildren* SPostProcessWidget::GetAllChildren()
{
	return SCompoundWidget::GetChildren();
}

void SPostProcessWidget::SetRenderingPhase(int32 InPhase, int32 InPhaseCount)
{
	Phase = InPhase;
	PhaseCount = InPhaseCount;
}

void SPostProcessWidget::RequestRender()
{
	bRenderRequested = true;
}

SPostProcessWidget::EPaintBackgroundResult SPostProcessWidget::PaintBackgroundImpl(const FSlateInvalidationContext& Context, const FGeometry& AllottedGeometry)
{
	if (RenderOnPhase)
	{
		if (LastTickedFrame != GFrameCounter && (GFrameCounter % PhaseCount) == Phase)
		{
			// If doing some phase based invalidation, just redraw everything again
			RequestRender();
		}
	}

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FVector2D RenderSize = PaintGeometry.GetLocalSize() * FVector2D(PaintGeometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector());

	if (RenderOnInvalidation)
	{
		// the invalidation root will take care of whether or not we actually rendered
		bRenderRequested = true;

		// Aggressively repaint when a base state changes.
		const FVector2D ClipRectSize = Context.CullingRect.GetSize().RoundToVector();
		const TOptional<FSlateClippingState> ClippingState = Context.WindowElementList->GetClippingState();
		const FLinearColor ColorAndOpacityTint = Context.WidgetStyle.GetColorAndOpacityTint();
		if (RenderSize != PreviousRenderSize
			|| AllottedGeometry != PreviousAllottedGeometry
			|| ClipRectSize != PreviousClipRectSize
			|| ClippingState != PreviousClippingState
			|| ColorAndOpacityTint != PreviousColorAndOpacity)
		{
			PreviousRenderSize = RenderSize;
			PreviousAllottedGeometry = AllottedGeometry;
			PreviousClipRectSize = ClipRectSize;
			PreviousClippingState = ClippingState;
			PreviousColorAndOpacity = ColorAndOpacityTint;

			RequestRender();
		}
	}
	else if (RenderSize != PreviousRenderSize)
	{
		RequestRender();
		PreviousRenderSize = RenderSize;
	}

	if (Shared_MaxRetainerWorkPerFrame > 0)
	{
		if (Shared_RetainerWorkThisFrame.TryGetValue(0) > Shared_MaxRetainerWorkPerFrame)
		{
			Shared_WaitingToRender.AddUnique(this);
			return EPaintBackgroundResult::Queued;
		}
	}

	if (bRenderRequested)
	{
		// In order to get material parameter collections to function properly, we need the current world's Scene
		// properly propagated through to any widgets that depend on that functionality. The SceneViewport and PostProcessWidget the 
		// only location where this information exists in Slate, so we push the current scene onto the current
		// Slate application so that we can leverage it in later calls.
		UWorld* TickWorld = OuterWorld.Get();
		if (TickWorld && TickWorld->Scene && IsInGameThread())
		{
			FSlateApplication::Get().GetRenderer()->RegisterCurrentScene(TickWorld->Scene);
		}
		else if (IsInGameThread())
		{
			FSlateApplication::Get().GetRenderer()->RegisterCurrentScene(nullptr);
		}

		// Update the number of retainers we've drawn this frame.
		Shared_RetainerWorkThisFrame = Shared_RetainerWorkThisFrame.TryGetValue(0) + 1;

		LastTickedFrame = GFrameCounter;
		const double TimeSinceLastDraw = FApp::GetCurrentTime() - LastDrawTime;

		// Size must be a positive integer to allocate the RenderTarget
		const uint32 RenderTargetWidth  = FMath::RoundToInt(FMath::Abs(RenderSize.X));
		const uint32 RenderTargetHeight = FMath::RoundToInt(FMath::Abs(RenderSize.Y));
		const bool bTextureIsTooLarge = FMath::Max(RenderTargetWidth, RenderTargetHeight) > GetMax2DTextureDimension();
		const bool bTextureSizeZero = (RenderTargetWidth == 0 || RenderTargetHeight == 0);

		if (bTextureIsTooLarge || bTextureSizeZero)
		{
			// if bTextureTooLarge then the user probably have a layout issue. Warn the user.
			if (!bInvalidSizeLogged)
			{
				bInvalidSizeLogged = true;
				if (bTextureIsTooLarge)
				{
					UE_LOG(LogPostProcessWidget, Error, TEXT("The requested size for SPostProcessWidget is too large. W:%i H:%i"), RenderTargetWidth, RenderTargetHeight);
				}
				else
				{
					UE_LOG(LogPostProcessWidget, Error, TEXT("The requested size for SPostProcessWidget is 0. W:%i H:%i"), RenderTargetWidth, RenderTargetHeight);
				}
			}
			return EPaintBackgroundResult::InvalidSize;
		}
		bInvalidSizeLogged = false;

		if (RenderTargetWidth >= 1 && RenderTargetHeight >= 1)
		{
			const FVector2D ViewOffset = PaintGeometry.GetAccumulatedRenderTransform().GetTranslation();

			UTextureRenderTarget2D* RenderTarget = RenderingResources->RenderTarget;
			FWidgetRenderer* WidgetRenderer = RenderingResources->WidgetRenderer;

			{
				if ( (int32)RenderTarget->GetSurfaceWidth() != (int32)RenderTargetWidth ||
					 (int32)RenderTarget->GetSurfaceHeight() != (int32)RenderTargetHeight )
				{
					
					// If the render target resource already exists just resize it.  Calling InitCustomFormat flushes render commands which could result in a huge hitch
					if(RenderTarget->GameThread_GetRenderTargetResource() && RenderTarget->OverrideFormat == PF_B8G8R8A8)
					{
						RenderTarget->ResizeTarget(RenderTargetWidth, RenderTargetHeight);
					}
					else
					{
						const bool bForceLinearGamma = false;
						RenderTarget->InitCustomFormat(RenderTargetWidth, RenderTargetHeight, PF_B8G8R8A8, bForceLinearGamma);
						RenderTarget->UpdateResourceImmediate();
					}
				}

				const float Scale = AllottedGeometry.Scale;

				const FVector2D DrawSize = FVector2D(RenderTargetWidth, RenderTargetHeight);
				// Update the surface brush to match the latest size.
				SurfaceBrush.ImageSize = DrawSize;

				WidgetRenderer->ViewOffset = -ViewOffset;

				bool bRepaintedWidgets = true;
					
				{
					FSlateRect RenderBoundingRect = AllottedGeometry.GetRenderBoundingRect();

					// Copy below texture
					Context.WindowElementList->PushClip(FSlateClippingZone(AllottedGeometry));

					if (PostProcessDrawer->InitializePostProcessParams(*Context.WindowElementList, Context.IncomingLayerId, PaintGeometry, RenderTarget))
					{
						FSlateDrawElement::MakeCustom(*Context.WindowElementList, Context.IncomingLayerId, PostProcessDrawer);
					}

					Context.WindowElementList->PopClip();

				}
				
				bRenderRequested = false;
				Shared_WaitingToRender.Remove(this);

				LastDrawTime = FApp::GetCurrentTime();

				return bRepaintedWidgets ? EPaintBackgroundResult::Painted : EPaintBackgroundResult::NotPainted;
			}
		}
	}

	return EPaintBackgroundResult::NotPainted;
}

int32 SPostProcessWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	STAT(FScopeCycleCounter PaintCycleCounter(MyStatId););

	SPostProcessWidget* MutableThis = const_cast<SPostProcessWidget*>(this);

	{
		SCOPE_CYCLE_COUNTER(STAT_SlatePostProcessWidgetPaint);

		FSlateInvalidationContext Context(OutDrawElements, InWidgetStyle);
		Context.bParentEnabled = bParentEnabled;
		Context.bAllowFastPathUpdate = true;
		Context.LayoutScaleMultiplier = GetPrepassLayoutScaleMultiplier();
		Context.PaintArgs = &Args;
		Context.IncomingLayerId = LayerId;
		Context.CullingRect = MyCullingRect;

		EPaintBackgroundResult PaintResult = MutableThis->PaintBackgroundImpl(Context, AllottedGeometry);

		if (PaintResult != EPaintBackgroundResult::InvalidSize)
		{
			UTextureRenderTarget2D* RenderTarget = RenderingResources->RenderTarget;
			check(RenderTarget);

			if (RenderTarget->GetSurfaceWidth() >= 1 && RenderTarget->GetSurfaceHeight() >= 1)
			{
				const FLinearColor ComputedColorAndOpacity(Context.WidgetStyle.GetColorAndOpacityTint() * /*ColorAndOpacity.Get() **/ SurfaceBrush.GetTint(Context.WidgetStyle));
				// PostProcess Widget uses pre-multiplied alpha, so pre-multiply the color by the alpha to respect opacity.
				const FLinearColor PremultipliedColorAndOpacity(ComputedColorAndOpacity * ComputedColorAndOpacity.A);

				FWidgetRenderer* WidgetRenderer = RenderingResources->WidgetRenderer;
				UMaterialInstanceDynamic* DynamicEffect = RenderingResources->DynamicEffect;

				const bool bDynamicMaterialInUse = (DynamicEffect != nullptr);
				if (bDynamicMaterialInUse)
				{
					DynamicEffect->SetTextureParameterValue(DynamicEffectTextureParameter, RenderTarget);
				}

				FSlateDrawElement::MakeBox(
					*Context.WindowElementList,
					Context.IncomingLayerId + 1,
					AllottedGeometry.ToPaintGeometry(),
					&SurfaceBrush,
					// We always write out the content in gamma space, so when we render the final version we need to
					// render without gamma correction enabled.
					ESlateDrawEffect::PreMultipliedAlpha | ESlateDrawEffect::NoGamma,
					FLinearColor(PremultipliedColorAndOpacity.R, PremultipliedColorAndOpacity.G, PremultipliedColorAndOpacity.B, PremultipliedColorAndOpacity.A)
				);
			}
		}
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

FVector2D SPostProcessWidget::ComputeDesiredSize(float LayoutScaleMuliplier) const
{
	if ( bEnablePostProcessRendering )
	{
		return MyWidget->GetDesiredSize();
	}
	else
	{
		return SCompoundWidget::ComputeDesiredSize(LayoutScaleMuliplier);
	}
}

bool SPostProcessWidget::CustomPrepass(float LayoutScaleMultiplier)
{
	if (bEnablePostProcessRendering)
	{
		if (NeedsPrepass())
		{
			FChildren* Children = SCompoundWidget::GetChildren();
			Prepass_ChildLoop(LayoutScaleMultiplier, Children);
		}
		return false;
	}
	else
	{
		return true;
	}
}

UTexture* SPostProcessWidget::GetTexture() const
{
	if (RenderingResources)
	{
		return RenderingResources->RenderTarget;
	}
	return nullptr;
}

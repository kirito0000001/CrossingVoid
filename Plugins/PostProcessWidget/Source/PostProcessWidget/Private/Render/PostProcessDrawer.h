// Copyright Qibo Pang 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Rendering/RenderingCommon.h"
#include "CanvasTypes.h"
#include "Widgets/SLeafWidget.h"
#include "PostProcessWidgetDefine.h"

class FRHICommandListImmediate;
class UTextureRenderTarget2D;

struct FPostProcessRectParams
{
	FTextureRHIRef SourceTexture;
	FTextureRHIRef TargetTexture;
	FSlateRect SourceRect;
	FSlateRect DestRect;
	FIntPoint SourceTextureSize;
	TFunction<void(FRHICommandListImmediate&, FGraphicsPipelineStateInitializer&)> RestoreStateFunc;
	TFunction<void()> RestoreStateFuncPostPipelineState;
};


/**
 * Custom Slate drawer to render a backgroundblur with mask
 */
class FPostProcessDrawer : public ICustomSlateElement
{
public:
	FPostProcessDrawer();
	~FPostProcessDrawer();

	bool InitializePostProcessParams(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, UTextureRenderTarget2D* InRenderTarget);

private:
	/**
	 * ICustomSlateElement interface 
	 */
	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override;

	const FSlateClippingState* ResolveClippingState(FSlateWindowElementList& ElementList) const;

private:
	
	/** HMD layer ID */
	uint32 LayerID;
	/** true if the RenderThreadCanvas rendered elements last frame */
	bool bCanvasRenderedLastFrame;

	bool bApplyColorDeficiencyCorrection;

	FVector4 QuadPositionData;
	FVector4 CachedQuadPositionData;
	const FSlateClippingState* ClippingState;

	UTextureRenderTarget2D* RenderTarget;

	FTextureResource* CachedCopyTextureResource = nullptr;
	TRefCountPtr<IPooledRenderTarget> CopyPooledTexture = nullptr;
};

// Copyright Qibo Pang 2023. All Rights Reserved.

#include "PostProcessDrawer.h"
#include "RenderingThread.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "EngineModule.h"
#include "Framework/Application/SlateApplication.h"
#include "PostProcessShaders.h"
#include "ScreenRendering.h"
#include "CommonRenderResources.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "ScreenPass.h"
#include "Layout/Clipping.h"

#define INVALID_LAYER_ID UINT_MAX

static const FName RendererModuleName("Renderer");

FPostProcessDrawer::FPostProcessDrawer()
	: LayerID(INVALID_LAYER_ID)
{
	
}

FPostProcessDrawer::~FPostProcessDrawer()
{
	if (CopyPooledTexture)
	{
		CopyPooledTexture.SafeRelease();
		CopyPooledTexture = nullptr;
	}
}

static bool ShouldCullWidget(const FSlateWindowElementList& ElementList)
{
	const FSlateClippingManager& ClippingManager = ElementList.GetClippingManager();
	const int32 CurrentIndex = ClippingManager.GetClippingIndex();
	if (CurrentIndex != INDEX_NONE)
	{
		const FSlateClippingState& ClippingState = ClippingManager.GetClippingStates()[CurrentIndex];
		return ClippingState.HasZeroArea();
	}

	return false;
}

bool FPostProcessDrawer::InitializePostProcessParams(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, UTextureRenderTarget2D* InRenderTarget)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCullWidget(ElementList))
	{
		return false;
	}

	const FSlateRenderTransform& RenderTransform = PaintGeometry.GetAccumulatedRenderTransform();
	const FVector2D& LocalSize = PaintGeometry.GetLocalSize();

	//@todo doesn't work with rotated or skewed objects yet
	const FVector2D& Position = FVector2D(PaintGeometry.DrawPosition);

	const int32 Layer = InLayer;

	// Determine the four corners of the quad
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D(LocalSize.X, 0);
	FVector2D BotLeft = FVector2D(0, LocalSize.Y);
	FVector2D BotRight = FVector2D(LocalSize.X, LocalSize.Y);

	FVector2D WorldTopLeft = TransformPoint(RenderTransform, TopLeft).RoundToVector();
	FVector2D WorldBotRight = TransformPoint(RenderTransform, BotRight).RoundToVector();

	FVector2D WindowSize = ElementList.GetPaintWindow()->GetViewportSize();;
	FVector2D SizeUV = (WorldBotRight - WorldTopLeft) / WindowSize;

	const FSlateClippingState* ClipState = ResolveClippingState(ElementList);

	RenderTarget = InRenderTarget;

	// These could be negative with rotation or negative scales.  This is not supported yet
	if (SizeUV.X > 0 && SizeUV.Y > 0)
	{
		ClippingState = ClipState;
		QuadPositionData = FVector4(WorldTopLeft, WorldBotRight);

		return true;
	}

	return false;
}

const FSlateClippingState* FPostProcessDrawer::ResolveClippingState(FSlateWindowElementList& ElementList) const
{
	FClipStateHandle ClipHandle;
	ClipHandle.SetPreCachedClipIndex(ElementList.GetClippingIndex());

	const TArray<FSlateClippingState>* PrecachedClippingStates = &ElementList.GetClippingManager().GetClippingStates();

	// Do cached first
	if (ClipHandle.GetCachedClipState())
	{
		// We should be working with cached elements if we have a cached clip state
		//check(ElementList);
		return ClipHandle.GetCachedClipState();
	}
	else if (PrecachedClippingStates->IsValidIndex(ClipHandle.GetPrecachedClipIndex()))
	{
		// Store the clipping state so we can use it later for rendering.
		return &(*PrecachedClippingStates)[ClipHandle.GetPrecachedClipIndex()];
	}

	return nullptr;
}

struct FSlateClippingOp
{
	union
	{
		struct
		{
			FSlateRect Rect;
		} Data_Scissor;

		struct
		{
			TConstArrayView<FSlateClippingZone> Zones;
		} Data_Stencil;
	};

	FVector2f Offset;
	EClippingMethod Method;
	uint8 MaskingId;

	static inline FSlateClippingOp* Scissor(FRDGBuilder& GraphBuilder, FVector2f Offset, FSlateRect Rect)
	{
		FSlateClippingOp* Op = GraphBuilder.AllocPOD<FSlateClippingOp>();
		Op->Data_Scissor.Rect = Rect;
		Op->Offset = Offset;
		Op->Method = EClippingMethod::Scissor;
		Op->MaskingId = 0;
		return Op;
	}

	static inline FSlateClippingOp* Stencil(FRDGBuilder& GraphBuilder, FVector2f Offset, TConstArrayView<FSlateClippingZone> Zones, int32 MaskingId)
	{
		FSlateClippingOp* Op = GraphBuilder.AllocPOD<FSlateClippingOp>();
		Op->Data_Stencil.Zones = Zones;
		Op->Offset = Offset;
		Op->Method = EClippingMethod::Stencil;
		Op->MaskingId = MaskingId;
		return Op;
	}
};

enum class ESlateClippingStencilAction : uint8
{
	None,
	Write,
	Clear
};

struct FSlateClippingCreateContext
{
	uint32 NumStencils = 0;
	uint32 NumScissors = 0;
	uint32 MaskingId = 0;
	ESlateClippingStencilAction StencilAction = ESlateClippingStencilAction::None;
};

const FSlateClippingOp* CreateSlateClippingNative(FRDGBuilder& GraphBuilder, const FVector2f ElementsOffset, const FSlateClippingState* ClippingState, FSlateClippingCreateContext& Context)
{
	Context.StencilAction = ESlateClippingStencilAction::None;

	if (ClippingState)
	{
		if (ClippingState->GetClippingMethod() == EClippingMethod::Scissor)
		{
			Context.NumScissors++;

			const FSlateClippingZone& ScissorRect = ClippingState->ScissorRect.GetValue();

			return FSlateClippingOp::Scissor(GraphBuilder, ElementsOffset, FSlateRect(ScissorRect.TopLeft.X, ScissorRect.TopLeft.Y, ScissorRect.BottomRight.X, ScissorRect.BottomRight.Y));
		}
		else
		{
			Context.NumStencils++;

			TConstArrayView<FSlateClippingZone> StencilQuads = ClippingState->StencilQuads;
			check(StencilQuads.Num() > 0);

			// Reset the masking ID back to zero if stencil is going to overflow.
			if (Context.MaskingId + StencilQuads.Num() > 255)
			{
				Context.MaskingId = 0;
			}

			// Mark stencil for clear when the masking id is 0.
			Context.StencilAction = Context.MaskingId == 0 ? ESlateClippingStencilAction::Clear : ESlateClippingStencilAction::Write;

			const FSlateClippingOp* Op = FSlateClippingOp::Stencil(GraphBuilder, ElementsOffset, StencilQuads, Context.MaskingId);
			Context.MaskingId += StencilQuads.Num();
			return Op;
		}
	}
	return nullptr;
}

struct FSlatePostProcessCopyRectPassInputs
{
	FRDGTexture* InputTexture;
	FRDGTexture* OutputTexture;

	FIntRect InputRect;

	const FSlateClippingOp* ClippingOp = nullptr;
	const FDepthStencilBinding* ClippingStencilBinding = nullptr;
	FIntRect ClippingElementsViewRect;
};

bool GetCopyClippingPipelineState(const FSlateClippingOp* ClippingStateOp, FRHIDepthStencilState*& OutDepthStencilState, uint8& OutStencilRef)
{
	if (ClippingStateOp && ClippingStateOp->Method == EClippingMethod::Stencil)
	{
		// Setup the stenciling state to be read only now, disable depth writes, and restore the color buffer
		// because we're about to go back to rendering widgets "normally", but with the added effect that now
		// we have the stencil buffer bound with a bunch of clipping zones rendered into it.
		OutDepthStencilState =
			TStaticDepthStencilState<
			/*bEnableDepthWrite*/ false
			, /*DepthTest*/ CF_Always
			, /*bEnableFrontFaceStencil*/ true
			, /*FrontFaceStencilTest*/ CF_Equal
			, /*FrontFaceStencilFailStencilOp*/ SO_Keep
			, /*FrontFaceDepthFailStencilOp*/ SO_Keep
			, /*FrontFacePassStencilOp*/ SO_Keep
			, /*bEnableBackFaceStencil*/ true
			, /*BackFaceStencilTest*/ CF_Equal
			, /*BackFaceStencilFailStencilOp*/ SO_Keep
			, /*BackFaceDepthFailStencilOp*/ SO_Keep
			, /*BackFacePassStencilOp*/ SO_Keep
			, /*StencilReadMask*/ 0xFF
			, /*StencilWriteMask*/ 0xFF>::GetRHI();

		// Set a StencilRef equal to the number of stenciling/clipping masks, so unless the pixel we're rendering
		// to is on top of a stencil pixel with the same number it's going to get rejected, thereby clipping
		// everything except for the cross-section of all the stenciling quads.
		OutStencilRef = ClippingStateOp->MaskingId + ClippingStateOp->Data_Stencil.Zones.Num();
		return true;
	}
	else
	{
		OutDepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		OutStencilRef = 0;
		return false;
	}
}

void CopysampleRect(FRDGBuilder& GraphBuilder, const FSlatePostProcessCopyRectPassInputs& Inputs)
{
	FScreenPassTexture CopyInputTexture(Inputs.InputTexture, Inputs.InputRect);

	const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessCopysamplePS> PixelShader(ShaderMap);
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);

	FScreenPassTextureViewport OutputViewport;
	OutputViewport.Extent = Inputs.OutputTexture->Desc.Extent;
	OutputViewport.Rect = FIntRect(0, 0, OutputViewport.Extent.X, OutputViewport.Extent.Y);

	FScreenPassTextureViewport InputViewport;
	InputViewport.Extent = Inputs.InputTexture->Desc.Extent;
	InputViewport.Rect = Inputs.InputRect;

	FPostProcessCopysamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPostProcessCopysamplePS::FParameters>();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(Inputs.OutputTexture, ERenderTargetLoadAction::EClear);
	PassParameters->ElementTexture = CopyInputTexture.Texture;
	PassParameters->ElementTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	// Get BlendState
	FRHIBlendState* BlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
	// Get ViewportSize

	FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState);
	GetCopyClippingPipelineState(Inputs.ClippingOp, PipelineState.DepthStencilState, PipelineState.StencilRef);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("CopysampleRect"),
		PassParameters,
		ERDGPassFlags::Raster,
		[OutputViewport, PixelShader, InputViewport, PassParameters, ClippingElementsViewRect = Inputs.ClippingElementsViewRect, PipelineState, ClippingOp = Inputs.ClippingOp](FRHICommandListImmediate& RHICmdList)
		{

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

			SetScreenPassPipelineState(RHICmdList, PipelineState);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
			DrawScreenPass_PostSetup(RHICmdList, FScreenPassViewInfo(), OutputViewport, InputViewport, PipelineState, EScreenPassDrawFlags::None);
		});
}

void FPostProcessDrawer::Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs)
{
	if (!TStaticDepthStencilState<false, CF_Always>::GetRHI())
	{
		return;
	}

	// Provided output texture is actually the input into our custom post process texture.
	const FScreenPassTexture InputTexture(Inputs.OutputTexture, Inputs.SceneViewRect);

	FSlateClippingCreateContext Context;
	FVector2f ElementsOffset = Inputs.ElementsOffset;
	const FSlateClippingOp* ClippingOp = CreateSlateClippingNative(GraphBuilder, ElementsOffset, ClippingState, Context);

	{
		FSlatePostProcessCopyRectPassInputs CopyInputs;
		CopyInputs.InputRect = FIntRect(QuadPositionData.X, QuadPositionData.Y, QuadPositionData.Z, QuadPositionData.W);
		CopyInputs.InputTexture = Inputs.OutputTexture;

		CopyInputs.ClippingOp = ClippingOp;
		CopyInputs.ClippingElementsViewRect = CopyInputs.InputRect;

		UTexture* OutputTexture = RenderTarget;
		FTextureResource* CopyTextureResource;
		if (OutputTexture)
		{
			CopyTextureResource = OutputTexture->GetResource();
		}
		else
		{
			CopyTextureResource = nullptr;
		}

		if (CopyTextureResource)
		{
			bool bInitTexture = CopyPooledTexture == nullptr;

			if ((CachedCopyTextureResource != CopyTextureResource
				|| QuadPositionData != CachedQuadPositionData) && CopyPooledTexture)
			{
				CopyPooledTexture.SafeRelease();
				CopyPooledTexture = nullptr;
			}

			if (!CopyPooledTexture)
			{
				if (!bInitTexture)
				{
					CachedQuadPositionData = QuadPositionData;
					CachedCopyTextureResource = CopyTextureResource;
				}
				CopyPooledTexture = CreateRenderTarget(CopyTextureResource->TextureRHI, TEXT("PostProcessCopyTexture"));
			}
			CopyInputs.OutputTexture = GraphBuilder.RegisterExternalTexture(CopyPooledTexture);
		}

		if (CopyInputs.OutputTexture)
		{
			CopysampleRect(GraphBuilder, CopyInputs);
		}
	}
}






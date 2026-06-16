// Copyright Qibo Pang 2023. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "RHI.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Runtime/RenderCore/Public/ShaderParameterUtils.h"
#include "Runtime/RenderCore/Public/RenderResource.h"
#include "Runtime/Renderer/Public/MaterialShader.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"
#include "Runtime/Renderer/Public/ScreenPass.h"
#include "Runtime/Renderer/Private/SceneTextureParameters.h"

#include "RenderResource.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Rendering/RenderingCommon.h"
#include "RHIStaticStates.h"
#include "ShaderPermutation.h"

class FPostProcessCopysamplePS : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FPostProcessCopysamplePS);
	SHADER_USE_PARAMETER_STRUCT(FPostProcessCopysamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ElementTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ElementTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

};
// Copyright Qibo Pang 2023. All Rights Reserved.

#include "PostProcessShaders.h"
#include "Rendering/RenderingCommon.h"
#include "PipelineStateCache.h"

IMPLEMENT_GLOBAL_SHADER(FPostProcessCopysamplePS, "/Plugin/PostProcessWidget/Private/PostProcessPixelShader.usf", "CopysampleMain", SF_Pixel);
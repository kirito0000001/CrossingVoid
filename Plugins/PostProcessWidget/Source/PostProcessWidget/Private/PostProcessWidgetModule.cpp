// Copyright Qibo Pang 2023. All Rights Reserved.


#include "PostProcessWidgetModule.h"
#include "PostProcessWidgetDefine.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FPostProcessWidgetModule"

DEFINE_LOG_CATEGORY(LogPostProcessWidget);

void FPostProcessWidgetModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("PostProcessWidget"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/PostProcessWidget"), PluginShaderDir);
}

void FPostProcessWidgetModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPostProcessWidgetModule, PostProcessWidget)
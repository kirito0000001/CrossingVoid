// AtlasFX —— 模块入口实现
#include "AtlasFXModule.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAtlasFXModule"

void FAtlasFXModule::StartupModule()
{
	// 第一步（CPU sim）不需要 shader。
	//
	// 第二步做 GPU 支持时在这里加：
	//   const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("AtlasFX"))->GetBaseDir(), TEXT("Shaders"));
	//   AddShaderSourceDirectoryMapping(TEXT("/Plugin/AtlasFX"), ShaderDir);
	// 两条硬约束（源码取证）：
	//   * ShaderCore.cpp:4237 `check(!GShaderSourceDirectoryMappings.Contains(VirtualShaderDirectory));`
	//     —— 虚拟目录名必须全局唯一，重复注册会 check 崩。
	//   * ShaderCore.cpp:4218-4225：cook 后的游戏里 RequiresCookedData() 直接 return，
	//     所以 HLSL 必须在 cook 期就编进 shader library，别指望运行时加载 .ush。
}

void FAtlasFXModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAtlasFXModule, AtlasFX)

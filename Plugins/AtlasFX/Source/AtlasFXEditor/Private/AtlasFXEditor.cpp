// AtlasFXEditor 模块实现。
//
// 模块干的活分两块：
//   1. 命令行工具 AtlasFXSetup（见 AtlasFXSetupCommandlet.cpp）—— 用 UCommandlet 的静态注册，
//      UnrealEditor-Cmd 的 -run= 按名字就能找到它；
//   2. PaperZD 序列编辑器的特效预览扩展（见 AtlasFXPaperZDPreview.h）—— 需要模块有启动/卸载
//      时机来挂编辑器 ticker，所以这里不能再图省事用 FDefaultModuleImpl。

#include "AtlasFXPaperZDPreview.h"
#include "Modules/ModuleManager.h"

class FAtlasFXEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FAtlasFXPaperZDPreview::Startup();
	}

	virtual void ShutdownModule() override
	{
		FAtlasFXPaperZDPreview::Shutdown();
	}
};

IMPLEMENT_MODULE(FAtlasFXEditorModule, AtlasFXEditor);

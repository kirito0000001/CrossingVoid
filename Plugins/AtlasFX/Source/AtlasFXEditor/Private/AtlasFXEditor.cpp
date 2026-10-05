// AtlasFXEditor 模块实现。
//
// 模块本身不需要启动逻辑：真正的活由命令行 AtlasFXSetup 干（见 AtlasFXSetupCommandlet.cpp）。
// 用 FDefaultModuleImpl 就够了 —— UCommandlet 子类靠 UObject 的静态注册，模块一加载
// UnrealEditor-Cmd 的 -run= 就能按名字找到它。

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, AtlasFXEditor);

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** ZDBridge 的运行时壳模块：没有导出函数，只保证插件在游戏里被加载、Content 被视为运行时资产。 */
class FZDBridgeRuntimeModule : public IModuleInterface
{
};

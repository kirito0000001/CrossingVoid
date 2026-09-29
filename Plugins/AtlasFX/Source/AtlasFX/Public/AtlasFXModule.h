// AtlasFX —— 模块入口
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FAtlasFXModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

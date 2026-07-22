// Copyright Qibo Pang 2023. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FPostProcessWidgetModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

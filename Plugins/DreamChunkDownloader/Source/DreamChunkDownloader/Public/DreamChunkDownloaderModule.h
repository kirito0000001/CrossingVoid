// Copyright (C) 2025 Dream Moon, All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FDreamChunkDownloaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

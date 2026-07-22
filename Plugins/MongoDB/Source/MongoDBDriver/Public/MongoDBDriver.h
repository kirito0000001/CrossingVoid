// 2025 Copyright Pandores Marketplace

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#	pragma comment(lib, "Dnsapi.lib")
#	pragma comment(lib, "Secur32.lib")
#	pragma comment(lib, "Bcrypt.lib")
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogMongoDB, Log, Log);

class MONGODBDRIVER_API FMongoDBDriverModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

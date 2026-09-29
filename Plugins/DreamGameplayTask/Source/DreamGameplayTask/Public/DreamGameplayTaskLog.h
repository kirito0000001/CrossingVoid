#pragma once

#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDreamGameplayTask, All, All);

#define DGT_LOG(V, F, ...) UE_LOG(LogDreamGameplayTask, V, F, ##__VA_ARGS__)
#define DGT_DEBUG_LOG(V, F, ...) if (UDreamGameplayTaskSetting::GetEnableDebug()) { UE_LOG(LogDreamGameplayTask, V, TEXT("[%hs] : " F), __FUNCTION__, ##__VA_ARGS__) }
#define DGT_UPDATER_DEBUG_LOG(V, F, ...) if (UDreamGameplayTaskSetting::GetEnableUpdaterDebug()) { DGT_LOG(V, TEXT("[%hs] : " F), __FUNCTION__, ##__VA_ARGS__) }

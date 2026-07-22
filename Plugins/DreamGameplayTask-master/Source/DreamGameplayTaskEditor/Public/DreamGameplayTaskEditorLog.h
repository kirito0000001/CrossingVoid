#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogDreamGameplayTaskEditor, All, All)

#define DGT_ED_LOG(V, F, ...) UE_LOG(LogDreamGameplayTaskEditor, V, F, ##__VA_ARGS__)
#define DGT_ED_FLOG(V, F, ...) UE_LOG(LogDreamGameplayTaskEditor, V, TEXT("[%hs] " F), __FUNCTION__, ##__VA_ARGS__)
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define DREAMGAMEPLAY_TASK_STYLENAME TEXT("DreamGameplayTaskEditorStyle")

class FDreamGameplayTaskEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterCommand();
    void MakeCommandList();
    TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);
    
    FName GetTaskManagerName();

    // Menu Builder
    void RegisterMenu();
    void MakeMenu(FMenuBuilder& MenuBuilder);

    TSharedPtr<class FUICommandList> CommandList;
};

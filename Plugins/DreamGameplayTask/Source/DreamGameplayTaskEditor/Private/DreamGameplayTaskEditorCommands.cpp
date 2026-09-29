#include "DreamGameplayTaskEditorCommands.h"
#include "DreamGameplayTaskEditorModule.h"

#define LOCTEXT_NAMESPACE "FDreamGameplayTaskEditorCommands"

FDreamGameplayTaskEditorCommands::FDreamGameplayTaskEditorCommands() : TCommands<FDreamGameplayTaskEditorCommands>(TEXT("DreamGameplayTaskEditor"), NSLOCTEXT("Context", "DreamGameplayTaskEditor", "DreamGameplayTaskEditor Plugin"), NAME_None, DREAMGAMEPLAY_TASK_STYLENAME)
{
	
}

void FDreamGameplayTaskEditorCommands::RegisterCommands()
{
	UI_COMMAND(CreateTask, "Create Task", "Create a new task", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateTaskType, "Create Task Type", "Create a new task type", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateCondition, "Create Task Condition Template", "Create a new task condition template", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenManager, "Open Task Manager", "Open the task manager", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE


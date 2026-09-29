#pragma once

#include "Framework/Commands/Commands.h"

class FDreamGameplayTaskEditorCommands : public TCommands<FDreamGameplayTaskEditorCommands>
{
public:
	FDreamGameplayTaskEditorCommands();
	virtual void RegisterCommands() override;
public:
	TSharedPtr<FUICommandInfo> CreateTask;
	TSharedPtr<FUICommandInfo> CreateTaskType;
	TSharedPtr<FUICommandInfo> CreateCondition;

	TSharedPtr<FUICommandInfo> OpenManager;
};

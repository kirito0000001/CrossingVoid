#include "DreamGameplayTaskEditorModule.h"

#include "ContentBrowserModule.h"
#include "DreamGameplayTaskEditorCommands.h"
#include "DreamGameplayTaskEditorLog.h"
#include "DreamGameplayTaskEditorSetting.h"
#include "DreamGameplayTaskEditorTools.h"
#include "DreamGameplayTaskEditorStyle.h"
#include "Manager/DreamTaskManager.h"
#include "LevelEditor.h"
#include "Classes/DreamTaskType.h"
#include "Classes/DreamTaskConditionTemplate.h"

#define LOCTEXT_NAMESPACE "FDreamGameplayTaskEditorModule"

void FDreamGameplayTaskEditorModule::StartupModule()
{
	FDreamGameplayTaskEditorStyle::Initialize();
	FDreamGameplayTaskEditorStyle::Register();

	RegisterCommand();
	MakeCommandList();
	RegisterMenu();
}

void FDreamGameplayTaskEditorModule::ShutdownModule()
{
	FDreamGameplayTaskEditorStyle::Unreginster();
}

void FDreamGameplayTaskEditorModule::RegisterCommand()
{
	if (!CommandList.IsValid())
	{
		CommandList = MakeShareable(new FUICommandList);
	}
	FDreamGameplayTaskEditorCommands::Register();
}

void FDreamGameplayTaskEditorModule::MakeCommandList()
{
	auto EditorCommand = FDreamGameplayTaskEditorCommands::Get();

	CommandList->MapAction(
		EditorCommand.CreateTask,
		FExecuteAction::CreateLambda([]()
		{
			FDreamGameplayTaskEditorTools::CreateObjectBlueprintByClass(
				UDreamGameplayTaskEditorSetting::Get()->GetCreateTaskClass(), FString("NewTask"));
		})
	);

	CommandList->MapAction(
		EditorCommand.CreateTaskType,
		FExecuteAction::CreateLambda([]()
		{
			FDreamGameplayTaskEditorTools::CreateDataAssetByClass(
				UDreamTaskType::StaticClass(), FString("NewTaskType"));
		}));

	CommandList->MapAction(
		EditorCommand.CreateCondition,
		FExecuteAction::CreateLambda([]()
		{
			FDreamGameplayTaskEditorTools::CreateObjectBlueprintByClass(
				UDreamGameplayTaskEditorSetting::Get()->GetCreateTaskConditionTemplateClass(), FString("NewCondition"));
		}));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GetTaskManagerName(),
	                                                  FOnSpawnTab::CreateRaw(this, &FDreamGameplayTaskEditorModule::OnSpawnPluginTab))
	                        .SetDisplayName(FText::FromString(TEXT("TaskManager")))
	                        .SetMenuType(ETabSpawnerMenuType::Hidden)
	                        .SetIcon(FSlateIcon(
		                        FSlateIcon(FDreamGameplayTaskEditorStyle::StyleName(),
		                                   TEXT("DreamGameplayTaskEditor.DreamTaskManager.TabIcon"))));

	CommandList->MapAction(
		EditorCommand.OpenManager,
		FExecuteAction::CreateLambda([this]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(GetTaskManagerName());
			}
		));
}

TSharedRef<class SDockTab> FDreamGameplayTaskEditorModule::OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDreamGameplayTaskManager)
		];
	return DockTab;
}

FName FDreamGameplayTaskEditorModule::GetTaskManagerName()
{
	static FName DreamGameplayTaskEditorManagerWindowName = FName(TEXT("DreamTaskManager"));
	return DreamGameplayTaskEditorManagerWindowName;
}

void FDreamGameplayTaskEditorModule::RegisterMenu()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);
	Extender->AddMenuExtension(
		"Tools" /* Hook */,
		EExtensionHook::After /* Hook Position */,
		CommandList /* Commands */,
		FMenuExtensionDelegate::CreateRaw(this, &FDreamGameplayTaskEditorModule::MakeMenu) /* Make Builder */);

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(Extender);
}

void FDreamGameplayTaskEditorModule::MakeMenu(FMenuBuilder& MenuBuilder)
{
	auto Commands = FDreamGameplayTaskEditorCommands::Get();

	MenuBuilder.BeginSection("DGTT_Window", FText::FromString("DGTT_Window"));
	// Add Window...
	MenuBuilder.AddMenuEntry(Commands.OpenManager);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("DGTT_Create", FText::FromString("DGTT_Create"));
	// Add Create...
	MenuBuilder.AddMenuEntry(Commands.CreateTask);
	MenuBuilder.AddMenuEntry(Commands.CreateTaskType);
	MenuBuilder.AddMenuEntry(Commands.CreateCondition);

	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDreamGameplayTaskEditorModule, DreamGameplayTaskEditor)

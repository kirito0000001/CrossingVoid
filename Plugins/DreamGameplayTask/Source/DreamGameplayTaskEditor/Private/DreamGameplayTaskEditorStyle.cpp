#include "DreamGameplayTaskEditorStyle.h"

#include "DreamGameplayTaskEditorModule.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"


TSharedPtr<FSlateStyleSet> FDreamGameplayTaskEditorStyle::StyleInstance = nullptr;

FName FDreamGameplayTaskEditorStyle::StyleName()
{
	return DREAMGAMEPLAY_TASK_STYLENAME;
}

void FDreamGameplayTaskEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
	}
}

void FDreamGameplayTaskEditorStyle::Register()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FDreamGameplayTaskEditorStyle::Unreginster()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

#define IMAGE_BRUSH( RelativePath, ... ) \
	new FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

#define SET_STYLE_CLASS_THUMB(ObjectName, FileName, ...) \
	Style->Set(*FString::Printf(TEXT("ClassThumbnail.%s"), TEXT(ObjectName)), IMAGE_BRUSH(FileName, __VA_ARGS__));

#define SET_STYLE_CLASS_ICON(ObjectName, FileName, ...) \
	Style->Set(*FString::Printf(TEXT("ClassIcon.%s"), TEXT(ObjectName)), IMAGE_BRUSH(FileName, __VA_ARGS__));

#define SET_STYLE_COMMAND_ICON(CommandName, FileName, ...) \
	Style->Set(*FString::Printf(TEXT("DreamGameplayTaskEditor.%s"), TEXT(CommandName)), IMAGE_BRUSH(FileName, __VA_ARGS__));

const FVector2D Icon16x16(16.f, 16.f);
const FVector2D Icon20x20(20.f, 20.f);
const FVector2D Icon40x40(40.f, 40.f);

TSharedRef<FSlateStyleSet> FDreamGameplayTaskEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style(MakeShareable(new FSlateStyleSet(StyleName())));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("DreamGameplayTask")->GetBaseDir() / TEXT("Resources"));

	SET_STYLE_CLASS_ICON("DreamTask", "/Icons/Task", Icon16x16);
	SET_STYLE_CLASS_ICON("DreamTaskConditionTemplate", "/Icons/Condition", Icon16x16);
	SET_STYLE_CLASS_ICON("DreamTaskComponent", "/Icons/TaskComp", Icon16x16);
	SET_STYLE_CLASS_ICON("DreamTaskType", "/Icons/Type", Icon16x16);
	SET_STYLE_CLASS_ICON("DreamTaskData", "/Icons/Data", Icon16x16);

	SET_STYLE_CLASS_THUMB("DreamTask", "/Icons/Task", Icon40x40);
	SET_STYLE_CLASS_THUMB("DreamTaskConditionTemplate", "/Icons/Condition", Icon40x40);
	SET_STYLE_CLASS_THUMB("DreamTaskComponent", "/Icons/Condition", Icon40x40);
	SET_STYLE_CLASS_THUMB("DreamTaskType", "/Icons/Type", Icon40x40);
	SET_STYLE_CLASS_THUMB("DreamTaskData", "/Icons/Data", Icon40x40);
	
	Style->Set("Editor.Toolbar", IMAGE_BRUSH(TEXT("/Icons/EdToolbar"), Icon40x40));

	SET_STYLE_COMMAND_ICON("CreateTask", "/Icons/CreateIcon", Icon16x16);
	SET_STYLE_COMMAND_ICON("CreateCondition", "/Icons/CreateIcon", Icon16x16);
	SET_STYLE_COMMAND_ICON("CreateTaskType", "/Icons/CreateIcon", Icon16x16);
	SET_STYLE_COMMAND_ICON("OpenManager", "/Icons/OpenIcon", Icon16x16);
	SET_STYLE_COMMAND_ICON("DreamTaskManager.TabIcon", "/Icons/TabBrowser", Icon16x16);
	SET_STYLE_COMMAND_ICON("Refresh", "/Icons/Refresh", Icon16x16);
	SET_STYLE_COMMAND_ICON("ForceLoadMemory", "/Icons/ForceLoadMemory", Icon16x16);
	SET_STYLE_COMMAND_ICON("OpenEditor", "/Icons/OpenIcon", Icon16x16);
	SET_STYLE_COMMAND_ICON("CreateTaskIcon", "/Icons/CreateIcon", Icon16x16);
	
	return Style;
}

#undef SET_STYLE_CLASS_ICON
#undef SET_STYLE_CLASS_THUMB
#undef SET_STYLE_COMMAND_ICON

TSharedPtr<ISlateStyle> FDreamGameplayTaskEditorStyle::Get()
{
	return StyleInstance;
}

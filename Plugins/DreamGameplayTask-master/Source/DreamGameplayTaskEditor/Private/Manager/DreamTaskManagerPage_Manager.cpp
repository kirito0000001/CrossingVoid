#include "Manager/DreamTaskManagerPage_Manager.h"

#include "DreamGameplayTaskEditorSetting.h"
#include "DreamGameplayTaskEditorTools.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Manager/DreamTaskManager_Util.h"
#include "Manager/Widgets/DreamTaskManageWidget_ItemRow.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "DreamTaskManagerPage_Manager"

using namespace FDreamTaskManagerUtil::Align;

void SDreamTaskManagerPage_Manager::Construct(const FArguments& InArgs)
{
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	ChildSlot[
		SNew(SOverlay)

		OSLOT("Main Page")
		VA(VFILL)
		HA(HFILL)
		[
			SNew(VB)

			VSLOT("Line 1")
			.AutoHeight()
			.Padding(5.f)
			[
				SNew(HB) // Title

				HSLOT("Title")
				VA(VFILL)
				HA(HRIGHT)
				[
					SNew(HB)

					HSLOT()
					.AutoWidth()
					VA(VFILL)
					HA(HRIGHT)
					.Padding(2.f)
					[
						SNew(SButton)
						.OnClicked_Raw(this, &SDreamTaskManagerPage_Manager::Action_MakeTaskInCurrentPath)
						[
							FDreamTaskManagerUtil::MakeIconAndTextWidget(LOCTEXT("CreateTaskInCurrentPathButton", "Create Task In Current"),
							                                             GET_STYLE_BRUSH("CreateTaskIcon"))
						]
					]


					HSLOT("Create Task")
					.AutoWidth()
					VA(VFILL)
					.Padding(2.f)
					[
						SNew(SButton)
						.OnClicked_Raw(this, &SDreamTaskManagerPage_Manager::Action_MakeTask)
						[
							FDreamTaskManagerUtil::MakeIconAndTextWidget(LOCTEXT("CreateTaskButton", "CreateTask"),
							                                             GET_STYLE_BRUSH("CreateTaskIcon"))
						]
					] // End Create Task

					HSLOT("Force Load")
					VA(VFILL)
					.AutoWidth()
					.Padding(2.f)
					[
						SNew(SButton)
						.OnClicked_Raw(this, &SDreamTaskManagerPage_Manager::Action_ForceLoadMemory)
						[
							FDreamTaskManagerUtil::MakeIconAndTextWidget(LOCTEXT("ForceLoadMemoryButton", "Force Load Memory"),
							                                             GET_STYLE_BRUSH("ForceLoadMemory"))
						]
					] // End Force Load

					HSLOT("Refresh")
					VA(VFILL)
					.AutoWidth()
					.Padding(2.f)
					[
						SNew(SButton)
						.OnClicked_Raw(this, &SDreamTaskManagerPage_Manager::Action_Refresh)
						[
							FDreamTaskManagerUtil::MakeIconAndTextWidget(LOCTEXT("RefreshButton", "Refresh"),
							                                             GET_STYLE_BRUSH("Refresh"))
						]
					] // End Refresh
				] // End Title
			] // End Line 1

			VSLOT("Line 2")
			VA(VFILL)
			HA(HFILL)
			.Padding(5.f)
			[
				SAssignNew(Switcher, SWidgetSwitcher)

				+ SWidgetSwitcher::Slot() // Page 1 Task list
				HA(HFILL)
				VA(VFILL)
				[
					SAssignNew(ListView, SListView<FDreamTaskManagerRowDataPtr>)
					.EnableAnimatedScrolling(true)
					.OnGenerateRow(this, &SDreamTaskManagerPage_Manager::OnGenerateRow)
					.ListItemsSource(&List)
					.HeaderRow(MakeHeaderRow())
					.SelectionMode(ESelectionMode::Type::Single)
				] // End Page 1 Task list

				+ SWidgetSwitcher::Slot() // Page 2 Error Text
				HA(HFILL)
				VA(VFILL)
				[
					SNew(SErrorText)
					.Font(FDreamTaskManagerUtil::GetTextFont(20.f))
					.ErrorText(LOCTEXT("Page1_ErrorText", "No task found, please refresh or create new task."))
				] // End Page 2 Error Text

			] // End Line 2
		] // End Main Page
	];

	if (UDreamGameplayTaskEditorSetting::Get()->bManagerStartupRefresh)
	{
		Refresh();
	}
}

SDreamTaskManagerPage_Manager::~SDreamTaskManagerPage_Manager()
{
	UnregisteredAutoRefreshList();
	List.Empty();
}

void SDreamTaskManagerPage_Manager::Refresh()
{
	Clear(); // First, clear the previous state

	UClass* FindClass = UDreamTask::StaticClass();

	// Ensure assets are loaded correctly
	FDreamGameplayTaskEditorTools::LoadAssetToMemory(FindClass);
	TArray<FAssetData> AssetData = FDreamGameplayTaskEditorTools::GetAssetData(FindClass);

	for (FAssetData& Data : AssetData)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Data.FastGetAsset(true));
		if (!Blueprint || !Blueprint->ParentClass)
			continue;

		if (Blueprint->ParentClass->IsChildOf(FindClass) || Blueprint->ParentClass == FindClass)
		{
			if (FindClass == UDreamTask::StaticClass())
			{
				UDreamTask* Task = Cast<UDreamTask>(Blueprint->GeneratedClass.GetDefaultObject());
				if (Task && Blueprint)
				{
					FDreamTaskManagerRowDataPtr ItemPtr = MakeShared<FDreamTaskManagerRowData>(Task, Blueprint);
					if (ItemPtr.IsValid())
					{
						List.Add(ItemPtr); // Add new tasks to the list
					}
				}
			}
		}
	}

	// Refresh the list after adding new items
	ListView->RequestListRefresh();
	Check(); // Make sure the correct page is displayed
}

void SDreamTaskManagerPage_Manager::Clear()
{
	List.Empty(); // Clear the task list
	ListView->RequestListRefresh(); // Refresh the list to show an empty state
	Check(); // Ensure the correct page is displayed (either list or error message)
}

void SDreamTaskManagerPage_Manager::Check()
{
	if (List.IsEmpty())
	{
		Switcher->SetActiveWidgetIndex(1);
	}
	else
	{
		Switcher->SetActiveWidgetIndex(0);
	}
}

FReply SDreamTaskManagerPage_Manager::Action_Refresh()
{
	Refresh();
	return FReply::Handled();
}

FReply SDreamTaskManagerPage_Manager::Action_ForceLoadMemory()
{
	FDreamGameplayTaskEditorTools::ForceLoadAssetToMemory(UDreamTask::StaticClass());
	Refresh();
	return FReply::Handled();
}

FReply SDreamTaskManagerPage_Manager::Action_MakeTaskInCurrentPath()
{
	UClass* TaskClass = UDreamGameplayTaskEditorSetting::Get()->GetCreateTaskClass();
	UClass* ChosenClass = UDreamGameplayTaskEditorSetting::Get()->GetCreateTaskClass();
	FClassViewerInitializationOptions ClassPickerOption = FClassViewerInitializationOptions();
	ClassPickerOption.Mode = EClassViewerMode::ClassPicker;
	ClassPickerOption.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassPickerOption.bShowUnloadedBlueprints = true;
	
	TSharedRef<FDreamGameplayTaskManagerTaskClassFiler> Filter = MakeShareable(new FDreamGameplayTaskManagerTaskClassFiler);
	ClassPickerOption.ClassFilters.Add(Filter);

	Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract | CLASS_HideDropDown;
	Filter->AllowedChildrenOfClasses.Add(TaskClass);

	const FText TitleText= LOCTEXT("Pick Task Class", "Pick Task Class");
	SClassPickerDialog::PickClass(TitleText, ClassPickerOption, ChosenClass, TaskClass);

	FDreamGameplayTaskEditorTools::CreateObjectBlueprintByClass(ChosenClass, BPTYPE_Normal, true, true);

	return FReply::Handled();
}

FReply SDreamTaskManagerPage_Manager::Action_MakeTask()
{
	UClass* TaskClass = UDreamGameplayTaskEditorSetting::Get()->GetCreateTaskClass();
	TaskClass = TaskClass ? TaskClass : UDreamTask::StaticClass();
	UBlueprint* Blueprint = FDreamGameplayTaskEditorTools::CreateObjectBlueprintByClass(TaskClass, FString("NewTask"));

	if (!Blueprint) return FReply::Handled();

	return FReply::Handled();
}

void SDreamTaskManagerPage_Manager::ListerAssetRemoved(const FAssetData& Data)
{
	Clear();
}

void SDreamTaskManagerPage_Manager::ListerAssetSaved(const FString& PackageName, UPackage* Package, FObjectPostSaveContext ObjectSavedContext)
{
	FSoftObjectPath Path(Package);
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Path.TryLoad()))
	{
		if (Blueprint->ParentClass->IsChildOf(UDreamTask::StaticClass()))
		{
			Refresh();
		}
	}
}

void SDreamTaskManagerPage_Manager::RegisterAutoRefreshList()
{
	UPackage::PackageSavedWithContextEvent.AddRaw(this, &SDreamTaskManagerPage_Manager::ListerAssetSaved);
	AssetRegistryModule->Get().OnAssetRemoved().AddRaw(this, &SDreamTaskManagerPage_Manager::ListerAssetRemoved);
}

void SDreamTaskManagerPage_Manager::UnregisteredAutoRefreshList()
{
	UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	AssetRegistryModule->Get().OnAssetRemoved().RemoveAll(this);
}

TSharedRef<ITableRow> SDreamTaskManagerPage_Manager::OnGenerateRow(FDreamTaskManagerRowDataPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SDreamTaskManageWidget_ItemRow, OwnerTable)
		.ItemShow(InItem);
}

TSharedRef<SHeaderRow> SDreamTaskManagerPage_Manager::MakeHeaderRow()
{
	return SNew(SHeaderRow)
			BUILD_HEADER(TEXT("TaskName"), LOCTEXT("Header_TaskName", "Task Name"))
			BUILD_HEADER(TEXT("TaskDisplayName"), LOCTEXT("Header_TaskDisplayName", "Task Display Name"))
			BUILD_HEADER(TEXT("TaskAction"), LOCTEXT("Header_TaskAction", "Task Action")).HAlignCell(HFILL).VAlignCell(VFILL);
}

#undef LOCTEXT_NAMESPACE

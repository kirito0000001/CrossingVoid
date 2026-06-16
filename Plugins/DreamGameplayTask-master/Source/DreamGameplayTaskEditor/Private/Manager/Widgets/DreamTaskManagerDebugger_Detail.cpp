#include "Manager/Widgets/DreamTaskManagerDebugger_Detail.h"

#include "DreamGameplayTaskEditorLog.h"
#include "DreamGameplayTaskEditorTools.h"
#include "Components/ListView.h"

#include "Classes/DreamTaskComponent.h"
#include "DreamGameplayTaskTypes.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Manager/Widgets/DreamTaskManagerDebugger_Detail_SpecRow.h"
#include "Manager/DreamTaskManager_Util.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Editor/ClassViewer/Public/ClassViewerFilter.h"

#define LOCTEXT_NAMESPACE "DreamTaskManagerDebugger_Detail_SpecRow"

#define CHECK_COMPONENT (Component.IsValid() && Component.Get()->IsValid() && Component.Get()->Get() != nullptr)

void SDreamTaskManagerDebugger_Detail::Construct(const FArguments& InArgs)
{
	ChildSlot[
		SNew(SOverlay)

		OSLOT()
		[
			SNew(VB)

			VSLOT()
			.AutoHeight()
			.Padding(5.f)
			[
				SNew(HB)

				// Start Tile Component Picker
				HSLOT()
				[
					SNew(TB)
					.Font(FDreamTaskManagerUtil::GetTextFont(12.f))
					.Text_Lambda([this]()
					{
						if (CHECK_COMPONENT)
						{
							return MAKE_TEXT(
								TEXT("Owner : %15s -> Task Component : %s"),
								*Component.Get()->Get()->GetOwner()->GetName(),
								*Component.Get()->Get()->GetName())							;
						}
						else
						{
							return MAKE_TEXT(TEXT("Empty"));
						}
					})
				]

				// Start Refresh Button
				HSLOT()
				.AutoWidth()
				HA(HRIGHT)
				[
					SNew(SButton)
					.Text(MAKE_TEXT(TEXT("Refresh")))
					.OnClicked_Lambda([this]()
					{
						Refresh();
						return FReply::Handled();
					})
				]

				// Start Give Task Button
				HSLOT()
				.AutoWidth()
				HA(HRIGHT)
				[
					SNew(SButton)
					.Text(LOCTEXT("GiveTask", "Give Task"))
					.OnClicked_Lambda([this]()
					{
						if (!CHECK_COMPONENT) return FReply::Handled();

						UClass* Class = UDreamTask::StaticClass();
						UClass* ChosenClass = UDreamTask::StaticClass();

						FClassViewerInitializationOptions Options;
						Options.DisplayMode = EClassViewerDisplayMode::Type::TreeView;
						Options.Mode = EClassViewerMode::ClassPicker;
						Options.bShowNoneOption = false;
						Options.bExpandAllNodes = true;

						TSharedPtr<FDreamGameplayTaskManagerTaskClassFiler> Filter = MakeShareable(
							new FDreamGameplayTaskManagerTaskClassFiler);
						Options.ClassFilters.Add(Filter.ToSharedRef());

						Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract |
							CLASS_HideDropDown;
						Filter->AllowedChildrenOfClasses.Add(Class);

						const FText TitleText = LOCTEXT("GiveTask", "Pick Task Class");
						SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, Class);

						GET_TSharedPtr(Component)->GiveTaskByClass(ChosenClass);

						return FReply::Handled();
					})
				]
			]

			// Start List View
			VSLOT()
			HA(HFILL)
			VA(VFILL)
			[
				SAssignNew(Switcher, SWidgetSwitcher)

				// Empty List
				+ SWidgetSwitcher::Slot()
				HA(HCENTER)
				VA(VCENTER)
				[
					SNew(TB)
					.Font(FDreamTaskManagerUtil::GetTextFont(15.f))
					.Text(LOCTEXT("EmptyText", "No task, task array is empty."))
				]

				//  List
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(ListView, SListView<FDreamManagerAccessKey>)
					.ListItemsSource(&Handles)
					.EnableAnimatedScrolling(true)
					.OnGenerateRow(this, &SDreamTaskManagerDebugger_Detail::OnGenerateRow)
					.HeaderRow(MakeHeaderRow())
				]
			]
		]
	];
}

void SDreamTaskManagerDebugger_Detail::SetComponent(FDreamManagerTaskComponent InComponent)
{
	if (CHECK_COMPONENT)
	{
		(*Component)->OnTaskListChangedDelegate.RemoveAll(this);
	}

	// Check in component
	if (!(InComponent.IsValid() && InComponent.Get()->IsValid() && InComponent.Get()->Get() != nullptr))
		return;

	// Set component
	Component = InComponent;

	// Add delegate
	if (Component.IsValid())
	{
		ComponentDelegateHandle = (*Component)->OnTaskListChangedDelegate.AddSP(
			this, &SDreamTaskManagerDebugger_Detail::HandleTaskListChanged);
	}

	HandleTaskListChanged((*Component)->TaskData);
}

void SDreamTaskManagerDebugger_Detail::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime,
                                            const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Check Component
	// if (CHECK_COMPONENT)
	// {
	// 	Switcher->SetActiveWidgetIndex(Handles.IsEmpty() ? 0 : 1);
	// }
	// else
	// {
	// 	ListView->RequestListRefresh();
	// }
}

void SDreamTaskManagerDebugger_Detail::HandleTaskListChanged(FDreamTaskSpecHandleContainer& InTaskList)
{
	Handles.Empty();

	const auto& RawHandles = InTaskList.GetHandles();

	for (int32 i = 0; i < RawHandles.Num(); i++)
	{
		Handles.Add(MakeShareable(new FDreamManagerRowKey(*Component.Get(), i)));
	}

	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(Handles.IsEmpty() ? 0 : 1);
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SDreamTaskManagerDebugger_Detail::OnGenerateRow(FDreamManagerAccessKey InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SDreamTaskManagerDebugger_Detail_SpecRow, OwnerTable)
		.AccessKey(InItem)
		.Detail(SharedThis(this));
}

TSharedRef<SHeaderRow> SDreamTaskManagerDebugger_Detail::MakeHeaderRow()
{
	return SNew(SHeaderRow)
			BUILD_HEADER(TEXT("Name"), LOCTEXT("Name", "Name"))
			BUILD_HEADER(TEXT("State"), LOCTEXT("State", "State"))
			BUILD_HEADER(TEXT("EndTime"), LOCTEXT("EndTime", "EndTime"))
			BUILD_HEADER(TEXT("CompletedCondition"), LOCTEXT("CompletedCondition", "CompletedCondition"));
}

void SDreamTaskManagerDebugger_Detail::Refresh()
{
	ListView->RequestListRefresh();
}

void SDreamTaskManagerDebugger_Detail::BeginPIE()
{
	DGT_ED_FLOG(Log, TEXT("BeginPIE"))

	Component.Reset();
	Handles.Empty();
	Switcher->SetActiveWidgetIndex(0);
	ListView->RequestListRefresh();


	DGT_ED_FLOG(Log, TEXT("CLEAN SUCCESS"))
}

void SDreamTaskManagerDebugger_Detail::EndPIE()
{
	DGT_ED_FLOG(Log, TEXT("EndPIE"))

	Component.Reset();
	Handles.Empty();
	Switcher->SetActiveWidgetIndex(0);
	ListView->RequestListRefresh();

	DGT_ED_FLOG(Log, TEXT("CLEAN SUCCESS"))
}

#undef LOCTEXT_NAMESPACE

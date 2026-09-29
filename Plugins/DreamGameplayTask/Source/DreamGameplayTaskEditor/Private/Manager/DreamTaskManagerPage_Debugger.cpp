#include "Manager/DreamTaskManagerPage_Debugger.h"

#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "DreamGameplayTaskEditorLog.h"
#include "Classes/DreamTaskComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Manager/DreamTaskManager_Util.h"
#include "Manager/Widgets/DreamTaskManagerDebugger_Detail.h"

#define LOCTEXT_NAMESPACE "DreamTaskManagerPage_Debugger"


void SDreamTaskManagerPage_Debugger::Construct(const FArguments& InArgs)
{
	RegisterLister();
	ChildSlot[
		SAssignNew(WidgetSwitcher, SWidgetSwitcher)

		//Start Error Text
		+ SWidgetSwitcher::Slot()
		[
			SNew(SErrorText)
			.ErrorText(LOCTEXT("ErrorText", "Current is not PIE (Play In Editor) Running Mode, Please Running."))
		]

		// Start Content
		+ SWidgetSwitcher::Slot()
		[
			SNew(VB)

			// Header
			VSLOT()
			.AutoHeight()
			[
				SNew(HB)

				HSLOT()
				.AutoWidth()
				.Padding(5.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Components", "Components"))
				]

				HSLOT()
				.Padding(5.f)
				[
					SAssignNew(TaskComponentPicker, SComboBox<FDreamManagerTaskComponent>)
					.OptionsSource(&TaskComponents)
					.OnComboBoxOpening(this, &SDreamTaskManagerPage_Debugger::HandleUpdateComponentPicker)
					.OnGenerateWidget(this, &SDreamTaskManagerPage_Debugger::HandlePickerGenerateWidget)
					.OnSelectionChanged(this, &SDreamTaskManagerPage_Debugger::HandlePickerSelectionChanged)
					[
						SAssignNew(TaskComponentPickerTextBlock, STextBlock)
						.Text(HandleGetPickerText())
					]
				]
			]

			// Content
			VSLOT()
			HA(HFILL)
			VA(VFILL)
			[
				SNew(SBorder)
				.Padding(5.f)
				[
					SAssignNew(TaskComponentDetail, SDreamTaskManagerDebugger_Detail)
					.Visibility(EVisibility::Collapsed)
				]
			]
		]
	];
}

SDreamTaskManagerPage_Debugger::~SDreamTaskManagerPage_Debugger()
{
	if (TaskComponentDetail.IsValid())
	{
		TaskComponentDetail->EndPIE();
	}

	UnregisterLister();
}

void SDreamTaskManagerPage_Debugger::RegisterLister()
{
	FEditorDelegates::PostPIEStarted.AddRaw(this, &SDreamTaskManagerPage_Debugger::HandlePIE_Begin);
	FEditorDelegates::ShutdownPIE.AddRaw(this, &SDreamTaskManagerPage_Debugger::HandlePIE_End);
}

void SDreamTaskManagerPage_Debugger::UnregisterLister()
{
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::ShutdownPIE.RemoveAll(this);
}

void SDreamTaskManagerPage_Debugger::HandlePIE_Begin(const bool bIsSimulating)
{
	if (bIsSimulating)
		return;

	// Start Internal
	WidgetSwitcher->SetActiveWidgetIndex(1);
	HandleUpdateComponentPicker();
	TaskComponentPicker->SetItemsSource(&TaskComponents);

	// Start Detail
	TaskComponentDetail->BeginPIE();
}

void SDreamTaskManagerPage_Debugger::HandlePIE_End(const bool bIsSimulating)
{
	if (bIsSimulating) return;

	// Pre Clean

	TaskComponentDetail->EndPIE();

	// Post Clean

	// 安全清空组件列表
	for (auto& CompPtr : TaskComponents)
	{
		if (CompPtr.IsValid())
		{
			*CompPtr = nullptr; // 显式置空指针
		}
	}
	TaskComponents.Empty();

	// 重置UI状态
	WidgetSwitcher->SetActiveWidgetIndex(0);
	SelectedComponent = nullptr;
	TaskComponentPicker->ClearSelection();
	TaskComponentPickerTextBlock->SetText(LOCTEXT("PleasePickComponent", "Please Pick Task Component"));
}

void SDreamTaskManagerPage_Debugger::HandlePickerSelectionChanged(FDreamManagerTaskComponent Component, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Type::OnMouseClick)
	{
		SelectedComponent = Component;
		TaskComponentPickerTextBlock->SetText(HandleGetPickerText());
		TaskComponentDetail->SetComponent(SelectedComponent);
		TaskComponentDetail->SetVisibility(EVisibility::Visible);
	}
}

TSharedRef<SWidget> SDreamTaskManagerPage_Debugger::HandlePickerGenerateWidget(FDreamManagerTaskComponent InComponent)
{
	return SNew(STextBlock)
		.Text(MAKE_TEXT(TEXT("Owner : %s Component : %s"),
		                *InComponent.ToSharedRef().Get()->GetOwner()->GetName(),
		                *InComponent.ToSharedRef().Get()->GetName())
		);
}

void SDreamTaskManagerPage_Debugger::HandleUpdateComponentPicker()
{
	TaskComponents.Empty();

	for (TObjectIterator<UDreamTaskComponent> It; It; ++It)
	{
		UDreamTaskComponent* Component = *It;

		if (Component->HasAnyFlags(RF_ClassDefaultObject) || !IsValid(Component))
		{
			continue;
		}

		AActor* Owner = Component->GetOwner();
		if (!IsValid(Owner) || Owner->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}

		if (!(Component->GetWorld()->WorldType == EWorldType::PIE
			|| Component->GetWorld()->WorldType == EWorldType::Editor))
		{
			continue;
		}

		const EWorldType::Type WorldType = Component->GetWorld()->WorldType;
		const bool bValidWorld =
			WorldType == EWorldType::PIE ||
			WorldType == EWorldType::Editor ||
			WorldType == EWorldType::Game ||
			WorldType == EWorldType::EditorPreview ||
			WorldType == EWorldType::GamePreview;

		if (!bValidWorld) continue;

		TaskComponents.Add(MakeShared<TWeakObjectPtr<UDreamTaskComponent>>(Component));
	}

	TaskComponentPicker->RefreshOptions();
}

FText SDreamTaskManagerPage_Debugger::HandleGetPickerText()
{
	FText Text = SELECT(SelectedComponent.IsValid(),
	                    MAKE_TEXT(TEXT("%s -> %s"), *SelectedComponent.ToSharedRef().Get()->GetOwner()->GetName(), *
		                    SelectedComponent.ToSharedRef().Get()->GetName()),
	                    MAKE_TEXT(TEXT("No Selected")));
	return Text;
}


#undef LOCTEXT_NAMESPACE

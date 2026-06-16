#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskTypes.h"
#include "Manager/DreamTaskManagerTypes.h"
#include "Widgets/SCompoundWidget.h"

class SDreamTaskManagerDebugger_Detail;
class UDreamTaskComponent;

class DREAMGAMEPLAYTASKEDITOR_API SDreamTaskManagerPage_Debugger : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDreamTaskManagerPage_Debugger)
		{
		}

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual ~SDreamTaskManagerPage_Debugger() override;

	void RegisterLister();
	void UnregisterLister();

	// PIE Handles
	void HandlePIE_Begin(const bool bIsSimulating);
	void HandlePIE_End(const bool bIsSimulating);

	// Picker Handles
	void HandlePickerSelectionChanged(FDreamManagerTaskComponent Component, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> HandlePickerGenerateWidget(FDreamManagerTaskComponent InComponent);
	void HandleUpdateComponentPicker();
	FText HandleGetPickerText();

protected:
	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
	TSharedPtr<STextBlock> TaskComponentPickerTextBlock;
	TSharedPtr<SComboBox<FDreamManagerTaskComponent>> TaskComponentPicker;
	TSharedPtr<SDreamTaskManagerDebugger_Detail> TaskComponentDetail;
	TArray<FDreamManagerTaskComponent> TaskComponents;
	FDreamManagerTaskComponent SelectedComponent;
};
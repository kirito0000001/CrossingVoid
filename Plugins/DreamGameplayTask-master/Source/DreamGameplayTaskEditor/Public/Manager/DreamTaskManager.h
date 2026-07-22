// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/SCompoundWidget.h"

class SDreamTaskManagerPage_Manager;
class SDreamTaskManagerPage_Debugger;
class UDreamTask;

/**
 * 
 */
class DREAMGAMEPLAYTASKEDITOR_API SDreamGameplayTaskManager : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDreamGameplayTaskManager)
		{
		}

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

protected:
	TSharedPtr<SDreamTaskManagerPage_Debugger> Page_Debugger;
	TSharedPtr<SDreamTaskManagerPage_Manager> Page_Manager;
	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	void OnNavigateHyperlink(FString URL);
};

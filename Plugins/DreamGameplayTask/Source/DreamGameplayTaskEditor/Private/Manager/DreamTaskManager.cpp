// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/DreamTaskManager.h"

#include "DreamGameplayTaskEditorSetting.h"
#include "SlateOptMacros.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/Input/SHyperlink.h"
#include "Manager/DreamTaskManager_Util.h"
#include "Manager/DreamTaskManagerPage_Debugger.h"
#include "Manager/DreamTaskManagerPage_Manager.h"

#define LOCTEXT_NAMESPACE "DreamGameplayTaskManager"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

using namespace FDreamTaskManagerUtil;

void SDreamGameplayTaskManager::Construct(const FArguments& InArgs)
{
	ChildSlot[
		SNew(SOverlay)


		+ SOverlay::Slot()
		.Padding(5.f)
		[
			SNew(SVerticalBox)

			VSLOT() // Header
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				HSLOT() // Title
				VA(VFILL)
				HA(HFILL)
				.Padding(5.f)
				[
					SNew(STextBlock)
					.Font(GetTextFont(15.f))
					.Text(LOCTEXT("Title", "Dream Gameplay Task"))
				] // End Title


				HSLOT()
				HA(HRIGHT)
				VA(VFILL)
				.Padding(5.f)
				[
					SNew(HB)

					HSLOT()
					HA(HFILL)
					VA(VFILL)
					.Padding(2.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ManagerButton", "Manager"))
						.OnClicked_Lambda([this]()
						{
							PageSwitcher->SetActiveWidget(Page_Manager.ToSharedRef());
							return FReply::Handled();
						})
					]

					HSLOT()
					.AutoWidth()
					HA(HRIGHT)
					VA(VFILL)
					.Padding(2.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("DebuggerButton", "Debugger"))
						.OnClicked_Lambda([this]()
						{
							PageSwitcher->SetActiveWidget(Page_Debugger.ToSharedRef());
							return FReply::Handled();
						})
					]

				]
			] // End Header

			VSLOT() // Start Content
			.Padding(5.f)
			HA(HFILL)
			VA(VFILL)
			[
				SAssignNew(PageSwitcher, SWidgetSwitcher)

				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(Page_Manager, SDreamTaskManagerPage_Manager)
				]

				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(Page_Debugger, SDreamTaskManagerPage_Debugger)
				]
			] // End Content

			VSLOT()
			.Padding(5.f)
			.AutoHeight()
			HA(HFILL)
			[
				SNew(HB)

				HSLOT()
				HA(HLEFT)
				[
					SNew(TB)
					.Text(MAKE_FORMATTED_TEXT(LOCTEXT("Version", "Version : P{0}M{1}D{2}"),
					                          FText::FromName(UDreamGameplayTaskEditorSetting::Get()->PluginVersion),
					                          FText::FromName(UDreamGameplayTaskEditorSetting::Get()->ManagerVersion),
					                          FText::FromName(UDreamGameplayTaskEditorSetting::Get()->DebuggerVersion))					)
				]

				HSLOT()
				HA(HRIGHT)
				[
					SNew(SHyperlink)
					.Text(MAKE_TEXT(TEXT("Powered By Dream Moon & 晓桀")))
					.OnNavigate(this, &SDreamGameplayTaskManager::OnNavigateHyperlink,
					            FString(TEXT("https://dmstudio.top")))
				]
			]
		]
	];
}

void SDreamGameplayTaskManager::OnNavigateHyperlink(FString URL)
{
	FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE

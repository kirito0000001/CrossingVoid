#include "Manager/Widgets/DreamTaskManagerDebugger_Detail_SpecRow.h"

#include "Classes/DreamTaskConditionTemplate.h"
#include "Manager/DreamTaskManager_Util.h"
#include "DreamGameplayTaskTypes.h"
#include "Classes/DreamTask.h"
#include "Manager/Widgets/DreamTaskManagerDebugger_Detail.h"

#define LOCTEXT_NAMESPACE "DreamTaskManagerDebugger_Detail_SpecRow"

#define GET_HANDLE	AccessKey->GetHandle()
#define DEF_PADDING 4.f

void SDreamTaskManagerDebugger_Detail_SpecRow::Construct(const FArguments& InArgs,
                                                         const TSharedRef<STableViewBase> InOwnerTableView)
{
	AccessKey = InArgs._AccessKey;
	Detail = InArgs._Detail;
	FSuperRowType::Construct(FSuperRowType::FArguments().Padding(2.f), InOwnerTableView);
}

TSharedRef<SWidget> SDreamTaskManagerDebugger_Detail_SpecRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	if (!AccessKey.IsValid() || !GET_HANDLE.GetTask()) return SNew(SErrorText).ErrorText(LOCTEXT("ErrorText", "Error Handle is not valid."));

	const auto& Conditions = GET_HANDLE.GetTask()->TaskCompletedCondition.Conditions;

	if (InColumnName == "Name")
	{
		return SNew(STextBlock)
			.Text_Lambda([this]()
			{
				if (!AccessKey.IsValid()) return LOCTEXT("Invalid", "Invalid");
				const FDreamTaskSpecHandle& Spec = GET_HANDLE;
				UDreamTask* Task = Spec.GetTask();
				if (!Task) return LOCTEXT("NoTask", "No Task");
				return FText::Format(LOCTEXT("NameFmt", "Name : {0}"),
				                     Task->GetTaskDisplayName());
			});
	}
	else if (InColumnName == "State")
	{
		return SNew(STextBlock)
			.Text_Lambda([this]()
			{
				if (!AccessKey.IsValid()) return LOCTEXT("Invalid", "Invalid");
				const FDreamTaskSpecHandle& Spec = GET_HANDLE;
				UDreamTask* Task = Spec.GetTask();
				FText StateText = Task
					                  ? StaticEnum<EDreamTaskState>()->GetDisplayValueAsText(Spec.GetTaskState())
					                  : LOCTEXT("NoTaskState", "No Task");
				FText TimeoutText = (Task && Spec.IsTimeout())
					                    ? LOCTEXT("Yes", "Yes")
					                    : LOCTEXT("No", "No");
				return FText::Format(LOCTEXT("StateFmt", "State : {0}\nTimeout: {1}"),
				                     StateText, TimeoutText);
			});
	}
	else if (InColumnName == "EndTime")
	{
		return SNew(STextBlock)
			.Text_Lambda([this]()
			{
				if (!AccessKey.IsValid()) return LOCTEXT("InvalidTask", "Invalid");
				const FDreamTaskSpecHandle& Spec = GET_HANDLE;
				UDreamTask* Task = Spec.GetTask();
				if (!Task) return LOCTEXT("InvalidTask", "Invalid");
				if (Spec.IsUseMaximumTime())
				{
					return FText::Format(LOCTEXT("EndTimeFmt",
					                             "Start Time: {0}\nEnd Time: {1}\nCurrent Time: {2}\nDeltaTime: {3}\nMaximumTime: {4}"),
					                     FText::FromString(Spec.GetStartTime().ToString()),
					                     FText::FromString(Spec.GetEndTime().ToString()),
					                     FText::FromString((Spec.GetStartTime() + Spec.GetRunningTime()).ToString()),
					                     FText::FromString(Spec.GetRunningTime().ToString()),
					                     FText::FromString(Spec.GetMaximumTime().ToString())
					);
				}
				else
				{
					return LOCTEXT("NoUseMaximumTime", "Handle 未启用最大时限");
				}
			});
	}
	else if (InColumnName == "CompletedCondition")
	{
		return SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (!AccessKey.IsValid()) return LOCTEXT("Invalid", "Invalid");
				const FDreamTaskSpecHandle& Spec = GET_HANDLE;
				UDreamTask* Task = Spec.GetTask();
				if (!Task) return LOCTEXT("Invalid", "Invalid");
				// 构建文本行
				TArray<FText> Lines;
				const auto& CondMap = Task->TaskCompletedCondition.GetConditionMapping();
				Lines.Add(FText::Format(LOCTEXT("ConditionTitleFmt", "Condition Number: {0}"),
				                        FText::AsNumber(CondMap.Num())));
				for (const auto& Pair : CondMap)
				{
					UDreamTaskConditionTemplate* Cond = Pair.Value;
					Lines.Add(FText::Format(LOCTEXT("ConditionFmt",
					                                "> ID : {0} Display Name : {1} Progress : {2} / {3} Must Be Completed : {4}"),
					                        FText::FromName(Pair.Key),
					                        FText::FromString(Cond->ConditionDisplayName.ToString()),
					                        FText::AsNumber(Cond->GetCount()),
					                        FText::AsNumber(Cond->GetCompletedCount()),
					                        Cond->bTaskMustBeCompleted ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No")
					));
				}
				Lines.Add(FText::Format(LOCTEXT("CompletedModeFmt", "Completed Mode : {0}"),
				                        StaticEnum<EDreamTaskConditionalCompletionMode>()->GetDisplayValueAsText(
					                        Task->TaskCompletedCondition.CompletionMode)));
				// 合并成一个字符串
				FString Combined;
				for (int32 i = 0; i < Lines.Num(); ++i)
				{
					Combined += Lines[i].ToString();
					if (i < Lines.Num() - 1) Combined += TEXT("\n");
				}
				return FText::FromString(Combined);
			});
	}

	return SNew(SErrorText).ErrorText(LOCTEXT("Error", "Unknown Column"));
}

void SDreamTaskManagerDebugger_Detail_SpecRow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SMultiColumnTableRow<FDreamManagerAccessKey>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!EndTimeTB.IsValid() || !AccessKey.IsValid())
	{
		return;
	}

	const FDreamTaskSpecHandle* Spec = &AccessKey->GetHandle();
	UDreamTask* Task = Spec ? Spec->GetTask() : nullptr;
	if (!Task)
	{
		EndTimeTB->SetText(LOCTEXT("InvalidTask", "Task 无效或已销毁"));
		return;
	}

	if (Spec->IsUseMaximumTime())
	{
		EndTimeTB->SetText(
			MAKE_FORMATTED_TEXT(LOCTEXT("EndTimeTB",
				                    "Start Time: {0}\nEnd Time: {1}\nCurrent Time: {2}\nDeltaTime: {3}\nMaximumTime: {4}"),
			                    FText::FromString(GET_HANDLE.GetStartTime().ToString()),
			                    FText::FromString(GET_HANDLE.GetEndTime().ToString()),
			                    FText::FromString((GET_HANDLE.GetStartTime() + GET_HANDLE.GetRunningTime()).ToString()),
			                    FText::FromString(GET_HANDLE.GetRunningTime().ToString()),
			                    FText::FromString(GET_HANDLE.GetMaximumTime().ToString())
			));
	}
}

#undef DEF_PADDING

#define BUILD_ACTION(Name, ToolTip, Action) \
	MenuBuilder.AddMenuEntry( \
		FText::FromString(Name), \
		FText::FromString(ToolTip), \
		FSlateIcon(), \
		FUIAction(Action) \
	);

FReply SDreamTaskManagerDebugger_Detail_SpecRow::OnMouseButtonDown(const FGeometry& MyGeometry,
                                                                   const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && AccessKey.IsValid())
	{
		const FDreamTaskSpecHandle& Spec = GET_HANDLE;
		UDreamTask* Task = Spec.GetTask();
		if (!Task)
		{
			return FReply::Unhandled();
		}

		FMenuBuilder MenuBuilder(true, nullptr);
		MenuBuilder.AddSearchWidget();

		// 显示目标任务
		MenuBuilder.AddWidget(
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("TargetFmt", "Target Task : {0}"), Task->GetTaskDisplayName())),
			LOCTEXT("Target", "Target"));

		MenuBuilder.AddSeparator();

		// 直接完成
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DirectComplete", "Direct completion"),
			LOCTEXT("DirectCompleteToolTip", "Complete the task directly"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([RowKey = AccessKey, ThisRow = SharedThis(this)]()
			{
				if (!RowKey.IsValid()) return;
				const FDreamTaskSpecHandle& InnerSpec = RowKey->GetHandle();
				UDreamTask* InnerTask = InnerSpec.GetTask();
				if (!InnerTask) return;
				for (auto& ElemPair : InnerSpec.GetTaskConditions())
				{
					ElemPair.Value->SetCount(ElemPair.Value->GetCompletedCount());
				}
				InnerTask->CheckTaskCompleted();
				if (ThisRow->Detail.IsValid())
				{
					ThisRow->Detail->Refresh();
				}
			})));

		// 重置任务
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ResetTask", "Resetting tasks"),
			LOCTEXT("ResetTaskToolTip", "Reset the task"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([RowKey = AccessKey, ThisRow = SharedThis(this)]()
			{
				if (!RowKey.IsValid()) return;
				const FDreamTaskSpecHandle& InnerSpec = RowKey->GetHandle();
				UDreamTask* InnerTask = InnerSpec.GetTask();
				if (!InnerTask) return;
				InnerTask->ResetTask();
				if (ThisRow->Detail.IsValid())
				{
					ThisRow->Detail->Refresh();
				}
			})));

		MenuBuilder.AddSeparator();

		// 条件设置
		const auto& CondMap = Spec.GetTaskConditions();
		for (const auto& Pair : CondMap)
		{
			UDreamTaskConditionTemplate* Cond = Pair.Value;
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("UpdateCondFmt", "Update {0} conditional progress"),
				              FText::FromString(Cond->ConditionDisplayName.ToString())),
				LOCTEXT("UpdateCondToolTip", "Adding conditional progress"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Pair, Cond, RowKey = AccessKey, ThisRow = SharedThis(this)]()
				{
					if (!Cond || !RowKey.IsValid()) return;
					Cond->Update();
					const FDreamTaskSpecHandle& InnerSpec = RowKey->GetHandle();
					UDreamTask* InnerTask = InnerSpec.GetTask();
					if (InnerTask)
					{
						InnerTask->UpdateTaskByName({Pair.Key});
					}
					if (ThisRow->Detail.IsValid())
					{
						ThisRow->Detail->Refresh();
					}
				})));
		}

		MenuBuilder.AddSeparator();

		for (const auto& Pair : CondMap)
		{
			UDreamTaskConditionTemplate* Cond = Pair.Value;
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("RemoveCondFmt", "Remove the {0} conditional progress"),
				              FText::FromString(Cond->ConditionDisplayName.ToString())),
				LOCTEXT("RemoveCondToolTip", "Remove conditional progress"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Pair, Cond, RowKey = AccessKey, ThisRow = SharedThis(this)]()
				{
					if (!Cond || !RowKey.IsValid()) return;
					int32 NewCount = FMath::Clamp(Cond->GetCount() - 1, 0, Cond->GetCompletedCount());
					Cond->SetCount(NewCount);
					const FDreamTaskSpecHandle& InnerSpec = RowKey->GetHandle();
					UDreamTask* InnerTask = InnerSpec.GetTask();
					if (InnerTask)
					{
						InnerTask->UpdateTaskByName({Pair.Key});
					}
					if (ThisRow->Detail.IsValid())
					{
						ThisRow->Detail->Refresh();
					}
				})));
		}

		FPopupTransitionEffect PopupEffect(FPopupTransitionEffect::ContextMenu);
		FSlateApplication::Get().PushMenu(
			SharedThis(this),
			FWidgetPath(),
			MenuBuilder.MakeWidget(),
			MouseEvent.GetScreenSpacePosition(),
			PopupEffect,
			true);

		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE

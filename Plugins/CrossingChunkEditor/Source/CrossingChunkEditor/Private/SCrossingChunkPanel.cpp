#include "SCrossingChunkPanel.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"                                          // GEditor->PlayEditorSound
#include "Framework/Notifications/NotificationManager.h"     // FSlateNotificationManager / FNotificationInfo
#include "Widgets/Notifications/SNotificationList.h"         // SNotificationItem
#include "Widgets/Input/SCheckBox.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "CrossingChunkEditorModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/TextLayout.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CrossingChunkPanel"

// ---------------------------------------------------------------------------
// 日志着色：SMultiLineEditableTextBox + 自定义 marshaller。
// 和引擎 Output Log 同一套做法（SOutputLog.cpp 的 FOutputLogTextLayoutMarshaller）：
// 既能按行上色，又保留选中 / 复制。
// ---------------------------------------------------------------------------
class FCrossingChunkLogMarshaller : public FBaseTextLayoutMarshaller
{
public:
	static TSharedRef<FCrossingChunkLogMarshaller> Create()
	{
		return MakeShareable(new FCrossingChunkLogMarshaller());
	}

	virtual ~FCrossingChunkLogMarshaller() = default;

	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override
	{
		PlainText = SourceString;   // 复制日志时原样交回

		TArray<FString> Lines;
		SourceString.ParseIntoArrayLines(Lines, /*InCullEmpty=*/false);
		if (Lines.Num() == 0) { Lines.Add(FString()); }

		TArray<FTextLayout::FNewLineData> LinesToAdd;
		LinesToAdd.Reserve(Lines.Num());
		for (const FString& Line : Lines)
		{
			TSharedRef<FString> LineText = MakeShared<FString>(Line);
			TArray<TSharedRef<IRun>> Runs;
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, MakeStyleForLine(Line)));
			// FNewLineData 在 5.8 要两个参数（Text + Runs）；Output Log 也是这么塞的
			LinesToAdd.Emplace(MoveTemp(LineText), MoveTemp(Runs));
		}

		TargetTextLayout.ClearLines();
		TargetTextLayout.AddLines(LinesToAdd);
	}

	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override
	{
		TargetString = PlainText;
	}

private:
	FCrossingChunkLogMarshaller()
	{
		BaseStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
		BaseStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Mono", 9));
		BaseStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.80f, 0.84f)));
	}

	FTextBlockStyle MakeStyleForLine(const FString& Line) const
	{
		FTextBlockStyle Style = BaseStyle;
		if (Line.Contains(TEXT("Error:")) || Line.Contains(TEXT("失败")) || Line.Contains(TEXT("错误"))
			|| Line.Contains(TEXT("!!")) || Line.Contains(TEXT("Exception")))
		{
			Style.SetColorAndOpacity(FSlateColor(FLinearColor(1.00f, 0.42f, 0.40f)));   // 红
		}
		else if (Line.Contains(TEXT("Warning:")) || Line.Contains(TEXT("警告")))
		{
			Style.SetColorAndOpacity(FSlateColor(FLinearColor(1.00f, 0.84f, 0.35f)));   // 黄
		}
		else if (Line.Contains(TEXT("[OK]")) || Line.Contains(TEXT("成功")) || Line.Contains(TEXT("完成"))
			|| Line.Contains(TEXT("已删除")))
		{
			Style.SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.90f, 0.55f)));   // 绿
		}
		else if (Line.Contains(TEXT("平台设置")) || Line.Contains(TEXT("[缓存]")) || Line.Contains(TEXT("基线")))
		{
			Style.SetColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.78f, 1.00f)));   // 青
		}
		return Style;
	}

	FString PlainText;
	FTextBlockStyle BaseStyle;
};

void SCrossingChunkPanel::Construct(const FArguments& InArgs)
{
	// 目标：和脚本的 -Platform / -Target 一一对应
	auto AddTarget = [this](const TCHAR* Label, const TCHAR* Platform, const TCHAR* Target)
	{
		TSharedPtr<FChoice> Choice = MakeShared<FChoice>();
		Choice->Label = Label;
		Choice->Platform = Platform;
		Choice->Target = Target;
		Choice->ReportDir = FString::Printf(TEXT("%s-%s"), Platform, Target);
		TargetOptions.Add(Choice);
	};
	AddTarget(TEXT("Windows"), TEXT("Win64"), TEXT("Client"));
	AddTarget(TEXT("安卓"),    TEXT("Android"), TEXT("Client"));
	AddTarget(TEXT("服务器"),  TEXT("Win64"), TEXT("Server"));
	SelectedTarget = TargetOptions[0];

	// 模式：界面上说人话，传给脚本的还是 Quick / Base / Patch。
	// 顺序按"平时最常用"排：补丁 → 版本更新 → 快速验证。
	// 「忽略缓存全量重 cook」不再是一个选项 —— 由脚本在《版本更新》时自动体检
	// （见 Pack-CrossingVoid.ps1 的 Test-CookCacheDirty）：缓存脏了才全量，
	// 干净就走增量。想手动强制全量用命令行的 -Cook。
	auto AddMode = [this](const TCHAR* Label, const TCHAR* Value)
	{
		TSharedPtr<FChoice> Choice = MakeShared<FChoice>();
		Choice->Label = Label;
		Choice->Value = Value;
		ModeOptions.Add(Choice);
	};
	AddMode(TEXT("常规补丁更新（新资源单独成包）"), TEXT("Patch"));
	AddMode(TEXT("版本更新（完整包 + 建基线）"), TEXT("Base"));
	AddMode(TEXT("快速验证（只测试：不产发布包）"), TEXT("Quick"));
	SelectedMode = ModeOptions[0];

	OutputDir = TEXT("D:\\Build\\CrossingVoid");   // 和脚本默认值一致
	MapsCsv.Empty();

	// 恢复上次的选项（页面关掉再打开、甚至重启编辑器都记得）
	ApplySavedSettings();

	// 日志着色器（按行上色 + 保留复制）
	LogMarshaller = FCrossingChunkLogMarshaller::Create();

	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- 头部：标题 + 刷新 + 状态 ----
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "分块规则")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(SButton).Text(LOCTEXT("Refresh", "刷新")).IsEnabled_Lambda([this]() { return AreInputsEnabled(); }).OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnRefreshClicked))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(this, &SCrossingChunkPanel::GetSummaryText).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			// ---- 打包：和命令行是同一套命令（Tools\Pack-CrossingVoid.ps1）----
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 2.f, 0.f)
			[
				SNew(SComboBox<TSharedPtr<FChoice>>)
				.OptionsSource(&TargetOptions)
				.InitiallySelectedItem(SelectedTarget)
				.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
				.ToolTipText(LOCTEXT("TargetTip", "Windows=PC 客户端 / 安卓=安卓客户端 / 服务器=出散件（不分包，走 git）"))
				.OnGenerateWidget_Lambda([](TSharedPtr<FChoice> Option) { return SNew(STextBlock).Text(FText::FromString(Option.IsValid() ? Option->Label : FString())); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FChoice> Option, ESelectInfo::Type)
				{
					if (Option.IsValid() && Option != SelectedTarget)
					{
						// 先把「旧目标」勾的地图存到它自己的键下（服务器和客户端要打的图不一样）
						UpdateMapsCsv();
						SaveSetting(*GetMapsSettingKey(), MapsCsv);

						SelectedTarget = Option;

						// 再把「新目标」上次勾的图读出来贴回勾选框
						MapsCsv = LoadSetting(*GetMapsSettingKey());
						ApplyMapsCsvToChoices();
						RebuildMapList();

						RefreshRules();   // 换目标 = 换一份报告，列表要跟着变
						RefreshStatus();  // 状态行也是按目标的（报告/产物/基线各一份）
						SaveSettings();
					}
				})
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(SelectedTarget.IsValid() ? SelectedTarget->Label : TEXT("Windows")); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(SComboBox<TSharedPtr<FChoice>>)
				.OptionsSource(&ModeOptions)
				.InitiallySelectedItem(SelectedMode)
				.IsEnabled_Lambda([this]()
				{
					// 服务器出散件、走 git，没有"版本/补丁"这一说 —— 直接禁掉，避免误选
					// 另外：打包进行中一律灰掉（见 AreInputsEnabled）
					return AreInputsEnabled() && !(SelectedTarget.IsValid() && SelectedTarget->Target == TEXT("Server"));
				})
				.ToolTipText(LOCTEXT("ModeTip", "快速验证 / 正式发布（建基线）/ 打补丁（基于已有基线算差异）"))
				.OnGenerateWidget_Lambda([](TSharedPtr<FChoice> Option) { return SNew(STextBlock).Text(FText::FromString(Option.IsValid() ? Option->Label : FString())); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FChoice> Option, ESelectInfo::Type)
				{
					if (Option.IsValid()) { SelectedMode = Option; SaveSettings(); }
				})
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						if (SelectedTarget.IsValid() && SelectedTarget->Target == TEXT("Server"))
						{
							return LOCTEXT("ServerLooseMode", "服务器散件（不分包，走 git）");
						}
						return FText::FromString(SelectedMode.IsValid() ? SelectedMode->Label : TEXT("快速验证"));
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f)
			[
				SNew(SButton)
				.Text(this, &SCrossingChunkPanel::GetPackButtonText)
				.ToolTipText(LOCTEXT("PackTip", "跑 Tools\\Pack-CrossingVoid.ps1（日志在下面实时滚动，结束自动刷新报告）"))
				.OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnPackClicked))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f)
			[
				SNew(SButton).Text(LOCTEXT("CopyCmd", "复制命令")).IsEnabled_Lambda([this]() { return AreInputsEnabled(); }).OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnCopyCommandClicked))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("UndoDisband", "撤销解散"))
				.ToolTipText(LOCTEXT("UndoDisbandTip", "把刚才解散掉的那个分块（连同它的目录）恢复回来"))
				.IsEnabled_Lambda([this]() { return AreInputsEnabled() && UndoRule.IsValid(); })
				.OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnUndoDisbandClicked))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f)
			[
				SNew(SButton).Text(LOCTEXT("OpenLog", "日志目录")).IsEnabled_Lambda([this]() { return AreInputsEnabled(); }).OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnOpenLogDirClicked))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearCache", "清除缓存"))
				.ToolTipText(LOCTEXT("ClearCacheTip", "删掉能再生的缓存（StagedBuilds / Cooked / Shaders）腾磁盘空间；不会动 Intermediate\\Build、Content、归档目录"))
				.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
				.OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnClearCacheClicked))
			]
		]

		// ---- 第二行：打包位置 / 选定地图 ----
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 8.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("OutDirLabel", "输出目录")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SBox).WidthOverride(240.f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this]() { return FText::FromString(OutputDir); })
					.ToolTipText(LOCTEXT("OutDirTip", "打包产物落在这里（脚本的 -ArchiveDir）；Both 模式会在它下面分 Client/Server 子目录"))
					.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
					.SelectAllTextWhenFocused(true)
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
					{
						OutputDir = NewText.ToString().TrimStartAndEnd();
						if (OutputDir.IsEmpty()) { OutputDir = TEXT("D:\\Build\\CrossingVoid"); }
						SaveSettings();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MapsLabel", "地图"))
				.ToolTipText(LOCTEXT("MapsTip",
					"本次要 cook 的地图，逗号分隔，例如 /Game/MapS/LoginMap,/Game/MapS/MainMap\n"
					"留空 = 用 DefaultGame.ini 里 +MapsToCook 配置的那几张（默认四张）"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(STextBlock).Text(this, &SCrossingChunkPanel::GetMapSummaryText).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		// ---- 版本行：玩家版本 / 基线版本（基线版本默认取玩家版本的主版本段）----
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 8.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("PlayerVerLabel", "玩家版本")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SBox).WidthOverride(110.f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this]() { return FText::FromString(PlayerVersion); })
					.ToolTipText(LOCTEXT("PlayerVerTip", "给玩家看的版本号。打包时写进 Config\\DefaultEngine.ini 的 AndroidRuntimeSettings.VersionDisplayName（StoreVersion 不动）"))
					.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
					.SelectAllTextWhenFocused(true)
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
					{
						PlayerVersion = NewText.ToString().TrimStartAndEnd();
						SaveSettings();   // 基线版本不用存：它由玩家版本现算
						RefreshStatus();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("RelVerLabel", "基线版本")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SBox).WidthOverride(110.f)
				[
					SNew(SEditableTextBox)
					.IsReadOnly(true)
					.Text_Lambda([this]() { return FText::FromString(GetReleaseVersion()); })
					.ToolTipText(LOCTEXT("RelVerTip", "基线目录名（脚本的 -ReleaseVersion）。由玩家版本自动推出来，不能手改。\n取玩家版本的主版本段：1.4.2 -> 1.4\n《版本更新》用它建基线；《常规补丁》基于它算差异。一条基线约 1.5 GB"))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(StatusReleaseRoot.IsEmpty() ? TEXT("基线目录：—") : FString::Printf(TEXT("基线目录：%s"), *StatusReleaseRoot)); })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		// ---- 状态行（单独一行）：报告 / 产物 / 基线 ----
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 2.f, 8.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.34f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(StatusReportText); })
			]
			+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(StatusArtifactText); })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().FillWidth(0.24f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(StatusBaselineText); })
			]
		]

		// ---- 运行设置（可折叠，默认收起）：这类是"额外 / 运行期"的开关，不掺和打包参数 ----
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 8.f, 0.f)
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("RunSettings", "运行设置（额外）"))
			// 这一条原来太矮、字太小（就一行 10 号字），折起来几乎点不到 —— 头部和正文都放大
			.AreaTitleFont(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.HeaderPadding(FMargin(6.f, 6.f))
			.Padding(FMargin(6.f, 4.f))
			.InitiallyCollapsed(true)
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CookProcessCountLabel", "Cook 进程数"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
					[
						SNew(SBox).WidthOverride(84.f).MinDesiredHeight(26.f)
						[
							SNew(SSpinBox<int32>)
							.MinValue(1)
							.MaxValue(MaxCookProcessCount)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
							.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
							.Value_Lambda([this]() { return CookProcessCount; })
							.OnValueChanged_Lambda([this](int32 NewValue)
							{
								CookProcessCount = NewValue;
								SaveSettings();
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return GetCookProcessCountHint(); })
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.f, 0.f)
			[
				SNew(SVerticalBox)

		// ---- 第三行：地图勾选（学项目启动程序那套；一个都不勾 = 用 ini 配置）----
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 8.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("MapPickLabel", "选择地图")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SBox).WidthOverride(160.f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("MapFilterHint", "搜索地图…"))
					.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
					{
						MapFilter = NewText.ToString().TrimStartAndEnd();
						RebuildMapList();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SButton).Text(LOCTEXT("MapReload", "重新扫描")).IsEnabled_Lambda([this]() { return AreInputsEnabled(); }).OnClicked(FOnClicked::CreateLambda([this]()
				{
					ReloadMaps();
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SButton).Text(LOCTEXT("MapClear", "全部不勾")).IsEnabled_Lambda([this]() { return AreInputsEnabled(); }).OnClicked(FOnClicked::CreateLambda([this]()
				{
					for (const TSharedPtr<FMapChoice>& C : MapChoices) { if (C.IsValid()) { C->bChecked = false; } }
					UpdateMapsCsv();
					RebuildMapList();
					SaveSettings();
					return FReply::Handled();
				}))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 2.f, 8.f, 4.f)
		[
			SNew(SBox).HeightOverride(192.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(4.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(MapListBox, SVerticalBox)
					]
				]
			]
		]

		// ---- 列头（宽度必须和下面每行一一对应，否则看着就是错位的） ----
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(36.f)[SNew(STextBlock).Text(LOCTEXT("ColId", "号")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[SNew(SBox).WidthOverride(170.f)[SNew(STextBlock).Text(LOCTEXT("ColName", "名字（双击可改）")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[SNew(SSpacer)]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(64.f)[SNew(STextBlock).Text(LOCTEXT("ColFolders", "目录")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("ColFiles", "文件")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(84.f)[SNew(STextBlock).Text(LOCTEXT("ColSize", "体积")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)[SNew(SBox).WidthOverride(110.f)[SNew(STextBlock).Text(LOCTEXT("ColDelta", "变化")).ToolTipText(LOCTEXT("ColDeltaTip", "+ 新增　- 删除　~ 修改（和上一次打包报告比）")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
		]

		// ---- 规则列表 ----
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(RowsBox, SVerticalBox)
				]
			]
		]

		// ---- 左栏到此为止，切到右栏 ----
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.f, 0.f)
			[
				SNew(SVerticalBox)

		// ---- 未分类（本次新增） ----
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f, 8.f, 2.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UnclassTitle", "本次新增（未分类目录）—— 归类用内容浏览器右键"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]
		+ SVerticalBox::Slot().FillHeight(0.6f).Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(UnclassBox, SVerticalBox)
				]
			]
		]

		// ---- 实时日志 ----
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 2.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LogTitle", "打包日志（实时）"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]
		+ SVerticalBox::Slot().FillHeight(0.5f).Padding(8.f, 0.f, 8.f, 8.f)
		[
			SAssignNew(LogBox, SMultiLineEditableTextBox)
			.Text(FText::FromString(LogText))
			.Marshaller(LogMarshaller)
			.IsReadOnly(true)
			.AlwaysShowScrollbars(true)
		]
		]
		]
	];

	RefreshRules();
	ReloadMaps();
	RefreshStatus();
}

void SCrossingChunkPanel::RefreshRules()
{
	Rows.Reset();
	Unclassified.Reset();
	ReportChunks.Reset();
	ReportStamp.Empty();
	FolderTotal = 0;
	StatusMessage.Empty();

	if (RowsBox.IsValid()) { RowsBox->ClearChildren(); }
	if (UnclassBox.IsValid()) { UnclassBox->ClearChildren(); }

	LoadReport();   // 先读报告：它决定每行多出来的"文件数/体积/变化"

	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!FCrossingChunkRuleService::LoadRules(Rules, Error))
	{
		StatusMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return;
	}

	for (const FCrossingChunkRule& Rule : Rules)
	{
		TSharedPtr<FCrossingChunkRule> Row = MakeShared<FCrossingChunkRule>(Rule);
		Rows.Add(Row);
		FolderTotal += Rule.Folders.Num();
		if (RowsBox.IsValid())
		{
			// 行高统一 26px：列表看起来才整齐
			RowsBox->AddSlot().AutoHeight().Padding(0.f, 1.f)[SNew(SBox).HeightOverride(26.f)[MakeChunkRow(Row)]];
		}
	}

	for (const TSharedPtr<FUnclassFolder>& Item : Unclassified)
	{
		if (UnclassBox.IsValid())
		{
			UnclassBox->AddSlot().AutoHeight().Padding(0.f, 1.f)[MakeUnclassRow(Item)];
		}
	}
	if (Unclassified.Num() == 0 && UnclassBox.IsValid())
	{
		UnclassBox->AddSlot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock)
			.Text(ReportStamp.IsEmpty()
				? LOCTEXT("NoReport", "还没有打包报告 —— 先在「平台」菜单里跑一次打包，这里就会显示未分类清单和体积")
				: LOCTEXT("NoUnclass", "没有未分类目录"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}
}

void SCrossingChunkPanel::ReloadMaps()
{
	MapChoices.Reset();

	if (IAssetRegistry* Registry = IAssetRegistry::Get())
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")));

		TArray<FAssetData> Found;
		Registry->GetAssets(Filter, Found);
		for (const FAssetData& Data : Found)
		{
			TSharedPtr<FMapChoice> Choice = MakeShared<FMapChoice>();
			Choice->PackagePath = Data.PackageName.ToString();
			FString Left;
			FString Leaf;
			Choice->PackagePath.Split(TEXT("/"), &Left, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			Choice->Label = Leaf;
			MapChoices.Add(Choice);
		}
	}

	MapChoices.Sort([](const TSharedPtr<FMapChoice>& A, const TSharedPtr<FMapChoice>& B)
	{
		return A.IsValid() && B.IsValid() ? A->PackagePath < B->PackagePath : false;
	});

	// 按上次记下来的勾选恢复（地图清单是扫出来的，勾选状态得自己贴回去）
	ApplyMapsCsvToChoices();

	RebuildMapList();
	UpdateMapsCsv();
}

void SCrossingChunkPanel::ApplyMapsCsvToChoices()
{
	TArray<FString> Picked;
	MapsCsv.ParseIntoArray(Picked, TEXT(","), true);
	for (const TSharedPtr<FMapChoice>& Choice : MapChoices)
	{
		if (Choice.IsValid())
		{
			Choice->bChecked = Picked.Contains(Choice->PackagePath);
		}
	}
}

// 服务器和客户端要打的图不一样，所以勾选单独存一份（Maps_Client / Maps_Server）。
FString SCrossingChunkPanel::GetMapsSettingKey() const
{
	const FString Target = (SelectedTarget.IsValid() && !SelectedTarget->Target.IsEmpty())
		? SelectedTarget->Target
		: FString(TEXT("Client"));
	return FString::Printf(TEXT("Maps_%s"), *Target);
}

void SCrossingChunkPanel::RebuildMapList()
{
	if (!MapListBox.IsValid())
	{
		return;
	}
	MapListBox->ClearChildren();

	int32 Shown = 0;
	for (const TSharedPtr<FMapChoice>& Choice : MapChoices)
	{
		if (!Choice.IsValid()) { continue; }
		if (!MapFilter.IsEmpty() && !Choice->Label.Contains(MapFilter)) { continue; }
		Shown++;

		TSharedPtr<FMapChoice> Item = Choice;
		MapListBox->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Item]() { return Item->bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
				.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState NewState)
				{
					Item->bChecked = (NewState == ECheckBoxState::Checked);
					UpdateMapsCsv();
					SaveSettings();
				})
				[
					SNew(STextBlock).Text(FText::FromString(
						FString::Printf(TEXT("%s    (%s)"), *Item->Label, *Item->PackagePath)))
				]
			]
		];
	}

	if (Shown == 0)
	{
		MapListBox->AddSlot().AutoHeight().Padding(2.f)
		[
			SNew(STextBlock)
			.Text(MapChoices.Num() == 0
				? LOCTEXT("NoMaps", "没扫到地图（点「重新扫描」；或者工程里确实还没有 World 资产）")
				: LOCTEXT("MapFiltered", "没有匹配搜索条件的地图"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}
}

void SCrossingChunkPanel::UpdateMapsCsv()
{
	TArray<FString> Picked;
	for (const TSharedPtr<FMapChoice>& Choice : MapChoices)
	{
		if (Choice.IsValid() && Choice->bChecked)
		{
			Picked.Add(Choice->PackagePath);
		}
	}
	MapsCsv = FString::Join(Picked, TEXT(","));
}

FText SCrossingChunkPanel::GetMapSummaryText() const
{
	if (MapsCsv.IsEmpty())
	{
		return LOCTEXT("MapSummaryDefault", "一个都不勾 = 用 DefaultGame.ini 里 +MapsToCook 配置的地图");
	}
	return FText::FromString(FString::Printf(TEXT("本次只 cook：%s"), *MapsCsv));
}

FText SCrossingChunkPanel::GetCookProcessCountHint() const
{
	// 1 = 单进程（引擎默认）。>1 才是 MPCook，两条代价都来自 CookDirector 源码：
	//   · CoreLimit = 物理核数 / 进程数（整除！）→ 本机 8 核时 N=4 每进程 2 核，N≥5 就掉到 1 核；
	//   · 每个 worker 都是一整个 editor 进程，多进程下引擎还会关掉"低内存就 GC"的保护。
	// 所以上限卡在 MaxCookProcessCount（实测 N=5 比单进程慢 44%），这里把切分结果直接写出来。
	const int32 NumCores = FPlatformMisc::NumberOfCores();
	const int32 NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	if (CookProcessCount <= 1)
	{
		return LOCTEXT("CookProcessHintSingle", "单进程 cook（引擎默认，最稳）");
	}
	const int32 CoresPerProc = FMath::Max(NumCores / CookProcessCount, 1);
	const int32 ThreadsPerProc = FMath::Max(CoresPerProc * (NumCores > 0 ? NumThreads / NumCores : 1), 1);
	return FText::FromString(FString::Printf(
		TEXT("多进程 cook：1 director + %d worker；本机 %d 核/%d 线程 → 每进程 %d 核 %d 线程，合计 %d 线程（每个 worker 都是一整个编辑器进程，很吃内存）"),
		CookProcessCount - 1, NumCores, NumThreads, CoresPerProc, ThreadsPerProc, ThreadsPerProc * CookProcessCount));
}

void SCrossingChunkPanel::LoadReport()
{
	// 报告按「目标」分开存：换目标 = 换一份报告
	FString ReportDir = SelectedTarget.IsValid() ? SelectedTarget->ReportDir : TEXT("Win64-Client");
	// 《快速验证》的报告是单独一份（见 Build-ChunkReport.ps1）：它不打 pak，
	// 体积列本来就是 0，不能冲掉正式报告里的体积 / 变化。
	ReportDir = ReportDir + ((SelectedMode.IsValid() && SelectedMode->Value == TEXT("Quick")) ? TEXT("-Quick") : TEXT(""));
	const FString Path = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("PackLogs/ChunkReports"), ReportDir, TEXT("latest.json"));
	if (!FPaths::FileExists(Path))
	{
		return;
	}

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		StatusMessage = FString::Printf(TEXT("报告读取失败：%s"), *Path);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		StatusMessage = TEXT("报告解析失败");
		return;
	}

	ReportStamp = Root->GetStringField(TEXT("generatedAt"));

	const TArray<TSharedPtr<FJsonValue>>* Chunks = nullptr;
	if (Root->TryGetArrayField(TEXT("chunks"), Chunks))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Chunks)
		{
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			if (!Obj.IsValid()) { continue; }
			FReportChunk Info;
			Info.MatchedFiles = (int32)Obj->GetNumberField(TEXT("matchedFiles"));
			Info.Bytes        = (int64)Obj->GetNumberField(TEXT("bytes"));
			const TSharedPtr<FJsonObject>* Delta = nullptr;
			if (Obj->TryGetObjectField(TEXT("delta"), Delta) && Delta && Delta->IsValid())
			{
				Info.Added    = (int32)(*Delta)->GetNumberField(TEXT("added"));
				Info.Removed  = (int32)(*Delta)->GetNumberField(TEXT("removed"));
				Info.Modified = (int32)(*Delta)->GetNumberField(TEXT("modified"));
			}
			ReportChunks.Add((int32)Obj->GetNumberField(TEXT("id")), Info);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Unclass = nullptr;
	if (Root->TryGetArrayField(TEXT("unclassified"), Unclass))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Unclass)
		{
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			if (!Obj.IsValid()) { continue; }
			TSharedPtr<FUnclassFolder> Item = MakeShared<FUnclassFolder>();
			Item->Folder = Obj->GetStringField(TEXT("folder"));
			Item->Files  = (int32)Obj->GetNumberField(TEXT("files"));
			Item->bIsNew = Obj->GetBoolField(TEXT("isNew"));
			Unclassified.Add(Item);
		}
	}
}

TSharedRef<SWidget> SCrossingChunkPanel::MakeChunkRow(TSharedPtr<FCrossingChunkRule> Rule)
{
	const int32 ChunkId = Rule->ChunkId;
	const bool bIsNewBucket = (ChunkId == INDEX_NONE);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(36.f)
			[
				SNew(STextBlock).Text(FText::AsNumber(ChunkId))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			// 双击文本框就能改名：提交后立刻写回 ini，右键菜单也会同步
			SNew(SBox).WidthOverride(170.f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(Rule->ChunkName))
				.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
				.SelectAllTextWhenFocused(true)
				.OnTextCommitted(FOnTextCommitted::CreateSP(this, &SCrossingChunkPanel::OnNameCommitted, ChunkId))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[SNew(SSpacer)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(64.f)
			[
				SNew(STextBlock).Text(FText::AsNumber(Rule->Folders.Num()))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(80.f)
			[
				SNew(STextBlock)
				.Text(ReportChunks.Contains(ChunkId)
					? FText::AsNumber(ReportChunks[ChunkId].MatchedFiles)
					: FText::FromString(TEXT("—")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(84.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ReportChunks.Contains(ChunkId)
					? FString::Printf(TEXT("%.1f MB"), ReportChunks[ChunkId].Bytes / 1048576.0)
					: TEXT("—")))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(110.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString([&]() -> FString
				{
					if (!ReportChunks.Contains(ChunkId)) { return FString(); }
					const FReportChunk& Info = ReportChunks[ChunkId];
					if (Info.Added == 0 && Info.Removed == 0 && Info.Modified == 0) { return TEXT(""); }
					return FString::Printf(TEXT("+%d  -%d  ~%d"), Info.Added, Info.Removed, Info.Modified);
				}()))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 0.4f)))
			]
		]
		// 解散这个分块：删掉规则、目录回到未归类；顶部「撤销解散」可以恢复
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("Disband", "解散"))
			.ToolTipText(LOCTEXT("DisbandTip", "解散这个分块：删掉这条规则，它下面的目录会重新变成未归类（顶部「撤销解散」可以恢复）"))
			.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
			.OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnDisbandClicked, ChunkId))
		];
}

TSharedRef<SWidget> SCrossingChunkPanel::MakeUnclassRow(TSharedPtr<FUnclassFolder> Item)
{
	const FString Folder = Item->Folder;

	// 这份清单来自"上一次打包报告"，而规则可能刚被改过 ——
	// 所以拿当前规则再核一次：已经被某个分块覆盖的，标成「已归类（下次打包生效）」。
	// （不直接删掉这一行：用户刚点完指派，得看得见"它去哪了"）
	TArray<FCrossingChunkRule> Plain;
	for (const TSharedPtr<FCrossingChunkRule>& R : Rows)
	{
		if (R.IsValid()) { Plain.Add(*R); }
	}
	FString Owner;
	const FCrossingChunkRule* Covered = FCrossingChunkRuleService::ResolveInheritedRule(Plain, Folder, Owner);

	const FText StateText = Covered
		? FText::FromString(FString::Printf(TEXT("[已归类] → Chunk %d · %s（下次打包生效）"), Covered->ChunkId, *Covered->ChunkName))
		: FText::FromString(Item->bIsNew ? TEXT("[新增]") : TEXT("[老欠账]"));
	const FLinearColor StateColor = Covered
		? FLinearColor(0.45f, 0.75f, 0.45f)
		: (Item->bIsNew ? FLinearColor(1.f, 0.8f, 0.35f) : FLinearColor::Gray);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(340.f)
			[
				SNew(STextBlock)
				.Text(StateText)
				.ColorAndOpacity(FSlateColor(StateColor))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Folder))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(90.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%d 个文件"), Item->Files)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			// 一键指派：不用再跳回内容浏览器右键。
			// 注意 RefreshRules() 会重建整棵树，而这个回调正跑在 combo 自己的回调里，
			// 所以刷新推迟到下一帧（并用 WeakSelf 防止面板已关时访问野指针）。
			SNew(SComboBox<TSharedPtr<FCrossingChunkRule>>)
			.OptionsSource(&Rows)
			.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
			.ToolTipText(LOCTEXT("AssignTip", "把这个目录划给某个分块（相当于在内容浏览器里右键它）"))
			.OnGenerateWidget_Lambda([](TSharedPtr<FCrossingChunkRule> Option)
			{
				return SNew(STextBlock).Text(FText::FromString(Option.IsValid()
					? FString::Printf(TEXT("Chunk %d · %s"), Option->ChunkId, *Option->ChunkName)
					: FString()));
			})
			.OnSelectionChanged_Lambda([this, Folder](TSharedPtr<FCrossingChunkRule> Option, ESelectInfo::Type SelectType)
			{
				if (SelectType == ESelectInfo::Direct || !Option.IsValid())
				{
					return;
				}
				FString Message;
				FCrossingChunkRuleService::Assign(Folder, Option->ChunkId, Option->ChunkName, Message);
				StatusMessage = Message;

				TWeakPtr<SWidget> WeakSelf = AsShared();
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf](float)
				{
					TSharedPtr<SWidget> Pinned = WeakSelf.Pin();
					if (Pinned.IsValid())
					{
						StaticCastSharedPtr<SCrossingChunkPanel>(Pinned)->RefreshRules();
					}
					return false;
				}), 0.f);
			})
			[
				// 只用文字：▾ 这类符号在编辑器默认字体里会渲染成方框
				SNew(STextBlock).Text(LOCTEXT("AssignTo", "指派到"))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Locate", "定位"))
			.ToolTipText(LOCTEXT("LocateTip", "在内容浏览器里选中这个目录，方便右键归类"))
			.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
			.OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnLocateClicked, Folder))
		];
}

FReply SCrossingChunkPanel::OnDisbandClicked(int32 ChunkId)
{
	// 二次确认：解散会直接改写规则文件（打包读的就是它），所以先问一句。
	// 提示里把「可以撤回」写清楚 —— 就算手滑点错，也还有第二条退路。
	FString ChunkName;
	for (const TSharedPtr<FCrossingChunkRule>& Row : Rows)
	{
		if (Row.IsValid() && Row->ChunkId == ChunkId) { ChunkName = Row->ChunkName; break; }
	}
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(FString::Printf(
		TEXT("确定解散分块 %d「%s」吗？\n\n")
		TEXT("· 它的目录会重新变成「未归类」\n")
		TEXT("· 规则文件会立刻改写（下次打包按新的来）\n\n")
		TEXT("点错了也别慌：顶部工具条右边的《撤销解散》，能把它连同目录一起恢复。"),
		ChunkId, *ChunkName)));
	if (Answer != EAppReturnType::Ok)
	{
		StatusMessage = TEXT("已取消解散");
		return FReply::Handled();
	}

	FCrossingChunkRule Removed;
	FString Message;
	if (FCrossingChunkRuleService::RemoveChunk(ChunkId, Removed, Message))
	{
		// 会话内留一份，供顶部「撤销解散」按钮恢复
		UndoRule = MakeShared<FCrossingChunkRule>(Removed);
		RefreshRules();
	}
	StatusMessage = Message;
	return FReply::Handled();
}

FReply SCrossingChunkPanel::OnUndoDisbandClicked()
{
	if (!UndoRule.IsValid())
	{
		return FReply::Handled();
	}

	FString Message;
	if (FCrossingChunkRuleService::RestoreChunk(*UndoRule, Message))
	{
		UndoRule.Reset();
		RefreshRules();
	}
	StatusMessage = Message;
	return FReply::Handled();
}

FReply SCrossingChunkPanel::OnLocateClicked(FString Folder)
{
	FContentBrowserModule& ContentBrowser =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FString> Paths;
	Paths.Add(Folder);
	ContentBrowser.Get().SyncBrowserToFolders(Paths, /*bForce=*/ false, /*bFocusContentBrowser=*/ true);
	return FReply::Handled();
}

FReply SCrossingChunkPanel::OnRefreshClicked()
{
	RefreshRules();
	RefreshStatus();
	return FReply::Handled();
}

SCrossingChunkPanel::~SCrossingChunkPanel()
{
	if (TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle); }
	if (ProcHandle.IsValid()) { FPlatformProcess::CloseProc(ProcHandle); }
	if (ReadPipe || WritePipe) { FPlatformProcess::ClosePipe(ReadPipe, WritePipe); }
}

	FString SCrossingChunkPanel::BuildPackCommand() const
{
	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Pack-CrossingVoid.ps1"));
	// 服务器目标没有"版本/补丁"概念（出散件、走 git），统一按 Quick 传
	const bool bServer = SelectedTarget.IsValid() && SelectedTarget->Target == TEXT("Server");
	const FString Mode = (bServer || !SelectedMode.IsValid()) ? TEXT("Quick") : SelectedMode->Value;
	const FString Platform = SelectedTarget.IsValid() ? SelectedTarget->Platform : TEXT("Win64");
	const FString Target = SelectedTarget.IsValid() ? SelectedTarget->Target : TEXT("Client");
	return FString::Printf(
		TEXT("& '%s' -Mode %s -Platform %s -Target %s%s"),
		*Script, *Mode, *Platform, *Target, *BuildExtraArgs());
}

FString SCrossingChunkPanel::BuildExtraArgs() const
{
	FString Extra;
	// 这两个值最终会被塞进外层 -Command "..." 的双引号里，所以内层引号必须写成 \"。
	// Windows 解析命令行时裸引号会被吃掉，PowerShell 收到就成了裸值：
	//   · 逗号被当成数组分隔符 -> -Maps 绑不定 [string]（实测报 ParameterArgumentTransformationError）
	//   · 带空格的路径被拆成两个参数
	if (!OutputDir.IsEmpty())
	{
		Extra += FString::Printf(TEXT(" -ArchiveDir \\\"%s\\\""), *OutputDir);
	}
	if (!MapsCsv.IsEmpty())
	{
		Extra += FString::Printf(TEXT(" -Maps \\\"%s\\\""), *MapsCsv);
	}
	const FString BaselineVer = GetReleaseVersion();
	if (!BaselineVer.IsEmpty())
	{
		Extra += FString::Printf(TEXT(" -ReleaseVersion \\\"%s\\\""), *BaselineVer);
	}
	if (!PlayerVersion.IsEmpty())
	{
		Extra += FString::Printf(TEXT(" -PlayerVersion \\\"%s\\\""), *PlayerVersion);
	}
	// 1 = 单进程，等价于不加参数（脚本默认值就是 1），所以这里只在 >1 时才拼
	if (CookProcessCount > 1)
	{
		Extra += FString::Printf(TEXT(" -CookProcessCount %d"), CookProcessCount);
	}
	return Extra;
}

FString SCrossingChunkPanel::BuildClearCacheCommand() const
{
	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Pack-CrossingVoid.ps1"));
	return FString::Printf(TEXT("& '%s' -ClearCache"), *Script);
}

FString SCrossingChunkPanel::BuildStatusCommand() const
{
	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Pack-CrossingVoid.ps1"));
	const FString Platform = SelectedTarget.IsValid() ? SelectedTarget->Platform : TEXT("Win64");
	const FString Target = SelectedTarget.IsValid() ? SelectedTarget->Target : TEXT("Client");
	const FString Mode = SelectedMode.IsValid() ? SelectedMode->Value : TEXT("Quick");
	// 内层引号要写成 \"（原因见 BuildExtraArgs）
	return FString::Printf(
		TEXT("& '%s' -Status -Mode %s -Platform %s -Target %s -ReleaseVersion \\\"%s\\\" -ArchiveDir \\\"%s\\\""),
		*Script, *Mode, *Platform, *Target, *GetReleaseVersion(), *OutputDir);
}

FString SCrossingChunkPanel::DeriveReleaseVersion(const FString& InPlayerVersion)
{
	const FString V = InPlayerVersion.TrimStartAndEnd();
	if (V.IsEmpty()) { return V; }

	TArray<FString> Parts;
	V.ParseIntoArray(Parts, TEXT("."), /*InCullEmpty=*/true);
	if (Parts.Num() >= 2) { return Parts[0] + TEXT(".") + Parts[1]; }
	return V;
}

FString SCrossingChunkPanel::GetReleaseVersion() const
{
	// 派生值：只在玩家版本上算，不存不缓存，所以不会出现"两边对不上"
	return DeriveReleaseVersion(PlayerVersion);
}

bool SCrossingChunkPanel::RunScriptCapture(const FString& ScriptBody, FString& OutText)
{
	OutText.Empty();

	void* R = nullptr;
	void* W = nullptr;
	FPlatformProcess::CreatePipe(R, W);

	const FString Args = FString::Printf(
		TEXT("-NoProfile -ExecutionPolicy Bypass -Command \"[Console]::OutputEncoding=[Text.Encoding]::UTF8; %s\""),
		*ScriptBody);

	uint32 ProcessId = 0;
	// 注意：WaitForProc / CloseProc 收的是 FProcHandle&（内部会改句柄），所以不能加 const
	FProcHandle H = FPlatformProcess::CreateProc(
		TEXT("powershell.exe"), *Args,
		/*bLaunchDetached=*/false, /*bLaunchHidden=*/true, /*bLaunchReallyHidden=*/true,
		&ProcessId, /*PriorityModifier=*/0, *FPaths::ProjectDir(),
		W, R);

	if (!H.IsValid())
	{
		FPlatformProcess::ClosePipe(R, W);
		return false;
	}

	FPlatformProcess::WaitForProc(H);

	// 输出只有三行，进程退出后读干即可；多读几次纯属保险
	for (int32 Guard = 0; Guard < 8; ++Guard)
	{
		const FString Chunk = FPlatformProcess::ReadPipe(R);
		if (Chunk.IsEmpty()) { break; }
		OutText += Chunk;
	}

	FPlatformProcess::CloseProc(H);
	FPlatformProcess::ClosePipe(R, W);
	return true;
}

void SCrossingChunkPanel::RefreshStatus()
{
	auto ModeLabel = [](const FString& M) -> FString
	{
		if (M == TEXT("Quick")) { return TEXT("快速验证"); }
		if (M == TEXT("Base"))  { return TEXT("版本更新"); }
		if (M == TEXT("Patch")) { return TEXT("常规补丁"); }
		return M;
	};

	FString Text;
	// 三个人肉兜底：解析不出来也要看得见，而不是留一格空白（那是最难查的毛病）
	StatusReportText = TEXT("报告：—");
	StatusArtifactText = TEXT("产物：—");
	StatusBaselineText = TEXT("基线：—");
	bBaselineExists = false;

	// 服务器目标是「散件走 git」：不写分包报告、也不建基线。这两格对它是"不适用"，
	// 不是"丢失" —— 文案要写清楚，否则每次切到服务器都会以为数据被删了（2026-09-20）。
	const bool bServerTarget = SelectedTarget.IsValid() && SelectedTarget->Target == TEXT("Server");

	// 状态由脚本写成一个小 json（不去解析 stdout：那东西会随终端宽度换行）
	const FString StatusPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("PackLogs/pack-status.json"));

	// 先删旧的，免得脚本万一没跑起来却读到上一次的状态
	IFileManager::Get().Delete(*StatusPath, /*bRequireExists=*/false, /*bEvenReadOnly=*/true, /*bQuiet=*/true);

	// 只为了让它把 json 写出来，stdout 不用了
	if (!RunScriptCapture(BuildStatusCommand(), Text))
	{
		StatusReportText = TEXT("报告：（状态查询没跑起来）");
		StatusArtifactText = TEXT("产物：—");
		StatusBaselineText = TEXT("基线：—");
		return;
	}

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *StatusPath))
	{
		StatusReportText = TEXT("报告：（状态文件没生成）");
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		StatusReportText = TEXT("报告：（状态解析失败）");
		return;
	}

	// 报告
	bool bRepExists = false;
	Root->TryGetBoolField(TEXT("reportExists"), bRepExists);
	if (bRepExists)
	{
		FString RepTime, RepMode;
		Root->TryGetStringField(TEXT("reportTime"), RepTime);   // 2026-09-19T12:32:28+08:00
		Root->TryGetStringField(TEXT("reportMode"), RepMode);
		if (RepTime.Len() >= 16) { RepTime = RepTime.Mid(5, 5) + TEXT(" ") + RepTime.Mid(11, 5); }
		StatusReportText = FString::Printf(TEXT("报告：%s · %s"), *RepTime, *ModeLabel(RepMode));
	}
	else if (bServerTarget)
	{
		StatusReportText = TEXT("报告：不适用（服务器散件走 git，不分包）");
	}
	else
	{
		StatusReportText = TEXT("报告：还没有（还没打包过）");
	}

	// 产物
	FString ArtPath;
	bool bArtExists = false;
	Root->TryGetStringField(TEXT("artifactPath"), ArtPath);
	Root->TryGetBoolField(TEXT("artifactExists"), bArtExists);
	StatusArtifactText = bArtExists
		? FString::Printf(TEXT("产物：%s ✓"), *ArtPath)
		: FString::Printf(TEXT("产物：%s（已删除）"), *ArtPath);

	// 基线
	FString BasePath, RelRoot;
	Root->TryGetStringField(TEXT("baselinePath"), BasePath);
	Root->TryGetStringField(TEXT("releaseRoot"), RelRoot);
	if (!RelRoot.IsEmpty()) { StatusReleaseRoot = RelRoot; }
	Root->TryGetBoolField(TEXT("baselineExists"), bBaselineExists);
	const FString BaselineVer = GetReleaseVersion();
	if (bServerTarget)
	{
		StatusBaselineText = TEXT("基线：不适用（服务器不分包，不建基线）");
	}
	else if (BaselineVer.IsEmpty())
	{
		StatusBaselineText = TEXT("基线：先填玩家版本");
	}
	else if (bBaselineExists)
	{
		StatusBaselineText = FString::Printf(TEXT("基线：%s ✓"), *BaselineVer);
	}
	else
	{
		StatusBaselineText = FString::Printf(TEXT("基线：%s 还没建"), *BaselineVer);
	}

	// 平台设置（防忘）：包名 / 版本号 / SDK / ABI / 图标状态
	const TSharedPtr<FJsonObject>* PS = nullptr;
	if (Root->TryGetObjectField(TEXT("platformSettings"), PS) && PS && (*PS).IsValid())
	{
		FString AVer, AName, ASdkMin, ASdkMax, AArm64, AStore;
		const TSharedPtr<FJsonObject>* A = nullptr;
		if ((*PS)->TryGetObjectField(TEXT("android"), A) && A && (*A).IsValid())
		{
			(*A)->TryGetStringField(TEXT("VersionDisplayName"), AVer);
			(*A)->TryGetStringField(TEXT("PackageName"), AName);
			(*A)->TryGetStringField(TEXT("MinSDKVersion"), ASdkMin);
			(*A)->TryGetStringField(TEXT("TargetSDKVersion"), ASdkMax);
			(*A)->TryGetStringField(TEXT("bBuildForArm64"), AArm64);
			(*A)->TryGetStringField(TEXT("StoreVersion"), AStore);
		}

		int32 AIconCount = 0;
		int32 AIconCompared = 0;
		int32 AIconDefault = 0;
		FString AIconTime;
		const TSharedPtr<FJsonObject>* AI = nullptr;
		if ((*PS)->TryGetObjectField(TEXT("androidIcons"), AI) && AI && (*AI).IsValid())
		{
			(*AI)->TryGetNumberField(TEXT("count"), AIconCount);
			(*AI)->TryGetStringField(TEXT("lastWrite"), AIconTime);
			(*AI)->TryGetNumberField(TEXT("compared"), AIconCompared);
			(*AI)->TryGetNumberField(TEXT("defaultCount"), AIconDefault);
		}

		bool bWinIcon = false;
		FString WinIconTime;
		const TSharedPtr<FJsonObject>* WI = nullptr;
		if ((*PS)->TryGetObjectField(TEXT("windowsIcon"), WI) && WI && (*WI).IsValid())
		{
			(*WI)->TryGetBoolField(TEXT("exists"), bWinIcon);
			(*WI)->TryGetStringField(TEXT("lastWrite"), WinIconTime);
		}

		// 别在 Printf 里塞临时对象（*FString::Printf(...) 会踩到悬垂指针），先落到局部变量
		const FString WinIconText = bWinIcon
			? FString::Printf(TEXT("Windows 图标 %s"), *WinIconTime)
			: FString(TEXT("[警告] Windows 图标没有 → 把 .ico 放到 Build\\Windows\\Application.ico（这项没有界面设置）"));

		StatusPlatformText = FString::Printf(
			TEXT("平台设置  安卓 %s（商店号 %s）· %s · SDK %s/%s%s · 图标 %d 张（%s）   ｜   %s"),
			*AVer, *AStore, *AName, *ASdkMin, *ASdkMax,
			(AArm64 == TEXT("True") ? TEXT(" · arm64") : TEXT("")),
			AIconCount, *AIconTime, *WinIconText);

		// 安卓图标是不是还是引擎默认（脚本按 hash 比出来的，不靠感觉）
		const FString AIconText = (AIconCompared > 0 && AIconDefault == AIconCompared)
			? FString::Printf(TEXT("[警告] 图标 %d 张全是引擎默认 → 去「项目设置 → 平台 → Android → 图标」换"), AIconCount)
			: (AIconDefault > 0
				? FString::Printf(TEXT("[警告] 图标有 %d/%d 张还是引擎默认 → 去「项目设置 → 平台 → Android → 图标」换"), AIconDefault, AIconCompared)
				: FString::Printf(TEXT("图标 %d 张（%s）"), AIconCount, *AIconTime));

		// 日志区最上面那份"打包前过目"的设置块（页面打开时写一次；每次打包开始时重写）
		PlatformLogHeader = FString::Printf(
			TEXT("===== 当前平台设置（打包前过目一遍）=====\n")
			TEXT("  安卓   版本 %s（商店号 %s）· %s\n")
			TEXT("         SDK %s/%s%s · %s\n")
			TEXT("  %s\n")
			TEXT("  （图标放这里：Build\\Windows\\Application.ico 与 Build\\Android\\res\\drawable*/icon.png）\n\n"),
			*AVer, *AStore, *AName, *ASdkMin, *ASdkMax,
			(AArm64 == TEXT("True") ? TEXT(" · arm64") : TEXT("")),
			*AIconText, *WinIconText);

		// 页面刚打开、日志还是空的时候，把它顶上去（刷新时不覆盖已有日志）
		if (LogText.IsEmpty() && !PlatformLogHeader.IsEmpty() && LogBox.IsValid())
		{
			LogText = PlatformLogHeader;
			LogBox->SetText(FText::FromString(LogText));
		}
	}
}

bool SCrossingChunkPanel::LaunchScript(const FString& ScriptBody, const FString& LogFirstLine, bool bIsClearCache)
{
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	const FString Args = FString::Printf(
		TEXT("-NoProfile -ExecutionPolicy Bypass -Command \"[Console]::OutputEncoding=[Text.Encoding]::UTF8; %s\""),
		*ScriptBody);

	uint32 ProcessId = 0;
	ProcHandle = FPlatformProcess::CreateProc(
		TEXT("powershell.exe"), *Args,
		/*bLaunchDetached=*/false, /*bLaunchHidden=*/true, /*bLaunchReallyHidden=*/true,
		&ProcessId, /*PriorityModifier=*/0, *FPaths::ProjectDir(),
		WritePipe, ReadPipe);

	if (!ProcHandle.IsValid())
	{
		StatusMessage = TEXT("启动 powershell.exe 失败。");
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
		return false;
	}

	bRunIsClearCache = bIsClearCache;
	LogText = FString::Printf(TEXT("> %s\n"), *LogFirstLine);
	if (LogBox.IsValid()) { LogBox->SetText(FText::FromString(LogText)); }

	bPacking = true;
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SCrossingChunkPanel::TickPump), 0.15f);
	}
	return true;
}

FReply SCrossingChunkPanel::OnClearCacheClicked()
{
	if (ProcHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcHandle))
	{
		StatusMessage = TEXT("有任务在跑 —— 先点「停止打包」，再来清缓存。");
		return FReply::Handled();
	}

	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Pack-CrossingVoid.ps1"));
	if (!FPaths::FileExists(Script))
	{
		StatusMessage = FString::Printf(TEXT("找不到打包脚本：%s"), *Script);
		return FReply::Handled();
	}

	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("ClearCacheConfirm",
			"清除缓存会删掉这三样（都能自动重建）：\n"
			"  · Saved\\StagedBuilds —— UAT 组装的产物\n"
			"  · Saved\\Cooked —— cooked 数据\n"
			"  · Saved\\Shaders —— shader 缓存\n\n"
			"不会动 Intermediate\\Build（编译产物）、Content、归档目录。\n"
			"下次打包会慢一些（要重新 cook / 组装）。继续吗？"));
	if (Answer != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	if (LaunchScript(BuildClearCacheCommand(), BuildClearCacheCommand(), /*bIsClearCache=*/true))
	{
		StatusMessage = TEXT("清缓存中…");
	}
	return FReply::Handled();
}

FReply SCrossingChunkPanel::OnPackClicked()
{
	SaveSettings();   // 打包前把当前选项记下来（下次打开就是这套）

	// 每次打包开始都把"当前平台设置"重新顶到日志最上面 —— 这就是防忘那一下
	if (!PlatformLogHeader.IsEmpty())
	{
		LogText = PlatformLogHeader;
		if (LogBox.IsValid()) { LogBox->SetText(FText::FromString(LogText)); }
	}
	if (ProcHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcHandle))
	{
		// 再点一次 = 中止（比如刚发现输出目录填错了，不用干等它跑完）
		StopPack();
		return FReply::Handled();
	}

	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Pack-CrossingVoid.ps1"));
	if (!FPaths::FileExists(Script))
	{
		StatusMessage = FString::Printf(TEXT("找不到打包脚本：%s"), *Script);
		return FReply::Handled();
	}

	// ---- 前置检查：这次需不需要编译？----
	// 目标 Shipping 二进制不存在时，打包脚本会给 UAT 加 -build；
	// 但编辑器开着（Live Coding 激活）时，UE 不允许从外部编译，
	// UBT 会直接抛 "Unable to build while Live Coding is active"。
	// 与其跑到一半失败，不如在点的那一刻就说清楚、并给出可执行的做法。
	const bool bServer = SelectedTarget.IsValid() && SelectedTarget->Target == TEXT("Server");
	const FString TargetName = bServer ? TEXT("CrossingVoidServer") : TEXT("CrossingVoid");
	const FString ShippingBinary = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("Binaries/Win64/") / (TargetName + TEXT("-Win64-Shipping.target")));
	if (!FPaths::FileExists(ShippingBinary))
	{
		// 打包不负责编译 —— 所以缺二进制时不硬闯（硬闯的话 UAT 会顺带编编辑器目标，
		// 而编辑器开着时 Live Coding 会挡住它：Unable to build while Live Coding is active）。
		StatusMessage = FString::Printf(
			TEXT("这个目标还没编译过（缺 %s-Win64-Shipping）。打包脚本会先单独把它编出来再继续 —— ")
			TEXT("单独编译只针对这个目标，不会被 Live Coding 挡住，所以编辑器不用关。"),
			*TargetName);
		LogText += FString::Printf(
			TEXT("\n[前置检查] 缺少 %s\n           %s\n           （第一次会编译得久一些，之后就不用再编了）\n"),
			*ShippingBinary, *StatusMessage);
		if (LogBox.IsValid()) { LogBox->SetText(FText::FromString(LogText)); }
		// 注意：这里【不 return】—— 让打包照常启动，脚本会先把目标编好。
	}

	// ---- 前置检查：《常规补丁》必须先有一条基线 ----
	// 脚本自己也会拦，但这里先拦能把"该怎么做"直接说清楚。
	if (!bServer && SelectedMode.IsValid() && SelectedMode->Value == TEXT("Patch"))
	{
		RefreshStatus();   // 现查一次，不拿旧状态判断
		if (!bBaselineExists)
		{
			const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(FString::Printf(
				TEXT("《常规补丁》要基于一条已建的基线才算得出差异，现在找不到基线版本「%s」对应的目录。\n\n")
				TEXT("  基线目录：%s\n\n")
				TEXT("要先切到《版本更新》跑一次吗？它出完整包并建立基线，之后改内容再用《常规补丁》。"),
				*GetReleaseVersion(), *StatusReleaseRoot)));
			if (Answer == EAppReturnType::Yes)
			{
				for (const TSharedPtr<FChoice>& Option : ModeOptions)
				{
					if (Option.IsValid() && Option->Value == TEXT("Base")) { SelectedMode = Option; break; }
				}
				SaveSettings();
				StatusMessage = TEXT("已切到《版本更新》—— 再点一次开始打包就会建立基线");
			}
			else
			{
				StatusMessage = TEXT("没有基线 —— 先跑一次《版本更新》");
			}
			return FReply::Handled();
		}
	}

	// 起进程、收日志那套收在 LaunchScript 里 —— 清缓存按钮走的是同一条路。
	if (LaunchScript(BuildPackCommand(), BuildPackCommand(), /*bIsClearCache=*/false))
	{
		StatusMessage = TEXT("打包中…");
	}
	return FReply::Handled();
}

void SCrossingChunkPanel::StopPack()
{
	// 杀进程树：脚本是 powershell.exe → RunUAT.bat → UnrealEditor-Cmd，
	// 只杀 PowerShell 的话，后台的 cook 进程会继续跑，等于没停。
	if (ProcHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(ProcHandle, /*KillTree=*/true);
		FPlatformProcess::CloseProc(ProcHandle);
		ProcHandle.Reset();
	}
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
	}
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	bPacking = false;

	// 中途被杀时，脚本末尾的"还原 DefaultGame.ini"没机会执行 —— 这里替它收尾，
	// 否则工程配置会停在「只 cook 勾选的那几张地图」的临时状态。
	const FString BackupPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini.bak-maps-tmp"));
	const FString GameIni = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"));

	if (FPaths::FileExists(BackupPath))
	{
		IFileManager::Get().Move(*GameIni, *BackupPath, /*bReplace=*/true, /*bEvenIfReadOnly=*/true, /*bAttributes=*/false, /*bDoNotRetryOrError=*/true);
		IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("PackLogs/maps-override.txt")), false, true, true);
		LogText += TEXT("\n[已停止] 地图清单已还原（DefaultGame.ini 恢复原样）\n");
	}
	else
	{
		LogText += TEXT("\n[已停止] 打包已中断\n");
	}
	if (LogBox.IsValid()) { LogBox->SetText(FText::FromString(LogText)); }
	RestoreLiveCodingAfterPack();
	StatusMessage = TEXT("已停止打包（要改输出目录就直接改，改完再点开始）");
}

void SCrossingChunkPanel::RestoreLiveCodingAfterPack()
{
	// 打包不再碰 Live Coding（打包一律不编译 → 根本不会撞上它的互斥体），
	// 这个函数保留成空实现，免得调用点删来删去。
}

// ---------------------------------------------------------------------------
// 记住上次的选项
// ---------------------------------------------------------------------------

namespace
{
	const TCHAR* GPackageSettingsSection = TEXT("CrossingChunkPackage");
}

FString SCrossingChunkPanel::LoadSetting(const TCHAR* Key)
{
	FString Value;
	GConfig->GetString(GPackageSettingsSection, Key, Value, GEditorPerProjectIni);
	return Value;
}

void SCrossingChunkPanel::SaveSetting(const TCHAR* Key, const FString& Value)
{
	GConfig->SetString(GPackageSettingsSection, Key, *Value, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

void SCrossingChunkPanel::ApplySavedSettings()
{
	const FString SavedTarget = LoadSetting(TEXT("Target"));
	if (!SavedTarget.IsEmpty())
	{
		for (const TSharedPtr<FChoice>& Option : TargetOptions)
		{
			if (Option.IsValid() && Option->Label == SavedTarget) { SelectedTarget = Option; break; }
		}
	}

	const FString SavedMode = LoadSetting(TEXT("Mode"));
	if (!SavedMode.IsEmpty())
	{
		for (const TSharedPtr<FChoice>& Option : ModeOptions)
		{
			if (Option.IsValid() && Option->Value == SavedMode) { SelectedMode = Option; break; }
		}
	}

	const FString SavedOutputDir = LoadSetting(TEXT("OutputDir"));
	if (!SavedOutputDir.IsEmpty())
	{
		OutputDir = SavedOutputDir;
	}

	// 勾选的地图：先记下来，ReloadMaps() 扫描完之后按这份清单恢复勾选。
	// 按目标分开存（服务器打的图跟客户端不一样）；旧的单键 "Maps" 当兜底，只有第一次会用到。
	FString SavedMaps = LoadSetting(*GetMapsSettingKey());
	if (SavedMaps.IsEmpty())
	{
		SavedMaps = LoadSetting(TEXT("Maps"));
	}
	if (!SavedMaps.IsEmpty())
	{
		MapsCsv = SavedMaps;
	}

	// 玩家版本：没存过就从工程现有的安卓版本号取一次当默认值。
	// 基线版本不用存也不用读 —— 它每次由玩家版本现算（GetReleaseVersion）。
	PlayerVersion = LoadSetting(TEXT("PlayerVersion"));
	if (PlayerVersion.IsEmpty())
	{
		FString IniVersion;
		if (GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("VersionDisplayName"), IniVersion, GEngineIni))
		{
			PlayerVersion = IniVersion;
		}
	}

	// Cook 进程数：没存过就是 1（单进程），和脚本的默认值对齐
	const FString SavedCookProcessCount = LoadSetting(TEXT("CookProcessCount"));
	if (!SavedCookProcessCount.IsEmpty())
	{
		const int32 Parsed = FCString::Atoi(*SavedCookProcessCount);
		// 上限跟着 MaxCookProcessCount 走：以前存过 5 的话，这里会被夹回 4
		CookProcessCount = FMath::Clamp(Parsed, 1, MaxCookProcessCount);
	}

}

void SCrossingChunkPanel::SaveSettings()
{
	SaveSetting(TEXT("Target"), SelectedTarget.IsValid() ? SelectedTarget->Label : TEXT(""));
	SaveSetting(TEXT("Mode"), SelectedMode.IsValid() ? SelectedMode->Value : TEXT(""));
	SaveSetting(TEXT("OutputDir"), OutputDir);
	SaveSetting(*GetMapsSettingKey(), MapsCsv);   // 勾选的地图按目标分开记
	SaveSetting(TEXT("PlayerVersion"), PlayerVersion);
	SaveSetting(TEXT("CookProcessCount"), FString::FromInt(CookProcessCount));
}

bool SCrossingChunkPanel::TickPump(float DeltaSeconds)
{
	if (ReadPipe)
	{
		const FString NewText = FPlatformProcess::ReadPipe(ReadPipe);
		if (!NewText.IsEmpty())
		{
			LogText += NewText;
			// 日志只留最后 20000 字符，避免长时间打包把内存堆满
			if (LogText.Len() > 20000) { LogText.RightChopInline(LogText.Len() - 20000); }
			if (LogBox.IsValid())
			{
				LogBox->SetText(FText::FromString(LogText));
				LogBox->GoTo(ETextLocation::EndOfDocument);
			}
		}
	}

	if (ProcHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcHandle))
	{
		return true;   // 继续轮询
	}

	int32 ReturnCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
	const bool bWasClearCache = bRunIsClearCache;
	bRunIsClearCache = false;

	LogText += FString::Printf(TEXT("\n===== %s进程结束，返回码 %d =====\n"), bWasClearCache ? TEXT("清缓存") : TEXT("打包"), ReturnCode);
	if (LogBox.IsValid()) { LogBox->SetText(FText::FromString(LogText)); }
	if (bWasClearCache)
	{
		StatusMessage = (ReturnCode == 0) ? TEXT("缓存已清理") : TEXT("清缓存失败 —— 看下面的日志");
	}
	else
	{
		StatusMessage = (ReturnCode == 0) ? TEXT("打包完成，报告已刷新") : TEXT("打包失败 —— 看下面的日志");
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	ReadPipe = WritePipe = nullptr;
	FPlatformProcess::CloseProc(ProcHandle);
	ProcHandle.Reset();
	bPacking = false;
	RestoreLiveCodingAfterPack();

	TickerHandle.Reset();
	RefreshRules();   // 报告是脚本刚产出的，重新读一遍
	RefreshStatus();  // 产物 / 基线的状态也跟着变

	// 跑完了给个动静：原版那声提示音 + 原生 toast（清缓存是顺手操作，不吵人）
	if (!bWasClearCache)
	{
		NotifyPackFinished(ReturnCode == 0);
	}
	return false;
}

void SCrossingChunkPanel::NotifyPackFinished(bool bSuccess)
{
	// 音效：和编辑器自己「编译成功 / 编译失败」播的是同两个 cue
	// （引擎就是这么干的：MainFrameModule.cpp 里 CompileSuccess_Cue / CompileFailed_Cue）
	if (GEditor)
	{
		GEditor->PlayEditorSound(bSuccess
			? TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue")
			: TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
	}

	// 提示：编辑器原生 toast —— 成功绿勾、失败红叉，右下角弹出来，点标题下的链接能直接开日志目录
	FNotificationInfo Info(bSuccess
		? LOCTEXT("PackSucceededTitle", "打包成功")
		: LOCTEXT("PackFailedTitle", "打包失败"));
	Info.bUseSuccessFailIcons = true;
	Info.ExpireDuration = bSuccess ? 6.0f : 12.0f;
	Info.SubText = bSuccess
		? LOCTEXT("PackSucceededSub", "产物和分包报告都刷新好了")
		: LOCTEXT("PackFailedSub", "看日志找原因（面板下面的日志区 / 日志目录）");
	Info.Hyperlink = FSimpleDelegate::CreateLambda([]()
	{
		FPlatformProcess::ExploreFolder(*(FPaths::ProjectSavedDir() / TEXT("PackLogs")));
	});
	Info.HyperlinkText = LOCTEXT("PackNotifyOpenLogs", "打开日志目录");

	if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

FReply SCrossingChunkPanel::OnCopyCommandClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*BuildPackCommand());
	StatusMessage = TEXT("命令已复制到剪贴板");
	return FReply::Handled();
}

FReply SCrossingChunkPanel::OnOpenLogDirClicked()
{
	FPlatformProcess::ExploreFolder(*(FPaths::ProjectSavedDir() / TEXT("PackLogs")));
	return FReply::Handled();
}

FText SCrossingChunkPanel::GetPackButtonText() const
{
	return bPacking ? LOCTEXT("StopPackLabel", "停止打包") : LOCTEXT("StartPack", "开始打包");
}

void SCrossingChunkPanel::OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType, int32 ChunkId)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	FString Message;
	if (FCrossingChunkRuleService::RenameChunk(ChunkId, NewText.ToString(), Message))
	{
		RefreshRules();
	}
	StatusMessage = Message;
}

FText SCrossingChunkPanel::GetSummaryText() const
{
	if (!StatusMessage.IsEmpty())
	{
		return FText::FromString(StatusMessage);
	}
	return FText::FromString(FString::Printf(
		TEXT("共 %d 个分块、%d 个目录标记   规则文件：%s"),
		Rows.Num(), FolderTotal, *FCrossingChunkRuleService::GetRuleIniPath()));
}

#undef LOCTEXT_NAMESPACE

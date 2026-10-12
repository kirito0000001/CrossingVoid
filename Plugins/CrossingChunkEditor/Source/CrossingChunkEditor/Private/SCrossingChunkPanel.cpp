#include "SCrossingChunkPanel.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"                                          // GEditor->PlayEditorSound
#include "Framework/Application/SlateApplication.h"          // FSlateApplication（新建分类的弹出输入框）
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
#include "Misc/DateTime.h"                                   // FDateTime（算"变化基准"隔了多久）
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

	OutputDir = TEXT("F:\\DaBaoV\\Crossingvoid");              // 和脚本 -ArchiveDir 的默认值一致
	ReleaseRoot = TEXT("F:\\DaBaoV\\Crossingvoid\\Release");   // 和脚本 -ReleaseRoot 的默认值一致
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
			// ---- 打包：和命令行是同一套命令（插件自带的 Tools\Pack-CrossingVoid.ps1）----
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
					if (Option.IsValid())
					{
						SelectedMode = Option;
						SaveSettings();

						// 换模式也要刷新列表：报告是按模式分的（《快速验证》单独一份），
						// 右栏在《版本更新》下还要切成「待正式划分」的补丁批次。
						// 推迟到下一帧 —— 这个回调正跑在 combo 自己的回调里，
						// 当场重建整棵树会踩到自己（同 CommitCategory / 指派到）。
						TWeakPtr<SWidget> WeakSelf = AsShared();
						FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf](float)
						{
							if (TSharedPtr<SWidget> Pinned = WeakSelf.Pin())
							{
								StaticCastSharedPtr<SCrossingChunkPanel>(Pinned)->RefreshRules();
							}
							return false;
						}), 0.f);
					}
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
				// 《版本更新》下还有补丁批次就把按钮灰掉（打包中除外 —— 那时它是停止打包）。
				// 灰着还能悬停看原因，所以 tooltip 也跟着变。
				.IsEnabled_Lambda([this]() { return CanStartPack(); })
				.ToolTipText_Lambda([this]()
				{
					if (ShouldShowPatchBatches())
					{
						return FText::FromString(FString::Printf(
							TEXT("《版本更新》前要先把 %d 个补丁批次正式划分掉（右栏那份清单）—— 分完按钮才会亮"),
							PatchBatches.Num()));
					}
					return LOCTEXT("PackTip", "跑插件自带的 Tools\\Pack-CrossingVoid.ps1（日志在下面实时滚动，结束自动刷新报告）");
				})
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
						if (OutputDir.IsEmpty()) { OutputDir = TEXT("F:\\DaBaoV\\Crossingvoid"); }
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
				// 显示界面里配置的那份（每次打包/查状态都会原样传给脚本）；脚本还没回报时退回它自己报的值
				.Text_Lambda([this]()
				{
					const FString& Root = ReleaseRoot.IsEmpty() ? StatusReleaseRoot : ReleaseRoot;
					return FText::FromString(Root.IsEmpty() ? TEXT("基线目录：—") : FString::Printf(TEXT("基线目录：%s"), *Root));
				})
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
				// ---- 基线位置：建基线和算补丁都走它，改完下次打包生效（存进 EditorPerProjectUserSettings.ini 的 [CrossingChunkPackage]）----
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 0.f, 6.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ReleaseRootLabel", "基线位置"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
					[
						SNew(SBox).WidthOverride(360.f).MinDesiredHeight(26.f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() { return FText::FromString(ReleaseRoot); })
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
							.ToolTipText(LOCTEXT("ReleaseRootTip",
								"基线（Releases）存放的根目录，传给脚本的 -ReleaseRoot。\n"
								"《版本更新》往这里建基线（一条约 1.5 GB），《常规补丁》也从这里读基线算差异。\n"
								"留空恢复默认 F:\\DaBaoV\\Crossingvoid\\Release"))
							.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
							.SelectAllTextWhenFocused(true)
							.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
							{
								ReleaseRoot = NewText.ToString().TrimStartAndEnd();
								if (ReleaseRoot.IsEmpty()) { ReleaseRoot = TEXT("F:\\DaBaoV\\Crossingvoid\\Release"); }
								SaveSettings();
								RefreshStatus();
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ReleaseRootHint", "改完下次打包生效；和「输出目录」放在同一个盘最省事"))
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

		// ---- 变化基准（单独一行，紧贴表格上方）----
		// 「增 / 删 / 改」是跟 latest.json 里那份报告比出来的，而打包失败不会刷新它 ——
		// 基准是哪一份、隔了多久必须一眼看得见，否则"打完包为什么不是无变化"又得靠翻文件查。
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 4.f, 8.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return GetDeltaBaseText(); })
			.ColorAndOpacity_Lambda([this]() { return GetDeltaBaseColor(); })
			.ToolTipText(LOCTEXT("DeltaBaseTip", "「变化」列 = 相对这份报告算出来的差异（增 = 新增文件，删 = 删除文件，改 = 内容有修改）。\n《版本更新》和《常规补丁》共用一份报告；《快速验证》单独一份，不会动这里的基准。\n打包失败不会刷新基准 —— 基准旧了，这一列显示的就是「自那以后的全部改动」，不只是这次的。"))
		]

		// ---- 列头 + 规则列表（外面套一层横向滚动：列多到一屏放不下时底部出现横向滚动条，
		//      平时变化列跟内容走很窄，根本用不到它） ----
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)

					// 列头（宽度必须和下面每行一一对应，否则看着就是错位的）
					+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 2.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(36.f)[SNew(STextBlock).Text(LOCTEXT("ColId", "号")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[SNew(SBox).WidthOverride(170.f)[SNew(STextBlock).Text(LOCTEXT("ColName", "名字（双击可改）")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)[SNew(SBox).WidthOverride(88.f)[SNew(STextBlock).Text(LOCTEXT("ColCategory", "分类")).ToolTipText(LOCTEXT("ColCategoryTip", "显示用分类：面板列表和右键菜单按它分组，不影响打包。\n从下拉选一个、选「未分组」清除，或选「新建分类…」—— 会弹一个小输入框，填名字回车即建。\n（说明写在这里而不是下拉上：挂在 combo 上的 tooltip 会盖住展开的列表）")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[SNew(SSpacer)]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(64.f)[SNew(STextBlock).Text(LOCTEXT("ColFolders", "目录")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(80.f)[SNew(STextBlock).Text(LOCTEXT("ColFiles", "文件")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(92.f)[SNew(STextBlock).Text(LOCTEXT("ColSize", "体积")).ToolTipText(LOCTEXT("ColSizeTip", "每个 chunk 的容器体积（pak + ucas + utoc），来自打包报告。\n安卓的 pak 打进 APK/OBB，没有容器文件可量 —— 那一列显示「不统计」（文件数和变化照常有）。")).ColorAndOpacity(FSlateColor::UseSubduedForeground())]]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SBox)
							.WidthOverride_Lambda([this]() -> FOptionalSize { return DeltaColumnWidth; })
							[
								SNew(STextBlock).Text(LOCTEXT("ColDelta", "变化")).ToolTipText(LOCTEXT("ColDeltaTip", "增 = 新增文件，删 = 删除文件，改 = 内容有修改（和上一次打包报告比）")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
							]
						]
					]

					// 行（纵向滚动）
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f, 0.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(RowsBox, SVerticalBox)
						]
					]
				]
			]
		]

		// ---- 左栏到此为止，切到右栏 ----
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.f, 0.f)
			[
				SNew(SVerticalBox)

		// ---- 未分类（本次变更）----
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 6.f, 8.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetUnclassBoxTitle(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("RescanUnclass", "重新扫描"))
				.ToolTipText(LOCTEXT("RescanUnclassTip", "重新扫一遍 AssetRegistry，列出当前没被任何分块规则覆盖的资源目录。\nWorld Partition 的外部 actor（__ExternalActors__ / __ExternalObjects__）不列 —— 它们 cook 期会并进关卡，不用归类"))
				.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
				.OnClicked(FOnClicked::CreateSP(this, &SCrossingChunkPanel::OnRescanUnclassClicked))
			]
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

/**
 * 「增 N / 删 N / 改 N」这一行的粗估宽度（默认字体：汉字≈13px、数字≈8px）。宁宽勿裁。
 * 放在 RefreshRules 之前：它就是给 RefreshRules 算列宽用的，C++ 要求先声明后使用。
 */
static float EstimateDeltaWidthOf(const SCrossingChunkPanel::FReportChunk& Info)
{
	float Width = 0.f;
	const auto Part = [&Width](int32 Count)
	{
		if (Count <= 0) { return; }
		const int32 DigitCount = FString::Printf(TEXT("%d"), Count).Len();
		Width += 13.f + 4.f + DigitCount * 8.f + 10.f;   // 汉字 + 空格 + 数字 + 块间距
	};
	Part(Info.Added);
	Part(Info.Removed);
	Part(Info.Modified);
	if (Width <= 0.f) { Width = 40.f; }   // 「无变化」
	return Width + 16.f;                  // 余量
}

/**
 * 右栏「本次变更」里「N 个文件」那一格的宽度（默认字体：汉字≈13px、数字≈8px）。宁宽勿裁。
 * 和「变化」列同一个思路：取全表最宽的一条当统一宽度，各行同宽 ——
 * 否则「6 个文件」和「1050 个文件」各占各的宽，右边那两个按钮会跟着左右跳。
 */
static float EstimateUnclassCountWidth(int32 Files)
{
	const int32 Digits = FString::Printf(TEXT("%d"), Files).Len();
	return 13.f * 3.f + 8.f * Digits + 24.f;   // 「个文件」+ 数字 + 余量
}

/**
 * 分类下拉里「新建分类…」那一项的哨兵值（**不是**分类名，别写进规则文件）。
 *
 * 为什么不用 nullptr：这个下拉的列表是 `SListView<TSharedPtr<FString>>`，而 SListView
 * 生成行时会**跳过无效项**（引擎源码 SListView.h:1557 `if (!IsPtrValid(CurItem)) continue;`），
 * nullptr 那一项根本不会被画出来 —— 表现就是"下拉里只有「未分组」加一根滚动条，
 * 「新建分类…」永远点不到"。改用内容特殊的真字符串当哨兵，行生成器/选中回调认得它就行。
 */
static const TCHAR* const GNewCategoryToken = TEXT("__CrossingChunk_NewCategory__");

/** 这一项是不是「新建分类…」。 */
static bool IsNewCategoryOption(const TSharedPtr<FString>& Option)
{
	return Option.IsValid() && Option->Equals(GNewCategoryToken, ESearchCase::CaseSensitive);
}

/**
 * 《常规补丁》登记出来的批次分块的名字前缀（「版本补丁1」「版本补丁2」…）。
 * 登记时按它起名，《版本更新》的硬拦也按它认（IsPatchChunkName）——
 * 名字就是判据，不用往规则文件里再加字段。
 */
static const TCHAR* const GPatchChunkNamePrefix = TEXT("版本补丁");

/** 这条规则是不是补丁批次。 */
static bool IsPatchChunkName(const FString& ChunkName)
{
	return ChunkName.StartsWith(GPatchChunkNamePrefix, ESearchCase::CaseSensitive);
}

void SCrossingChunkPanel::RefreshRules()
{
	// 重建列表会销毁旧行上的控件。如果有没关干净的孤儿弹层（下拉菜单/弹出输入框
	// 还悬在半空，宿主却没了），它会一直挡在那儿吃掉点击——表现为某行的下拉
	// 「点不开」、提示条卡着不走。重建前把菜单栈清干净（没菜单时是无害空操作）。
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().AnyMenusVisible())
	{
		FSlateApplication::Get().DismissAllMenus();
	}

	Rows.Reset();
	Unclassified.Reset();
	ReportChunks.Reset();
	FolderTotal = 0;
	StatusMessage.Empty();

	if (RowsBox.IsValid()) { RowsBox->ClearChildren(); }
	if (UnclassBox.IsValid()) { UnclassBox->ClearChildren(); }

	LoadReport();           // 先读报告：它决定每行多出来的"文件数/体积/变化"
	RescanUnclassified();   // 未分类清单实时扫（对比当前规则和 AssetRegistry，不再依赖报告）
	RescanPatchBatches();   // 现有的「版本补丁N」批次（右栏显示 + 《版本更新》硬拦都用它）

	// 变化列宽度 = 全表最宽的内容：平时跟着「无变化」走很窄（不用横向滚动），
	// 某次报告变化很多时自己变宽（超宽由外层横向滚动条兜底）。
	// 全表统一宽度，各行的「解散」按钮才能上下对齐。
	DeltaColumnWidth = 56.f;
	for (const TPair<int32, FReportChunk>& Pair : ReportChunks)
	{
		DeltaColumnWidth = FMath::Max(DeltaColumnWidth, EstimateDeltaWidthOf(Pair.Value));
	}

	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!FCrossingChunkRuleService::LoadRules(Rules, Error))
	{
		StatusMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return;
	}

	// 分类下拉的选项（所有行共用）：未分组（空串）+ 现有分类（字典序）+ 「新建分类…」。
	// 「新建分类…」用**非空**哨兵（GNewCategoryToken），不能用 nullptr ——
	// SListView 生成行时会跳过无效项（SListView.h:1557 `if (!IsPtrValid(CurItem)) continue;`），
	// nullptr 那一项**永远不会被画出来**：下拉里只剩「未分组」加一根滚动条，
	// 表现就是"新建分类永远点不到"（2026-10-11 踩过）。
	CategoryOptions.Reset();
	CategoryOptions.Add(MakeShared<FString>());
	{
		TArray<FString> Categories;
		for (const FCrossingChunkRule& Rule : Rules)
		{
			// 顺带防一手：真是有人把哨兵那串名字当分类用，也不让它混进来
			if (!Rule.Category.IsEmpty() && !Rule.Category.Equals(GNewCategoryToken, ESearchCase::CaseSensitive))
			{
				Categories.AddUnique(Rule.Category);
			}
		}
		Categories.Sort();
		for (const FString& Category : Categories)
		{
			CategoryOptions.Add(MakeShared<FString>(Category));
		}
	}
	CategoryOptions.Add(MakeShared<FString>(GNewCategoryToken));

	// 显示排序：有任一分类时 → 分类（字典序，未分组垫底）→ ChunkId，组间插只读标题行；
	// 一个分类都没有 → 按号平铺（和从前一样）。排序只影响面板显示和「指派到」下拉的顺序，
	// 规则文件里的 ChunkId 不动。
	TArray<FCrossingChunkRule> Sorted = Rules;
	const bool bAnyCategory = Sorted.ContainsByPredicate(
		[](const FCrossingChunkRule& Rule) { return !Rule.Category.IsEmpty(); });
	if (bAnyCategory)
	{
		Sorted.Sort([](const FCrossingChunkRule& A, const FCrossingChunkRule& B)
		{
			const bool bAEmpty = A.Category.IsEmpty();
			const bool bBEmpty = B.Category.IsEmpty();
			if (bAEmpty != bBEmpty) { return bBEmpty; }
			if (!bAEmpty && !A.Category.Equals(B.Category, ESearchCase::CaseSensitive)) { return A.Category < B.Category; }
			return A.ChunkId < B.ChunkId;
		});
	}

	FString CurrentCategory;
	bool bGroupOpen = false;
	for (const FCrossingChunkRule& Rule : Sorted)
	{
		if (bAnyCategory && (!bGroupOpen || !Rule.Category.Equals(CurrentCategory, ESearchCase::CaseSensitive)))
		{
			CurrentCategory = Rule.Category;
			bGroupOpen = true;
			if (RowsBox.IsValid())
			{
				// 分组标题：加大一号、居中，跟数据行拉开档次
				RowsBox->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(CurrentCategory.IsEmpty() ? TEXT("未分组") : CurrentCategory))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				];
			}
		}

		TSharedPtr<FCrossingChunkRule> Row = MakeShared<FCrossingChunkRule>(Rule);
		Rows.Add(Row);
		FolderTotal += Rule.Folders.Num();
		if (RowsBox.IsValid())
		{
			// 行高统一 26px：列表看起来才整齐
			RowsBox->AddSlot().AutoHeight().Padding(0.f, 1.f)[SNew(SBox).HeightOverride(26.f)[MakeChunkRow(Row)]];
		}
	}

	if (ShouldShowPatchBatches())
	{
		// 《版本更新》要做正式划分，所以这时右栏先把补丁批次顶上来：按分块分组平铺，
		// 每个目录一行 —— 直接「指派到」正式分块，那个目录就从批次里搬走了
		//（搬空的分块规则会自动删掉）。《常规补丁》下不顶，那时右栏要显示未分类清单。
		for (const TSharedPtr<FPatchBatch>& Batch : PatchBatches)
		{
			if (!Batch.IsValid() || !UnclassBox.IsValid()) { continue; }

			UnclassBox->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Batch->Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.75f, 0.25f)))
			];
			for (const TSharedPtr<FUnclassFolder>& Folder : Batch->Folders)
			{
				UnclassBox->AddSlot().AutoHeight().Padding(0.f, 1.f)[MakeUnclassRow(Folder)];
			}
		}
	}
	else
	{
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
				.Text(LOCTEXT("NoUnclass", "没有未分类资源 —— 所有目录都在分块规则覆盖范围内"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
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

/**
 * 「变化基准」那行字：说清「增 / 删 / 改」是跟哪一份报告比出来的。
 *
 * 这行的由来：《变化》列只跟 Saved\PackLogs\ChunkReports\<目标>\latest.json 比，
 * 而那份 json 只有打包【成功】时才会被重写（Pack-CrossingVoid.ps1 把
 * Build-ChunkReport.ps1 的调用放在退出码 0 的分支里）。于是"上一次打包失败了"
 * 这件事在界面上完全不可见 —— 你看到的是自上一次成功打包以来的全部改动。
 * 2026-10-11 那次就是这么被问住的（基准停在 9-20，差了整整三周），所以把基准写出来。
 */
FText SCrossingChunkPanel::GetDeltaBaseText() const
{
	if (!bReportLoaded)
	{
		return LOCTEXT("DeltaBaseNoReport", "变化基准：—（这个目标还没有报告）");
	}
	if (DeltaBaseId.IsEmpty())
	{
		// Build-ChunkReport.ps1 对"第一份报告"的处理就是不写 previousReport（变化列全 0）
		return LOCTEXT("DeltaBaseFirst", "变化基准：无（这是第一份报告，所以变化列必然是 0）");
	}
	if (!bDeltaBaseExists)
	{
		return FText::FromString(FString::Printf(TEXT("变化基准：时间读不出来（报告 id：%s）"), *DeltaBaseId));
	}

	FString Text = FString::Printf(TEXT("变化基准：%s 的报告（%s）"), *DeltaBaseLabel, *DeltaBaseAgeText);
	if (DeltaBaseAgeDays >= StaleDeltaBaseDays)
	{
		Text += TEXT("，这中间的全部改动都算在这里");
	}
	if (bLastPackFailed)
	{
		Text += TEXT("；上次打包失败，基准没刷新");
	}
	return FText::FromString(Text);
}

FSlateColor SCrossingChunkPanel::GetDeltaBaseColor() const
{
	const bool bWarn = bLastPackFailed
		|| (bDeltaBaseExists && DeltaBaseAgeDays >= StaleDeltaBaseDays);
	return bWarn ? FSlateColor(FLinearColor(1.f, 0.75f, 0.25f)) : FSlateColor::UseSubduedForeground();
}

void SCrossingChunkPanel::LoadReport()
{
	// 基准这几项每次都要先清干净：换目标时另一个目标可能压根没有报告（下面直接 return），
	// 留着上一次的值就会显示成"基准是某某"，其实那一列是空的。
	bReportLoaded = false;
	bContainerStats = true;   // 缺字段就当"有容器统计"（老报告都是 Windows 的，本来就有）
	DeltaBaseId.Empty();
	DeltaBaseLabel.Empty();
	DeltaBaseAgeText.Empty();
	DeltaBaseAgeDays = -1;
	bDeltaBaseExists = false;

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
			FString Kind;
			if (Obj->TryGetStringField(TEXT("kind"), Kind))
			{
				Info.bIsUnclassified = Kind.Equals(TEXT("unclassified"), ESearchCase::CaseSensitive);
			}
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

	bReportLoaded = true;

	// 体积列能不能累到数：安卓 false（pak 打进 APK/OBB，没有 Content\Paks 可量）。
	// 老报告里没这个字段 —— 它们都是 Windows 产出的，按 true 处理。
	bool bHasContainerStats = true;
	Root->TryGetBoolField(TEXT("containerStats"), bHasContainerStats);
	bContainerStats = bHasContainerStats;

	// ---- 变化列的对比基准：报告里记的 previousReport ----
	// 它的格式就是 Build-ChunkReport.ps1 的 $Stamp + 模式：yyyyMMdd-HHmmss[-模式]，
	// 时间直接从 id 里拆（比去 stat 上一份报告的文件时间可靠：那是"写文件的时间"，
	// 不是"那次打包的时间"）。读不出来就当没有基准，界面上明说，别猜。
	FString PrevId;
	if (Root->TryGetStringField(TEXT("previousReport"), PrevId) && !PrevId.IsEmpty())
	{
		DeltaBaseId = PrevId;
		if (PrevId.Len() >= 15 && PrevId[8] == TEXT('-'))
		{
			const FString Iso = FString::Printf(TEXT("%s-%s-%sT%s:%s:%s"),
				*PrevId.Mid(0, 4), *PrevId.Mid(4, 2), *PrevId.Mid(6, 2),
				*PrevId.Mid(9, 2), *PrevId.Mid(11, 2), *PrevId.Mid(13, 2));
			FDateTime When;
			if (FDateTime::ParseIso8601(*Iso, When))
			{
				DeltaBaseLabel = FString::Printf(TEXT("%s-%s %s:%s"),
					*PrevId.Mid(4, 2), *PrevId.Mid(6, 2), *PrevId.Mid(9, 2), *PrevId.Mid(11, 2));
				DeltaBaseAgeDays = FMath::Max(0, (int32)(FDateTime::Now() - When).GetDays());
				DeltaBaseAgeText = (DeltaBaseAgeDays == 0) ? FString(TEXT("今天"))
					: (DeltaBaseAgeDays == 1) ? FString(TEXT("昨天"))
					: FString::Printf(TEXT("%d 天前"), DeltaBaseAgeDays);
				bDeltaBaseExists = true;
			}
		}
	}

	// 报告里的 unclassified 段不再读：右栏的未分类清单改由 RescanUnclassified() 实时扫出，
	// 「拆散全部 chunk 后清单还是老的」这类漂移从根上消掉。报告本身（chunks/体积/变化）照旧。
}

/**
 * 右栏「本次变更」清单里**不用归类**的目录前缀。
 *
 * 这两个是 UE5 One File Per Actor（OFPA）的存储目录，属于 World Partition 关卡：
 * 每个 actor / 对象一个 .uasset（`<关卡路径>/<bucket>/<hash>.uasset`）。
 * 它们 cook 之后**不落成独立文件** —— 会被折进关卡的 `_Generated_` 流送单元，
 * 跟关卡本体进同一个 chunk（实测：`MapS/PlotTurnMap/_Generated_/*.umap` 全在「地图资源」包里，
 * 而 cook 产物里压根没有 `__ExternalActors__` 这个目录）。
 *
 * 所以它们永远匹配不到任何规则：列在清单里只会当成"欠账"催人归类，
 * 真去加规则又是永不命中的死规则（报告会报「chunkN 一条文件都没命中」）。
 * 这里直接忽略。以后遇到同类的"cook 期会被消化掉"的目录，往这个数组里加就行。
 */
static const TCHAR* const GIgnoredUnclassPrefixes[] =
{
	TEXT("/Game/__ExternalActors__"),
	TEXT("/Game/__ExternalObjects__"),
};

/** Folder 是不是落在上面那些"不用归类"的目录里。按路径段判断，免得 /Game/__ExternalActors__Xxx 被误伤。 */
static bool IsIgnoredUnclassFolder(const FString& Folder)
{
	for (const TCHAR* Prefix : GIgnoredUnclassPrefixes)
	{
		const int32 PrefixLen = FCString::Strlen(Prefix);
		if (Folder.Len() < PrefixLen) { continue; }
		if (!Folder.Left(PrefixLen).Equals(Prefix, ESearchCase::IgnoreCase)) { continue; }
		if (Folder.Len() == PrefixLen || Folder[PrefixLen] == TEXT('/')) { return true; }
	}
	return false;
}

void SCrossingChunkPanel::RescanUnclassified()
{
	Unclassified.Reset();
	UncoveredFolders.Reset();

	// 规则读不出来就不扫：空规则集（所有 chunk 被解散）会把整个 /Game 列出来 ——
	// 那是真实情况，但要等规则文件本身能读时才列。
	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!FCrossingChunkRuleService::LoadRules(Rules, Error))
	{
		return;
	}

	TSet<FString> RuleFolders;
	for (const FCrossingChunkRule& Rule : Rules)
	{
		for (const FString& Folder : Rule.Folders)
		{
			RuleFolders.Add(Folder);
		}
	}

	// 这个目录（含自己）有没有被任何规则标记 —— 标记是继承的，逐级向上问。
	// 注意和 ResolveInheritedRule 的语义一致：父目录被标记，子目录就算已归类。
	auto IsCovered = [&RuleFolders](const FString& Folder) -> bool
	{
		FString Path = Folder;
		while (!Path.IsEmpty())
		{
			if (RuleFolders.Contains(Path)) { return true; }
			Path = FPaths::GetPath(Path);
		}
		return false;
	};

	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		return;
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));

	TArray<FAssetData> Found;
	Registry->GetAssets(Filter, Found);

	// 未分类资产按「顶层未覆盖目录」归组。
	// 能走到归组这步的目录，整条祖先链必然都没被标记（否则 IsCovered 就拦下了），
	// 所以锚点一定是 /Game 的第一层子目录 —— 全拆散时清单收敛成第一层目录列表，
	// 正好对应「逐个重新分配」的操作粒度；指派上层目录后子目录全部跟着消失。
	TMap<FString, int32> Counts;
	for (const FAssetData& Data : Found)
	{
		if (Data.PackageName.IsNone()) { continue; }
		const FString Folder = FCrossingChunkRuleService::NormalizeFolderPath(
			FPaths::GetPath(Data.PackageName.ToString()));
		if (Folder.IsEmpty() || IsCovered(Folder)) { continue; }
		// World Partition 的外部 actor / 对象不列：cook 期它们会并进关卡，不用归类
		if (IsIgnoredUnclassFolder(Folder)) { continue; }

		// 精确目录也留一份（右栏显示的是锚点，登记「版本补丁N」用的是这一份）
		UncoveredFolders.AddUnique(Folder);

		FString Anchor = Folder;
		while (!Anchor.Equals(TEXT("/Game"), ESearchCase::IgnoreCase))
		{
			const FString Parent = FPaths::GetPath(Anchor);
			if (Parent.IsEmpty() || Parent.Equals(TEXT("/Game"), ESearchCase::IgnoreCase))
			{
				break;
			}
			Anchor = Parent;
		}

		int32& Count = Counts.FindOrAdd(Anchor);
		++Count;
	}

	TArray<FString> Anchors;
	Counts.GetKeys(Anchors);
	Anchors.Sort();
	for (const FString& Anchor : Anchors)
	{
		TSharedPtr<FUnclassFolder> Item = MakeShared<FUnclassFolder>();
		Item->Folder = Anchor;
		Item->Files  = Counts[Anchor];
		Unclassified.Add(Item);
	}

	// 「N 个文件」那一列统一宽度：跟随最宽的一条（右侧「指派到 / 定位」两个按钮就固定住了）。
	UnclassCountWidth = 90.f;
	for (const TSharedPtr<FUnclassFolder>& Item : Unclassified)
	{
		if (Item.IsValid())
		{
			UnclassCountWidth = FMath::Max(UnclassCountWidth, EstimateUnclassCountWidth(Item->Files));
		}
	}

	// 精确目录排一下序：登记成规则时写进 ini 的顺序才稳定（diff 好看、不会每次抖动）
	UncoveredFolders.Sort();

	// 只留「最浅的那一层」：/Game/A/B 和 /Game/A/B/C 都没被覆盖时，父目录已经覆盖了子目录，
	// 规则里只写父目录就够了（覆盖范围一样，规则行短一截）。
	{
		TArray<FString> Pruned;
		for (const FString& Folder : UncoveredFolders)
		{
			if (Pruned.Num() > 0)
			{
				const FString& Last = Pruned.Last();
				if (Folder.Len() > Last.Len() && Folder[Last.Len()] == TEXT('/')
					&& Folder.StartsWith(Last, ESearchCase::IgnoreCase))
				{
					continue;   // 已被上一个（更浅的）目录覆盖
				}
			}
			Pruned.Add(Folder);
		}
		UncoveredFolders = MoveTemp(Pruned);
	}
}

/**
 * 扫出所有「版本补丁N」批次：每个批次的标签 + 它名下的目录（带当前资产数）。
 *
 * 两个用途：右栏在《版本更新》下把它们平铺出来（供「指派到」正式分块），
 * 以及《版本更新》点打包时的硬拦。判据就是名字前缀，不往规则里加字段。
 */
void SCrossingChunkPanel::RescanPatchBatches()
{
	PatchBatches.Reset();

	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!FCrossingChunkRuleService::LoadRules(Rules, Error)) { return; }

	// 先把批次里的目录收进集合：随后一遍 AssetRegistry 扫描就能把每个目录的资产数点出来
	TSet<FString> Watched;
	for (const FCrossingChunkRule& Rule : Rules)
	{
		if (!IsPatchChunkName(Rule.ChunkName)) { continue; }
		for (const FString& Folder : Rule.Folders) { Watched.Add(Folder); }
	}
	if (Watched.Num() == 0) { return; }

	TMap<FString, int32> Counts;
	if (IAssetRegistry* Registry = IAssetRegistry::Get())
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(TEXT("/Game"));

		TArray<FAssetData> Found;
		Registry->GetAssets(Filter, Found);
		for (const FAssetData& Data : Found)
		{
			if (Data.PackageName.IsNone()) { continue; }
			// 规则目录是"含子树"的（和右键菜单状态行、脚本那边一致）：逐级向上找一次
			FString Path = FCrossingChunkRuleService::NormalizeFolderPath(FPaths::GetPath(Data.PackageName.ToString()));
			while (!Path.IsEmpty())
			{
				if (Watched.Contains(Path)) { ++Counts.FindOrAdd(Path); break; }
				Path = FPaths::GetPath(Path);
			}
		}
	}

	// 规则是按 ChunkId 排序存盘的，所以批次天然按号排
	for (const FCrossingChunkRule& Rule : Rules)
	{
		if (!IsPatchChunkName(Rule.ChunkName)) { continue; }

		TSharedPtr<FPatchBatch> Batch = MakeShared<FPatchBatch>();
		Batch->Label = FString::Printf(TEXT("%s · chunk %d（%d 个目录）"),
			*Rule.ChunkName, Rule.ChunkId, Rule.Folders.Num());
		for (const FString& Folder : Rule.Folders)
		{
			TSharedPtr<FUnclassFolder> Item = MakeShared<FUnclassFolder>();
			Item->Folder = Folder;
			Item->Files  = Counts.Contains(Folder) ? Counts[Folder] : 0;
			Batch->Folders.Add(Item);
		}
		PatchBatches.Add(Batch);
	}
}

FText SCrossingChunkPanel::GetUnclassBoxTitle() const
{
	if (ShouldShowPatchBatches())
	{
		return FText::FromString(FString::Printf(
			TEXT("待正式划分（%d 个补丁批次）—— 逐个「指派到」正式分块；分完才能打《版本更新》"),
			PatchBatches.Num()));
	}
	return LOCTEXT("UnclassTitle", "本次变更（未分类资源）—— 归类用内容浏览器右键");
}

/**
 * 《常规补丁》专有：把"当前没有任何规则覆盖的目录"登记成一条新规则「版本补丁N」。
 *
 * 为什么必须登记：脚本对未分类文件只能无脑兜底到一个号（规则里 max(ChunkId)+1），
 * 规则不动那个号就永远不变 —— 每批新资源都挤进同一块，那块越滚越大，
 * 每次补丁玩家都要重下整块。登记成真规则之后：这批拿到自己的号、下一批再拿下一个号，
 * 补丁只带新块；右栏的清单也当场清空（那些目录从此有规则覆盖了）。
 *
 * 登记的是 UncoveredFolders（**精确目录**，如 /Game/AssetMaterial/Sound/UI_Effect），
 * 不是右栏显示的锚点（/Game/AssetMaterial）—— 锚点等于把整棵树永久认领，
 * 以后往老目录里塞新文件就再也不会被认成"新的一批"。
 */
bool SCrossingChunkPanel::RegisterPatchBatch(FString& OutMessage)
{
	// 现扫一次，不拿可能过期的清单（面板开着的时候美术可能刚又加了几张图）
	RescanUnclassified();
	if (UncoveredFolders.Num() == 0)
	{
		OutMessage = TEXT("没有未分类目录 —— 这次补丁不需要新建分块。");
		return false;
	}

	// 「版本补丁N」的 N 取现有最大值 +1（号和名字都不复用；解散掉一个也不回收）
	TArray<FCrossingChunkRule> Rules;
	FString Error;
	int32 NextNumber = 1;
	if (FCrossingChunkRuleService::LoadRules(Rules, Error))
	{
		const FString Prefix = GPatchChunkNamePrefix;
		for (const FCrossingChunkRule& Rule : Rules)
		{
			if (IsPatchChunkName(Rule.ChunkName))
			{
				const FString Tail = Rule.ChunkName.RightChop(Prefix.Len());
				if (Tail.IsNumeric())
				{
					NextNumber = FMath::Max(NextNumber, FCString::Atoi(*Tail) + 1);
				}
			}
		}
	}

	int32 NewChunkId = INDEX_NONE;

	// 新号的下限：上一次打包那个兜底块的号 +1。兜底块的号没写进规则（脚本每次按 max+1 现算），
	// 但已经随包发出去过 —— 复用它等于让同一个号在新包里装另一批内容（启动器会认成同一块）。
	int32 MinNewId = 0;
	for (const TPair<int32, FReportChunk>& Pair : ReportChunks)
	{
		if (Pair.Value.bIsUnclassified) { MinNewId = FMath::Max(MinNewId, Pair.Key + 1); }
	}

	return FCrossingChunkRuleService::AddChunk(
		FString::Printf(TEXT("版本补丁%d"), NextNumber), UncoveredFolders, NewChunkId, OutMessage, MinNewId);
}

FReply SCrossingChunkPanel::OnRescanUnclassClicked()
{
	RefreshRules();
	StatusMessage = TEXT("未分类资源已重新扫描");
	return FReply::Handled();
}

/** 体积列的自适应单位：B/KB/MB/GB，后缀永远带全。 */
static FString FormatChunkBytes(int64 Bytes)
{
	if (Bytes >= int64(1) << 30) { return FString::Printf(TEXT("%.2f GB"), Bytes / 1073741824.0); }
	if (Bytes >= int64(1) << 20) { return FString::Printf(TEXT("%.1f MB"), Bytes / 1048576.0); }
	if (Bytes >= int64(1) << 10) { return FString::Printf(TEXT("%.1f KB"), Bytes / 1024.0); }
	return FString::Printf(TEXT("%d B"), static_cast<int32>(Bytes));
}

TSharedRef<SWidget> SCrossingChunkPanel::MakeChunkRow(TSharedPtr<FCrossingChunkRule> Rule)
{
	const int32 ChunkId = Rule->ChunkId;
	const bool bIsNewBucket = (ChunkId == INDEX_NONE);

	// 体积列：报告里 containerStats=false 表示「这次没有容器文件可量」——安卓的 pak 打进
	// APK/OBB，快速验证本来也不打 pak。这时写「不统计」，别用 0 B 骗人；
	// 文件数 / 变化 / 未分类清单都照常有数，只有体积这一列是空的。
	FString SizeText;
	if (!ReportChunks.Contains(ChunkId)) { SizeText = TEXT("—"); }
	else if (!bContainerStats)           { SizeText = TEXT("不统计"); }
	else                                 { SizeText = FormatChunkBytes(ReportChunks[ChunkId].Bytes); }

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
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
		[
			// 显示分类：下拉选现有分类，或选「新建分类…」弹输入框。纯显示字段，打包不读它。
			SNew(SBox).WidthOverride(88.f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&CategoryOptions)
				.ContentPadding(FMargin(4.f, 2.f))
				.IsEnabled_Lambda([this]() { return AreInputsEnabled(); })
				// 必须打开"重选也回调"：SComboBox 选中之后会把 SelectedItem 记成所选项，
				// 而「新建分类…」那一项的值是 nullptr 哨兵 —— 不打开这个开关，
				// 选过一次之后**再点它就不触发 OnSelectionChanged 了**（表现：点了没反应，
				// 分类建不出来）。这就是"无法创建"的一半原因。
				.ReselectionTriggersOnSelectionChanged(true)
				// 这里不挂 tooltip：挂在 combo 上时它正好盖住展开的列表（把「新建分类…」压住）。
				// 说明挪到「分类」列头上去（列头离下拉很远，不会挡）。
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
				{
					// 哨兵项显示成「新建分类…」（它是个真字符串，所以会被正常画出来）
					if (IsNewCategoryOption(Option)) { return SNew(STextBlock).Text(FText::FromString(TEXT("新建分类…"))); }
					const FString Label = (!Option.IsValid() || Option->IsEmpty()) ? FString(TEXT("未分组")) : *Option;
					return SNew(STextBlock).Text(FText::FromString(Label));
				})
				.OnSelectionChanged_Lambda([this, ChunkId](TSharedPtr<FString> Option, ESelectInfo::Type SelectType)
				{
					if (SelectType == ESelectInfo::Direct)
					{
						return;
					}
					if (IsNewCategoryOption(Option))
					{
						OpenNewCategoryPopup(ChunkId);   // 哨兵项 = 「新建分类…」
						return;
					}
					// 未分组（空串）→ 清掉分类；其余就是分类名
					CommitCategory(ChunkId, Option.IsValid() ? *Option : FString());
				})
				[
					SNew(STextBlock)
					.Text_Lambda([WeakRule = TWeakPtr<FCrossingChunkRule>(Rule)]()
					{
						const FCrossingChunkRule* P = WeakRule.Pin().Get();
						return FText::FromString(P ? (P->Category.IsEmpty() ? TEXT("未分组") : P->Category) : FString());
					})
				]
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
			SNew(SBox).WidthOverride(92.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(SizeText))
			]
		]
		// 变化列：每个指标一小块、各自带颜色和中文动词，只显示非零项——
		// 比 "+3 -1 ~2" 这种裸符号好读（那条写法是给懂符号的人看的）。
		// 宽度用全表统一的 DeltaColumnWidth（跟随最宽内容），解散按钮才能对齐。
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(DeltaColumnWidth)
			.ToolTipText(LOCTEXT("DeltaTip", "和上一次打包报告比：增 = 新增的文件，删 = 删除的文件，改 = 内容有修改的文件（基准是哪一份，看表格上方那行「变化基准」）"))
			[
				!ReportChunks.Contains(ChunkId)
				? TSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(TEXT("—"))).ColorAndOpacity(FSlateColor::UseSubduedForeground()))
				: ([&]() -> TSharedRef<SWidget>
				{
					const FReportChunk& Info = ReportChunks[ChunkId];
					if (Info.Added == 0 && Info.Removed == 0 && Info.Modified == 0)
					{
						return SNew(STextBlock)
							.Text(FText::FromString(TEXT("无变化")))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground());
					}
					return SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("增 %d"), Info.Added)))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.8f, 0.45f)))
							.Visibility(Info.Added > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("删 %d"), Info.Removed)))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.5f, 0.4f)))
							.Visibility(Info.Removed > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("改 %d"), Info.Modified)))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.7f, 0.95f)))
							.Visibility(Info.Modified > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						];
				})()
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

	// 这份清单是实时扫出来的（对比当前规则和 AssetRegistry），不是上次打包报告 ——
	// 不存在「规则刚改过、清单还是旧的」的对账问题：指派完一刷新，行就直接消失。
	const FText StateText = LOCTEXT("UnclassTag", "[未分类]");
	const FLinearColor StateColor(1.f, 0.8f, 0.35f);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[
			// 标签列按内容宽（原来写死 340px：那一格大半是空的，等于白白吃掉目录名的宽度，
			// 面板一窄目录名就被裁掉）。每一行都是同一个标签，所以各行仍然对齐。
			SNew(STextBlock)
			.Text(StateText)
			.ColorAndOpacity(FSlateColor(StateColor))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			// 目录名：吃掉剩下的全部宽度（自适应）。面板很窄时尾巴会被裁，
			// 鼠标停上去能看到完整路径。
			SNew(STextBlock)
			.Text(FText::FromString(Folder))
			.ToolTipText(FText::FromString(Folder))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
		[
			// 文件数：宽度取全表最宽的一条（见 RescanUnclassified），右对齐 —— 数字位数不同也整齐
			SNew(SBox)
			.WidthOverride_Lambda([this]() -> FOptionalSize { return UnclassCountWidth; })
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%d 个文件"), Item->Files)))
				.Justification(ETextJustify::Right)
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
				FString Label;
				if (Option.IsValid())
				{
					if (Option->Category.IsEmpty())
					{
						Label = FString::Printf(TEXT("Chunk %d · %s"), Option->ChunkId, *Option->ChunkName);
					}
					else
					{
						Label = FString::Printf(TEXT("[%s] Chunk %d · %s"), *Option->Category, Option->ChunkId, *Option->ChunkName);
					}
				}
				return SNew(STextBlock).Text(FText::FromString(Label));
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
	const FString Script = FCrossingChunkRuleService::GetScriptPath(TEXT("Pack-CrossingVoid.ps1"));
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
	// 工程根：脚本自己也能推（从脚本位置向上找 .uproject），这里显式给一份，
	// 是为了插件被装到 Engine\Plugins\ 下时（那种位置推不出工程）也能跑。
	// 引号写法与下面同理。
	Extra += FString::Printf(TEXT(" -ProjectRoot \\\"%s\\\""), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	// 这两个值最终会被塞进外层 -Command "..." 的双引号里，所以内层引号必须写成 \"。
	// Windows 解析命令行时裸引号会被吃掉，PowerShell 收到就成了裸值：
	//   · 逗号被当成数组分隔符 -> -Maps 绑不定 [string]（实测报 ParameterArgumentTransformationError）
	//   · 带空格的路径被拆成两个参数
	if (!OutputDir.IsEmpty())
	{
		Extra += FString::Printf(TEXT(" -ArchiveDir \\\"%s\\\""), *OutputDir);
	}
	if (!ReleaseRoot.IsEmpty())
	{
		Extra += FString::Printf(TEXT(" -ReleaseRoot \\\"%s\\\""), *ReleaseRoot);
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
	const FString Script = FCrossingChunkRuleService::GetScriptPath(TEXT("Pack-CrossingVoid.ps1"));
	return FString::Printf(TEXT("& '%s' -ClearCache -ProjectRoot \\\"%s\\\""), *Script, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
}

FString SCrossingChunkPanel::BuildStatusCommand() const
{
	const FString Script = FCrossingChunkRuleService::GetScriptPath(TEXT("Pack-CrossingVoid.ps1"));
	const FString Platform = SelectedTarget.IsValid() ? SelectedTarget->Platform : TEXT("Win64");
	const FString Target = SelectedTarget.IsValid() ? SelectedTarget->Target : TEXT("Client");
	const FString Mode = SelectedMode.IsValid() ? SelectedMode->Value : TEXT("Quick");
	// 内层引号要写成 \"（原因见 BuildExtraArgs）
	return FString::Printf(
		TEXT("& '%s' -Status -ProjectRoot \\\"%s\\\" -Mode %s -Platform %s -Target %s -ReleaseVersion \\\"%s\\\" -ArchiveDir \\\"%s\\\" -ReleaseRoot \\\"%s\\\""),
		*Script, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), *Mode, *Platform, *Target, *GetReleaseVersion(), *OutputDir, *ReleaseRoot);
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

	// 边跑边抽干，绝不能"先等退出再读"：
	// 管道缓冲区只有几 KB，子进程写满就阻塞在写上、永远不退出，而 WaitForProc 会一直等它。
	// 2026-10-11 就是这么把编辑器冻在启动里的：面板恢复时在这里跑状态查询，那一刻 Slate ticker
	// 还没跑（TickPump 没机会抽管道），而脚本那次因为脚本被 PowerShell 5.1 按 GBK 误读、吐了
	// 5.5 KB 的解析报错 —— 正好超过缓冲区 ⇒ 子进程等读者、父进程等子进程，窗口再也出不来
	// （进程活着、Rider 还显示开着）。写法与 TickPump 一致：一边等一边读。
	// 另外给个上限：只读查询不该无限期占住游戏线程。
	int32 ReturnCode = 0;
	double WaitedSeconds = 0.0;
	while (FPlatformProcess::IsProcRunning(H))
	{
		OutText += FPlatformProcess::ReadPipe(R);
		FPlatformProcess::Sleep(0.005f);
		WaitedSeconds += 0.005;
		if (WaitedSeconds > 20.0)
		{
			FPlatformProcess::TerminateProc(H, /*bKillTree=*/true);
			break;
		}
	}

	// 进程退出后把剩下的读干；多读几次是保险（管道里的数据可能分几段到）
	for (int32 Guard = 0; Guard < 8; ++Guard)
	{
		const FString Chunk = FPlatformProcess::ReadPipe(R);
		if (Chunk.IsEmpty()) { break; }
		OutText += Chunk;
	}

	FPlatformProcess::GetProcReturnCode(H, &ReturnCode);
	FPlatformProcess::CloseProc(H);
	FPlatformProcess::ClosePipe(R, W);
	return ReturnCode == 0;
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

	const FString Script = FCrossingChunkRuleService::GetScriptPath(TEXT("Pack-CrossingVoid.ps1"));
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

	const FString Script = FCrossingChunkRuleService::GetScriptPath(TEXT("Pack-CrossingVoid.ps1"));
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

	// ---- 前置检查：《版本更新》不允许留着「版本补丁N」批次 ----
	// 补丁批次是《常规补丁》自动攒出来的临时块；版本更新就是做正式划分的时候 ——
	// 留着它们，下次补丁只会继续往后堆。所以硬拦：打包不启动。
	if (!bServer && SelectedMode.IsValid() && SelectedMode->Value == TEXT("Base"))
	{
		RescanPatchBatches();
		if (PatchBatches.Num() > 0)
		{
			TArray<FString> Labels;
			for (const TSharedPtr<FPatchBatch>& Batch : PatchBatches)
			{
				if (Batch.IsValid()) { Labels.Add(Batch->Label); }
			}

			RefreshRules();   // 右栏立刻切到「待正式划分」那份清单（它会清 StatusMessage，所以顺序在后）
			StatusMessage = TEXT("《版本更新》前要先把补丁批次正式划分掉 —— 打包没有启动");

			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
				TEXT("《版本更新》之前要先把补丁批次正式划分掉，这次打包没有启动。\n\n")
				TEXT("还留着 %d 个补丁批次：\n  %s\n\n")
				TEXT("怎么做：右栏现在列的就是这些批次的目录（每个目录一行）——\n")
				TEXT("  · 用「指派到」把它分进正式分块（或去内容浏览器右键指定）；\n")
				TEXT("  · 一个批次里的目录被搬空，它自己就消失了。\n")
				TEXT("全部分完，右栏回到「本次变更」，再点开始打包。"),
				PatchBatches.Num(), *FString::Join(Labels, TEXT("\n  ")))));
			return FReply::Handled();
		}
	}

	// ---- 《常规补丁》：先把"这批未分类资源"登记成自己的新分块（版本补丁N）----
	// 不登记的话，它们只会进脚本的兜底块（号固定），每批新东西都往那一块里加，
	// 补丁只会越来越大。必须赶在打包之前 —— 脚本是在 cook 前读规则文件的。
	FString BatchMessage;
	if (!bServer && SelectedMode.IsValid() && SelectedMode->Value == TEXT("Patch"))
	{
		if (RegisterPatchBatch(BatchMessage))
		{
			RefreshRules();   // 列表里立刻能看到这个新分块（右栏也跟着清空）
		}
	}

	// 起进程、收日志那套收在 LaunchScript 里 —— 清缓存按钮走的是同一条路。
	if (LaunchScript(BuildPackCommand(), BuildPackCommand(), /*bIsClearCache=*/false))
	{
		StatusMessage = TEXT("打包中…");

		// 登记结果写在 LaunchScript 之后：它会把日志重置成"命令行那一行"，写在前面会被冲掉
		if (!BatchMessage.IsEmpty())
		{
			LogText += FString::Printf(TEXT("\n[规则] %s\n"), *BatchMessage);
			if (LogBox.IsValid()) { LogBox->SetText(FText::FromString(LogText)); }
		}
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

	const FString SavedReleaseRoot = LoadSetting(TEXT("ReleaseRoot"));
	if (!SavedReleaseRoot.IsEmpty())
	{
		ReleaseRoot = SavedReleaseRoot;
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
	SaveSetting(TEXT("ReleaseRoot"), ReleaseRoot);
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
		// 失败时 latest.json 不会被刷新（脚本把生成报告的调用挂在退出码 0 的分支里），
		// 「变化基准」那行要据此提醒。必须在下面的 RefreshRules 之前赋值：那一行是它填的。
		bLastPackFailed = (ReturnCode != 0);
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

void SCrossingChunkPanel::CommitCategory(int32 ChunkId, const FString& NewCategory)
{
	FString Message;
	if (FCrossingChunkRuleService::SetChunkCategory(ChunkId, NewCategory, Message))
	{
		// 和「指派到」一样：回调正跑在下拉自己的回调里，重建整棵树推迟到下一帧
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
	}
	StatusMessage = Message;
}

void SCrossingChunkPanel::OpenNewCategoryPopup(int32 ChunkId)
{
	// 输入框留个句柄：弹层推上去之后要显式把键盘焦点给它。
	// PushMenu 只把焦点给"菜单内容"本身，输入框拿不到 —— 不给的话敲字和回车都进不来
	// （表现：弹层出来了，但打不了字、回车没反应）。这就是"无法创建"的另一半原因。
	TSharedPtr<SEditableTextBox> NameBox;

	TSharedRef<SWidget> Input =
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("Menu.Background"))
		.Padding(8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewCategoryLabel", "新分类的名字（回车确认）："))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(NameBox, SEditableTextBox)
				.MinDesiredWidth(180.f)
				.OnTextCommitted(FOnTextCommitted::CreateSP(this, &SCrossingChunkPanel::OnNewCategoryCommitted, ChunkId))
			]
		];

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		Input,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));

	if (NameBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(NameBox, EFocusCause::SetDirectly);

		// 下一帧再补一次：窗口刚推上去时控件可能还没进布局，第一次设焦点会被丢掉。
		TWeakPtr<SEditableTextBox> WeakBox = NameBox;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakBox](float)
		{
			if (TSharedPtr<SEditableTextBox> Box = WeakBox.Pin())
			{
				FSlateApplication::Get().SetKeyboardFocus(Box, EFocusCause::SetDirectly);
			}
			return false;
		}), 0.f);
	}
}

void SCrossingChunkPanel::OnNewCategoryCommitted(const FText& NewText, ETextCommit::Type CommitType, int32 ChunkId)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	// 名字空着就不往下走：CommitCategory 收到空串 = "移出分类"，而这里的空串更可能是
	// 用户点了别处（OnUserMovedFocus）—— 那会把分类悄悄清掉。空名字只提示，弹层留着继续填。
	const FString Name = NewText.ToString().TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		StatusMessage = TEXT("分类名字是空的，没有改动（要清空分类请从下拉里选「未分组」）");
		return;
	}

	FSlateApplication::Get().DismissAllMenus();
	CommitCategory(ChunkId, Name);
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

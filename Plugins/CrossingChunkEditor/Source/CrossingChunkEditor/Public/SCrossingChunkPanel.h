#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Ticker.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SCompoundWidget.h"

struct FCrossingChunkRule;
class FCrossingChunkLogMarshaller;   // 日志着色（定义在 .cpp 里，照引擎 Output Log 的做法）

/**
 * 《二游打包》页（第一版）：显示分块规则 + 改名。
 *
 * 接下来要加的（按设计文档 `Docs\2026-09-19-二游打包页设计.md`）：
 *   · chunk 体积/变化（读 Saved\PackLogs\ChunkReports\...\latest.json）
 *   · 未分类清单 + 一键指派
 *   · 打包按钮 + 实时日志
 */
class SCrossingChunkPanel : public SCompoundWidget
{
public:
	/** 报告里每个 chunk 的一行统计（来自 Build-ChunkReport.ps1 产出的 json）。 */
	struct FReportChunk
	{
		int32 MatchedFiles = 0;
		int64 Bytes = 0;
		int32 Added = 0;
		int32 Removed = 0;
		int32 Modified = 0;
	};

	struct FUnclassFolder
	{
		FString Folder;
		int32 Files = 0;
		bool bIsNew = true;
	};

	/** 地图勾选项（勾中的会拼成脚本的 -Maps；一个都不勾 = 用 DefaultGame.ini 的 +MapsToCook）。 */
	struct FMapChoice
	{
		FString PackagePath;
		FString Label;
		bool bChecked = false;
	};

	/**
	 * Cook 进程数上限。
	 * 引擎侧没这个限制（上限 254），卡在这里纯粹是因为【再高只会更慢】：
	 * CookDirector 里 CoreLimit = 物理核数 / 进程数（整除），本机 8 核时
	 * N=4 → 每进程 2 核；N≥5 直接掉到 1 核，再往上加只是多几个进程抢同一块 CPU，
	 * 而且每个 worker 都是一整个 editor 进程。实测 N=5 比单进程慢 44%。
	 * 换机器（物理核数变了）时，这个值要跟着重算。
	 */
	static constexpr int32 MaxCookProcessCount = 4;

	SLATE_BEGIN_ARGS(SCrossingChunkPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SCrossingChunkPanel();

	/** 重新读规则文件，重建列表。 */
	void RefreshRules();

private:
	TSharedRef<SWidget> MakeChunkRow(TSharedPtr<FCrossingChunkRule> Rule);
	TSharedRef<SWidget> MakeUnclassRow(TSharedPtr<FUnclassFolder> Item);

	FReply OnRefreshClicked();
	FReply OnPackClicked();
	/** 《清除缓存》：删掉能再生的缓存目录（脚本的 -ClearCache），腾磁盘空间。 */
	FReply OnClearCacheClicked();
	/** 中止正在跑的打包：杀进程树，并把被临时改写的地图清单还原回去。 */
	void StopPack();
	/** 打包结束后把临时关掉的 Live Coding 恢复回来。 */
	void RestoreLiveCodingAfterPack();

	/** 记住/恢复上次的选项（输出目录、目标、模式、勾选的地图），存在编辑器用户配置里。 */
	void ApplySavedSettings();
	void SaveSettings();
	static FString LoadSetting(const TCHAR* Key);
	static void SaveSetting(const TCHAR* Key, const FString& Value);
	FReply OnCopyCommandClicked();
	FReply OnOpenLogDirClicked();
	FReply OnLocateClicked(FString Folder);
	FReply OnDisbandClicked(int32 ChunkId);
	FReply OnUndoDisbandClicked();
	void OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType, int32 ChunkId);
	FText GetSummaryText() const;
	FText GetPackButtonText() const;
	FText GetMapSummaryText() const;
	/** 「Cook 进程数」右边那行说明（单进程 / 多进程各写各的）。 */
	FText GetCookProcessCountHint() const;

	/**
	 * 打包期间要不要让按钮 / 输入框可用。
	 * 打包跑起来之后除了最上面那个「打包/停止打包」按钮（和下面的日志框）之外，
	 * 其它控件一律灰掉 —— 打包中途改目标/改地图/改规则只会让这次打包的结果对不上。
	 */
	bool AreInputsEnabled() const { return !bPacking; }

	/** 扫 AssetRegistry 里的 World 资产，重建地图勾选列表。 */
	void ReloadMaps();
	void RebuildMapList();
	/** 勾选状态 → MapsCsv。 */
	void UpdateMapsCsv();
	/** MapsCsv → 勾选框（换目标时把另一个目标上次勾的图贴回来）。 */
	void ApplyMapsCsvToChoices();
	/** 「勾选地图」按目标分开存的键：Maps_Client / Maps_Server。 */
	FString GetMapsSettingKey() const;

	/** 每帧把子进程的输出捞出来贴到日志区；进程结束就刷新报告。 */
	bool TickPump(float DeltaSeconds);
	/**
	 * 打包/清缓存跑完之后弹提示 + 播音效。
	 * 复刻编辑器原版那一套（引擎 MainFrameModule.cpp:1113-1126 的 Compile Complete 流程）：
	 *   · 音效是同两个 cue：/Engine/EditorSounds/Notifications/Compile{Success,Failed}_Cue
	 *   · 提示是编辑器原生 toast（FSlateNotificationManager），成功绿勾、失败红叉
	 */
	void NotifyPackFinished(bool bSuccess);

	/** 本次要跑的命令（也用于「复制命令」）。 */
	FString BuildPackCommand() const;
	/** 清缓存命令（同样用于日志首行）。 */
	FString BuildClearCacheCommand() const;
	/** 状态查询命令（脚本的 -Status）。 */
	FString BuildStatusCommand() const;
	/** 拼 -ArchiveDir / -Maps 这些附加参数（空值就不加）。 */
	FString BuildExtraArgs() const;
	/**
	 * 起一个 powershell 跑脚本，输出进下面的日志区。
	 * ScriptBody 是 -Command 里的完整内容（如 `& 'xx.ps1' -ClearCache`）。
	 */
	bool LaunchScript(const FString& ScriptBody, const FString& LogFirstLine, bool bIsClearCache);
	/**
	 * 隐藏窗口同步跑一次脚本并拿回 stdout（只用于 -Status 这种秒级只读查询；
	 * 打包那种长任务走 LaunchScript 的异步管道）。
	 */
	bool RunScriptCapture(const FString& ScriptBody, FString& OutText);
	/** 同步刷新状态行（报告 / 产物 / 基线）。 */
	void RefreshStatus();
	/** 基线版本默认取玩家版本的主版本段：1.4.2 -> 1.4；0.5 -> 0.5。 */
	static FString DeriveReleaseVersion(const FString& InPlayerVersion);
	/** 基线版本 = 现算的派生值：不落盘、不给改，永远和玩家版本一致。 */
	FString GetReleaseVersion() const;

	/** 读最近一次打包报告（没有就留空，页面只显示规则本身）。 */
	void LoadReport();

	/** 下拉项：中文标签 + 真正传给脚本的值。 */
	struct FChoice
	{
		FString Label;     // 界面上显示的（中文）
		FString Platform;  // Win64 / Android（目标专用）
		FString Target;    // Client / Server（目标专用）
		FString Value;     // Quick / Base / Patch（模式专用）
		FString ReportDir; // 报告目录名，如 Win64-Client
	};

	TArray<TSharedPtr<FCrossingChunkRule>> Rows;
	TSharedPtr<SVerticalBox> RowsBox;
	TArray<TSharedPtr<FUnclassFolder>> Unclassified;
	TSharedPtr<SVerticalBox> UnclassBox;
	TMap<int32, FReportChunk> ReportChunks;
	FString ReportStamp;
	FString StatusMessage;
	int32 FolderTotal = 0;

	// ---- 打包 ----
	TArray<TSharedPtr<FChoice>> TargetOptions;  // Windows / 安卓 / 服务器
	TSharedPtr<FChoice> SelectedTarget;
	TArray<TSharedPtr<FChoice>> ModeOptions;    // 快速验证 / 正式发布 / 打补丁
	TSharedPtr<FChoice> SelectedMode;
	FString OutputDir;   // 产物目录（传给脚本的 -ArchiveDir）
	FString MapsCsv;     // 选定地图（逗号分隔；空 = 用 DefaultGame.ini 的 +MapsToCook）
	FString PlayerVersion;   // 给玩家看的版本号（打包时写进安卓 VersionDisplayName）
	/**
	 * Cook 进程数（脚本的 -CookProcessCount）。
	 * 1 = 单进程 cook（引擎默认，最稳）；>1 = MPCook：1 个 director + (N-1) 个 cook worker。
	 */
	int32 CookProcessCount = 1;
	// ---- 状态行（RefreshStatus 里填好，界面直接显示）----
	FString StatusReportText;
	FString StatusArtifactText;
	FString StatusBaselineText;
	FString StatusReleaseRoot;   // 基线根目录（脚本报上来的，界面直接用，别在代码里再写一遍）
	FString StatusPlatformText;  // 平台设置一行摘要（包名/版本号/SDK/ABI/图标状态）
	FString PlatformLogHeader;   // 显示在日志最上面的「当前平台设置」块
	bool bBaselineExists = false;
	TArray<TSharedPtr<FMapChoice>> MapChoices;
	TSharedPtr<SVerticalBox> MapListBox;
	FString MapFilter;
	/** 被解散的分块（会话内保留一份，供「撤销解散」）。 */
	TSharedPtr<FCrossingChunkRule> UndoRule;
	FString LogText;
	TSharedPtr<SMultiLineEditableTextBox> LogBox;
	TSharedPtr<FCrossingChunkLogMarshaller> LogMarshaller;
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FProcHandle ProcHandle;
	FTSTicker::FDelegateHandle TickerHandle;
	bool bPacking = false;
	/** 当前这一轮是清缓存（决定结束时的提示文案）。 */
	bool bRunIsClearCache = false;
	/** 打包期间是否临时关掉了 Live Coding（打完要恢复）。 */
	bool bDisabledLiveCoding = false;
};

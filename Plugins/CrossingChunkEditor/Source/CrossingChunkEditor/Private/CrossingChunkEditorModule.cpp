#include "CrossingChunkEditorModule.h"

#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SCrossingChunkPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CrossingChunkEditor"

static const FName CrossingChunkPanelTabName(TEXT("CrossingChunkPanel"));

namespace
{
	/** 右下角提示：成功绿色、失败红色。 */
	void Notify(const FString& Message, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = bSuccess ? 4.0f : 9.0f;
		Info.bUseSuccessFailIcons = true;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess
				? SNotificationItem::CS_Success
				: SNotificationItem::CS_Fail);
		}
	}

	/** 取 Left 与 Stop 之间的内容（都不含）。 */
	FString Between(const FString& Text, const TCHAR* Left, const TCHAR* Stop)
	{
		int32 Start = Text.Find(Left, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += FCString::Strlen(Left);
		const int32 End = Text.Find(Stop, ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			return FString();
		}
		return Text.Mid(Start, End - Start);
	}

	/** 取第一个 Key="..." 的值。 */
	FString Quoted(const FString& Text, const TCHAR* Key)
	{
		const FRegexPattern Pattern(FString::Printf(TEXT("%s=\"([^\"]*)\""), Key));
		FRegexMatcher Matcher(Pattern, Text);
		return Matcher.FindNext() ? Matcher.GetCaptureGroup(1) : FString();
	}

	/**
	 * 读文本文件，强制按 UTF-8 解码。
	 *
	 * 为什么不用 FFileHelper::LoadFileToStringArray：那个 API 在「没有 BOM」时按 ANSI 处理，
	 * 而我们的规则文件是 UTF-8 无 BOM 且含中文（分块名字、注释）——按 ANSI 读进来就成了乱码，
	 * 再写回去就把用户的中文规则名毁掉了。所以这里自己解。
	 */
	bool LoadUtf8Lines(const FString& Filename, TArray<FString>& OutLines, FString& OutError)
	{
		TArray<uint8> Raw;
		if (!FFileHelper::LoadFileToArray(Raw, *Filename))
		{
			OutError = FString::Printf(TEXT("读不出 %s"), *Filename);
			return false;
		}

		int32 Offset = 0;
		if (Raw.Num() >= 3 && Raw[0] == 0xEF && Raw[1] == 0xBB && Raw[2] == 0xBF)
		{
			Offset = 3;   // 吃掉 UTF-8 BOM（写回时不带，和原文件保持一致）
		}

		FString Text;
		const int32 Length = Raw.Num() - Offset;
		if (Length > 0)
		{
			const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Raw.GetData() + Offset), Length);
			Text = FString(Converter.Length(), Converter.Get());
		}

		OutLines.Reset();
		Text.ParseIntoArrayLines(OutLines, /*bCullEmpty*/ false);
		return true;
	}

	/** 按 UTF-8（无 BOM）写回，行尾统一 CRLF。 */
	bool SaveUtf8Lines(const FString& Filename, const TArray<FString>& Lines, FString& OutError)
	{
		FString Text = FString::Join(Lines, LINE_TERMINATOR);
		Text += LINE_TERMINATOR;

		const FTCHARToUTF8 Converter(*Text);
		TArray<uint8> Raw;
		Raw.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());

		if (!FFileHelper::SaveArrayToFile(Raw, *Filename))
		{
			OutError = FString::Printf(TEXT("写不回 %s"), *Filename);
			return false;
		}
		return true;
	}

	FString ChunkLabel(int32 ChunkId, const FString& ChunkName)
	{
		return ChunkName.IsEmpty()
			? FString::Printf(TEXT("Chunk %d"), ChunkId)
			: FString::Printf(TEXT("Chunk %d · %s"), ChunkId, *ChunkName);
	}

	/** 右键菜单正文：状态行 + 指定为 Chunk + 取消标记。 */
	void BuildFolderChunkMenu(const FString& Folder, FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection(TEXT("CrossingChunk"), LOCTEXT("Section", "Chunk 分块"));

		TArray<FCrossingChunkRule> Rules;
		FString Error;
		if (!FCrossingChunkRuleService::LoadRules(Rules, Error))
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(FString::Printf(TEXT("规则文件读不了：%s"), *Error)),
				FText::FromString(FCrossingChunkRuleService::GetRuleIniPath()),
				FSlateIcon(),
				FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([] { return false; })));
			MenuBuilder.EndSection();
			return;
		}

		const FCrossingChunkRule* Direct = FCrossingChunkRuleService::FindDirectRule(Rules, Folder);
		FString Owner;
		const FCrossingChunkRule* Inherited = FCrossingChunkRuleService::ResolveInheritedRule(Rules, Folder, Owner);
		const int32 NextId = FCrossingChunkRuleService::NextChunkId(Rules);

		// ---- 状态行：这块只读，但要说清楚「现在到底算哪个 chunk」 ----
		FText Status;
		FText StatusTip;
		if (Direct)
		{
			Status = FText::FromString(FString::Printf(
				TEXT("当前：%s（本目录直接标记）"), *ChunkLabel(Direct->ChunkId, Direct->ChunkName)));
			StatusTip = LOCTEXT("StatusDirectTip", "本目录自己有一条规则；子目录里没另外标记的，都跟这个走。");
		}
		else if (Inherited)
		{
			Status = FText::FromString(FString::Printf(
				TEXT("当前：%s（继承自 %s）"), *ChunkLabel(Inherited->ChunkId, Inherited->ChunkName), *Owner));
			StatusTip = LOCTEXT("StatusInheritedTip", "本目录没有自己的标记，跟最近的上级目录走。");
		}
		else
		{
			Status = FText::FromString(FString::Printf(
				TEXT("当前：未分类 —— 本次新增，会进新建的 Chunk %d"), NextId));
			StatusTip = LOCTEXT("StatusNoneTip",
				"上级目录也没标记过，说明这批资源是这次新加的：打包时会被收进一个新的 chunk，"
				"基础包不动，启动器只需要下一个新包。想让它单独成包，就右键指定一个 chunk。");
		}

		MenuBuilder.AddMenuEntry(
			Status, StatusTip, FSlateIcon(),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([] { return false; })));

		MenuBuilder.AddMenuSeparator();

		// ---- 指定为 Chunk：列出现有 chunk（带名字），最后一个是用目录名新建 ----
		const FString LeafName = FCrossingChunkRuleService::GetFolderLeafName(Folder);

		TArray<FCrossingChunkRule> SortedRules = Rules;
		SortedRules.Sort([](const FCrossingChunkRule& A, const FCrossingChunkRule& B) { return A.ChunkId < B.ChunkId; });

		MenuBuilder.AddSubMenu(
			LOCTEXT("AssignSubmenu", "指定为 Chunk"),
			LOCTEXT("AssignSubmenuTip", "把这个文件夹（含子目录）划给某个分块。"),
			FNewMenuDelegate::CreateLambda([Folder, SortedRules, Direct, NextId, LeafName](FMenuBuilder& SubMenu)
			{
				const int32 DirectChunkId = Direct ? Direct->ChunkId : INDEX_NONE;

				for (const FCrossingChunkRule& Rule : SortedRules)
				{
					const int32 RuleId = Rule.ChunkId;
					SubMenu.AddMenuEntry(
						FText::FromString(ChunkLabel(RuleId, Rule.ChunkName)),
						FText::FromString(FString::Printf(TEXT("%d 个目录标记在这个分块上"), Rule.Folders.Num())),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([Folder, RuleId, Name = Rule.ChunkName]()
							{
								FString Message;
								Notify(Message, FCrossingChunkRuleService::Assign(Folder, RuleId, Name, Message));
							}),
							FCanExecuteAction(),
							FIsActionChecked::CreateLambda([DirectChunkId, RuleId] { return DirectChunkId == RuleId; })),
						NAME_None,
						EUserInterfaceActionType::RadioButton);
				}

				SubMenu.AddMenuSeparator();
				SubMenu.AddMenuEntry(
					FText::FromString(FString::Printf(TEXT("新建 Chunk %d · 「%s」"), NextId, *LeafName)),
					LOCTEXT("AssignNewTip",
						"用目录名当分块名字（之后可以改名）。注意：没标记过的资源本来也会自动进这个新 chunk。"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Folder, NextId, LeafName]()
					{
						FString Message;
						Notify(Message, FCrossingChunkRuleService::Assign(Folder, NextId, LeafName, Message));
					})));
			}));

		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Remove", "取消本目录的标记"),
			LOCTEXT("RemoveTip", "取消后，本目录的资产重新跟随最近的上级目录。"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Folder]()
				{
					FString Message;
					Notify(Message, FCrossingChunkRuleService::RemoveAssignment(Folder, Message));
				}),
				FCanExecuteAction::CreateLambda([Direct] { return Direct != nullptr; })));

		MenuBuilder.EndSection();
	}
}

// ---------------------------------------------------------------------------
// FCrossingChunkRuleService
// ---------------------------------------------------------------------------

FString FCrossingChunkRuleService::GetRuleIniPath()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultCrossingChunk.ini"));
}

FString FCrossingChunkRuleService::GetRuleSection()
{
	return TEXT("/Script/CrossingChunk.CrossingChunkRuleSet");
}

FString FCrossingChunkRuleService::GetPluginToolsDir()
{
	// <插件>\Tools。插件装在工程里时是 <工程>\Plugins\CrossingChunkEditor\Tools\，
	// 装在引擎里时是 Engine\Plugins\...\Tools\ —— 两种都取得到，所以别去猜工程相对路径。
	// 插件名 = .uplugin 的文件名，写死在这里（插件不打算改名）。
	// 显式构造 FString：IPluginManager 同时有 FString 和 ANSIStringView 两个重载，
	// 直接传 TEXT("...") 会落到「哪个都不精确匹配」的尴尬位置。
	const FString PluginName(TEXT("CrossingChunkEditor"));
	// UE5 的 IPluginManager::Get() 返回引用（UE4 时代才返回指针），所以这里不能用 if (ptr = ...) 判空。
	IPluginManager& PluginManager = IPluginManager::Get();
	if (const TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName))
	{
		const FString Dir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Tools"));
		if (FPaths::DirectoryExists(Dir))
		{
			return FPaths::ConvertRelativePathToFull(Dir);
		}
	}
	return FString();
}

FString FCrossingChunkRuleService::GetScriptPath(const FString& ScriptFileName)
{
	// 插件自带的那份优先；找不到再退回 <工程>\Tools\（脚本还挂在工程里的旧布局）。
	const FString PluginDir = GetPluginToolsDir();
	if (!PluginDir.IsEmpty())
	{
		const FString PluginScript = FPaths::ConvertRelativePathToFull(FPaths::Combine(PluginDir, ScriptFileName));
		if (FPaths::FileExists(PluginScript))
		{
			return PluginScript;
		}
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools"), ScriptFileName));
}

FString FCrossingChunkRuleService::NormalizeFolderPath(const FString& FolderPath)
{
	FString Path = FolderPath;
	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
	{
		Path.LeftChopInline(1);
	}
	return Path;
}

FString FCrossingChunkRuleService::GetFolderLeafName(const FString& FolderPath)
{
	const FString Path = NormalizeFolderPath(FolderPath);
	FString Left;
	FString Leaf;
	Path.Split(TEXT("/"), &Left, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	return Leaf.IsEmpty() ? TEXT("新分块") : Leaf;
}

bool FCrossingChunkRuleService::LoadRules(TArray<FCrossingChunkRule>& OutRules, FString& OutError)
{
	OutRules.Reset();

	const FString Path = GetRuleIniPath();
	if (!FPaths::FileExists(Path))
	{
		OutError = FString::Printf(TEXT("找不到 %s"), *Path);
		return false;
	}

	TArray<FString> Lines;
	if (!LoadUtf8Lines(Path, Lines, OutError))
	{
		return false;
	}

	const FString SectionHeader = FString::Printf(TEXT("[%s]"), *GetRuleSection());
	const int32 SectionIndex = Lines.IndexOfByPredicate([&SectionHeader](const FString& Line)
	{
		return Line.TrimStartAndEnd().Equals(SectionHeader, ESearchCase::IgnoreCase);
	});
	if (SectionIndex == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("%s 里找不到段 [%s]"), *Path, *GetRuleSection());
		return false;
	}

	for (int32 Index = SectionIndex + 1; Index < Lines.Num(); ++Index)
	{
		const FString Trim = Lines[Index].TrimStartAndEnd();
		if (Trim.StartsWith(TEXT("[")) && Trim.EndsWith(TEXT("]")))
		{
			break;   // 到了下一段
		}
		if (!Trim.StartsWith(TEXT("+Chunks=")))
		{
			continue;
		}

		FCrossingChunkRule Rule;

		{
			const FRegexPattern IdPattern(TEXT("ChunkId=(-?\\d+)"));
			FRegexMatcher Matcher(IdPattern, Trim);
			if (Matcher.FindNext())
			{
				Rule.ChunkId = FCString::Atoi(*Matcher.GetCaptureGroup(1));
			}
		}
		if (Rule.ChunkId == INDEX_NONE)
		{
			continue;   // 没写 ChunkId 的行直接跳过
		}

		Rule.ChunkName = Quoted(Trim, TEXT("ChunkName"));
		Rule.Comment   = Quoted(Trim, TEXT("Comment"));
		Rule.bIncludeInInstallPackage =
			Between(Trim, TEXT("bIncludeInInstallPackage="), TEXT(",")).Equals(TEXT("True"), ESearchCase::IgnoreCase);

		const FString FoldersBlock = Between(Trim, TEXT("Folders=("), TEXT("),Priority="));
		const FRegexPattern PathPattern(TEXT("Path=\"([^\"]+)\""));
		FRegexMatcher PathMatcher(PathPattern, FoldersBlock);
		while (PathMatcher.FindNext())
		{
			Rule.Folders.AddUnique(NormalizeFolderPath(PathMatcher.GetCaptureGroup(1)));
		}
		Rule.Folders.Sort();

		OutRules.Add(MoveTemp(Rule));
	}

	if (OutRules.Num() == 0)
	{
		OutError = FString::Printf(TEXT("段 [%s] 里一条 +Chunks= 都没解析出来"), *GetRuleSection());
		return false;
	}

	OutRules.Sort([](const FCrossingChunkRule& A, const FCrossingChunkRule& B) { return A.ChunkId < B.ChunkId; });
	return true;
}

const FCrossingChunkRule* FCrossingChunkRuleService::FindRuleById(const TArray<FCrossingChunkRule>& Rules, int32 ChunkId)
{
	return Rules.FindByPredicate([ChunkId](const FCrossingChunkRule& Rule) { return Rule.ChunkId == ChunkId; });
}

const FCrossingChunkRule* FCrossingChunkRuleService::FindDirectRule(const TArray<FCrossingChunkRule>& Rules, const FString& Folder)
{
	const FString Path = NormalizeFolderPath(Folder);
	for (const FCrossingChunkRule& Rule : Rules)
	{
		if (Rule.Folders.Contains(Path))
		{
			return &Rule;
		}
	}
	return nullptr;
}

const FCrossingChunkRule* FCrossingChunkRuleService::ResolveInheritedRule(
	const TArray<FCrossingChunkRule>& Rules, const FString& Folder, FString& OutOwnerFolder)
{
	FString Path = NormalizeFolderPath(Folder);
	while (!Path.IsEmpty())
	{
		for (const FCrossingChunkRule& Rule : Rules)
		{
			if (Rule.Folders.Contains(Path))
			{
				OutOwnerFolder = Path;
				return &Rule;
			}
		}

		int32 SlashIndex = INDEX_NONE;
		if (!Path.FindLastChar(TEXT('/'), SlashIndex) || SlashIndex <= 0)
		{
			break;
		}
		Path.LeftChopInline(Path.Len() - SlashIndex);
	}
	return nullptr;
}

int32 FCrossingChunkRuleService::NextChunkId(const TArray<FCrossingChunkRule>& Rules)
{
	int32 Next = 0;
	for (const FCrossingChunkRule& Rule : Rules)
	{
		if (Rule.ChunkId >= Next)
		{
			Next = Rule.ChunkId + 1;
		}
	}
	return Next;
}

FString FCrossingChunkRuleService::MakeRuleLine(const FCrossingChunkRule& Rule)
{
	FString Folders;
	for (int32 Index = 0; Index < Rule.Folders.Num(); ++Index)
	{
		if (Index > 0)
		{
			Folders += TEXT(",");
		}
		Folders += FString::Printf(TEXT("(Path=\"%s\")"), *Rule.Folders[Index]);
	}

	return FString::Printf(
		TEXT("+Chunks=(ChunkName=\"%s\",ChunkId=%d,Folders=(%s),Priority=0,bIncludeInInstallPackage=%s,ForceExcludeFolders=,Comment=\"%s\")"),
		*Rule.ChunkName,
		Rule.ChunkId,
		*Folders,
		Rule.bIncludeInInstallPackage ? TEXT("True") : TEXT("False"),
		*Rule.Comment);
}

bool FCrossingChunkRuleService::SaveRules(const TArray<FCrossingChunkRule>& Rules, FString& OutError)
{
	const FString Path = GetRuleIniPath();

	TArray<FString> Lines;
	if (!LoadUtf8Lines(Path, Lines, OutError))
	{
		return false;
	}

	const FString SectionHeader = FString::Printf(TEXT("[%s]"), *GetRuleSection());
	int32 SectionIndex = Lines.IndexOfByPredicate([&SectionHeader](const FString& Line)
	{
		return Line.TrimStartAndEnd().Equals(SectionHeader, ESearchCase::IgnoreCase);
	});
	if (SectionIndex == INDEX_NONE)
	{
		Lines.Add(TEXT(""));
		Lines.Add(SectionHeader);
		SectionIndex = Lines.Num() - 1;
	}

	// 找出这一段里现有的 +Chunks= 行
	TArray<int32> OldLineIndices;
	for (int32 Index = SectionIndex + 1; Index < Lines.Num(); ++Index)
	{
		const FString Trim = Lines[Index].TrimStartAndEnd();
		if (Trim.StartsWith(TEXT("[")) && Trim.EndsWith(TEXT("]")))
		{
			break;
		}
		if (Trim.StartsWith(TEXT("+Chunks=")))
		{
			OldLineIndices.Add(Index);
		}
	}

	const int32 InsertAt = OldLineIndices.Num() > 0 ? OldLineIndices[0] : SectionIndex + 1;

	for (int32 Index = OldLineIndices.Num() - 1; Index >= 0; --Index)
	{
		Lines.RemoveAt(OldLineIndices[Index]);
	}

	int32 Cursor = InsertAt;
	for (const FCrossingChunkRule& Rule : Rules)
	{
		Lines.Insert(MakeRuleLine(Rule), Cursor++);
	}

	return SaveUtf8Lines(Path, Lines, OutError);
}

bool FCrossingChunkRuleService::Assign(const FString& Folder, int32 ChunkId, const FString& ChunkName, FString& OutMessage)
{
	if (ChunkId < 0)
	{
		OutMessage = TEXT("Chunk 号不能是负数。");
		return false;
	}

	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!LoadRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return false;
	}

	const FString Path = NormalizeFolderPath(Folder);

	// 先从别的分块里摘掉（换 chunk 的情况），顺手记下被清空的分块号
	TArray<int32> EmptiedChunks;
	for (FCrossingChunkRule& Rule : Rules)
	{
		if (Rule.Folders.Remove(Path) > 0 && Rule.Folders.Num() == 0)
		{
			EmptiedChunks.Add(Rule.ChunkId);
		}
	}
	Rules.RemoveAll([](const FCrossingChunkRule& Rule) { return Rule.Folders.Num() == 0; });

	FCrossingChunkRule* Target = Rules.FindByPredicate([ChunkId](const FCrossingChunkRule& Rule) { return Rule.ChunkId == ChunkId; });
	if (!Target)
	{
		FCrossingChunkRule NewRule;
		NewRule.ChunkId = ChunkId;
		NewRule.ChunkName = ChunkName.IsEmpty() ? FString::Printf(TEXT("Chunk %d"), ChunkId) : ChunkName;
		NewRule.bIncludeInInstallPackage = (ChunkId == 0);
		Rules.Add(MoveTemp(NewRule));
		Target = &Rules.Last();
	}

	Target->Folders.AddUnique(Path);
	Target->Folders.Sort();

	const FString TargetName = Target->ChunkName;
	Rules.Sort([](const FCrossingChunkRule& A, const FCrossingChunkRule& B) { return A.ChunkId < B.ChunkId; });

	if (!SaveRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("写入失败：%s"), *Error);
		return false;
	}

	FString Extra;
	if (EmptiedChunks.Num() > 0)
	{
		TArray<FString> Parts;
		for (const int32 Id : EmptiedChunks)
		{
			Parts.Add(FString::Printf(TEXT("%d"), Id));
		}
		Extra = FString::Printf(TEXT("；Chunk %s 已没有目录，规则已删除"), *FString::Join(Parts, TEXT("/")));
	}

	OutMessage = FString::Printf(
		TEXT("已把 %s 划给 %s%s（下次打包自动生效）"),
		*Path, *ChunkLabel(ChunkId, TargetName), *Extra);
	return true;
}

bool FCrossingChunkRuleService::RenameChunk(int32 ChunkId, const FString& NewName, FString& OutMessage)
{
	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!LoadRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return false;
	}

	FCrossingChunkRule* Target = Rules.FindByPredicate([ChunkId](const FCrossingChunkRule& Rule)
	{
		return Rule.ChunkId == ChunkId;
	});
	if (!Target)
	{
		OutMessage = FString::Printf(TEXT("找不到 Chunk %d 对应的规则。"), ChunkId);
		return false;
	}

	const FString Trimmed = NewName.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		OutMessage = TEXT("名字不能为空。");
		return false;
	}

	Target->ChunkName = Trimmed;

	if (!SaveRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("写入失败：%s"), *Error);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Chunk %d 已改名为「%s」"), ChunkId, *Trimmed);
	return true;
}

bool FCrossingChunkRuleService::RemoveChunk(int32 ChunkId, FCrossingChunkRule& OutRemoved, FString& OutMessage)
{
	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!LoadRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return false;
	}

	const int32 Index = Rules.IndexOfByPredicate([ChunkId](const FCrossingChunkRule& Rule)
	{
		return Rule.ChunkId == ChunkId;
	});
	if (Index == INDEX_NONE)
	{
		OutMessage = FString::Printf(TEXT("找不到 Chunk %d 对应的规则。"), ChunkId);
		return false;
	}

	OutRemoved = Rules[Index];
	Rules.RemoveAt(Index);

	if (!SaveRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("写入失败：%s"), *Error);
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("已解散 Chunk %d「%s」—— 它下面的 %d 个目录重新变成未归类（可以点「撤销解散」恢复）"),
		ChunkId, *OutRemoved.ChunkName, OutRemoved.Folders.Num());
	return true;
}

bool FCrossingChunkRuleService::RestoreChunk(const FCrossingChunkRule& Rule, FString& OutMessage)
{
	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!LoadRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return false;
	}

	if (FindRuleById(Rules, Rule.ChunkId))
	{
		OutMessage = FString::Printf(TEXT("Chunk %d 已经存在，不用恢复。"), Rule.ChunkId);
		return false;
	}

	Rules.Add(Rule);
	Rules.Sort([](const FCrossingChunkRule& A, const FCrossingChunkRule& B) { return A.ChunkId < B.ChunkId; });

	if (!SaveRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("写入失败：%s"), *Error);
		return false;
	}

	OutMessage = FString::Printf(TEXT("已恢复 Chunk %d「%s」（%d 个目录）"), Rule.ChunkId, *Rule.ChunkName, Rule.Folders.Num());
	return true;
}

bool FCrossingChunkRuleService::RemoveAssignment(const FString& Folder, FString& OutMessage)
{
	TArray<FCrossingChunkRule> Rules;
	FString Error;
	if (!LoadRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("规则读取失败：%s"), *Error);
		return false;
	}

	const FString Path = NormalizeFolderPath(Folder);

	bool bRemoved = false;
	TArray<int32> EmptiedChunks;
	for (FCrossingChunkRule& Rule : Rules)
	{
		if (Rule.Folders.Remove(Path) > 0)
		{
			bRemoved = true;
			if (Rule.Folders.Num() == 0)
			{
				EmptiedChunks.Add(Rule.ChunkId);
			}
		}
	}

	if (!bRemoved)
	{
		OutMessage = FString::Printf(TEXT("%s 本来就没有直接标记。"), *Path);
		return false;
	}

	Rules.RemoveAll([](const FCrossingChunkRule& Rule) { return Rule.Folders.Num() == 0; });

	if (!SaveRules(Rules, Error))
	{
		OutMessage = FString::Printf(TEXT("写入失败：%s"), *Error);
		return false;
	}

	// 取消之后它跟谁走？给用户一个明确答复
	FString Owner;
	const FCrossingChunkRule* NowInherited = ResolveInheritedRule(Rules, Path, Owner);

	FString Extra;
	if (EmptiedChunks.Num() > 0)
	{
		TArray<FString> Parts;
		for (const int32 Id : EmptiedChunks)
		{
			Parts.Add(FString::Printf(TEXT("%d"), Id));
		}
		Extra += FString::Printf(TEXT("；Chunk %s 已没有目录，规则已删除"), *FString::Join(Parts, TEXT("/")));
	}
	if (NowInherited)
	{
		Extra += FString::Printf(TEXT("；现在跟随 %s"), *ChunkLabel(NowInherited->ChunkId, NowInherited->ChunkName));
	}
	else
	{
		Extra += TEXT("；现在没有上级标记，打包时进基础包");
	}

	OutMessage = FString::Printf(TEXT("已取消 %s 的标记%s"), *Path, *Extra);
	return true;
}

// ---------------------------------------------------------------------------
// FCrossingChunkEditorModule
// ---------------------------------------------------------------------------

void FCrossingChunkEditorModule::StartupModule()
{
	FContentBrowserModule& ContentBrowser =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FContentBrowserMenuExtender_SelectedPaths Extender;
	Extender.BindRaw(this, &FCrossingChunkEditorModule::OnExtendPathContextMenu);
	ContentBrowser.GetAllPathViewContextMenuExtenders().Add(Extender);
	PathMenuExtenderHandle = Extender.GetHandle();

	// 《二游打包》页：注册成一个可停靠页签。
	// 入口不放「窗口」菜单（Hidden），而是挂到「平台」菜单里 —— 见 RegisterMainMenu()。
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		CrossingChunkPanelTabName,
		FOnSpawnTab::CreateRaw(this, &FCrossingChunkEditorModule::SpawnPanelTab))
		.SetDisplayName(LOCTEXT("PanelTabTitle", "二游打包"))
		.SetTooltipText(LOCTEXT("PanelTabTip", "分块规则、未分类清单、打包与实时日志"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	RegisterMainMenu();
}

void FCrossingChunkEditorModule::RegisterMainMenu()
{
	// 用 UToolMenus 扩展「平台」菜单。
	// 菜单名是 LevelEditor.MainMenu.Platforms（带 s），扩展点靠这个字符串绑定 ——
	// 这条是照着被停用的旧插件（_CrossingChunk.disabled）里的写法搬过来的，那条写法验证过能出现。
	MainMenuStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(FName(TEXT("CrossingChunkEditor")));

			if (UToolMenu* PlatformsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Platforms")))
			{
				FToolMenuSection& Section = PlatformsMenu->FindOrAddSection(
					TEXT("CrossingChunkPack"),
					LOCTEXT("PackMenuSection", "二游打包"));

				Section.AddMenuEntry(
					TEXT("OpenCrossingChunkPanel"),
					LOCTEXT("OpenPanel", "二游打包"),
					LOCTEXT("OpenPanelTip",
						"打开《二游打包》页：分块规则、未分类清单、打包与实时日志\n"
						"和命令行跑插件自带的 Tools\\Pack-CrossingVoid.ps1 是同一套命令"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]()
					{
						FGlobalTabmanager::Get()->TryInvokeTab(CrossingChunkPanelTabName);
					})));
			}
		}));
}

void FCrossingChunkEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(MainMenuStartupHandle);
	UToolMenus::UnregisterOwner(FName(TEXT("CrossingChunkEditor")));
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CrossingChunkPanelTabName);

	if (FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
	{
		FContentBrowserModule& ContentBrowser =
			FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowser.GetAllPathViewContextMenuExtenders().RemoveAll(
			[this](const FContentBrowserMenuExtender_SelectedPaths& Delegate)
			{
				return Delegate.GetHandle() == PathMenuExtenderHandle;
			});
	}
}

// ---------------------------------------------------------------------------
// 《二游打包》页签
// ---------------------------------------------------------------------------

TSharedRef<SDockTab> FCrossingChunkEditorModule::SpawnPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SCrossingChunkPanel)
		];
}

TSharedRef<FExtender> FCrossingChunkEditorModule::OnExtendPathContextMenu(const TArray<FString>& SelectedPaths)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	// 只在「刚好右键一个文件夹」时出现
	if (SelectedPaths.Num() != 1 || SelectedPaths[0].IsEmpty())
	{
		return Extender;
	}

	const FString Folder = FCrossingChunkRuleService::NormalizeFolderPath(SelectedPaths[0]);

	Extender->AddMenuExtension(
		"PathViewFolderOptions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([Folder](FMenuBuilder& MenuBuilder)
		{
			BuildFolderChunkMenu(Folder, MenuBuilder);
		}));

	return Extender;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCrossingChunkEditorModule, CrossingChunkEditor)

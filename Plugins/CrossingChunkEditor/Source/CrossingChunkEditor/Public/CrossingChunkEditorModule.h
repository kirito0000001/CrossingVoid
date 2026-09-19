#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 一条分块规则，对应 Config\DefaultCrossingChunk.ini 里的一行：
 *   +Chunks=(ChunkName="基础包",ChunkId=0,Folders=((Path="/Game/Base"),...),
 *            Priority=0,bIncludeInInstallPackage=True,ForceExcludeFolders=,Comment="...")
 */
struct FCrossingChunkRule
{
	FString ChunkName;
	int32   ChunkId = INDEX_NONE;
	TArray<FString> Folders;          // 形如 /Game/GameActor2D/SAO_Kirito
	FString Comment;
	bool    bIncludeInInstallPackage = false;
};

/**
 * 规则集的唯一读写入口。
 *
 * 为什么是 ini 而不是 AST 标签（2026-09-18 实测 + 读引擎源码得出的结论）：
 *   · UE 5.8 的 UPrimaryAssetLabel 没有 PostLoad 覆写，规则注册逻辑写在 editor-only 的
 *     UpdateAssetBundleData() 末尾 —— cook 阶段根本不会执行，所以标签那条路是死的。
 *   · 真正生效的是 Tools\Build-PakFileRules.ps1：它读这份 ini，翻译成
 *     Config\DefaultPakFileRules.ini，再由 pak 阶段的 ApplyPakFileRules 逐文件改写 chunk 归属。
 *   => 编辑器里点右键、和打包时读的，必须是同一份 ini。这就是本类存在的理由。
 */
class FCrossingChunkRuleService
{
public:
	/** <工程>\Config\DefaultCrossingChunk.ini */
	static FString GetRuleIniPath();

	/** 规则段名 */
	static FString GetRuleSection();

	/** 读出全部规则；失败时 OutError 带原因。 */
	static bool LoadRules(TArray<FCrossingChunkRule>& OutRules, FString& OutError);

	/** 该目录「自己」有没有直接标记；有就返回那条规则。 */
	static const FCrossingChunkRule* FindDirectRule(const TArray<FCrossingChunkRule>& Rules, const FString& Folder);

	/** 从该目录往上找最近的已声明目录（含自己），OutOwnerFolder 返回是谁定的。 */
	static const FCrossingChunkRule* ResolveInheritedRule(const TArray<FCrossingChunkRule>& Rules, const FString& Folder, FString& OutOwnerFolder);

	static const FCrossingChunkRule* FindRuleById(const TArray<FCrossingChunkRule>& Rules, int32 ChunkId);

	/** 下一个没被占用的 ChunkId。 */
	static int32 NextChunkId(const TArray<FCrossingChunkRule>& Rules);

	/** 把目录划给某个 Chunk；ChunkId 不存在就新建一条规则（名字用 ChunkName）。 */
	static bool Assign(const FString& Folder, int32 ChunkId, const FString& ChunkName, FString& OutMessage);

	/** 去掉该目录的直接标记（从此继承父目录）；规则空了会一并删掉。 */
	static bool RemoveAssignment(const FString& Folder, FString& OutMessage);

	/** 改分块名字（号不动；名字只影响显示，不影响已发布的包）。 */
	static bool RenameChunk(int32 ChunkId, const FString& NewName, FString& OutMessage);

	/** 解散一个分块：删掉这条规则，它下面的目录重新变成未归类。返回被删的规则（供撤销）。 */
	static bool RemoveChunk(int32 ChunkId, FCrossingChunkRule& OutRemoved, FString& OutMessage);

	/** 撤销解散：把之前删掉的规则原样塞回去（号相同就直接跳过）。 */
	static bool RestoreChunk(const FCrossingChunkRule& Rule, FString& OutMessage);

	/** 归一化：反斜杠转正斜杠、去尾部斜杠。 */
	static FString NormalizeFolderPath(const FString& FolderPath);

	/** 目录名（最后一段），用来给新建的 Chunk 起默认名字。 */
	static FString GetFolderLeafName(const FString& FolderPath);

	/** 序列化成一行 +Chunks=(...)。 */
	static FString MakeRuleLine(const FCrossingChunkRule& Rule);

private:
	/** 整份写回：保留文件里其它段和注释，只替换 +Chunks= 那些行。 */
	static bool SaveRules(const TArray<FCrossingChunkRule>& Rules, FString& OutError);
};

class FCrossingChunkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 《二游打包》页签的生成回调。 */
	TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args);

	/** 把入口挂到「平台」菜单（`LevelEditor.MainMenu.Platforms`）里"打包项目"旁边。 */
	void RegisterMainMenu();

	/** 内容浏览器「文件夹右键」菜单扩展。 */
	TSharedRef<FExtender> OnExtendPathContextMenu(const TArray<FString>& SelectedPaths);

	FDelegateHandle PathMenuExtenderHandle;
	FDelegateHandle MainMenuStartupHandle;
};

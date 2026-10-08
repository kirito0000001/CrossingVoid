// AtlasFX 的 Content Browser 右键菜单实现。
//
// 菜单挂在 UPaperFlipbook 的资产右键菜单上，放在**最前面**（FToolMenuInsert(NAME_None, First)）。
// 点一下做四件事：
//   1. 复制模板（精灵 / 网格两套）到 Flipbook 所在目录，命名 NS_<Flipbook名>_Sprite / _Mesh；
//   2. 在复制出来的系统里按**类型**找到 AtlasFX 的 Data Interface（不按名字找，改名也不影响）；
//   3. 把它的 Flipbook 指向右键那个资产，调 RefreshFromSource() 把帧表烘好；
//   4. 存盘 + 在 Content Browser 里选中新资产。
//
// 材质的贴图绑定不用管：模板里已经有 Sheet ← DI 的 ResolvedTexture 的属性绑定。
//
// 多选时每个 Flipbook 各生成一个系统；选中项里混了别的资产时只处理其中的 Flipbook。

#include "AtlasFXFlipbookActions.h"

#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraDataInterfaceSpriteAtlas.h"
#include "NiagaraSystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookFactory.h"
#include "PaperFlipbookHelpers.h"
#include "PaperSprite.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AtlasFXFlipbookActions"

namespace AtlasFXFlipbookMenu
{
	/** 分区名与 owner 名：注销时按 owner 名整片清掉。 */
	const FName SectionName(TEXT("AtlasFX"));
	const FName OwnerName(TEXT("AtlasFXFlipbookActions"));

	/** 两个模板 —— 和「创建 Niagara 系统」向导里的 CrossingvoidAtlas 分类用的是同一对。 */
	const TCHAR* SpriteTemplatePackage = TEXT("/AtlasFX/Templates/NS_Atlas2D_Sprite");
	const TCHAR* MeshTemplatePackage = TEXT("/AtlasFX/Templates/NS_Atlas2D_Mesh");

	enum class ERendererKind : uint8
	{
		Sprite,
		Mesh,
	};

	const TCHAR* KindSuffix(ERendererKind Kind)
	{
		return Kind == ERendererKind::Sprite ? TEXT("_Sprite") : TEXT("_Mesh");
	}

	FText KindLabel(ERendererKind Kind)
	{
		return Kind == ERendererKind::Sprite
			? LOCTEXT("KindSprite", "精灵渲染器")
			: LOCTEXT("KindMesh", "网格渲染器");
	}

	/** 找系统用户参数里第一个 AtlasFX 的 DI —— 按类型找，不按名字（与 AtlasFXSetupCommandlet 同一套判据）。 */
	UNiagaraDataInterfaceSpriteAtlas* FindAtlasDataInterface(UNiagaraSystem* System)
	{
		for (UNiagaraDataInterface* DataInterface : System->GetExposedParameters().GetDataInterfaces())
		{
			if (UNiagaraDataInterfaceSpriteAtlas* Atlas = Cast<UNiagaraDataInterfaceSpriteAtlas>(DataInterface))
			{
				return Atlas;
			}
		}
		return nullptr;
	}

	/** 选中项里所有 Paper Flipbook（混选时只取 Flipbook）。GetAsset() 会加载包，右键时资产通常已经常驻。 */
	TArray<UPaperFlipbook*> CollectSelectedFlipbooks(const UContentBrowserAssetContextMenuContext& Context)
	{
		TArray<UPaperFlipbook*> Result;
		for (const FAssetData& AssetData : Context.SelectedAssets)
		{
			if (UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(AssetData.GetAsset()))
			{
				Result.Add(Flipbook);
			}
		}
		return Result;
	}

	void Notify(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = bSuccess ? 4.0f : 8.0f;
		Info.bUseSuccessFailIcons = true;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	/** 真正干活：为每个 Flipbook 复制一个模板并接好图集。 */
	void CreateSystems(TArray<UPaperFlipbook*> Flipbooks, ERendererKind Kind)
	{
		UObject* Template = LoadObject<UObject>(nullptr, Kind == ERendererKind::Sprite ? SpriteTemplatePackage : MeshTemplatePackage);
		if (Template == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("AtlasFX：找不到模板 %s，右键菜单创建不了。"),
				Kind == ERendererKind::Sprite ? SpriteTemplatePackage : MeshTemplatePackage);
			Notify(LOCTEXT("TemplateMissing", "AtlasFX：模板资产不存在，创建失败（看 Output Log）"), false);
			return;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		const FScopedTransaction Transaction(LOCTEXT("CreateAtlasFXSystems", "创建 AtlasFX 特效"));

		TArray<UObject*> CreatedAssets;
		int32 BoundCount = 0;
		int32 UnboundCount = 0;

		for (UPaperFlipbook* Flipbook : Flipbooks)
		{
			if (Flipbook == nullptr)
			{
				continue;
			}

			// 同目录 + NS_ 前缀 + 渲染器后缀；重名时 DuplicateAsset 自己会加 _1、_2。
			const FString PackagePath = FPackageName::GetLongPackagePath(Flipbook->GetOutermost()->GetName());
			const FString BaseName = FString::Printf(TEXT("NS_%s%s"), *Flipbook->GetName(), KindSuffix(Kind));

			UObject* NewAsset = AssetTools.DuplicateAsset(BaseName, PackagePath, Template);
			UNiagaraSystem* NewSystem = Cast<UNiagaraSystem>(NewAsset);
			if (NewSystem == nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("AtlasFX：为 %s 复制模板失败，跳过。"), *Flipbook->GetName());
				continue;
			}

			if (UNiagaraDataInterfaceSpriteAtlas* Atlas = FindAtlasDataInterface(NewSystem))
			{
				// 安全闸：复制出来的系统必须是**自己的** DI 实例。万一复制没深拷 DI（那就是和模板共用同一个对象），
				// 改它等于改模板 —— 宁可报错也不要悄悄污染模板。
				if (UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(Template))
				{
					if (Atlas == FindAtlasDataInterface(TemplateSystem))
					{
						UE_LOG(LogTemp, Error,
							TEXT("AtlasFX：%s 的 Data Interface 与模板共用同一个实例，跳过它以免改坏模板。"),
							*NewSystem->GetName());
						++UnboundCount;
						NewSystem->MarkPackageDirty();
						CreatedAssets.Add(NewSystem);
						continue;
					}
				}

				Atlas->Modify();
				Atlas->Flipbook = Flipbook;
				// 就地烘帧表：这一步之后 DI 里的图集矩形 / 画布矩形 / 尺寸 / FPS 才是这个 Flipbook 的。
				Atlas->RefreshFromSource();
				Atlas->MarkPackageDirty();
				++BoundCount;
			}
			else
			{
				++UnboundCount;
				UE_LOG(LogTemp, Warning,
					TEXT("AtlasFX：%s 里没找到 AtlasFX 的 Data Interface，特效建出来了但没接图集（检查模板 %s）。"),
					*NewSystem->GetName(), *Template->GetName());
			}

			NewSystem->MarkPackageDirty();

			// ⚠️ 改完 DI 之后**必须让系统编译一次**，而且要等编译完成再存盘。
			// Niagara 的编译结果（脚本 + 参数哈希那一套）是**存在资产里**的，而
			// 「DuplicateAsset → 改 DI → SavePackage」这条路上没有人编译 ⇒ 存下去的是一个
			// **没编译**的系统：在编辑器里得把它打开一次（打开才会编译）才生效，
			// 症状就是「右键建完不显示，打开一次就好」（用户 2026-10-06 报）。
			NewSystem->RequestCompile(/*bForce=*/false);
			NewSystem->WaitForCompilationComplete(/*bIncludingGPUShaders=*/false, /*bShowProgress=*/false);

			CreatedAssets.Add(NewSystem);
		}

		// 新资产的包要落盘：帧表是烘在资产里的，不存盘下次打开就还是空的。
		for (UObject* Asset : CreatedAssets)
		{
			UPackage* Package = Asset->GetOutermost();
			const FString FileName = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (UPackage::SavePackage(Package, Asset, *FileName, SaveArgs) == false)
			{
				UE_LOG(LogTemp, Warning, TEXT("AtlasFX：%s 存盘失败（帧表可能没写进去）。"), *Package->GetName());
			}
		}

		if (CreatedAssets.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(CreatedAssets);
		}

		if (CreatedAssets.Num() == 0)
		{
			Notify(LOCTEXT("NothingCreated", "AtlasFX：没有创建任何资产（看 Output Log）"), false);
		}
		else if (UnboundCount > 0)
		{
			Notify(FText::Format(
				LOCTEXT("CreatedPartly", "AtlasFX：创建了 {0} 个系统，其中 {1} 个没接上图集（看 Output Log）"),
				FText::AsNumber(CreatedAssets.Num()), FText::AsNumber(UnboundCount)), false);
		}
		else
		{
			Notify(FText::Format(
				LOCTEXT("CreatedOk", "AtlasFX：已创建 {0} 个特效系统（{1}）"),
				FText::AsNumber(CreatedAssets.Num()), KindLabel(Kind)), true);
		}
	}

	/** 菜单项按选中数量动态生成：多选时每个 Flipbook 各来一个系统。 */
	void PopulateSection(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context =
			UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (Context == nullptr)
		{
			return;
		}

		const TArray<UPaperFlipbook*> Flipbooks = CollectSelectedFlipbooks(*Context);
		if (Flipbooks.Num() == 0)
		{
			return;
		}

		const int32 Count = Flipbooks.Num();
		for (const ERendererKind Kind : { ERendererKind::Sprite, ERendererKind::Mesh })
		{
			const FText Label = Count > 1
				? FText::Format(LOCTEXT("CreateMany", "为 {0} 个 Flipbook 创建特效（{1}）"),
					FText::AsNumber(Count), KindLabel(Kind))
				: FText::Format(LOCTEXT("CreateOne", "创建 AtlasFX 特效（{0}）"), KindLabel(Kind));

			const FText Tooltip = LOCTEXT("CreateTip",
				"在同一个目录生成 NS_<Flipbook 名>_Sprite / _Mesh，并把图集指向这个 Flipbook：帧表当场烘好，材质贴图由渲染器自动绑定。");

			InSection.AddMenuEntry(
				Kind == ERendererKind::Sprite ? TEXT("AtlasFX.CreateSprite") : TEXT("AtlasFX.CreateMesh"),
				Label,
				Tooltip,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&CreateSystems, Flipbooks, Kind)));
		}
	}

	// ------------------------------------------------------------------
	// Sprite 表 → 创建 Flipbook（名字加 _Flipbook 后缀）
	// ------------------------------------------------------------------

	/**
	 * Sprite 表的类。`UPaperSpriteSheet` 声明在引擎插件 PaperSpriteSheetImporter 的 **Private** 头里
	 * （`Source/PaperSpriteSheetImporter/Private/PaperSpriteSheet.h`），外部模块 include 不到，
	 * 所以按类路径找，属性也走反射读。
	 */
	UClass* GetSpriteSheetClass()
	{
		static UClass* Cached = FindObject<UClass>(nullptr, TEXT("/Script/PaperSpriteSheetImporter.PaperSpriteSheet"));
		return Cached;
	}

	/** 收集选中的 Sprite 表（.paper2dsprites 导入出来的那个资产）。 */
	TArray<UObject*> CollectSelectedSpriteSheets(const UContentBrowserAssetContextMenuContext& Context)
	{
		TArray<UObject*> Result;
		UClass* SheetClass = GetSpriteSheetClass();
		if (SheetClass == nullptr)
		{
			return Result;
		}

		for (const FAssetData& AssetData : Context.SelectedAssets)
		{
			if (UObject* Asset = AssetData.GetAsset())
			{
				if (Asset->IsA(SheetClass))
				{
					Result.Add(Asset);
				}
			}
		}
		return Result;
	}

	/**
	 * 复刻引擎自带的「Create Flipbooks」（PaperSpriteSheetAssetTypeActions.cpp:76-164），
	 * **只改一件事：最终名字加 `_Flipbook` 后缀**。
	 *
	 * 为什么需要：引擎那版的名字是由 sprite 名推导出来的，往往和刚导入的 Sprite 表/贴图同名
	 * （例如都叫 `Ko`）⇒ `CreateUniqueAssetName` 于是给出 `Ko1`。用户定案（2026-10-06）：
	 * 导入保持原始命名，**Flipbook 加 `_Flipbook` 后缀**（工程里 `KO_Flipbook.uasset` 就是这个风格）。
	 */
	void CreateFlipbooksFromSpriteSheets(TArray<UObject*> Sheets)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		const FScopedTransaction Transaction(LOCTEXT("CreateAtlasFlipbooks", "创建 Flipbook（_Flipbook 后缀）"));

		TArray<UObject*> Created;
		for (UObject* Sheet : Sheets)
		{
			if (Sheet == nullptr)
			{
				continue;
			}

			const FString PackagePath = FPackageName::GetLongPackagePath(Sheet->GetOutermost()->GetName());

			// 反射读 Sprites（TArray<TSoftObjectPtr<UPaperSprite>>）与 SpriteNames（TArray<FString>）。
			TArray<UPaperSprite*> Sprites;
			TArray<FString> SpriteNames;
			{
				const FArrayProperty* SpritesProp = FindFProperty<FArrayProperty>(Sheet->GetClass(), TEXT("Sprites"));
				const FArrayProperty* NamesProp = FindFProperty<FArrayProperty>(Sheet->GetClass(), TEXT("SpriteNames"));
				if (SpritesProp == nullptr || NamesProp == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("AtlasFX：%s 上找不到 Sprites/SpriteNames 属性，跳过。"), *Sheet->GetName());
					continue;
				}

				FScriptArrayHelper SpritesHelper(SpritesProp, SpritesProp->ContainerPtrToValuePtr<void>(Sheet));
				FScriptArrayHelper NamesHelper(NamesProp, NamesProp->ContainerPtrToValuePtr<void>(Sheet));

				// 和引擎一样：表里记的名字与 Sprite 数量对得上就用它，否则退回 Sprite 自己的名字。
				const bool bUseSpriteNames = NamesHelper.Num() == SpritesHelper.Num();
				for (int32 Index = 0; Index < SpritesHelper.Num(); ++Index)
				{
					// TSoftObjectPtr 的内存布局就是 FSoftObjectPtr。
					const FSoftObjectPtr& SoftSprite = *reinterpret_cast<const FSoftObjectPtr*>(SpritesHelper.GetRawPtr(Index));
					UPaperSprite* Sprite = Cast<UPaperSprite>(SoftSprite.LoadSynchronous());
					if (Sprite == nullptr)
					{
						continue;
					}

					Sprites.Add(Sprite);
					SpriteNames.Add(bUseSpriteNames
						? *reinterpret_cast<const FString*>(NamesHelper.GetRawPtr(Index))
						: Sprite->GetName());
				}
			}

			TMap<FString, TArray<UPaperSprite*>> SpriteFlipbookMap;
			FPaperFlipbookHelpers::ExtractFlipbooksFromSprites(SpriteFlipbookMap, Sprites, SpriteNames);

			for (const TPair<FString, TArray<UPaperSprite*>>& Pair : SpriteFlipbookMap)
			{
				const FString DesiredName = Pair.Key + TEXT("_Flipbook");
				const FString TentativePath = UPackageTools::SanitizePackageName(PackagePath + TEXT("/") + DesiredName);

				FString PackageName;
				FString AssetName;
				AssetTools.CreateUniqueAssetName(TentativePath, FString(), /*out*/ PackageName, /*out*/ AssetName);

				UPaperFlipbookFactory* Factory = NewObject<UPaperFlipbookFactory>();
				for (UPaperSprite* Sprite : Pair.Value)
				{
					FPaperFlipbookKeyFrame* KeyFrame = new (Factory->KeyFrames) FPaperFlipbookKeyFrame();
					KeyFrame->Sprite = Sprite;
					KeyFrame->FrameRun = 1;
				}

				if (UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UPaperFlipbook::StaticClass(), Factory))
				{
					Created.Add(NewAsset);
					UE_LOG(LogTemp, Log, TEXT("AtlasFX：从 Sprite 表 %s 创建 Flipbook %s（%d 帧）"),
						*Sheet->GetName(), *NewAsset->GetName(), Pair.Value.Num());
				}
			}
		}

		if (Created.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(Created);
			Notify(FText::Format(LOCTEXT("FlipbooksCreated", "AtlasFX：创建了 {0} 个 Flipbook（_Flipbook 后缀）"),
				FText::AsNumber(Created.Num())), true);
		}
		else
		{
			Notify(LOCTEXT("FlipbooksNone", "AtlasFX：没创建任何 Flipbook（这张表里没解析出成组的帧名？看 Output Log）"), false);
		}
	}

	void PopulateSpriteSheetSection(FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context =
			UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
		if (Context == nullptr)
		{
			return;
		}

		const TArray<UObject*> Sheets = CollectSelectedSpriteSheets(*Context);
		if (Sheets.Num() == 0)
		{
			return;
		}

		InSection.AddMenuEntry(
			TEXT("AtlasFX.CreateFlipbooks"),
			Sheets.Num() > 1
				? FText::Format(LOCTEXT("CreateFlipbooksMany", "创建 Flipbook（{0} 张表，名字加 _Flipbook）"), FText::AsNumber(Sheets.Num()))
				: LOCTEXT("CreateFlipbooksOne", "创建 Flipbook（名字加 _Flipbook）"),
			LOCTEXT("CreateFlipbooksTip",
				"和引擎自带的「Create Flipbooks」同一套名字推导，只是最终名字加 _Flipbook 后缀 —— "
				"避免和刚导入的 Sprite 表/贴图撞名（撞名会被唯一化成 Ko1 这种）。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&CreateFlipbooksFromSpriteSheets, Sheets)));
	}

	void RegisterMenus()
	{
		// 所有菜单项都记在这个 owner 名下，Shutdown 时一次 UnregisterOwnerByName 全清掉。
		FToolMenuOwnerScoped OwnerScoped(OwnerName);

		// ---- Paper Flipbook：一键建 AtlasFX 特效 ----
		if (UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UPaperFlipbook::StaticClass()))
		{
			// 放在最前面：Paper Flipbook 本来没有别的插件分区，这样一眼就能看到。
			FToolMenuSection& Section = Menu->AddSection(
				SectionName,
				LOCTEXT("AtlasFXSection", "AtlasFX"),
				FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

			Section.AddDynamicEntry(
				TEXT("AtlasFX.FlipbookActions"),
				FNewToolMenuSectionDelegate::CreateStatic(&PopulateSection));
		}

		// ---- Sprite 表（.paper2dsprites 导入出来的那个资产）：建 Flipbook 时加 _Flipbook 后缀 ----
		UClass* SheetClass = GetSpriteSheetClass();
		if (UToolMenu* SpriteSheetMenu = SheetClass ? UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(SheetClass) : nullptr)
		{
			FToolMenuSection& SpriteSection = SpriteSheetMenu->AddSection(
				SectionName,
				LOCTEXT("AtlasFXSection", "AtlasFX"),
				FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

			SpriteSection.AddDynamicEntry(
				TEXT("AtlasFX.SpriteSheetActions"),
				FNewToolMenuSectionDelegate::CreateStatic(&PopulateSpriteSheetSection));
		}
	}
}

void FAtlasFXFlipbookActions::Startup()
{
	// UToolMenus 可能还没建好，菜单更没建好；启动回调是引擎给的排队方式。
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&AtlasFXFlipbookMenu::RegisterMenus));
}

void FAtlasFXFlipbookActions::Shutdown()
{
	if (!IsEngineExitRequested() && UToolMenus::Get() != nullptr)
	{
		UToolMenus::Get()->UnregisterOwnerByName(AtlasFXFlipbookMenu::OwnerName);
	}
}

#undef LOCTEXT_NAMESPACE

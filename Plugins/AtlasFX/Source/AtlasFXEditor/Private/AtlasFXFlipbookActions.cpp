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
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraDataInterfaceSpriteAtlas.h"
#include "NiagaraSystem.h"
#include "PaperFlipbook.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
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

	void RegisterMenus()
	{
		// 所有菜单项都记在这个 owner 名下，Shutdown 时一次 UnregisterOwnerByName 全清掉。
		FToolMenuOwnerScoped OwnerScoped(OwnerName);

		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UPaperFlipbook::StaticClass());
		if (Menu == nullptr)
		{
			return;
		}

		// 放在最前面：Paper Flipbook 本来没有别的插件分区，这样一眼就能看到。
		FToolMenuSection& Section = Menu->AddSection(
			SectionName,
			LOCTEXT("AtlasFXSection", "AtlasFX"),
			FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

		Section.AddDynamicEntry(
			TEXT("AtlasFX.FlipbookActions"),
			FNewToolMenuSectionDelegate::CreateStatic(&PopulateSection));
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

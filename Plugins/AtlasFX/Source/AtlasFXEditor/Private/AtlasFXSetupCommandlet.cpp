// AtlasFX 收尾命令的实现。
//
// 为什么非要有这个命令（两条硬事实，来自引擎源码勘察）：
//   1) 用户资产标签（User Asset Tag）存在**资产包的元数据**里，键名 `UAT.<标签名>`
//      （UserAssetTagEditorUtilities.cpp:102-113 AddUserAssetTag → Package->GetMetaData().RootMetaDataMap）。
//      只能靠编辑器代码或 Content Browser 的 Manage Tags 写进去，手改 .uasset 不现实。
//   2) 「创建 Niagara 系统」对话框左侧分类来自 UTaggedAssetBrowserConfiguration 的 FilterRoot。
//      引擎那份基础配置（ProfileName = NiagaraWizard.System，资产 /Niagara/DefaultAssets/TABC_SystemWizard）
//      是引擎资产、不该动；但可以另放一个 bIsExtension = true 的扩展资产 —— 对话框打开时会把它的
//      section 复制进基础配置（STaggedAssetBrowser.cpp:194-318 ApplyFilterExtensions）。
//      注意扩展资产必须在磁盘上（GetExtensionAssets 用 bIncludeOnlyOnDiskAssets=true 查 Asset Registry）。
//   3) 分类里的过滤器**只能用目录**，不能用用户资产标签：UTaggedAssetBrowserFilter_UserAssetTag 的
//      AR 预过滤是 Filter.TagsAndValues.Add("UAT.<标签>")，而 UAT.* 进不了 Asset Registry ——
//      实测连引擎自己的 UAT.Template / UAT.Lightweight / UAT.LearningContent 都查到 0 个资产。
//      所以标签照写（Content Browser 的标签界面能用），但分类显示走 /AtlasFX/Templates 目录过滤。
//
// 本命令是幂等的：重复跑只会重建分类层级、重复写同一个标签。

#include "AtlasFXSetupCommandlet.h"

#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/Players/PaperZDAnimPlayer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Assets/TaggedAssetBrowserConfiguration.h"
#include "AtlasFXPaperZDPreview.h"
#include "DataHierarchyCommonTypes.h"
#include "DataHierarchyViewModelBase.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Texture.h"
#include "Misc/PackageName.h"
#include "NiagaraDataInterfaceSpriteAtlas.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "Notifies/PaperZDAnimNotify_Base.h"
#include "UObject/UObjectIterator.h"
#include "TaggedAssetBrowserFilters/TaggedAssetBrowser_CommonFilters.h"
#include "UObject/AssetRegistryTagsContext.h"   // EAssetRegistryTagsCaller 的定义（AssetData.h 里只有前向声明）
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"                 // FindFProperty
#include "UserAssetTagEditorUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogAtlasFXSetup, Log, All);

namespace AtlasFXSetup
{
	/** 分类名兼标签名。section 的显示名与标签名互相独立，这里取同一个纯粹图省事。 */
	static const FName TagName(TEXT("CrossingvoidAtlas"));

	/** 改名前的标签，跑一次把它清掉，免得两个分类都冒出来。 */
	static const FName LegacyTagName(TEXT("Crossingvoid2D"));

	/** 基础配置的 ProfileName（取自引擎资产 /Niagara/DefaultAssets/TABC_SystemWizard 的字符串表）。 */
	static const FName WizardProfileName(TEXT("NiagaraWizard.System"));

	/** 采样层材质 /AtlasFX/M_FXAtlasSheet 里的贴图参数名。渲染器上的纹理槽必须写这个名字才对得上。 */
	static const FName SheetMaterialParameterName(TEXT("Sheet"));

	/** 扩展配置资产。 */
	static const TCHAR* ConfigPackageName = TEXT("/AtlasFX/TABC_Atlas2DWizard");
	static const TCHAR* ConfigAssetName   = TEXT("TABC_Atlas2DWizard");

	/** 要打标签的模板（包路径，脚本里给的是包路径，取对象时再补 .资产名）。 */
	static const TCHAR* TemplatePackages[] =
	{
		TEXT("/AtlasFX/Templates/NS_Atlas2D_Sprite"),
		TEXT("/AtlasFX/Templates/NS_Atlas2D_Mesh"),
	};

	/** 包路径 → 完整对象路径（/A/B → /A/B.B）。 */
	static FString ToObjectPath(const TCHAR* PackageName)
	{
		return FString::Printf(TEXT("%s.%s"), PackageName, *FPackageName::GetShortName(PackageName));
	}

	/** 存一个包，失败时把文件名打进日志。 */
	static bool SaveAssetPackage(UPackage* Package, UObject* Asset, const TCHAR* PackageName)
	{
		Package->MarkPackageDirty();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;

		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (UPackage::SavePackage(Package, Asset, *FileName, SaveArgs) == false)
		{
			UE_LOG(LogAtlasFXSetup, Error, TEXT("保存失败：%s"), *FileName);
			return false;
		}

		return true;
	}
}

UAtlasFXSetupCommandlet::UAtlasFXSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UAtlasFXSetupCommandlet::Main(const FString& Params)
{
	UE_LOG(LogAtlasFXSetup, Display, TEXT("=== AtlasFX 模板配置：开始 ==="));

	const bool bConfigOk = BuildWizardConfig();
	const bool bSeedOk   = SeedRendererMaterialParameters();
	const bool bTagsOk   = TagTemplates();

	// 新资产要能被对话框里的 Asset Registry 查询看到。
	IAssetRegistry::Get()->ScanPathsSynchronous({ TEXT("/AtlasFX") }, /*bForceRescan=*/true);

	// 标签刷新必须排在重扫之后：bForceRescan 会用文件头重建 AR 条目，把先推进去的标签冲掉。
	RefreshAssetRegistryTags();

	// PaperZD 特效预览扩展的反射自查（纯诊断，不影响命令成败）。
	DumpPaperZDPreviewTargets();

	// 自查：用和「创建 Niagara 系统」对话框一模一样的方式查一遍标签。
	// （UTaggedAssetBrowserFilter_UserAssetTag::ModifyARFilterInternal 写的就是
	//   Filter.TagsAndValues.Add(GetUATPrefixedTag(Tag), TOptional<FString>()) —— 值必须为空。）
	{
		// 前缀常量在头里是 inline 的（可以直接用）；GetUATPrefixedTag() 没导出，链接不上。
		const FName PrefixedTag(*FString::Printf(TEXT("%s%s"), *UE::UserAssetTags::UAT_METADATA_PREFIX, *AtlasFXSetup::TagName.ToString()));

		// 先直接问 Asset Registry：这两个系统的标签到底进去没有。
		for (const TCHAR* TemplatePackage : AtlasFXSetup::TemplatePackages)
		{
			const FAssetData AssetData = IAssetRegistry::Get()->GetAssetByObjectPath(FSoftObjectPath(AtlasFXSetup::ToObjectPath(TemplatePackage)));
			if (AssetData.IsValid() == false)
			{
				UE_LOG(LogAtlasFXSetup, Display, TEXT("  自查 · AR 里根本没有 %s"), TemplatePackage);
				continue;
			}

			FString Value;
			const bool bHasTag = AssetData.GetTagValue(PrefixedTag, Value);
			UE_LOG(LogAtlasFXSetup, Display, TEXT("  自查 · AR[%s] 的 %s：%s（值='%s'）"),
				TemplatePackage, *PrefixedTag.ToString(), bHasTag ? TEXT("有") : TEXT("没有"), *Value);
		}

		FARFilter Filter;
		Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.TagsAndValues.Add(PrefixedTag, TOptional<FString>());

		TArray<FAssetData> Found;
		IAssetRegistry::Get()->GetAssets(Filter, Found);

		UE_LOG(LogAtlasFXSetup, Display, TEXT("自查：按 %s 过滤到 %d 个 Niagara 系统"), *PrefixedTag.ToString(), Found.Num());
		for (const FAssetData& AssetData : Found)
		{
			UE_LOG(LogAtlasFXSetup, Display, TEXT("        · %s"), *AssetData.GetSoftObjectPath().ToString());
		}

		// 诊断一：第一个模板在 AR 里到底有哪些标签（UAT.* 究竟进没进去）。
		{
			const FAssetData AssetData = IAssetRegistry::Get()->GetAssetByObjectPath(FSoftObjectPath(AtlasFXSetup::ToObjectPath(AtlasFXSetup::TemplatePackages[0])));
			UE_LOG(LogAtlasFXSetup, Display, TEXT("  诊断 · %s 的 AR 标签共 %d 条："),
				AtlasFXSetup::TemplatePackages[0], AssetData.TagsAndValues.Num());
			for (const TPair<FName, FAssetTagValueRef>& TagValue : AssetData.TagsAndValues)
			{
				UE_LOG(LogAtlasFXSetup, Display, TEXT("        %s = %s"), *TagValue.Key.ToString(), *TagValue.Value.AsString());
			}
		}

		// 诊断二：引擎那份基础配置长什么样（它的 section 用哪些 filter 类，是扩展该照抄的结构）。
		if (const UTaggedAssetBrowserConfiguration* EngineConfig = LoadObject<UTaggedAssetBrowserConfiguration>(nullptr, TEXT("/Niagara/DefaultAssets/TABC_SystemWizard.TABC_SystemWizard")))
		{
			const UHierarchyRoot* EngineRoot = EngineConfig->FilterRoot;
			UE_LOG(LogAtlasFXSetup, Display, TEXT("  诊断 · 引擎向导配置：ProfileName=%s / bIsExtension=%d / section=%d / child=%d"),
				*EngineConfig->ProfileName.ToString(), EngineConfig->bIsExtension ? 1 : 0,
				EngineRoot ? EngineRoot->GetSectionData().Num() : -1,
				EngineRoot ? EngineRoot->GetChildren().Num() : -1);

			if (EngineRoot != nullptr)
			{
				for (const UHierarchySection* Section : EngineRoot->GetSectionData())
				{
					UE_LOG(LogAtlasFXSetup, Display, TEXT("        section '%s'（类 %s）"),
						*Section->GetSectionName().ToString(), *Section->GetClass()->GetName());

					if (const UTaggedAssetBrowserSection* TaggedSection = Cast<UTaggedAssetBrowserSection>(Section))
					{
						for (const UTaggedAssetBrowserFilterBase* SectionFilter : TaggedSection->Filters)
						{
							UE_LOG(LogAtlasFXSetup, Display, TEXT("            filter %s"),
								SectionFilter ? *SectionFilter->GetClass()->GetName() : TEXT("<null>"));
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogAtlasFXSetup, Warning, TEXT("  诊断 · 读不到引擎向导配置 /Niagara/DefaultAssets/TABC_SystemWizard"));
		}

		// 诊断三：引擎自己的 UAT 标签在 AR 里查得到吗？
		// 引擎的「Template / Learning Content / Lightweight」分类用的就是 UTaggedAssetBrowserFilter_UserAssetTag，
		// 如果连它们都查不到，说明这条 AR 查询路径本身在这个引擎版本里就是断的，得改用 Directories 过滤。
		for (const TCHAR* ProbeTag : { TEXT("Template"), TEXT("Lightweight"), TEXT("LearningContent") })
		{
			FARFilter ProbeFilter;
			ProbeFilter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
			ProbeFilter.bRecursiveClasses = true;
			ProbeFilter.TagsAndValues.Add(FName(*FString::Printf(TEXT("%s%s"), *UE::UserAssetTags::UAT_METADATA_PREFIX, ProbeTag)), TOptional<FString>());

			TArray<FAssetData> ProbeFound;
			IAssetRegistry::Get()->GetAssets(ProbeFilter, ProbeFound);

			UE_LOG(LogAtlasFXSetup, Display, TEXT("  诊断 · AR 按 UAT.%s 查到 %d 个 Niagara 系统"), ProbeTag, ProbeFound.Num());
			for (int32 Index = 0; Index < FMath::Min(ProbeFound.Num(), 3); ++Index)
			{
				UE_LOG(LogAtlasFXSetup, Display, TEXT("        · %s"), *ProbeFound[Index].GetSoftObjectPath().ToString());
			}
		}

		// 诊断五：扩展资产能不能被对话框那套查询找到（GetExtensionAssets 用的就是这两个键）。
		{
			FARFilter ExtFilter;
			ExtFilter.ClassPaths.Add(UTaggedAssetBrowserConfiguration::StaticClass()->GetClassPathName());
			ExtFilter.bRecursiveClasses = true;
			ExtFilter.TagsAndValues.Add(FName(TEXT("bIsExtension")), FString(TEXT("True")));
			ExtFilter.TagsAndValues.Add(FName(TEXT("ProfileName")), AtlasFXSetup::WizardProfileName.ToString());

			TArray<FAssetData> ExtFound;
			IAssetRegistry::Get()->GetAssets(ExtFilter, ExtFound);

			UE_LOG(LogAtlasFXSetup, Display, TEXT("  诊断 · AR 里 bIsExtension=True + ProfileName=%s 的扩展资产：%d 个"),
				*AtlasFXSetup::WizardProfileName.ToString(), ExtFound.Num());
			for (const FAssetData& AssetData : ExtFound)
			{
				UE_LOG(LogAtlasFXSetup, Display, TEXT("        · %s"), *AssetData.GetSoftObjectPath().ToString());
			}
		}

		// 诊断四：退路 —— 按目录查（对话框里的 Directories 过滤走的就是这条路）。
		{
			FARFilter DirFilter;
			DirFilter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
			DirFilter.bRecursiveClasses = true;
			DirFilter.PackagePaths.Add(FName(TEXT("/AtlasFX/Templates")));
			DirFilter.bRecursivePaths = true;

			TArray<FAssetData> DirFound;
			IAssetRegistry::Get()->GetAssets(DirFilter, DirFound);

			UE_LOG(LogAtlasFXSetup, Display, TEXT("  诊断 · AR 按目录 /AtlasFX/Templates 查到 %d 个 Niagara 系统"), DirFound.Num());
			for (const FAssetData& AssetData : DirFound)
			{
				UE_LOG(LogAtlasFXSetup, Display, TEXT("        · %s"), *AssetData.GetSoftObjectPath().ToString());
			}
		}
	}

	UE_LOG(LogAtlasFXSetup, Display, TEXT("=== AtlasFX 模板配置：结束（分类资产 %s / 模板标签 %s）==="),
		bConfigOk ? TEXT("OK") : TEXT("失败"),
		bTagsOk   ? TEXT("OK") : TEXT("失败"));

	return (bConfigOk && bTagsOk) ? 0 : 1;
}

bool UAtlasFXSetupCommandlet::BuildWizardConfig()
{
	using namespace AtlasFXSetup;

	UPackage* Package = LoadPackage(nullptr, ConfigPackageName, LOAD_Quiet | LOAD_NoVerify);
	if (Package == nullptr)
	{
		Package = CreatePackage(ConfigPackageName);
	}
	if (Package == nullptr)
	{
		UE_LOG(LogAtlasFXSetup, Error, TEXT("建不出包：%s"), ConfigPackageName);
		return false;
	}

	UTaggedAssetBrowserConfiguration* Config = FindObject<UTaggedAssetBrowserConfiguration>(Package, ConfigAssetName);
	if (Config == nullptr)
	{
		Config = NewObject<UTaggedAssetBrowserConfiguration>(Package, ConfigAssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Config);
		UE_LOG(LogAtlasFXSetup, Display, TEXT("新建配置资产 %s"), *ToObjectPath(ConfigPackageName));
	}
	else
	{
		UE_LOG(LogAtlasFXSetup, Display, TEXT("复用已有配置资产 %s"), *ToObjectPath(ConfigPackageName));
	}

	// 扩展资产的两条必填项：挂到哪个基础配置、以及「我是扩展」。
	Config->ProfileName = WizardProfileName;
	Config->bIsExtension = true;

	UTaggedAssetBrowserFilterRoot* Root = Cast<UTaggedAssetBrowserFilterRoot>(Config->FilterRoot);
	if (Root == nullptr)
	{
		Root = NewObject<UTaggedAssetBrowserFilterRoot>(Config, TEXT("FilterRoot"), RF_Public | RF_Transactional);
		Config->FilterRoot = Root;
	}

	// 幂等：整个层级重建，避免重复跑越加越多。
	Root->EmptyAllData();

	// ① 分类本身（对话框左侧那一栏）。注意只能进 Sections，不能挂成 child ——
	//    IsDataValid 会把 child 逐个拿去比对 ExtensionFilterClasses，UTaggedAssetBrowserSection 不在名单里。
	UTaggedAssetBrowserSection* Section = NewObject<UTaggedAssetBrowserSection>(Root, NAME_None, RF_Transactional);
	Section->SetSectionName(TagName);
	Section->SetTooltip(FText::FromString(TEXT("CrossingVoid 2D 特效模板（AtlasFX 图集播放：精灵渲染器 / 网格渲染器）")));
	Section->bAllowSectionMerge = true;
	Section->IconData.bUseTextureForIcon = false;
	Section->IconData.StyleName = FName(TEXT("ClassIcon.NiagaraSystem"));
	Root->GetSectionDataMutable().Add(Section);

	// ② 分类激活时显示哪些资产 —— 按目录过滤。
	//    为什么不用 UTaggedAssetBrowserFilter_UserAssetTag：那个过滤器的 AR 预过滤写的是
	//    Filter.TagsAndValues.Add("UAT.<标签>")（TaggedAssetBrowser_CommonFilters.cpp:63-68），
	//    而 UAT.* 根本进不了 Asset Registry —— 实测连引擎自己的 UAT.Template / UAT.Lightweight
	//    都查到 0 个（见本命令的诊断日志），所以那条路永远筛不出东西。
	//    目录过滤走 Filter.PackagePaths，是 AR 一定有的数据，实测能查到两个模板。
	UTaggedAssetBrowserFilter_Directories* SectionFilter = NewObject<UTaggedAssetBrowserFilter_Directories>(Section, NAME_None, RF_Transactional);
	SectionFilter->DirectoryPaths.Add(FDirectoryPath(FString(TEXT("/AtlasFX/Templates"))));
	SectionFilter->FilterName = TagName;
	Section->Filters.Add(SectionFilter);

	// ③ 层级树里也挂一份（与引擎基础配置的结构一致：左侧列表就是 root 的 children），并把它关联到分类上。
	UTaggedAssetBrowserFilter_Directories* RootFilter = Root->AddChild<UTaggedAssetBrowserFilter_Directories>();
	RootFilter->DirectoryPaths.Add(FDirectoryPath(FString(TEXT("/AtlasFX/Templates"))));
	RootFilter->FilterName = TagName;
	FDataHierarchyElementMetaData_SectionAssociation* Association = RootFilter->FindOrAddMetaDataOfType<FDataHierarchyElementMetaData_SectionAssociation>();
	Association->Section = Section;

	if (SaveAssetPackage(Package, Config, ConfigPackageName) == false)
	{
		return false;
	}

	UE_LOG(LogAtlasFXSetup, Display, TEXT("已保存分类资产：section=%s / ProfileName=%s / bIsExtension=true"),
		*TagName.ToString(), *WizardProfileName.ToString());
	return true;
}

bool UAtlasFXSetupCommandlet::SeedRendererMaterialParameters()
{
	using namespace AtlasFXSetup;

	bool bOk = true;

	// 找系统用户参数里第一个 AtlasFX 的 DI —— 按**类型**找而不是按名字，因为使用者可能把 Atlas 改名。
	auto FindAtlasDataInterfaceParameter = [](const UNiagaraSystem* InSystem) -> FName
	{
		TArray<FNiagaraVariable> UserParameters;
		InSystem->GetExposedParameters().GetParameters(UserParameters);

		for (const FNiagaraVariable& Variable : UserParameters)
		{
			if (Variable.GetType().GetClass() == UNiagaraDataInterfaceSpriteAtlas::StaticClass())
			{
				return Variable.GetName();
			}
		}

		return NAME_None;
	};

	for (const TCHAR* TemplatePackage : TemplatePackages)
	{
		const FString ObjectPath = ToObjectPath(TemplatePackage);

		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath);
		if (System == nullptr)
		{
			UE_LOG(LogAtlasFXSetup, Error, TEXT("找不到模板资产：%s"), *ObjectPath);
			bOk = false;
			continue;
		}

		UPackage* Package = System->GetPackage();
		int32 SeededCount = 0;

		const FName DiParameterName = FindAtlasDataInterfaceParameter(System);
		if (DiParameterName.IsNone())
		{
			UE_LOG(LogAtlasFXSetup, Warning,
				TEXT("%s：没找到 AtlasFX 的 DI 用户参数，渲染器的「材质参数 → 属性绑定」这条会跳过（纹理槽照补）。"),
				*ObjectPath);
		}

		// 用对象迭代器按包过滤找渲染器，绕开 FVersionedNiagaraEmitterData 那套版本化句柄 API
		// （5.7 起在改名：UE_DEPRECATED 提示改用 FVersionedNiagaraEmitterBase）。
		// 渲染器对象一定住在模板包里，按 GetOutermost() 过滤最稳。
		for (TObjectIterator<UNiagaraRendererProperties> It; It; ++It)
		{
			UNiagaraRendererProperties* Renderer = *It;
			if (Renderer->GetOutermost() != Package)
			{
				continue;
			}

			// MaterialParameters 是**各渲染器自己的**成员（不在 UNiagaraRendererProperties 基类上），
			// 所以得按类型分别取（NiagaraMeshRendererProperties.h:380 / NiagaraSpriteRendererProperties.h:365）。
			FNiagaraRendererMaterialParameters* Parameters = nullptr;
			if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				Parameters = &MeshRenderer->MaterialParameters;
			}
			else if (UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Parameters = &SpriteRenderer->MaterialParameters;
			}

			if (Parameters == nullptr)
			{
				continue;
			}

			const bool bHasTextureSlot = Parameters->TextureParameters.ContainsByPredicate(
				[](const FNiagaraRendererMaterialTextureParameter& Entry)
				{
					return Entry.MaterialParameterName == SheetMaterialParameterName;
				});

			// 属性绑定：Sheet ← DI 的 ResolvedTexture 子变量。
			// 这条链是引擎自带的能力（不是我们发明的）：
			//   NiagaraRenderer.cpp:509-577 每帧遍历 MaterialParameters.AttributeBindings，
			//   遇到「基变量是 DI + 子变量类型是 UTexture」就 MatDyn->SetTextureParameterValue；
			//   取值的实现是 FNiagaraEmitterInstance::GetBoundRendererValue_GT
			//   （NiagaraEmitterInstance.cpp:110-144）→ DI 的 GetExposedVariableValue。
			// 于是使用者只要选 Flipbook，材质自动拿到图集 —— 不用建材质实例、不用填纹理槽。
			const bool bHasBinding = Parameters->AttributeBindings.ContainsByPredicate(
				[](const FNiagaraMaterialAttributeBinding& Binding)
				{
					return Binding.MaterialParameterName == SheetMaterialParameterName;
				});

			if (bHasTextureSlot && bHasBinding)
			{
				continue;
			}

			Renderer->Modify();
			int32 SeededForRenderer = 0;

			if (!bHasTextureSlot)
			{
				FNiagaraRendererMaterialTextureParameter& Entry = Parameters->TextureParameters.AddDefaulted_GetRef();
				Entry.MaterialParameterName = SheetMaterialParameterName;
				Entry.Texture = nullptr;   // 故意留空：默认走下面的属性绑定；想换贴图再在这里填
				++SeededForRenderer;
			}

			if (!bHasBinding && !DiParameterName.IsNone())
			{
				FNiagaraMaterialAttributeBinding& Binding = Parameters->AttributeBindings.AddDefaulted_GetRef();
				Binding.MaterialParameterName = SheetMaterialParameterName;
				Binding.NiagaraVariable = FNiagaraVariableBase(FNiagaraTypeDefinition(UNiagaraDataInterfaceSpriteAtlas::StaticClass()), DiParameterName);
				// 用户参数没有 emitter 别名要解析（CacheValues 对 User.* 也是恒等变换），
				// 这里直接把解析结果写成同一个变量；不调 CacheValues 是因为它要 UNiagaraEmitterBase*。
				Binding.ResolvedNiagaraVariable = Binding.NiagaraVariable;
				// 子变量名必须和 DI 的 GetExposedVariables 里给的一致（"ResolvedTexture"），
				// 类型必须是 UTexture —— NiagaraRenderer.cpp:561 认的就是 GetUTextureDef()。
				Binding.NiagaraChildVariable = FNiagaraVariableBase(FNiagaraTypeDefinition(UTexture::StaticClass()), TEXT("ResolvedTexture"));
				++SeededForRenderer;
			}

			SeededCount += SeededForRenderer;
		}

		if (SeededCount > 0 && SaveAssetPackage(Package, System, TemplatePackage) == false)
		{
			bOk = false;
			continue;
		}

		UE_LOG(LogAtlasFXSetup, Display,
			TEXT("渲染器材质参数：%s 补了 %d 项（Sheet 纹理槽 + Sheet ← %s.ResolvedTexture 属性绑定；已有的跳过）"),
			*ObjectPath, SeededCount, DiParameterName.IsNone() ? TEXT("<无 DI>") : *DiParameterName.ToString());
	}

	return bOk;
}

bool UAtlasFXSetupCommandlet::TagTemplates()
{
	using namespace AtlasFXSetup;

	bool bOk = true;

	for (const TCHAR* TemplatePackage : TemplatePackages)
	{
		const FString ObjectPath = ToObjectPath(TemplatePackage);

		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath);
		if (System == nullptr)
		{
			UE_LOG(LogAtlasFXSetup, Error, TEXT("找不到模板资产：%s"), *ObjectPath);
			bOk = false;
			continue;
		}

		UPackage* Package = System->GetPackage();

		// 清掉改名前的旧标签，免得旧分类跟着一起冒出来。
		// （不用 UE::UserAssetTags::RemoveUserAssetTag —— 它和 GetUATPrefixedTag 一样没导出，链接不上。）
		const FName StaleTag(*FString::Printf(TEXT("%s%s"), *UE::UserAssetTags::UAT_METADATA_PREFIX, *LegacyTagName.ToString()));
		if (Package->GetMetaData().RootMetaDataMap.Remove(StaleTag) > 0)
		{
			UE_LOG(LogAtlasFXSetup, Display, TEXT("清掉旧标签 %s：%s"), *StaleTag.ToString(), *ObjectPath);
		}

		// 内部会先查重，重复跑不会写两遍。
		UE::UserAssetTags::AddUserAssetTag(System, TagName);

		if (SaveAssetPackage(Package, System, TemplatePackage) == false)
		{
			bOk = false;
			continue;
		}

		UE_LOG(LogAtlasFXSetup, Display, TEXT("已打标签：%s → %s"), *TagName.ToString(), *ObjectPath);
	}

	return bOk;
}

void UAtlasFXSetupCommandlet::RefreshAssetRegistryTags()
{
	using namespace AtlasFXSetup;

	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		return;
	}

	// 扩展配置靠 bIsExtension / ProfileName 这两个 AssetRegistrySearchable 属性被对话框找出来，
	// 模板靠包元数据里的 UAT.<标签名>。两者都得在 AR 里查得到。
	if (UTaggedAssetBrowserConfiguration* Config = LoadObject<UTaggedAssetBrowserConfiguration>(nullptr, *ToObjectPath(ConfigPackageName)))
	{
		Registry->AssetUpdateTags(Config, EAssetRegistryTagsCaller::FullUpdate);
	}

	for (const TCHAR* TemplatePackage : TemplatePackages)
	{
		if (UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *ToObjectPath(TemplatePackage)))
		{
			Registry->AssetUpdateTags(System, EAssetRegistryTagsCaller::FullUpdate);
		}
	}
}

namespace
{
	/** 把一张蓝图的所有图 / 节点 / 连线打到日志里（只用 Engine 里的 UEdGraph* 类型，不必依赖 BlueprintGraph 模块）。 */
	void DumpBlueprintGraph(const UBlueprint* Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);

		UE_LOG(LogAtlasFXSetup, Display, TEXT("蓝图连线 dump · %s（%d 张图）"), *Blueprint->GetName(), Graphs.Num());

		for (const UEdGraph* Graph : Graphs)
		{
			if (Graph == nullptr)
			{
				continue;
			}

			UE_LOG(LogAtlasFXSetup, Display, TEXT("  图 %s（%d 个节点）"), *Graph->GetName(), Graph->Nodes.Num());

			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node == nullptr)
				{
					continue;
				}

				UE_LOG(LogAtlasFXSetup, Display, TEXT("    节点 [%s] %s"),
					*Node->GetClass()->GetName(),
					*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr || Pin->LinkedTo.Num() == 0)
					{
						continue;
					}

					FString Links;
					for (const UEdGraphPin* Linked : Pin->LinkedTo)
					{
						const UEdGraphNode* LinkedNode = Linked ? Linked->GetOwningNodeUnchecked() : nullptr;
						Links += FString::Printf(TEXT("[%s.%s] "),
							LinkedNode ? *LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("?"),
							Linked ? *Linked->PinName.ToString() : TEXT("?"));
					}

					UE_LOG(LogAtlasFXSetup, Display, TEXT("      %s (%s) -> %s"),
						*Pin->PinName.ToString(),
						Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"),
						*Links);
				}
			}
		}
	}
}

void UAtlasFXSetupCommandlet::DumpPaperZDPreviewTargets()
{
	// ① 播放器上那个 private 的 RegisteredRenderComponent（PaperZDAnimPlayer.h:52，UPROPERTY + 无 getter）。
	//    预览扩展靠它认出「这是预览播放器」并拿到要挂特效的组件，反射不到就整个扩展失效。
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
			UPaperZDAnimPlayer::StaticClass(), TEXT("RegisteredRenderComponent"));
		UE_LOG(LogAtlasFXSetup, Display, TEXT("PaperZD 预览自查 · 播放器 RegisteredRenderComponent 反射：%s"),
			Property ? TEXT("OK") : TEXT("失败"));
	}

	// ② 用户实际在用的那条序列（Misaka 的 DefAtk，通知轨上挂的就是 TxSpawn）。
	UPaperZDAnimSequence* Sequence = LoadObject<UPaperZDAnimSequence>(
		nullptr, TEXT("/Game/GameActor2D/Misaka/AnimSequences/DefAtk.DefAtk"));
	if (Sequence == nullptr)
	{
		UE_LOG(LogAtlasFXSetup, Warning, TEXT("PaperZD 预览自查 · 读不到序列 /Game/GameActor2D/Misaka/AnimSequences/DefAtk"));
		return;
	}

	const TArray<UPaperZDAnimNotify_Base*>& Notifies = Sequence->GetAnimNotifies();
	UE_LOG(LogAtlasFXSetup, Display, TEXT("PaperZD 预览自查 · 序列 %s（%.2f 秒）有 %d 条通知："),
		*Sequence->GetName(), Sequence->GetTotalDuration(), Notifies.Num());

	for (const UPaperZDAnimNotify_Base* Notify : Notifies)
	{
		FAtlasFXPaperZDPreview::DumpNotifyDiagnostics(Notify);
	}

	// ③ 通知蓝图的连线 dump：核对 TxSpawn 里「选择」节点到底比较 Scale 的哪两个分量
	//    （可读代码导出把所有 Scale 取值都收敛成同一个名字「Get Scale」，看不出分量）。
	for (const UPaperZDAnimNotify_Base* Notify : Notifies)
	{
		const UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Notify ? Notify->GetClass() : nullptr);
		const UBlueprint* Blueprint = GeneratedClass ? Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy) : nullptr;
		if (Blueprint != nullptr)
		{
			DumpBlueprintGraph(Blueprint);
		}
	}
}

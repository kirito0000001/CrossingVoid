// AtlasFXEditor —— 只跑在编辑器里的收尾工具（模板打标签、造「创建 Niagara 系统」向导分类）。
//
// 为什么单开一个模块：
//   * AtlasFX 是 Runtime 模块，要跟着游戏打包，绝不能依赖 UnrealEd / UserAssetTagsEditor；
//   * 自定义 DataInterface 必须留在 Runtime 模块（Editor 模块打包后加载不到，DI 反序列化会失败），
//     所以这里只放编辑器工具，不碰任何运行时资产。

using UnrealBuildTool;

public class AtlasFXEditor : ModuleRules
{
	public AtlasFXEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",              // UPackage::SavePackage / FAssetRegistryModule::AssetCreated
			"AssetRegistry",         // IAssetRegistry::AssetUpdateTags
			"DataHierarchyEditor",   // UHierarchyRoot / UHierarchySection / FDataHierarchyElementMetaData_SectionAssociation
			"UserAssetTagsEditor",   // UTaggedAssetBrowserConfiguration + UE::UserAssetTags
			"Niagara",               // UNiagaraSystem
		});
	}
}

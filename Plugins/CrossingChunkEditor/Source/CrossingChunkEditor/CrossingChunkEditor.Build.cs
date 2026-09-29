using UnrealBuildTool;

public class CrossingChunkEditor : ModuleRules
{
	public CrossingChunkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"ApplicationCore",
			"AssetRegistry",
			"UnrealEd",
			"ContentBrowser",
			"Json",
			"ToolMenus",
			// 为了 IPluginManager：打包脚本跟着插件走，路径得从插件目录推（见 GetScriptPath）
			"Projects"
		});
	}
}

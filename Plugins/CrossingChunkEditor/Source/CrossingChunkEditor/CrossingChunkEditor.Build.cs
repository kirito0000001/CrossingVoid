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
			"ToolMenus"
		});
	}
}

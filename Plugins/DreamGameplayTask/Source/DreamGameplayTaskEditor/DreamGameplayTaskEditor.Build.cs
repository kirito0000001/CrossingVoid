using UnrealBuildTool;

public class DreamGameplayTaskEditor : ModuleRules
{
	public DreamGameplayTaskEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AssetRegistry",
				"DreamGameplayTask", 
				"ClassViewer",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Projects",
				"AssetTools",
				"ContentBrowser",
				"UnrealEd",
				"DeveloperSettings",
				"InputCore", 
				"Blutility"
			}
		);
	}
}
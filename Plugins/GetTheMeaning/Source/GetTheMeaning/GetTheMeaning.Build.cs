// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GetTheMeaning : ModuleRules
{
	public GetTheMeaning(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"UnrealEd",
				"AssetRegistry",
				"ApplicationCore",
				"BlueprintGraph",
				"Kismet",
				"KismetCompiler",
				"EditorStyle",
				"Projects",
				"InputCore",
				"EnhancedInput",
				"Json",
				"MovieScene",
				"ToolMenus",
				"UMG",
				"UMGEditor",
				"ContentBrowser",
				"LevelEditor",
				"GraphEditor",
				"AssetManagerEditor",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}

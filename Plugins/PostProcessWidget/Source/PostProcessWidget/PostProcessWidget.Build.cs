// Copyright Qibo Pang 2023. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PostProcessWidget : ModuleRules
{
	public PostProcessWidget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);
        PrivateIncludePaths.AddRange(
                new string []{
                    // �� add other private include paths required here ��
                    Path.Combine(GetModuleDirectory("Renderer"), "Internal"),
                }
                );

        PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Projects",
                "Slate",
                "SlateCore"
            });

        PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UMG",
                "Renderer",
                "RenderCore",
                "SlateRHIRenderer",
                "RHI",
                "RHICore",
                "MovieSceneCapture",
                "Projects",
                "DeveloperSettings",
                "ApplicationCore",
                "Renderer",

            });
    }
}

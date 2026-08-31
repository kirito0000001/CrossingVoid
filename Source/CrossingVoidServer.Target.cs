// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class CrossingVoidServerTarget : TargetRules
{
    public CrossingVoidServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		BuildEnvironment = TargetBuildEnvironment.Unique;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		ExtraModuleNames.Add("CrossingVoid");
	}
}

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

		// 和 CrossingVoid.Target.cs 同理：Shipping 默认把日志整个关掉
		// （Core\Public\Misc\Build.h 里 USE_LOGGING_IN_SHIPPING=0），
		// 服务器崩了只会留一个退出码，什么都看不到 —— 2026-09-19 实测就是这个下场。
		// Development 配置本来就带日志，这个开关只影响 Shipping。
		// 打包脚本给服务器的就是 -serverconfig=Shipping，所以要开。
		bUseLoggingInShipping = true;

		// 同理，Shipping 默认也把控制台关掉：Build.h:214 `ALLOW_CONSOLE_IN_SHIPPING=0`，
		// 而 Shipping 下 `ALLOW_CONSOLE = ALLOW_CONSOLE_IN_SHIPPING`（Build.h:348）。
		// 这个开关会让 UBT 加 `ALLOW_CONSOLE_IN_SHIPPING=1`（UEBuildTarget.cs:6465），
		// 把这几处 `#if ALLOW_CONSOLE` 的代码编回来：
		//   · UConsole / GameViewportClient 建控制台（GameViewportClient.cpp:2807、Console.cpp）
		//   · APlayerController::ConsoleKey / SendToConsole（PlayerController.cpp:3956 / 3968）
		// 服务器的蓝图逻辑要用控制台去开新进程，所以必须打开。
		bUseConsoleInShipping = true;

		ExtraModuleNames.Add("CrossingVoid");
	}
}

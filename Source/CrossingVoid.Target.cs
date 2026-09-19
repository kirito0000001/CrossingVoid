// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class CrossingVoidTarget : TargetRules
{
	public CrossingVoidTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		// 让 Shipping 包也写日志。
		// 默认是 false（Core\Public\Misc\Build.h 里 USE_LOGGING_IN_SHIPPING=0），
		// 结果是打包出来的客户端【一个字都不写】：
		//   · -ABSLOG=/LOG= 都救不了（日志设备根本没建）
		//   · %LOCALAPPDATA%\<项目>\Saved\Logs\ 是空的
		//   · 崩了只能看到一个退出码 1，没法定位
		// 打开后 Shipping 会写 <项目>\Saved\Logs\<项目>.log（见 Tools\打包说明.md）。
		// 代价：UEBuildTarget.cs:6458 会加 USE_LOGGING_IN_SHIPPING=1，
		// 且该属性带 [RequiresUniqueBuildEnvironment] → 这个目标要独立编译环境（首次会慢一些）。
		bUseLoggingInShipping = true;

		// 控制台【刻意不开】—— 别顺手加回来：
		// 客户端只负责把信号发给服务端，真正用「控制台 + 蓝图」拉起新进程（开房间）的是服务端，
		// 所以控制台只给服务器开了（见 CrossingVoidServer.Target.cs）。
		// 客户端开了等于给玩家一个能敲命令的入口（Windows 上按 ~ 就出来），没必要冒这个风险。
		// 链路细节见 CrossingVoidServer.Target.cs 和 Tools\打包说明.md。

		ExtraModuleNames.Add("CrossingVoid");
	}
}

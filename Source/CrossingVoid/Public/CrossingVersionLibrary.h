// 工程版本号（内容版本 / 网络兼容版本）
//
// 背景（2026-10-27 查引擎源码确认）：
//   引擎握手时比对的是 FNetworkVersion::GetLocalNetworkVersion()，
//   它 = CRC32("工程名 + 项目版本 + 引擎 NetCL + 网络协议版本")，而且判断是【严格相等】。
//   那个"项目版本"读的就是 Config\DefaultGame.ini 里
//   [/Script/EngineSettings.GeneralProjectSettings] ProjectVersion
//   （UnrealEngine.cpp:2539-2541 原文注释："Make sure networking checksum has access to project version"）。
//
//   所以：
//     · ProjectVersion 一变 → 旧客户端在 connect 阶段就被引擎拒连（不是弹提示，是连不上）；
//     · 内容版本（0.6.1 这种）不能放进去，否则每次发版都变成"全网旧包连不上"。
//
// 这里把两者分开暴露给蓝图：
//     · GetGameVersion()             → 内容版本，走 Config\DefaultGame.ini 的 [CrossingVersion] GameVersion，
//                                     打包时由 CrossingChunkEditor 面板写入，用来做"客户端 vs 服务器"软校验；
//     · GetNetworkCompatibleVersion() → 网络兼容版本（ProjectVersion），只做诊断显示，别改成日常发版号。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CrossingVersionLibrary.generated.h"

UCLASS()
class CROSSINGVOID_API UCrossingVersionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 内容版本（如 "0.6.1"）。
	 * 来源：Config\DefaultGame.ini → [CrossingVersion] GameVersion（打包时由插件面板的「玩家版本」写入）。
	 * 老包没有这一段时，退回读 GeneralProjectSettings.ProjectVersion。
	 */
	UFUNCTION(BlueprintPure, Category="版本", DisplayName="获取内容版本")
	static FString GetGameVersion();

	/**
	 * 网络兼容版本（GeneralProjectSettings.ProjectVersion）。
	 * 它参与引擎的 NetCL：两端不一致 = 直接连不上。默认是空的（引擎退回 "1.0.0"），
	 * 只在真的改了网络协议/包结构时才抬它。
	 */
	UFUNCTION(BlueprintPure, Category="版本", DisplayName="获取网络兼容版本")
	static FString GetNetworkCompatibleVersion();
};

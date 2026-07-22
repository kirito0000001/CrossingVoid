// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;

/**
 * 将蓝图图结构导出为可读文本，便于 AI 或人工理解逻辑，并作为 C++ 实现参考。
 * - 按执行顺序（沿 exec 连线）输出，体现控制流；
 * - 输出引脚数据来源（数据流），便于理解参数与条件；
 * - 使用伪代码/结构化格式，便于答辩讲述与 C++ 对照。
 */
class GETTHEMEANING_API FBlueprintToTextExporter
{
public:
	/** 将单个蓝图导出为可读文本（含执行顺序与数据流）。若蓝图无效则返回空字符串。 */
	static FString ExportBlueprintToText(UBlueprint* Blueprint);

	/** 将单个蓝图导出为机器可读 JSON（元信息、变量、函数/事件、节点和连线）。若蓝图无效则返回空字符串。 */
	static FString ExportBlueprintToJson(UBlueprint* Blueprint);

	/** 将单个图按执行顺序 + 数据流导出（主入口，用于每个 Graph）。 */
	static FString ExportGraphToText(class UEdGraph* Graph, const FString& GraphName);

	/** 将单个节点转为一行描述（节点类型 + 标题/名称）。 */
	static FString ExportNodeToText(class UEdGraphNode* Node);

	/** 返回节点的简短标识，用于在“数据来自 XXX”中引用。 */
	static FString GetNodeShortLabel(class UEdGraphNode* Node);
};

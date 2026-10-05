// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/**
 * 将材质（Material / Material Instance）导出为“AI 可读文档”：
 * - 固定顺序的分段（Header / Summary / Pass / Params / Refs / Textures / PseudoHLSL ...）
 * - 第一阶段以“可理解、可复写”的伪 HLSL 为目标，而非引擎最终编译产物。
 */
class GETTHEMEANING_API FMaterialToTextExporter
{
public:
	/** 导出材质接口（UMaterial/UMaterialInstance*）为可读文档文本。无效则返回空字符串。 */
	static FString ExportMaterialToText(UMaterialInterface* MaterialInterface);

	/** 导出材质函数（Material Function）为可读文档文本。无效则返回空字符串。 */
	static FString ExportMaterialFunctionToText(class UMaterialFunctionInterface* MaterialFunction);
};


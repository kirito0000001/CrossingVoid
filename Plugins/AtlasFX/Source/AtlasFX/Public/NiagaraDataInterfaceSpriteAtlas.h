// AtlasFX —— 数据层：把「任意矩形（非等分、trim 过）精灵图集」的帧表交给 Niagara
//
// 坐标约定：**像素、左上原点、x 向右 y 向下**（和 `Defatk_sequence.json` / `.paper2dsprites` 一致）。
// 用法：建为 System/Emitter 的 User Parameter，模块和材质用它取当前帧的两套矩形。
#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"

#include "NiagaraDataInterfaceSpriteAtlas.generated.h"

class UPaperFlipbook;
class UTexture2D;
enum class ENiagaraSimTarget : uint8;
struct FPropertyChangedEvent;

/**
 * 精灵图集帧表。
 *
 * 帧表在编辑器里烘好、存在**普通（非编辑器专用）数组属性**上，所以打包后的游戏直接可用，
 * 运行时不需要 UPaperFlipbook / UPaperSprite / Paper2D 参与。
 *
 * 第一步只支持 CPU sim（`CanExecuteOnTarget` 只放 CPUSim）：GPU 需要 HLSL + Proxy +
 * AddShaderSourceDirectoryMapping，留第二步。
 */
UCLASS(EditInlineNew, Category = "Sprite", CollapseCategories, meta = (DisplayName = "Sprite Atlas"), MinimalAPI)
class UNiagaraDataInterfaceSpriteAtlas : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	// ------------------------------------------------------------------ 数据源

	/** 首选：绑一个 Flipbook，编辑期自动烘表（改完点 Refresh From Source）。 */
	UPROPERTY(EditAnywhere, Category = "Atlas", meta = (DisplayPriority = 1))
	TObjectPtr<UPaperFlipbook> Flipbook;

	/** 兜底：不绑 Flipbook 时自己指定图集贴图（只用来取尺寸）+ 下面的矩形数组。 */
	UPROPERTY(EditAnywhere, Category = "Atlas|Fallback", meta = (DisplayPriority = 2))
	TObjectPtr<UTexture2D> AtlasTexture;

	/** 兜底：图集矩形，每项 = (x, y, w, h) 像素。 */
	UPROPERTY(EditAnywhere, Category = "Atlas|Fallback", meta = (DisplayPriority = 2))
	TArray<FVector4> ManualFrameRects;

	/** 兜底：画布矩形，每项 = (x, y, w, h) 像素；留空则等于图集矩形。 */
	UPROPERTY(EditAnywhere, Category = "Atlas|Fallback", meta = (DisplayPriority = 2))
	TArray<FVector4> ManualCanvasRects;

	/** 兜底：原始画布尺寸（像素）。 */
	UPROPERTY(EditAnywhere, Category = "Atlas|Fallback", meta = (DisplayPriority = 2))
	FIntPoint ManualCanvasSize = FIntPoint(928, 640);

	/** 兜底：帧率（工具只在 grid 模式的 sequence.json 里写 Fps，这里手填）。 */
	UPROPERTY(EditAnywhere, Category = "Atlas|Fallback", meta = (DisplayPriority = 2))
	float ManualFps = 12.0f;

	/** 取矩形时四周内缩多少**像素**：吃掉图集 padding，避免双线性过滤把邻帧采进来。0 = 不内缩。 */
	UPROPERTY(EditAnywhere, Category = "Atlas", meta = (ClampMin = "0.0", DisplayPriority = 0))
	float RectInsetPixels = 0.5f;

	// ------------------------------------------------------------------ 烘好的表（只读）

	/** 每帧在图集贴图里的矩形 (x, y, w, h) 像素。 */
	UPROPERTY(VisibleAnywhere, Category = "Atlas|Baked")
	TArray<FVector4> FrameRects;

	/** 每帧在原始未裁剪画布里的矩形 (x, y, w, h) 像素。 */
	UPROPERTY(VisibleAnywhere, Category = "Atlas|Baked")
	TArray<FVector4> CanvasRects;

	/** 原始画布尺寸（像素）——「画布还原」模式用。 */
	UPROPERTY(VisibleAnywhere, Category = "Atlas|Baked")
	FIntPoint CanvasSize = FIntPoint(928, 640);

	/** 图集贴图尺寸（像素）。 */
	UPROPERTY(VisibleAnywhere, Category = "Atlas|Baked")
	FIntPoint TextureSize = FIntPoint(1, 1);

	/** 帧率（来自 Flipbook。工具在 pack 模式不写 Fps，所以这是唯一的来源）。 */
	UPROPERTY(VisibleAnywhere, Category = "Atlas|Baked")
	float Fps = 12.0f;

	// ------------------------------------------------------------------ 接口

	/** 从 Flipbook / 手工数据重烘帧表。编辑器详情面板上是个按钮；Python 里 `refresh_from_source()`。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Atlas")
	void RefreshFromSource();

	// ~UObject
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ~UNiagaraDataInterface
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif

protected:
	// VM 实现（每个函数一个，避免 lambda 里塞逻辑）
	void GetFrameCountVM(FVectorVMExternalFunctionContext& Context);
	void GetAtlasRectVM(FVectorVMExternalFunctionContext& Context);
	void GetCanvasRectVM(FVectorVMExternalFunctionContext& Context);
	void GetCanvasSizeVM(FVectorVMExternalFunctionContext& Context);
	void GetTextureSizeVM(FVectorVMExternalFunctionContext& Context);
	void GetFpsVM(FVectorVMExternalFunctionContext& Context);

	/** 取第 Index 帧的矩形（越界 clamp 到端点），并已应用 RectInsetPixels。 */
	FVector4 GetRectClamped(const TArray<FVector4>& Rects, int32 Index) const;

private:
	static const FName GetFrameCountName;
	static const FName GetAtlasRectName;
	static const FName GetCanvasRectName;
	static const FName GetCanvasSizeName;
	static const FName GetTextureSizeName;
	static const FName GetFpsName;
};

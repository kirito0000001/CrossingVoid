// AtlasFX —— 数据层：把「任意矩形（非等分、trim 过）精灵图集」的帧表交给 Niagara
//
// 坐标约定：**像素、左上原点、x 向右 y 向下**（和 `Defatk_sequence.json` / `.paper2dsprites` 一致）。
// 用法：建为 System/Emitter 的 User Parameter，模块和材质用它取当前帧的两套矩形。
//
// 汉化：面板上显示的名字来自 UPROPERTY/UFUNCTION 的 DisplayName 元数据，底下属性名（Flipbook、
// FrameRects…）保持英文，因为它们是 C++/Python 侧用到的标识符。Niagara 图表里的函数名
// （GetAtlasRect 等）也保持英文 —— 它们要进 HLSL 当标识符，中文过不了编译。
#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"

#include "NiagaraDataInterfaceSpriteAtlas.generated.h"

class UPaperFlipbook;
class UTexture2D;
enum class ENiagaraSimTarget : uint8;
struct FPropertyChangedEvent;

/**
 * 精灵图集帧表（Sprite Atlas）。
 *
 * 帧表在编辑器里烘好、存在**普通（非编辑器专用）数组属性**上，所以打包后的游戏直接可用，
 * 运行时不需要 UPaperFlipbook / UPaperSprite / Paper2D 参与。
 *
 * 第一步只支持 CPU sim（`CanExecuteOnTarget` 只放 CPUSim）：GPU 需要 HLSL + Proxy +
 * AddShaderSourceDirectoryMapping，留第二步。
 */
UCLASS(EditInlineNew, Category = "Sprite", CollapseCategories, meta = (DisplayName = "Sprite Atlas (精灵图集)"), MinimalAPI)
class UNiagaraDataInterfaceSpriteAtlas : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	// ------------------------------------------------------------------ 数据源

	/** 首选：绑一个 Flipbook，编辑期自动烘表（改完点 Refresh From Source）。 */
	UPROPERTY(EditAnywhere, Category = "图集", meta = (DisplayName = "序列帧资产（Flipbook）", ToolTip = "首选数据源：绑一个 Flipbook，编辑期自动烘帧表。改完要点上面的「重烘帧表」按钮。", DisplayPriority = 1))
	TObjectPtr<UPaperFlipbook> Flipbook;

	/** 兜底：不绑 Flipbook 时自己指定图集贴图（只用来取尺寸）+ 下面的矩形数组。 */
	UPROPERTY(EditAnywhere, Category = "图集|兜底", meta = (DisplayName = "图集贴图（兜底）", ToolTip = "兜底数据源：不绑 Flipbook 时自己指定图集贴图，只用来取贴图尺寸。", DisplayPriority = 2))
	TObjectPtr<UTexture2D> AtlasTexture;

	/** 兜底：图集矩形，每项 = (x, y, w, h) 像素。 */
	UPROPERTY(EditAnywhere, Category = "图集|兜底", meta = (DisplayName = "图集矩形（兜底）", ToolTip = "兜底数据源：每帧在图集里的矩形，每项 = (x, y, w, h) 像素；顺序就是帧序。", DisplayPriority = 2))
	TArray<FVector4> ManualFrameRects;

	/** 兜底：画布矩形，每项 = (x, y, w, h) 像素；留空则等于图集矩形。 */
	UPROPERTY(EditAnywhere, Category = "图集|兜底", meta = (DisplayName = "画布矩形（兜底）", ToolTip = "兜底数据源：每帧在原始画布里的矩形，每项 = (x, y, w, h) 像素；留空则等于图集矩形。", DisplayPriority = 2))
	TArray<FVector4> ManualCanvasRects;

	/** 兜底：原始画布尺寸（像素）。 */
	UPROPERTY(EditAnywhere, Category = "图集|兜底", meta = (DisplayName = "画布尺寸（兜底）", ToolTip = "兜底数据源：原始未裁剪画布的尺寸（像素）。", DisplayPriority = 2))
	FIntPoint ManualCanvasSize = FIntPoint(928, 640);

	/** 兜底：帧率（工具只在 grid 模式的 sequence.json 里写 Fps，这里手填）。 */
	UPROPERTY(EditAnywhere, Category = "图集|兜底", meta = (DisplayName = "帧率（兜底）", ToolTip = "兜底数据源：每秒帧数。图集工具在 pack 模式下不写 Fps，用 Flipbook 时以 Flipbook 为准。", DisplayPriority = 2))
	float ManualFps = 12.0f;

	/** 取矩形时四周内缩多少**像素**：吃掉图集 padding，避免双线性过滤把邻帧采进来。0 = 不内缩。 */
	UPROPERTY(EditAnywhere, Category = "图集", meta = (DisplayName = "取帧内缩（像素）", ToolTip = "取矩形时四周内缩多少像素：吃掉图集 packing 留下的空边，避免双线性过滤把邻帧像素采进来。0 = 不内缩。默认 0.5。", ClampMin = "0.0", DisplayPriority = 0))
	float RectInsetPixels = 0.5f;

	// ------------------------------------------------------------------ 烘好的表（只读）

	/** 每帧在图集贴图里的矩形 (x, y, w, h) 像素。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "图集矩形"))
	TArray<FVector4> FrameRects;

	/** 每帧在原始未裁剪画布里的矩形 (x, y, w, h) 像素。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "画布矩形"))
	TArray<FVector4> CanvasRects;

	/** 原始画布尺寸（像素）——「画布还原」模式用。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "画布尺寸"))
	FIntPoint CanvasSize = FIntPoint(928, 640);

	/** 图集贴图尺寸（像素）。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "贴图尺寸"))
	FIntPoint TextureSize = FIntPoint(1, 1);

	/** 烘表时解析出来的图集贴图（Flipbook 第一帧 Sprite 的源贴图，或兜底贴图）。
	 *  它同时通过 GetExposedVariables 暴露给渲染器的「材质参数 → 属性绑定」（子变量名 ResolvedTexture），
	 *  于是材质不用再手工填 Sheet —— 建系统只要选 Flipbook 就够了。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "解析到的贴图", ToolTip = "烘表时解析出来的图集贴图。渲染器「材质参数 → 属性绑定」里可以绑到它（显示为 Atlas | ResolvedTexture），这样材质自动拿到图集，不用手工填 Sheet。"))
	TObjectPtr<UTexture2D> ResolvedTexture;

	/** 帧率（来自 Flipbook。工具在 pack 模式不写 Fps，所以这是唯一的来源）。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "帧率"))
	float Fps = 12.0f;

	/** 上次烘表时的「源签名」：Flipbook 路径 + 帧数 + 帧率 + 每帧 Sprite 的源矩形。
	 *  和当前源算出来的签名不一致 ⇒ 帧表过期 ⇒ 加载时自动重烘（见 PostLoad）。 */
	UPROPERTY(VisibleAnywhere, Category = "图集|烘好的表", meta = (DisplayName = "源签名"))
	FString BakedSourceSignature;

	// ------------------------------------------------------------------ 接口

	/** 从 Flipbook / 手工数据重烘帧表。编辑器详情面板上是个按钮；Python 里 `refresh_from_source()`。 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "图集", meta = (DisplayName = "重烘帧表（Refresh From Source）"))
	void RefreshFromSource();

	// ~UObject
	virtual void PostInitProperties() override;
	/** 加载资产时如果源（Flipbook）和上次烘的表对不上，就地重烘 —— 于是 `.dfs` 里
	 *  只写一行 Flipbook，帧表数字交给这里自动生成（② 帧表自动化）。 */
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ~UNiagaraDataInterface
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	/** 把 ResolvedTexture 暴露给渲染器的「材质参数 → 属性绑定」。
	 *  调用链（引擎实证）：NiagaraRenderer.cpp:509-577 遍历 MaterialParameters.AttributeBindings →
	 *  FNiagaraEmitterInstance::GetBoundRendererValue_GT（NiagaraEmitterInstance.cpp:110-144）
	 *  在 CanExposeVariables() 为真时调 GetExposedVariableValue → MatDyn->SetTextureParameterValue。
	 *  子变量名 "ResolvedTexture"（与 UNiagaraDataInterfaceRenderTarget2D::ExposedRTVar 同写法）。 */
	virtual bool CanExposeVariables() const override { return true; }
	virtual void GetExposedVariables(TArray<FNiagaraVariableBase>& OutVariables) const override;
	virtual bool GetExposedVariableValue(const FNiagaraVariableBase& InVariable, void* InPerInstanceData, FNiagaraSystemInstance* InSystemInstance, void* OutData) const override;

	/** 复制自有数据。
	 *  每个 NiagaraComponent 都会给自己造一份 DI 实例 —— NiagaraComponent.cpp:3949-3950 用的是
	 *  `NewObject` + `CopyTo`（不是 DuplicateObject），而 UNiagaraDataInterface::CopyTo
	 *  （NiagaraDataInterface.cpp:216）只搬 CopyToInternal 里复制的东西。
	 *  不覆盖本函数 ⇒ 运行时那份 DI 是个空实例：Flipbook / 帧表 / 贴图尺寸全丢。
	 *  实测（2026-10-04）：FrameRects=0 CanvasRects=0 TextureSize=(1,1) Flipbook=<null>。
	 *  其他引擎 DI 的写法见 NiagaraDataInterfaceTexture.cpp:126-128。 */
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

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
	/** 一次拿齐当前帧的三包数据：图集矩形 / 画布矩形 / 尺寸包（贴图W, 贴图H, 画布W, 画布H）。 */
	void GetFrameParamsVM(FVectorVMExternalFunctionContext& Context);
	/** 把任意帧号回绕到 [0, FrameCount)（正模，负数也正确）。 */
	void WrapFrameVM(FVectorVMExternalFunctionContext& Context);

	/** 取第 Index 帧的矩形（越界 clamp 到端点），并已应用 RectInsetPixels。 */
	FVector4 GetRectClamped(const TArray<FVector4>& Rects, int32 Index) const;

	/** 当前数据源的签名（见 BakedSourceSignature）。没有 Flipbook 时返回空串：
	 *  兜底数据本身就存在资产里，不存在过期问题。 */
	FString ComputeSourceSignature() const;

	/** 帧表是否过期（源变了 / 从没烘过）。非编辑器构建恒 false —— 运行时绝不重烘，
	 *  否则会读到编辑器专用字段而把表清空。 */
	bool NeedsRebakeFromSource() const;

	/** 最后一道保险：贴图尺寸如果比「帧矩形的最大边界」还小，就一定是错的（异步编译中的替身、
	 *  取不到贴图、软引用失效……）。这时改用帧矩形范围 —— 它由帧表本身推出，不依赖贴图。
	 *  不这么做的话材质拿 Sizes.xy 做 UV 归一化，UV 会 >1 ⇒ 图集在面片里平铺。 */
	void ClampTextureSizeToRects(UTexture2D* InTexture);

private:
	static const FName GetFrameCountName;
	static const FName GetAtlasRectName;
	static const FName GetCanvasRectName;
	static const FName GetCanvasSizeName;
	static const FName GetTextureSizeName;
	static const FName GetFpsName;
	static const FName GetFrameParamsName;
	static const FName WrapFrameName;
};

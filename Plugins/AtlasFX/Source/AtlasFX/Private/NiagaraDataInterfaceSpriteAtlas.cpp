// AtlasFX —— 数据层实现
#include "NiagaraDataInterfaceSpriteAtlas.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraTypeRegistry.h"

#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAtlasFX, Log, All);

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceSpriteAtlas"

const FName UNiagaraDataInterfaceSpriteAtlas::GetFrameCountName(TEXT("GetFrameCount"));
const FName UNiagaraDataInterfaceSpriteAtlas::GetAtlasRectName(TEXT("GetAtlasRect"));
const FName UNiagaraDataInterfaceSpriteAtlas::GetCanvasRectName(TEXT("GetCanvasRect"));
const FName UNiagaraDataInterfaceSpriteAtlas::GetCanvasSizeName(TEXT("GetCanvasSize"));
const FName UNiagaraDataInterfaceSpriteAtlas::GetTextureSizeName(TEXT("GetTextureSize"));
const FName UNiagaraDataInterfaceSpriteAtlas::GetFpsName(TEXT("GetFps"));

UNiagaraDataInterfaceSpriteAtlas::UNiagaraDataInterfaceSpriteAtlas(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------- 注册

void UNiagaraDataInterfaceSpriteAtlas::PostInitProperties()
{
	Super::PostInitProperties();

	// 不注册的话，这个 DI 不会出现在 "Add User Parameter -> Data Interface" 列表里。
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

// ---------------------------------------------------------------------------- 烘表

void UNiagaraDataInterfaceSpriteAtlas::RefreshFromSource()
{
	FrameRects.Reset();
	CanvasRects.Reset();
	Fps = ManualFps;
	TextureSize = FIntPoint(1, 1);

#if WITH_EDITOR
	if (Flipbook)
	{
		const int32 NumFrames = Flipbook->GetNumKeyFrames();
		Fps = Flipbook->GetFramesPerSecond();

		FIntPoint ResolvedCanvasSize = ManualCanvasSize;
		bool bCanvasSizeResolved = false;
		UTexture2D* Texture = nullptr;

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			const UPaperSprite* Sprite = Flipbook->GetKeyFrameChecked(FrameIndex).Sprite;

			// 取不到 Sprite 时给一个"整张贴图"的安全值，让帧数依然对得上。
			FVector4 AtlasRect(0.0, 0.0, 1.0, 1.0);
			FVector4 CanvasRect(0.0, 0.0, 1.0, 1.0);

			if (Sprite)
			{
				// GetSourceUV/GetSourceSize 是编辑器字段：SourceUV = 帧在图集里的位置，
				// SourceDimension = **裁剪后**的帧尺寸（PaperSprite.h:269-270）。
				const FVector2D AtlasPos = Sprite->GetSourceUV();
				const FVector2D FrameSize = Sprite->GetSourceSize();
				AtlasRect = FVector4(AtlasPos.X, AtlasPos.Y, FrameSize.X, FrameSize.Y);

				// 画布矩形 = 帧在原图里的位置 + 帧尺寸（trim 偏移由它还原）。
				FVector2D CanvasPos = AtlasPos;
				if (Sprite->IsTrimmedInSourceImage())
				{
					CanvasPos = Sprite->GetOriginInSourceImageBeforeTrimming();

					const FVector2D ThisCanvas/*Dim*/ = Sprite->GetSourceImageDimensionBeforeTrimming();
					if (ThisCanvas.X > 0.0 && ThisCanvas.Y > 0.0)
					{
						const FIntPoint ThisCanvasSize(FMath::RoundToInt(ThisCanvas.X), FMath::RoundToInt(ThisCanvas.Y));
						if (!bCanvasSizeResolved)
						{
							ResolvedCanvasSize = ThisCanvasSize;
							bCanvasSizeResolved = true;
						}
						else if (ResolvedCanvasSize != ThisCanvasSize)
						{
							UE_LOG(LogAtlasFX, Warning,
								TEXT("%s：第 %d 帧的画布尺寸 %dx%d 与第 0 帧的 %dx%d 不一致，画布还原模式会偏。"),
								*GetName(), FrameIndex,
								ThisCanvasSize.X, ThisCanvasSize.Y,
								ResolvedCanvasSize.X, ResolvedCanvasSize.Y);
						}
					}
				}
				else if (!bCanvasSizeResolved)
				{
					// 没 trim 过：画布就是帧本身。
					ResolvedCanvasSize = FIntPoint(FMath::RoundToInt(FrameSize.X), FMath::RoundToInt(FrameSize.Y));
					bCanvasSizeResolved = true;
				}

				CanvasRect = FVector4(CanvasPos.X, CanvasPos.Y, FrameSize.X, FrameSize.Y);

				if (!Texture)
				{
					Texture = Sprite->GetSourceTexture();
				}
			}

			FrameRects.Add(AtlasRect);
			CanvasRects.Add(CanvasRect);
		}

		if (Texture)
		{
			TextureSize = FIntPoint(Texture->GetSizeX(), Texture->GetSizeY());
		}
		CanvasSize = ResolvedCanvasSize;
		return;
	}
#endif // WITH_EDITOR

	// 兜底：手工数据
	if (ManualFrameRects.Num() > 0)
	{
		FrameRects = ManualFrameRects;
		CanvasRects = (ManualCanvasRects.Num() == ManualFrameRects.Num()) ? ManualCanvasRects : ManualFrameRects;
		CanvasSize = ManualCanvasSize;
	}

	if (AtlasTexture)
	{
		TextureSize = FIntPoint(AtlasTexture->GetSizeX(), AtlasTexture->GetSizeY());
	}
}

#if WITH_EDITOR
void UNiagaraDataInterfaceSpriteAtlas::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 数据源一改就重烘，省得忘记点按钮。只读属性（Baked 那组）不触发，避免自我循环。
	const FName ChangedName = PropertyChangedEvent.GetPropertyName();
	if (ChangedName == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSpriteAtlas, Flipbook)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSpriteAtlas, AtlasTexture)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSpriteAtlas, ManualFrameRects)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSpriteAtlas, ManualCanvasRects)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSpriteAtlas, ManualCanvasSize)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSpriteAtlas, ManualFps))
	{
		RefreshFromSource();
	}
}
#endif

// ---------------------------------------------------------------------------- 通用

FVector4 UNiagaraDataInterfaceSpriteAtlas::GetRectClamped(const TArray<FVector4>& Rects, int32 Index) const
{
	if (Rects.Num() <= 0)
	{
		// 还没烘表：给整张贴图，效果是"一帧铺满"，看得出问题但不崩。
		return FVector4(0.0, 0.0, 1.0, 1.0);
	}

	const FVector4& Source = Rects[FMath::Clamp(Index, 0, Rects.Num() - 1)];

	// 内缩最多到矩形短边的 1/4，免得把手填的小矩形缩没了。
	const double MaxInset = FMath::Min(Source.Z, Source.W) * 0.25;
	const double Inset = FMath::Clamp(static_cast<double>(RectInsetPixels), 0.0, FMath::Max(MaxInset, 0.0));

	return FVector4(
		Source.X + Inset,
		Source.Y + Inset,
		FMath::Max(Source.Z - 2.0 * Inset, 1.0),
		FMath::Max(Source.W - 2.0 * Inset, 1.0));
}

bool UNiagaraDataInterfaceSpriteAtlas::CanExecuteOnTarget(ENiagaraSimTarget Target) const
{
	// 第一步只支持 CPU sim。GPU 版需要 .ush + Proxy + shader 目录映射（第二步）。
	return Target == ENiagaraSimTarget::CPUSim;
}

bool UNiagaraDataInterfaceSpriteAtlas::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfaceSpriteAtlas* OtherTyped = CastChecked<const UNiagaraDataInterfaceSpriteAtlas>(Other);
	return OtherTyped->Flipbook == Flipbook
		&& OtherTyped->AtlasTexture == AtlasTexture
		&& OtherTyped->ManualFrameRects == ManualFrameRects
		&& OtherTyped->ManualCanvasRects == ManualCanvasRects
		&& OtherTyped->ManualCanvasSize == ManualCanvasSize
		&& OtherTyped->ManualFps == ManualFps
		&& OtherTyped->RectInsetPixels == RectInsetPixels;
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceSpriteAtlas::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
	{
		return false;
	}

	// 帧表变了就当编译输入变了：CPU 侧其实用不到，但这样切到 GPU 时行为一致。
	InVisitor->UpdatePOD(TEXT("AtlasFX.FrameCount"), FrameRects.Num());
	InVisitor->UpdatePOD(TEXT("AtlasFX.CanvasSizeX"), CanvasSize.X);
	InVisitor->UpdatePOD(TEXT("AtlasFX.CanvasSizeY"), CanvasSize.Y);
	InVisitor->UpdatePOD(TEXT("AtlasFX.TextureSizeX"), TextureSize.X);
	InVisitor->UpdatePOD(TEXT("AtlasFX.TextureSizeY"), TextureSize.Y);
	InVisitor->UpdatePOD(TEXT("AtlasFX.RectInset"), RectInsetPixels);

	// 只能喂标量和裸浮点数组：UpdatePOD / UpdateArray 内部会用 LexToString 生成调试字符串，
	// 而 FVector4 / FIntPoint 没有 LexToString 重载（引擎头 NiagaraCompileHashVisitor.h:54）。
	auto AppendRects = [InVisitor](const TCHAR* InDebugName, const TArray<FVector4>& InRects)
	{
		TArray<double> Flattened;
		Flattened.Reserve(InRects.Num() * 4);
		for (const FVector4& Rect : InRects)
		{
			Flattened.Add(Rect.X);
			Flattened.Add(Rect.Y);
			Flattened.Add(Rect.Z);
			Flattened.Add(Rect.W);
		}
		if (Flattened.Num() > 0)
		{
			InVisitor->UpdateArray(InDebugName, Flattened.GetData(), static_cast<uint64>(Flattened.Num()));
		}
	};

	AppendRects(TEXT("AtlasFX.FrameRects"), FrameRects);
	AppendRects(TEXT("AtlasFX.CanvasRects"), CanvasRects);
	return true;
}

void UNiagaraDataInterfaceSpriteAtlas::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	// 每个函数第一个输入固定是 DI 自身，且 bMemberFunction 必须为 true。
	auto MakeSignature = [this](FName InName, const FText& InDescription)
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = InName;
		Sig.Description = InDescription;
		Sig.bMemberFunction = true;
		Sig.bSupportsCPU = true;
		Sig.bSupportsGPU = false;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Sprite Atlas")));
		return Sig;
	};

	{
		FNiagaraFunctionSignature Sig = MakeSignature(GetFrameCountName, LOCTEXT("GetFrameCountDesc", "总帧数（= 图集里的帧矩形个数）。"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Frame Count")), LOCTEXT("GetFrameCountOut", "帧数"));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = MakeSignature(GetAtlasRectName, LOCTEXT("GetAtlasRectDesc", "第 Frame Index 帧在图集贴图里的矩形（像素 x, y, w, h）。越界会 clamp 到首/尾帧。"));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Frame Index")), LOCTEXT("GetAtlasRectIn", "帧号（小数会被向下取整）"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Atlas Rect")), LOCTEXT("GetAtlasRectOut", "图集矩形，像素"));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = MakeSignature(GetCanvasRectName, LOCTEXT("GetCanvasRectDesc", "第 Frame Index 帧在原始未裁剪画布里的矩形（像素 x, y, w, h）。"));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Frame Index")), LOCTEXT("GetCanvasRectIn", "帧号"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Canvas Rect")), LOCTEXT("GetCanvasRectOut", "画布矩形，像素"));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = MakeSignature(GetCanvasSizeName, LOCTEXT("GetCanvasSizeDesc", "原始画布尺寸（像素）。"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Canvas Size")), LOCTEXT("GetCanvasSizeOut", "画布尺寸"));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = MakeSignature(GetTextureSizeName, LOCTEXT("GetTextureSizeDesc", "图集贴图尺寸（像素）。"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Texture Size")), LOCTEXT("GetTextureSizeOut", "贴图尺寸"));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = MakeSignature(GetFpsName, LOCTEXT("GetFpsDesc", "源帧率（来自 Flipbook）。"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("FPS")), LOCTEXT("GetFpsOut", "帧率"));
		OutFunctions.Add(Sig);
	}
}
#endif // WITH_EDITORONLY_DATA

// ---------------------------------------------------------------------------- VM

void UNiagaraDataInterfaceSpriteAtlas::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == GetFrameCountName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetFrameCountVM(Context); });
	}
	else if (BindingInfo.Name == GetAtlasRectName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetAtlasRectVM(Context); });
	}
	else if (BindingInfo.Name == GetCanvasRectName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetCanvasRectVM(Context); });
	}
	else if (BindingInfo.Name == GetCanvasSizeName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetCanvasSizeVM(Context); });
	}
	else if (BindingInfo.Name == GetTextureSizeName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetTextureSizeVM(Context); });
	}
	else if (BindingInfo.Name == GetFpsName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetFpsVM(Context); });
	}
	else
	{
		UE_LOG(LogAtlasFX, Warning,
			TEXT("未知的图集数据接口函数。收到 Name: %s（可能工程里有旧版模块还在引用已删除的函数）"),
			*BindingInfo.Name.ToString());
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetFrameCountVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIOutputParam<int32> OutFrameCount(Context);
	const int32 NumFrames = FrameRects.Num();

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		OutFrameCount.SetAndAdvance(NumFrames);
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetAtlasRectVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIInputParam<float> InFrameIndex(Context);
	FNDIOutputParam<FVector4f> OutRect(Context);

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		const FVector4 Rect = GetRectClamped(FrameRects, FMath::FloorToInt32(InFrameIndex.GetAndAdvance()));
		OutRect.SetAndAdvance(FVector4f(static_cast<float>(Rect.X), static_cast<float>(Rect.Y), static_cast<float>(Rect.Z), static_cast<float>(Rect.W)));
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetCanvasRectVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIInputParam<float> InFrameIndex(Context);
	FNDIOutputParam<FVector4f> OutRect(Context);

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		const FVector4 Rect = CanvasRects.Num() > 0
			? GetRectClamped(CanvasRects, FMath::FloorToInt32(InFrameIndex.GetAndAdvance()))
			: GetRectClamped(FrameRects, FMath::FloorToInt32(InFrameIndex.GetAndAdvance()));
		OutRect.SetAndAdvance(FVector4f(static_cast<float>(Rect.X), static_cast<float>(Rect.Y), static_cast<float>(Rect.Z), static_cast<float>(Rect.W)));
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetCanvasSizeVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIOutputParam<FVector2f> OutSize(Context);
	const FVector2f Size(static_cast<float>(CanvasSize.X), static_cast<float>(CanvasSize.Y));

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		OutSize.SetAndAdvance(Size);
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetTextureSizeVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIOutputParam<FVector2f> OutSize(Context);
	const FVector2f Size(static_cast<float>(TextureSize.X), static_cast<float>(TextureSize.Y));

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		OutSize.SetAndAdvance(Size);
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetFpsVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIOutputParam<float> OutFps(Context);
	const float Value = Fps;

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		OutFps.SetAndAdvance(Value);
	}
}

#undef LOCTEXT_NAMESPACE

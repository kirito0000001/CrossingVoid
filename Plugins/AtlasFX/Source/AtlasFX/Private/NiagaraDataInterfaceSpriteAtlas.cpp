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
const FName UNiagaraDataInterfaceSpriteAtlas::GetFrameParamsName(TEXT("GetFrameParams"));
const FName UNiagaraDataInterfaceSpriteAtlas::WrapFrameName(TEXT("WrapFrame"));

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

	// 记下这次烘表对应的「源」：下次加载时用它判断要不要自动重烘（见 PostLoad）。
	BakedSourceSignature = ComputeSourceSignature();

	// 贴图尺寸要留退路：无 RHI 的进程里 GetSizeX() 直接返回 0（Texture2D.cpp:349-363），
	// 所以绝不能把 0 写进去 —— 材质用 Sizes.xy 做 UV 归一化，0 会让 UV 变 NaN（面片全透明）。
	const FIntPoint PreviousTextureSize = TextureSize;
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
					// SourceTexture 是编辑器软引用，PaperSprite.cpp:1442-1470 只认它；
					// 取不到就退到 DI 自己的兜底贴图（Sprite 的 BakedSourceTexture 是 protected，
					// 插件访问不到）。烘表整段都在 WITH_EDITOR 里，所以这条链够用。
					Texture = Sprite->GetSourceTexture();
					if (!Texture)
					{
						Texture = AtlasTexture;
					}
				}
			}

			FrameRects.Add(AtlasRect);
			CanvasRects.Add(CanvasRect);
		}

		if (Texture)
		{
			// 贴图的平台数据可能还在异步编译：这期间 GetSizeX() 返回的是「临时替身」的尺寸
			// （引擎默认贴图 32×32 —— Texture2D.cpp:344-363 + Texture.cpp:511-516），
			// 拿它做 UV 归一化会让图集在面片里平铺几百遍（症状：一坨细碎条纹）。
			// 引擎自己的写法就是先阻塞到编译完成：Texture.cpp:518-529 BlockOnAnyAsyncBuild()。
			Texture->BlockOnAnyAsyncBuild();

			TextureSize = FIntPoint(Texture->GetSizeX(), Texture->GetSizeY());
			if (TextureSize.X <= 0 || TextureSize.Y <= 0)
			{
				// 无 RHI（headless build / 命令行）时退回「导入尺寸」：Texture2D.cpp:638-645 在
				// 非 cooked 包里返回源图尺寸，正是图集贴图的像素尺寸。
				TextureSize = Texture->GetImportedSize();
			}
			if (TextureSize.X <= 0 || TextureSize.Y <= 0)
			{
				TextureSize = PreviousTextureSize;
				UE_LOG(LogAtlasFX, Warning,
					TEXT("%s：取不到图集贴图尺寸（%s），沿用上次烘好的 %dx%d。"),
					*GetName(), *Texture->GetPathName(), TextureSize.X, TextureSize.Y);
			}
		}
		else
		{
			// Flipbook 里一帧都没拿到贴图：沿用上次的值，别把材质搞成 NaN。
			TextureSize = PreviousTextureSize;
		}

		// 暴露给渲染器「材质参数 → 属性绑定」用（子变量名 ResolvedTexture）。
		ResolvedTexture = Texture;

		ClampTextureSizeToRects(Texture);

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
		AtlasTexture->BlockOnAnyAsyncBuild();
		TextureSize = FIntPoint(AtlasTexture->GetSizeX(), AtlasTexture->GetSizeY());
	}

	ResolvedTexture = AtlasTexture;

	ClampTextureSizeToRects(AtlasTexture);
}

void UNiagaraDataInterfaceSpriteAtlas::ClampTextureSizeToRects(UTexture2D* InTexture)
{
	// 帧矩形的最大边界 = 图集「至少得多大」的下界。它完全由帧表推出来，不依赖贴图能不能加载，
	// 所以可以当最后一道保险：材质用 Sizes.xy 做 UV 归一化，尺寸只要比矩形范围还小，
	// UV 就会 >1 ⇒ 图集在面片里平铺（症状就是一坨细碎条纹）。
	FIntPoint Extent(0, 0);
	for (const FVector4& Rect : FrameRects)
	{
		Extent.X = FMath::Max(Extent.X, FMath::CeilToInt(Rect.X + Rect.Z));
		Extent.Y = FMath::Max(Extent.Y, FMath::CeilToInt(Rect.Y + Rect.W));
	}

	if (Extent.X <= 0 || Extent.Y <= 0)
	{
		return;
	}

	if (TextureSize.X >= Extent.X && TextureSize.Y >= Extent.Y)
	{
		return;
	}

	UE_LOG(LogAtlasFX, Warning,
		TEXT("%s：贴图尺寸 %dx%d 比帧矩形范围 %dx%d 还小（贴图 %s），改用帧矩形范围做 UV 归一化。"),
		*GetName(), TextureSize.X, TextureSize.Y, Extent.X, Extent.Y,
		InTexture ? *InTexture->GetPathName() : TEXT("<取不到贴图>"));
	TextureSize = Extent;
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

// ---------------------------------------------------------------------------- 自动烘表（帧表自动化）

void UNiagaraDataInterfaceSpriteAtlas::PostLoad()
{
	Super::PostLoad();

	// 资产里只留一行 Flipbook，帧表在加载时自己算出来。只做增量判断（签名不一致才动手），
	// 所以正常情况下加载不会反复标脏。标脏是必要的：这次烘的结果要能跟着资产存下去，
	// 打包后的游戏读到的就是存下来的表（运行时不需要 Paper2D 参与）。
	if (NeedsRebakeFromSource())
	{
		RefreshFromSource();
		MarkPackageDirty();

		UE_LOG(LogAtlasFX, Log,
			TEXT("%s：加载时自动重烘帧表 —— %d 帧 / %.2f fps / 画布 %dx%d / 贴图 %dx%d（源 = %s）"),
			*GetName(), FrameRects.Num(), Fps, CanvasSize.X, CanvasSize.Y, TextureSize.X, TextureSize.Y,
			Flipbook ? *Flipbook->GetPathName() : TEXT("<兜底>"));
	}
}

FString UNiagaraDataInterfaceSpriteAtlas::ComputeSourceSignature() const
{
	// 兜底数据源：矩形本身就存在资产里，永远不会「过期」。
	if (!Flipbook)
	{
		return FString();
	}

#if WITH_EDITOR
	// 烘表算法版本：改了烘表逻辑就 +1，强制所有资产在下次加载时重烘一次。
	// （v2 = 贴图尺寸在无 RHI 进程里退回导入尺寸，避免写出 0x0。）
	// （v3 = 读贴图尺寸前先 BlockOnAnyAsyncBuild，并加「尺寸不得小于帧矩形范围」的保险。）
	// （v4 = NeedsRebakeFromSource 增加「结果残缺就重烘」的判据 —— 修
	//       「必须先打开一次特效、别处才显示」那类症状。）
	const int32 BakeVersion = 4;

	FString Signature = FString::Printf(TEXT("v%d|fb:%s|%d|%.4f"),
		BakeVersion, *Flipbook->GetPathName(), Flipbook->GetNumKeyFrames(), Flipbook->GetFramesPerSecond());

	// 每帧的 Sprite 也要进签名：换图 / 重排图集时帧数和帧率可能都不变。
	const int32 NumFrames = Flipbook->GetNumKeyFrames();
	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		const UPaperSprite* Sprite = Flipbook->GetKeyFrameChecked(Index).Sprite;
		if (!Sprite)
		{
			Signature += TEXT("|null");
			continue;
		}

		const FVector2D AtlasPos = Sprite->GetSourceUV();
		const FVector2D FrameSize = Sprite->GetSourceSize();
		const FVector2D TrimOrigin = Sprite->GetOriginInSourceImageBeforeTrimming();
		Signature += FString::Printf(TEXT("|%s@%.2f,%.2f+%.2fx%.2f>%.2f,%.2f"),
			*Sprite->GetPathName(), AtlasPos.X, AtlasPos.Y, FrameSize.X, FrameSize.Y, TrimOrigin.X, TrimOrigin.Y);
	}
	return Signature;
#else
	// 非编辑器构建读不到 Sprite 的源矩形（编辑器专用字段），签名退化成路径；
	// 反正 NeedsRebakeFromSource() 在这种构建里恒 false，这个值只用于显示/调试。
	return FString::Printf(TEXT("fb:%s"), *Flipbook->GetPathName());
#endif
}

bool UNiagaraDataInterfaceSpriteAtlas::NeedsRebakeFromSource() const
{
#if WITH_EDITOR
	if (!Flipbook)
	{
		// 没绑 Flipbook：兜底数据就是权威数据，不重烘（重烘反而会把它清空）。
		return false;
	}

	// ① 源变了（换 Flipbook / 改帧率 / 换帧 Sprite …）：签名不一样就重烘。
	if (BakedSourceSignature != ComputeSourceSignature())
	{
		return true;
	}

	// ② 源没变，但**烘出来的结果本身是残缺的** —— 也要重烘。
	//
	// 这不是理论情况：在无头进程 / 贴图还没编译好的时候跑烘表，会写出
	// `ResolvedTexture = None` 或退化成 1×1 的贴图尺寸。而签名只描述「源」，
	// 源没变就永远不会再烘 ⇒ 症状正是「这个特效必须先打开一次（重烘/保存一次），
	// 别的序列/游戏里才显示」（2026-10-06 用户报）。
	if (FrameRects.Num() != Flipbook->GetNumKeyFrames())
	{
		return true;
	}
	if (ResolvedTexture == nullptr)
	{
		return true;
	}
	if (TextureSize.X <= 1 || TextureSize.Y <= 1)
	{
		return true;
	}

	return false;
#else
	// 运行时绝不重烘：RefreshFromSource 的 Flipbook 分支整段在 WITH_EDITOR 里，
	// 非编辑器构建跑一遍会把烘好的表清空。
	return false;
#endif
}

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

namespace
{
	/** 暴露给渲染器的子变量。名字就是成员名 —— 引擎里 UNiagaraDataInterfaceRenderTarget2D::ExposedRTVar
	 *  也是这么写的（NiagaraDataInterfaceRenderTarget2D.cpp:119：
	 *  `FNiagaraVariableBase(FNiagaraTypeDefinition(UTexture::StaticClass()), TEXT("RenderTarget"))`）。
	 *  类型必须是 UTexture：NiagaraRenderer.cpp:561 认的就是 GetUTextureDef()。 */
	const FNiagaraVariableBase& GetExposedTextureVariable()
	{
		static const FNiagaraVariableBase Variable(FNiagaraTypeDefinition(UTexture::StaticClass()), TEXT("ResolvedTexture"));
		return Variable;
	}
}

void UNiagaraDataInterfaceSpriteAtlas::GetExposedVariables(TArray<FNiagaraVariableBase>& OutVariables) const
{
	OutVariables.Emplace(GetExposedTextureVariable());
}

bool UNiagaraDataInterfaceSpriteAtlas::GetExposedVariableValue(const FNiagaraVariableBase& InVariable, void* InPerInstanceData, FNiagaraSystemInstance* InSystemInstance, void* OutData) const
{
	if (InVariable == GetExposedTextureVariable())
	{
		UObject** Var = (UObject**)OutData;
		*Var = ResolvedTexture;
		return true;
	}
	return false;
}

bool UNiagaraDataInterfaceSpriteAtlas::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceSpriteAtlas* DestinationTyped = CastChecked<UNiagaraDataInterfaceSpriteAtlas>(Destination);

	// 数据源（Flipbook 自动烘表 / 手工兜底）
	DestinationTyped->Flipbook = Flipbook;
	DestinationTyped->AtlasTexture = AtlasTexture;
	DestinationTyped->ManualFrameRects = ManualFrameRects;
	DestinationTyped->ManualCanvasRects = ManualCanvasRects;
	DestinationTyped->ManualCanvasSize = ManualCanvasSize;
	DestinationTyped->ManualFps = ManualFps;
	DestinationTyped->RectInsetPixels = RectInsetPixels;

	// 烘好的帧表 —— 运行时 VM 真正读的就是这几份
	DestinationTyped->FrameRects = FrameRects;
	DestinationTyped->CanvasRects = CanvasRects;
	DestinationTyped->CanvasSize = CanvasSize;
	DestinationTyped->TextureSize = TextureSize;
	DestinationTyped->Fps = Fps;
	// 渲染器的「材质参数 → 属性绑定」是从**实例 DI** 上取值（NiagaraEmitterInstance.cpp:114
	// RendererBindings.GetDataInterface），所以这份也得复制过去，否则运行时拿不到贴图。
	DestinationTyped->ResolvedTexture = ResolvedTexture;

	return true;
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
	{
		// 一次调用拿齐材质要的三包数据：模块里只要一个节点 + 三个 Map Set。
		FNiagaraFunctionSignature Sig = MakeSignature(GetFrameParamsName, LOCTEXT("GetFrameParamsDesc", "第 Frame Index 帧的全套数据：图集矩形、画布矩形、尺寸包（贴图W, 贴图H, 画布W, 画布H）。越界 clamp 到首/尾帧。"));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Frame Index")), LOCTEXT("GetFrameParamsIn", "帧号（小数向下取整）"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Atlas Rect")), LOCTEXT("GetFrameParamsOutAtlas", "图集矩形 (x,y,w,h) 像素 → 接 Dynamic Material Parameter 1"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Canvas Rect")), LOCTEXT("GetFrameParamsOutCanvas", "画布矩形 (x,y,w,h) 像素 → 接 Dynamic Material Parameter 2"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Sizes")), LOCTEXT("GetFrameParamsOutSizes", "(贴图W, 贴图H, 画布W, 画布H) → 接 Dynamic Material Parameter 3"));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = MakeSignature(WrapFrameName, LOCTEXT("WrapFrameDesc", "把任意帧号回绕到 [0, 帧数) 区间（循环播放用）。"));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Raw Frame")), LOCTEXT("WrapFrameIn", "原始帧号（可以是负数，也可以很大）"));
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Frame")), LOCTEXT("WrapFrameOut", "回绕后的帧号"));
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
	else if (BindingInfo.Name == GetFrameParamsName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { GetFrameParamsVM(Context); });
	}
	else if (BindingInfo.Name == WrapFrameName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { WrapFrameVM(Context); });
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

void UNiagaraDataInterfaceSpriteAtlas::GetFrameParamsVM(FVectorVMExternalFunctionContext& Context)
{
	// 输出顺序必须和 GetFunctionsInternal 里 AddOutput 的顺序一致。
	FNDIInputParam<float> InFrameIndex(Context);
	FNDIOutputParam<FVector4f> OutAtlasRect(Context);
	FNDIOutputParam<FVector4f> OutCanvasRect(Context);
	FNDIOutputParam<FVector4f> OutSizes(Context);

	const FVector4f Sizes(
		static_cast<float>(TextureSize.X),
		static_cast<float>(TextureSize.Y),
		static_cast<float>(CanvasSize.X),
		static_cast<float>(CanvasSize.Y));

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		const int32 FrameIndex = FMath::FloorToInt32(InFrameIndex.GetAndAdvance());
		const FVector4 AtlasRect = GetRectClamped(FrameRects, FrameIndex);
		const FVector4 CanvasRectValue = CanvasRects.Num() > 0 ? GetRectClamped(CanvasRects, FrameIndex) : AtlasRect;

		OutAtlasRect.SetAndAdvance(FVector4f(
			static_cast<float>(AtlasRect.X), static_cast<float>(AtlasRect.Y),
			static_cast<float>(AtlasRect.Z), static_cast<float>(AtlasRect.W)));
		OutCanvasRect.SetAndAdvance(FVector4f(
			static_cast<float>(CanvasRectValue.X), static_cast<float>(CanvasRectValue.Y),
			static_cast<float>(CanvasRectValue.Z), static_cast<float>(CanvasRectValue.W)));
		OutSizes.SetAndAdvance(Sizes);
	}
}

void UNiagaraDataInterfaceSpriteAtlas::WrapFrameVM(FVectorVMExternalFunctionContext& Context)
{
	FNDIInputParam<float> InRawFrame(Context);
	FNDIOutputParam<float> OutFrame(Context);

	const float NumFrames = static_cast<float>(FrameRects.Num());

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		const float RawFrame = InRawFrame.GetAndAdvance();
		// 用 floor 做「正模」：frac() 对负数会给出负值，循环播放会卡在第 0 帧。
		const float Wrapped = NumFrames > 0.0f
			? RawFrame - FMath::FloorToFloat(RawFrame / NumFrames) * NumFrames
			: 0.0f;
		OutFrame.SetAndAdvance(Wrapped);
	}
}

#undef LOCTEXT_NAMESPACE

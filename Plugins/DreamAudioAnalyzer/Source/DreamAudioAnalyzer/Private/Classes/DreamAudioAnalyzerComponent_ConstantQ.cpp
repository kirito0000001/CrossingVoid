#include "Classes/DreamAudioAnalyzerComponent.h"


void UDreamAudioAnalyzerComponent_ConstantQ::BeginPlay()
{
	Super::BeginPlay();
	InitializeAnalysisTexture();
}

void UDreamAudioAnalyzerComponent_ConstantQ::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto Texture : AnalysisTextures)
	{
		Texture->RemoveFromRoot();
	}
	
	if (ConstantQAnalyzer)
	{
		ConstantQAnalyzer->StopAnalyzing(); 
		ConstantQAnalyzer->OnLatestConstantQResultsNative.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UDreamAudioAnalyzerComponent_ConstantQ::Initialize()
{
	Super::Initialize();

	UWorld* ThisWorld = GetWorld();
	if (!ThisWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("World is nullptr"));
		return;
	}

	const FAudioDeviceHandle AudioDevice = ThisWorld->GetAudioDevice();

	ConstantQAnalyzer = NewObject<UConstantQAnalyzer>(this);
	ConstantQAnalyzer->Settings = ConstantQSettings;
	ConstantQAnalyzer->OnLatestConstantQResultsNative.AddUObject(this, &UDreamAudioAnalyzerComponent_ConstantQ::OnAnalysisConstantQResults);
	ConstantQAnalyzer->StartAnalyzing(AudioDevice.GetDeviceID(), AnalysisAudioBus);

	if (!bEnableCreateAnalysisTexture || !bIsCreated || AnalysisTextures.IsEmpty())
	{
		return;
	}

	// 线程锁：防止音频线程正在写入 Buffer 时我们进行读取
	FScopeLock Lock(&DataGuard);

	// 确保 Buffer 数据量匹配声道数
	if (ConstantQResultBuffer.Num() < ChannelNums)
	{
		return;
	}

	// 更新纹理像素
	for (int32 ChannelIdx = 0; ChannelIdx < ChannelNums; ++ChannelIdx)
	{
		if (!AnalysisTextures.IsValidIndex(ChannelIdx) || !ConstantQResultBuffer.IsValidIndex(ChannelIdx))
		{
			continue;
		}

		UTexture2D* Texture = AnalysisTextures[ChannelIdx];
		const TArray<float>& SpectrumData = ConstantQResultBuffer[ChannelIdx].SpectrumValues;

		if (Texture && !SpectrumData.IsEmpty())
		{
			UpdateTextureFromSpectrum(Texture, SpectrumData);
		}
	}
}

void UDreamAudioAnalyzerComponent_ConstantQ::StartAnalysis()
{
	Super::StartAnalysis();

	if (!ConstantQAnalyzer)
	{
		return;
	}

	ConstantQAnalyzer->StartAnalyzing(this, AnalysisAudioBus);
}

void UDreamAudioAnalyzerComponent_ConstantQ::StopAnalysis()
{
	Super::StopAnalysis();

	if (!ConstantQAnalyzer)
	{
		return;
	}

	ConstantQAnalyzer->StopAnalyzing(this);
}

void UDreamAudioAnalyzerComponent_ConstantQ::InitializeAnalysisTexture()
{
	const int32 TextureWidth = (ConstantQSettings && ConstantQSettings->NumBands > 0) ? ConstantQSettings->NumBands : 48;
	const int32 TextureHeight = 1;

	for (int i = 0; i < ChannelNums; ++i)
	{
		if (UTexture2D* CacheTexture = UTexture2D::CreateTransient(TextureWidth, TextureHeight, PF_B8G8R8A8))
		{
			//CacheTexture->MipGenSettings = TMGS_NoMipmaps;
			CacheTexture->Filter = TF_Nearest;
			CacheTexture->CompressionSettings = TC_VectorDisplacementmap;
			CacheTexture->SRGB = 0; // 线性空间
			CacheTexture->AddToRoot(); // 防止 GC
			CacheTexture->UpdateResource();
			AnalysisTextures.Add(CacheTexture);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Failed to create texture for channel %d"), i);
		}
	}

	if (AnalysisTextures.Num() == ChannelNums)
	{
		bIsCreated = true;
		OnAnalysisTextureLoaded.Broadcast(AnalysisTextures);
	}
}

void UDreamAudioAnalyzerComponent_ConstantQ::UpdateTextureFromSpectrum(UTexture2D* InTexture, const TArray<float>& InSpectrumData)
{
	if (!InTexture || InSpectrumData.IsEmpty()) return;

	FTexturePlatformData* PlatformData = InTexture->GetPlatformData();
	if (!PlatformData || !PlatformData->Mips.IsValidIndex(0)) return;

	const int32 Width = PlatformData->SizeX;
	// 防止数组越界
	const int32 BandsToCopy = FMath::Min(Width, InSpectrumData.Num());

	// 锁定纹理内存进行写入
	void* TextureDataPtr = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	if (!TextureDataPtr) return;

	uint8* DestPixels = static_cast<uint8*>(TextureDataPtr);

	for (int32 i = 0; i < BandsToCopy; ++i)
	{
		float Value = InSpectrumData[i];
		// 简单的钳制，确保颜色在 0-1 之间 (根据需要可能需要 log 缩放)
		Value = FMath::Clamp(Value, 0.0f, 1.0f);
		uint8 ByteValue = static_cast<uint8>(Value * 255.0f);

		const int32 PixelIdx = i * 4;
		DestPixels[PixelIdx + 0] = ByteValue; // B
		DestPixels[PixelIdx + 1] = ByteValue; // G
		DestPixels[PixelIdx + 2] = ByteValue; // R
		DestPixels[PixelIdx + 3] = 255; // A (不透明)
	}

	// 填充剩余部分为黑色
	if (BandsToCopy < Width)
	{
		const int32 RemainingBytes = (Width - BandsToCopy) * 4;
		FMemory::Memzero(DestPixels + (BandsToCopy * 4), RemainingBytes);
	}

	PlatformData->Mips[0].BulkData.Unlock();
	InTexture->UpdateResource();
}

void UDreamAudioAnalyzerComponent_ConstantQ::OnAnalysisConstantQResults(UConstantQAnalyzer* Analyzer, int32 ChannelIndex, const FConstantQResults& Results)
{
	FScopeLock Lock(&DataGuard);

	if (ChannelNums == 0 && ConstantQResultBuffer.IsEmpty())
	{
		return;
	}

	if (ConstantQResultBuffer.Num() != ChannelNums)
	{
		ConstantQResultBuffer.SetNum(ChannelNums);
	}

	if (!ConstantQResultBuffer.IsValidIndex(ChannelIndex))
	{
		return;
	}

	ConstantQResultBuffer[ChannelIndex] = Results;

	UpdateConstantQAnalysisData();

	if (ChannelIndex == ChannelNums - 1)
	{
		if (OnAnalysisResult.IsBound())
		{
			TArray<FConstantQResults> BroadcastBuffer = ConstantQResultBuffer;
			TWeakObjectPtr<UDreamAudioAnalyzerComponent_ConstantQ> WeakThis(this);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, BroadcastBuffer]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->UpdateConstantQAnalysisData();

					if (WeakThis->OnAnalysisResult.IsBound())
					{
						WeakThis->Callback(FDreamAudioAnalyzerResult(WeakThis->AnalysisTextures, BroadcastBuffer));
					}
				}
			});
		}
	}
}

void UDreamAudioAnalyzerComponent_ConstantQ::UpdateConstantQAnalysisData()
{
	if (!ConstantQAnalyzer)
	{
		return;
	}

	if (!bEnableCreateAnalysisTexture || !bIsCreated || AnalysisTextures.IsEmpty())
	{
		return;
	}

	FScopeLock Lock(&DataGuard);

	if (ConstantQResultBuffer.Num() < ChannelNums)
	{
		return;
	}

	for (int32 ChannelIdx = 0; ChannelIdx < ChannelNums; ++ChannelIdx)
	{
		if (!AnalysisTextures.IsValidIndex(ChannelIdx) || !ConstantQResultBuffer.IsValidIndex(ChannelIdx))
		{
			continue;
		}

		UTexture2D* Texture = AnalysisTextures[ChannelIdx];
		const TArray<float>& SpectrumData = ConstantQResultBuffer[ChannelIdx].SpectrumValues;

		if (Texture && !SpectrumData.IsEmpty())
		{
			UpdateTextureFromSpectrum(Texture, SpectrumData);
		}
	}
}
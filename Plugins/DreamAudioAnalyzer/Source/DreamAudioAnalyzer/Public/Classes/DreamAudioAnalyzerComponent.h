// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Loudness.h"
#include "ConstantQ.h"
#include "Components/ActorComponent.h"
#include "DreamAudioAnalyzerComponent.generated.h"


UENUM(BlueprintType)
enum class EDreamAudioAnalyzerResultType : uint8
{
	None,
	ConstantQ,
	Loudness,
};

USTRUCT(BlueprintType)
struct FDreamAudioAnalyzerResult
{
	GENERATED_BODY()

public:
	FDreamAudioAnalyzerResult()
		: Type(EDreamAudioAnalyzerResultType::None)
	{
	}

	FDreamAudioAnalyzerResult(const TArray<TObjectPtr<UTexture2D>>& InTextures, const TArray<FConstantQResults>& InConstantQResults)
		: Textures(InTextures), Type(EDreamAudioAnalyzerResultType::ConstantQ), ConstantQResults(InConstantQResults)
	{
	}

	FDreamAudioAnalyzerResult(const FLoudnessResults& InResults)
		: Type(EDreamAudioAnalyzerResultType::Loudness), LoudnessResults(InResults)
	{
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<UTexture2D>> Textures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDreamAudioAnalyzerResultType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FConstantQResults> ConstantQResults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLoudnessResults LoudnessResults;
};

UCLASS(ClassGroup=(DreamComponent), meta=(BlueprintSpawnableComponent))
class DREAMAUDIOANALYZER_API UDreamAudioAnalyzerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnalysisResult, const FDreamAudioAnalyzerResult&, Result);

public:
	UDreamAudioAnalyzerComponent();
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bAutoStartAudioBus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UAudioBus* AnalysisAudioBus;

	UPROPERTY(BlueprintAssignable)
	FOnAnalysisResult OnAnalysisResult;

	UFUNCTION(BlueprintCallable)
	virtual void StartAnalysis();

	UFUNCTION(BlueprintCallable)
	virtual void StopAnalysis();

protected:
	int32 ChannelNums;
	virtual void Initialize();
	void Callback(const FDreamAudioAnalyzerResult& InResult) const { OnAnalysisResult.Broadcast(InResult); }
};

UCLASS(ClassGroup=(DreamComponent), meta=(BlueprintSpawnableComponent))
class DREAMAUDIOANALYZER_API UDreamAudioAnalyzerComponent_ConstantQ : public UDreamAudioAnalyzerComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAnalysisTextureLoaded, const TArray<UTexture2D*>&, Textures);

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	bool bEnableCreateAnalysisTexture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", Instanced)
	UConstantQSettings* ConstantQSettings;

	UPROPERTY(BlueprintAssignable)
	FOnAnalysisTextureLoaded OnAnalysisTextureLoaded;

	UFUNCTION(BlueprintPure, Category = "Dream Audio Analysis|Functions")
	TArray<UTexture2D*> GetAnalysisTexture() { return AnalysisTextures; }

	UFUNCTION(BlueprintPure, Category = "Dream Audio Analysis|Functions")
	UTexture2D* GetAnalysisTextureAtChannel(int32 Channel) { return AnalysisTextures.IsValidIndex(Channel) ? AnalysisTextures[Channel] : nullptr; }


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void Initialize() override;
	virtual void StartAnalysis() override;
	virtual void StopAnalysis() override;
	void InitializeAnalysisTexture();
	void UpdateTextureFromSpectrum(UTexture2D* InTexture, const TArray<float>& InSpectrumData);

	bool bIsCreated = false;
	FCriticalSection DataGuard;
	TArray<FConstantQResults> ConstantQResultBuffer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> AnalysisTextures;

	UPROPERTY(Transient)
	UConstantQAnalyzer* ConstantQAnalyzer;

	UFUNCTION()
	void OnAnalysisConstantQResults(UConstantQAnalyzer* Analyzer, int32 ChannelIndex, const FConstantQResults& Results);
	void UpdateConstantQAnalysisData();
};

UCLASS(ClassGroup=(DreamComponent), meta=(BlueprintSpawnableComponent))
class DREAMAUDIOANALYZER_API UDreamAudioAnalyzerComponent_Loudness : public UDreamAudioAnalyzerComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", Instanced)
	ULoudnessSettings* LoudnessSettings;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Initialize() override;
	virtual void StartAnalysis() override;
	virtual void StopAnalysis() override;
	UPROPERTY(Transient)
	ULoudnessAnalyzer* LoudnessAnalyzer;

	UFUNCTION()
	void OnAnalysisLoudnessResults(const FLoudnessResults& Results);
};

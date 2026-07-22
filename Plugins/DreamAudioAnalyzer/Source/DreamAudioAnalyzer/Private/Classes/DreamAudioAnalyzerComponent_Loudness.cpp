#include "Classes/DreamAudioAnalyzerComponent.h"

void UDreamAudioAnalyzerComponent_Loudness::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LoudnessAnalyzer)
	{
		LoudnessAnalyzer->StopAnalyzing();
		LoudnessAnalyzer->OnLatestOverallLoudnessResults.RemoveDynamic(this, &UDreamAudioAnalyzerComponent_Loudness::OnAnalysisLoudnessResults);
	}

	Super::EndPlay(EndPlayReason);
}

void UDreamAudioAnalyzerComponent_Loudness::Initialize()
{
	Super::Initialize();

	UWorld* ThisWorld = GetWorld();
	if (!ThisWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("World is nullptr"));
		return;
	}

	const FAudioDeviceHandle AudioDevice = ThisWorld->GetAudioDevice();

	LoudnessAnalyzer = NewObject<ULoudnessAnalyzer>(this);
	LoudnessAnalyzer->Settings = LoudnessSettings;
	LoudnessAnalyzer->OnLatestOverallLoudnessResults.AddUniqueDynamic(this, &UDreamAudioAnalyzerComponent_Loudness::OnAnalysisLoudnessResults);
	LoudnessAnalyzer->StartAnalyzing(AudioDevice.GetDeviceID(), AnalysisAudioBus);
}

void UDreamAudioAnalyzerComponent_Loudness::StartAnalysis()
{
	Super::StartAnalysis();

	if (!LoudnessAnalyzer)
	{
		return;
	}

	LoudnessAnalyzer->StartAnalyzing(this, AnalysisAudioBus);
}

void UDreamAudioAnalyzerComponent_Loudness::StopAnalysis()
{
	Super::StopAnalysis();

	if (!LoudnessAnalyzer)
	{
		return;
	}

	LoudnessAnalyzer->StopAnalyzing(this);
}

void UDreamAudioAnalyzerComponent_Loudness::OnAnalysisLoudnessResults(const FLoudnessResults& Results)
{
	if (OnAnalysisResult.IsBound())
	{
		Callback(FDreamAudioAnalyzerResult(Results));
	}
}

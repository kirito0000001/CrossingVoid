// Fill out your copyright notice in the Description page of Project Settings.


#include "Classes/DreamAudioAnalyzerComponent.h"

#include "AudioMixerBlueprintLibrary.h"

UDreamAudioAnalyzerComponent::UDreamAudioAnalyzerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDreamAudioAnalyzerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UDreamAudioAnalyzerComponent::BeginPlay()
{
	Super::BeginPlay();

	switch (AnalysisAudioBus->AudioBusChannels)
	{
	case EAudioBusChannels::Mono:
		ChannelNums = 1;
		break;
	case EAudioBusChannels::Stereo:
		ChannelNums = 2;
		break;
	case EAudioBusChannels::Quad:
		ChannelNums = 4;
		break;
	case EAudioBusChannels::FivePointOne:
		ChannelNums = 6;
		break;
	case EAudioBusChannels::SevenPointOne:
		ChannelNums = 8;
		break;
	default:
		check(false);
	}

	Initialize();
}

void UDreamAudioAnalyzerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAnalysis();
	Super::EndPlay(EndPlayReason);
}

void UDreamAudioAnalyzerComponent::StartAnalysis()
{
}

void UDreamAudioAnalyzerComponent::StopAnalysis()
{
}

void UDreamAudioAnalyzerComponent::Initialize()
{
	if (bAutoStartAudioBus)
	{
		UAudioMixerBlueprintLibrary::StartAudioBus(this, AnalysisAudioBus);
	}
}

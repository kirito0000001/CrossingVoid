// Fill out your copyright notice in the Description page of Project Settings.


#include "DreamAudioAnalyzerEditorBlueprint.h"

void UDreamAudioAnalyzerEditorBlueprint::AddUniqueAudioSoundBus(UAudioBus* InAudioBus, USoundWave* InSoundWave)
{
	if (!InSoundWave || !InAudioBus) return;

	InSoundWave->bEnableBusSends = true;

	for (const auto& BusSendInfo : InSoundWave->BusSends)
	{
		if (BusSendInfo.AudioBus == InAudioBus)
		{
			return;
		}
	}

	FSoundSourceBusSendInfo BusSendInfo;
	BusSendInfo.AudioBus = InAudioBus;
	BusSendInfo.SendLevel = 1.0f;

	InSoundWave->BusSends.Add(BusSendInfo);
	bool bDirty = InSoundWave->MarkPackageDirty();
}

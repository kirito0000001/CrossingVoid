// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DreamAudioAnalyzerEditorBlueprint.generated.h"

/**
 * 
 */
UCLASS()
class DREAMAUDIOANALYZEREDITOR_API UDreamAudioAnalyzerEditorBlueprint : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Meta = (DevelopmentOnly), Category = "Dream Audio Analyzer")
	static void AddUniqueAudioSoundBus(UAudioBus* InAudioBus, USoundWave* InSoundWave);
};

// Copyright 2022 - 2024 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DreamTaskType.generated.h"

/**
 * 
 */
UCLASS()
class DREAMGAMEPLAYTASK_API UDreamTaskType : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TypeName = FName(TEXT("TaskType"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText TypeDisplayName = FText::FromString(TEXT("New Task Type"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> TypeIcon = nullptr;
};

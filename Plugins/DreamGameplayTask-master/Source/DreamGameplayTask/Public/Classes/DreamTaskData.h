// Copyright 2022 - 2025 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamTaskType.h"
#include "UObject/Object.h"
#include "DreamTaskData.generated.h"

/**
 * Custom Task Data
 */
UCLASS(EditInlineNew, Blueprintable)
class DREAMGAMEPLAYTASK_API UDreamTaskData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UTexture2D> TaskIcon;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UTexture2D> TaskImage;

	UPROPERTY(EditAnywhere)
	UDreamTaskType* TaskType;

public:
	UFUNCTION(BlueprintPure)
	UTexture2D* GetTaskIcon() const;

	UFUNCTION(BlueprintPure)
	UTexture2D* GetTaskImage() const;

	UFUNCTION(BlueprintPure)
	UDreamTaskType* GetTaskType() const;

public:
	virtual UWorld* GetWorld() const override;
};

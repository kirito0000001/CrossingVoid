// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CommonLaunchBlueprint.generated.h"

/**
 * 
 */
UCLASS()
class COMMONLAUNCH_API UCommonLaunchBlueprint : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CommonLaunch")
	static bool LaunchApp(FString Path, FString Args);

	UFUNCTION(BlueprintCallable, Category = "CommonLaunch")
	static void LaunchAppAdvanced(FString Path, FString Args, bool& bIsLaunch, int32& ProcessId, bool bLaunchDetached = true, bool bLaunchHidden = false,
	                              bool bLaunchReallyHidden = false,
	                              int PriorityModifier = 0);
};

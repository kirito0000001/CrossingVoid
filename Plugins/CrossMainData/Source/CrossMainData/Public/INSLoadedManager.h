// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "INSLoadedManager.generated.h"

/**
 * 用于加载资产用
 */
UCLASS()
class CROSSMAINDATA_API UINSLoadedManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public://变量
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category="加载管理器",DisplayName="资产")
	TSet<UObject*> LoadedList;
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category="加载管理器",DisplayName="类")
	TSet<UClass*> ClassList;
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category="加载管理器",DisplayName="基础资产")
	TSet<UObject*> BaseLoadedList;
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category="加载管理器",DisplayName="基础类")
	TSet<UClass*> BaseClassList;
public://函数
	UFUNCTION(BlueprintCallable,Category="加载管理器",DisplayName="卸载资产")
	void ClearAsset()
	{
		LoadedList.Empty();
		ClassList.Empty();
	}
};

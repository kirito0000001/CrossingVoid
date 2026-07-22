// Copyright 2025 Dream Moon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DreamAccountSettings.generated.h"

/**
 * 
 */
UCLASS(Config=DreamAccountSettings, DefaultConfig)
class DREAMACCOUNT_API UDreamAccountSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("DreamPlugin"); }
	virtual FName GetSectionName() const override { return TEXT("DreamAccount"); }

public:
	static UDreamAccountSettings* Get();
	
	/**
	 * AccountServerApiURL - 配置账号服务器API的URL地址
	 * 
	 * 该属性用于存储账号服务器的API接口地址，可通过编辑器修改，
	 * 支持蓝图读写操作，并且会保存到配置文件中。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config)
	FString AccountServerURL = TEXT("https://api.xxx.com");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config)
	float TimeoutTime = 5.0f;
};

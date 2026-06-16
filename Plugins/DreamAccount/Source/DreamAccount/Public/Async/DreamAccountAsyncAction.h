// Copyright 2025 Dream Moon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamAccountSubsystem.h"
#include "DreamAccountTypes.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "DreamAccountAsyncAction.generated.h"

/**
 * 委托声明：用于用户操作完成后的回调
 * @param Result 操作结果，包含成功或失败的信息
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDreamAccountActionUserCallback, FDreamAccountResult, Result);

/**
 * 用户注册
 * 该类继承自UBlueprintAsyncActionBase，用于在蓝图中异步执行用户注册操作。
 */
UCLASS()
class DREAMACCOUNT_API UDreamAccountAsyncAction_UserRegister : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * 用户注册
	 * @param WorldContextObject 世界上下文对象
	 * @param User 用户信息结构体，包含注册所需的数据
	 * @return 返回一个异步操作实例，用于监听注册结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Dream Account", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UDreamAccountAsyncAction_UserRegister* UserRegister(UObject* WorldContextObject, FDreamAccountInfo User);

	virtual void Activate() override;

	/** 注册成功的回调事件 */
	UPROPERTY(BlueprintAssignable)
	FDreamAccountActionUserCallback OnSuccess;

	/** 注册失败的回调事件 */
	UPROPERTY(BlueprintAssignable)
	FDreamAccountActionUserCallback OnFailure;

protected:
	/** 子系统引用，用于与账户系统交互 */
	UPROPERTY()
	UDreamAccountSubsystem* Subsystem;

	/** 存储用户注册信息 */
	UPROPERTY()
	FDreamAccountInfo Info;
};

/**
 * 用户登录
 * 该类继承自UBlueprintAsyncActionBase，用于在蓝图中异步执行用户登录操作。
 */
UCLASS()
class DREAMACCOUNT_API UDreamAccountAsyncAction_UserLogin : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * 用户登录
	 * @param WorldContextObject 世界上下文对象
	 * @param User 用户信息结构体，包含登录所需的凭证数据
	 * @return 返回一个异步操作实例，用于监听登录结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Dream Account", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UDreamAccountAsyncAction_UserLogin* UserLogin(UObject* WorldContextObject, FDreamAccountInfo User);

	virtual void Activate() override;

	/** 登录成功的回调事件 */
	UPROPERTY(BlueprintAssignable)
	FDreamAccountActionUserCallback OnSuccess;

	/** 登录失败的回调事件 */
	UPROPERTY(BlueprintAssignable)
	FDreamAccountActionUserCallback OnFailure;

protected:
	/** 子系统引用，用于与账户系统交互 */
	UPROPERTY()
	UDreamAccountSubsystem* Subsystem;

	/** 存储用户登录信息 */
	UPROPERTY()
	FDreamAccountInfo Info;
};

/**
 * 用户身份验证
 * 该类继承自UBlueprintAsyncActionBase，用于在蓝图中异步执行用户身份验证操作。
 */
UCLASS()
class DREAMACCOUNT_API UDreamAccountAsyncAction_UserAuthentication : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * 用户身份验证
	 * @param WorldContextObject 世界上下文对象
	 * @return 返回一个异步操作实例，用于监听验证结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Dream Account", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UDreamAccountAsyncAction_UserAuthentication* UserAuthentication(UObject* WorldContextObject);

	virtual void Activate() override;

	/** 验证成功的回调事件 */
	UPROPERTY(BlueprintAssignable)
	FDreamAccountActionUserCallback OnSuccess;

	/** 验证失败的回调事件 */
	UPROPERTY(BlueprintAssignable)
	FDreamAccountActionUserCallback OnFailure;

protected:
	/** 子系统引用，用于与账户系统交互 */
	UPROPERTY()
	UDreamAccountSubsystem* Subsystem;
};

/**
 * @brief 异步Ping服务器的蓝图异步操作类
 * 
 * 该类用于在Unreal Engine中异步执行服务器Ping操作，
 * 通过HTTP请求测试服务器连接延迟，并提供成功和失败的回调事件。
 */
UCLASS()
class DREAMACCOUNT_API UDreamPingServer : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief 创建并启动服务器Ping操作
	 * 
	 * @param WorldContextObject 世界上下文对象
	 * @param InURL 要Ping的服务器URL地址
	 * @return 返回创建的UDreamPingServer对象实例，用于绑定回调事件
	 */
	UFUNCTION(BlueprintCallable, Category = "Dream Account", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UDreamPingServer* PingServer(UObject* WorldContextObject, const FString& InURL);

	// 定义服务器Ping回调委托，包含延迟时间参数
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDreamPingServerCallback, float, Time);

	// Ping成功时触发的事件回调
	UPROPERTY(BlueprintAssignable)
	FDreamPingServerCallback OnSuccess;

	// Ping失败时触发的事件回调
	UPROPERTY(BlueprintAssignable)
	FDreamPingServerCallback OnFailure;

public:
	virtual void Activate() override;

	// 存储要Ping的服务器URL地址
	UPROPERTY()
	FString URL;

	// 记录Ping请求开始的时间戳
	double StartTime = 0.0;
};

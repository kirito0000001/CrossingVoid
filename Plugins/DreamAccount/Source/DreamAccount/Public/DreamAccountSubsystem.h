// Copyright 2025 Dream Moon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "DreamAccountTypes.h"
#include "DreamAccountSubsystem.generated.h"

/**
 * @class UDreamAccountSubsystem
 * @brief 账户子系统，用于处理用户注册、登录、认证和登出等账户相关操作。
 *
 * 该类继承自 UEngineSubsystem，作为引擎中的一个子系统运行，
 * 提供与账户服务交互的接口，并管理当前用户的认证令牌。
 */
UCLASS()
class DREAMACCOUNT_API UDreamAccountSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief 动态委托定义：用于账户操作结果的回调。
	 * @param Result 操作结果信息。
	 */
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAccountResult, const FDreamAccountResult&, Result);

	/**
	 * @brief 多播动态委托定义：当用户令牌发生变化时触发。
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTokenChanged);

public:
	/**
	 * @brief 蓝图可绑定事件：当用户令牌变化时调用。
	 */
	UPROPERTY(BlueprintAssignable)
	FOnTokenChanged OnTokenChanged;

public:
	/**
	 * @brief 注册一个新用户。
	 *
	 * @param User 需要注册的用户信息。
	 * @param OnResult 注册完成后的回调函数。
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamAccount|Users")
	void UserRegister(FDreamAccountInfo User, FOnAccountResult OnResult);

	/**
	 * @brief 内部实现版本的用户注册方法。
	 *
	 * @param User 需要注册的用户信息。
	 * @param Callback 注册完成后的回调函数。
	 */
	void UserRegister_Internal(FDreamAccountInfo User, FDreamAccountResultCallback Callback);

	/**
	 * @brief 用户登录。
	 *
	 * @param User 登录所需的用户信息。
	 * @param OnResult 登录完成后的回调函数。
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamAccount|Users")
	void UserLogin(FDreamAccountInfo User, FOnAccountResult OnResult);

	/**
	 * @brief 内部实现版本的用户登录方法。
	 *
	 * @param User 登录所需的用户信息。
	 * @param Callback 登录完成后的回调函数。
	 */
	void UserLogin_Internal(FDreamAccountInfo User, FDreamAccountResultCallback Callback);

	/**
	 * @brief 对当前已登录用户进行身份验证（使用 Token）。
	 *
	 * @param OnResult 验证完成后的回调函数。
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamAccount|Users|Auth")
	void AuthenticationToken(FOnAccountResult OnResult);

	/**
	 * @brief 内部实现版本的身份验证方法。
	 *
	 * @param Callback 验证完成后的回调函数。
	 */
	void AuthenticationToken_Internal(FDreamAccountResultCallback Callback);

	/**
	 * @brief 用户登出，清除本地保存的用户状态。
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamAccount|Users")
	void UserLogout();

	/**
	 * @brief 清除当前用户的认证令牌。
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamAccount|Users|Auth")
	void ClearToken();

	/**
	 * @brief 获取当前用户的认证令牌。
	 *
	 * @return 当前用户的认证令牌字符串。
	 */
	UFUNCTION(BlueprintPure, Category = "DreamAccount|Users|Auth")
	FString GetToken() const { return Token; }

protected:
	/**
	 * @brief 设置当前用户的认证令牌，并触发 OnTokenChanged 事件。
	 *
	 * @param NewToken 新的认证令牌。
	 */
	void SetToken(FString NewToken);

	/**
	 * @brief 存储当前用户的认证令牌。
	 */
	FString Token;
};

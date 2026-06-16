// Copyright 2025 Dream Moon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamAccountSettings.h"
#include "DreamAccountTypes.h"
#include "Interfaces/IHttpRequest.h"

/**
 * FDreamAccountUtil类
 * 提供账户相关的工具函数，包括HTTP请求发送、JSON解析和错误处理功能
 */
class DREAMACCOUNT_API FDreamAccountUtil
{
public:
	/**
	 * 发送HTTP请求
	 * @param URL 请求的目标URL地址
	 * @param Verb HTTP请求方法（如GET、POST等）
	 * @param Content 请求体内容
	 * @param Headers HTTP请求头信息映射表
	 * @param OnComplete 请求完成后的回调函数，参数分别为：请求指针、响应指针、是否成功标志
	 */
	static void SendHttpRequest(
		const FString& URL,
		const FString& Verb,
		const FString& Content,
		const TMap<FString, FString>& Headers,
		const TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)>& OnComplete
	);

	/**
	 * 发送HTTP请求
	 * @param URL 请求的目标URL地址
	 * @param Verb HTTP请求方法（如GET、POST等）
	 * @param Headers HTTP请求头信息映射表
	 * @param OnComplete 请求完成后的回调函数，参数分别为：请求指针、响应指针、是否成功标志
	 */
	static void SendHttpRequest(
		const FString& URL,
		const FString& Verb,
		const TMap<FString, FString>& Headers,
		const TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)>& OnComplete
	);

	/**
	 * 从HTTP响应中解析JSON对象
	 * @param Response HTTP响应指针
	 * @return 解析后的JSON对象共享指针，解析失败时返回空指针
	 */
	static TSharedPtr<FJsonObject> ParseJsonFromResponse(
		FHttpResponsePtr Response);

	/**
	* 从JSON对象中解析账户信息
	* @param JsonObject JSON对象共享指针
	* @return 解析后的账户信息对象
	*/
	static FDreamAccountUser ParseAccountUserFromJson(
		TSharedPtr<FJsonObject> JsonObject);

	/**
	 *  从JSON对象中解析Token
	 * @param JsonObject JSON对象
	 * @return 解析后的Token
	 */
	static FString ParseTokenFromJson(
		TSharedPtr<FJsonObject> JsonObject);

	/**
	 * 处理通用错误响应
	 * @param Response HTTP响应指针
	 * @param Type 账户结果类型枚举值
	 * @param OnResult 账户结果回调函数
	 */
	static void HandleCommonErrorResponse(
		FHttpResponsePtr Response,
		EDreamAccountResultType Type,
		const FDreamAccountResultCallback& OnResult
	);

	static EDreamAccountErrorType GetErrorTypeFromString(const FString& ErrorString);
};

namespace FDreamAccountAPI
{
#define API_SERVER_URL			FString((UDreamAccountSettings::Get() != nullptr) ? UDreamAccountSettings::Get()->AccountServerURL : TEXT(""))
#define API_MAKE(API_URL)		FString(API_SERVER_URL + TEXT(API_URL))
#define API_REGISTER			API_MAKE("/api/account/register")
#define API_LOGIN				API_MAKE("/api/account/login")
#define API_AUTH				API_MAKE("/api/account/auth")
}

namespace FDreamAccountFields
{
	static FString FIELD_USER_NAME = TEXT("user_name");
	static FString FIELD_USER_PASSWORD = TEXT("user_password");
	static FString FIELD_USER_ID = TEXT("user_id");
	static FString FIELD_TOKEN = TEXT("token");
}

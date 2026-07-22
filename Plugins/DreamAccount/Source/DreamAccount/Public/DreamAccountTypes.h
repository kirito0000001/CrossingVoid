// Copyright 2025 Dream Moon. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamAccountTypes.generated.h"

struct FDreamAccountUser;
struct FDreamAccountResult;
enum class EDreamAccountResultType : uint8;
enum class EDreamAccountErrorType : uint8;

using FDreamAccountResultCallback = TFunction<void(const FDreamAccountResult&)>;

/**
 * @brief 账户操作结果类型枚举
 * 
 * 定义了用户账户相关操作的结果类型，用于标识不同的账户操作状态
 * 该枚举可以在蓝图中使用
 */
UENUM(BlueprintType)
enum class EDreamAccountResultType : uint8
{
	None, // 无操作/初始状态
	Register, // 注册操作
	Login, // 登录操作
	Auth, // 认证操作
};

/**
 * @brief 账户错误类型枚举类
 * 
 * 定义了账户相关操作可能出现的各种错误类型，用于标识和处理用户账户操作中的异常情况。
 * 该枚举类可在蓝图中使用，每个枚举值都配有显示名称以便在UI中展示。
 */
UENUM(BlueprintType)
enum class EDreamAccountErrorType : uint8
{
	UNKNOWN UMETA(DisplayName = "Unknown Error"), // 未知错误
	NORMAL UMETA(DisplayName = "Normal"), // 没有错误

	// 网络错误
	NETWORK_MISSING_FIELDS UMETA(DisplayName = "Missing Fields"), // 缺少必填字段
	NETWORK_INVALID_USERNAME UMETA(DisplayName = "Invalid Username"), // 用户名格式不正确
	NETWORK_INVALID_PASSWORD UMETA(DisplayName = "Invalid Password"), // 密码格式不正确
	NETWORK_USERNAME_EXISTS UMETA(DisplayName = "Username Exists"), // 用户名已存在
	NETWORK_TOO_MANY_REQUESTS UMETA(DisplayName = "Too Many Requests"), // 请求过于频繁，请稍后再试
	NETWORK_USER_NOT_FOUND UMETA(DisplayName = "User Not Found"), // 用户不存在
	NETWORK_INVALID_CREDENTIALS UMETA(DisplayName = "Invalid Credentials"), // 用户名或密码错误
	NETWORK_USER_BANNED UMETA(DisplayName = "User Banned"), // 您的账号已被封禁
	NETWORK_BAN_NOT_FOUND UMETA(DisplayName = "Ban Not Found"), // 未找到封禁记录
	NETWORK_USER_NOT_AUTHENTICATED UMETA(DisplayName = "User Not Authenticated"), // 未提供认证 Token
	NETWORK_INVALID_AUTH_HEADER UMETA(DisplayName = "Invalid Auth Header"), // 错误的认证头格式，应为 Bearer <token>
	NETWORK_INVALID_TOKEN UMETA(DisplayName = "Invalid Token"), // 无效或已过期的 Token
	NETWORK_INTERNAL_ERROR UMETA(DisplayName = "Internal Error"), // 服务器内部错误，请稍后重试
	NETWORK_VALIDATION_ERROR UMETA(DisplayName = "Validation Error"), // 数据验证失败
	NETWORK_ERROR UMETA(DisplayName = "Network Error"), // 网络错误

	// 本地错误
	LOCAL_INPUT_DATA_NOT_VALID UMETA(DisplayName = "Input Data Not Valid"), // 输入数据错误
	LOCAL_TOKEN_NOT_VALID UMETA(DisplayName = "Token Not Valid"), // 令牌无效
};

USTRUCT(BlueprintType)
struct FDreamAccountInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Password;

public:
	FString Serialize() const;
};

/**
 * FDreamAccountUser 结构体
 * 
 * 该结构体用于表示用户账户信息，包含用户的基本信息如用户名、密码、用户ID等。
 * 支持从JSON对象初始化用户数据。
 */
USTRUCT(BlueprintType)
struct FDreamAccountUser
{
	GENERATED_BODY()

public:
	/**
	 * 默认构造函数
	 * 
	 * 初始化一个空的用户账户对象
	 */
	FDreamAccountUser()
	{
	}

	/**
	 * 从TSharedRef<FJsonObject>构造函数
	 * 
	 * 通过传入的JSON对象引用初始化用户账户信息
	 * 
	 * @param InUserJsonObject 包含用户信息的JSON对象引用
	 */
	FDreamAccountUser(const TSharedRef<FJsonObject>& InUserJsonObject);

	/**
	 * 从TSharedPtr<FJsonObject>指针构造函数
	 * 
	 * 通过传入的JSON对象指针初始化用户账户信息
	 * 
	 * @param InUserJsonObject 指向包含用户信息的JSON对象的指针
	 */
	FDreamAccountUser(const TSharedPtr<FJsonObject>* InUserJsonObject);

public:
	/** 用户基本信息 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDreamAccountInfo UserInfo;

	/** 用户ID，默认值为9999 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UserID = 9999;
};


/**
 * @brief 账户操作结果结构体，用于存储账户注册或登录的结果信息
 * 
 * 该结构体继承自USTRUCT，可以在Unreal Engine的Blueprint中使用，
 * 用于封装账户操作的结果，包括操作类型、消息、错误原因、用户信息和访问令牌等
 */
USTRUCT(BlueprintType)
struct FDreamAccountResult
{
	GENERATED_BODY()

public:
	/**
	 * @brief 默认构造函数
	 * 
	 * 初始化账户结果结构体，将有效性标志设置为false
	 */
	FDreamAccountResult()
		: bIsValidResult(false)
	{
	}

	/**
	 * @brief 注册结果构造函数
	 * 
	 * 用于创建注册操作的结果对象
	 * 
	 * @param InResultType 操作结果类型枚举值
	 * @param InUser 账户用户信息对象
	 */
	FDreamAccountResult(EDreamAccountResultType InResultType, EDreamAccountErrorType InErrorType, FDreamAccountUser InUser)
		: ResultType(InResultType), ErrorType(InErrorType), User(InUser), bIsValidResult(true)
	{
	}

	/**
	 * @brief 登录结果构造函数
	 * 
	 * 用于创建登录操作的结果对象，包含访问令牌
	 * 
	 * @param InResultType 操作结果类型枚举值
	 * @param InErrorType 错误类型
	 * @param InUser 账户用户信息对象
	 * @param InToken 访问令牌字符串
	 */
	FDreamAccountResult(EDreamAccountResultType InResultType, EDreamAccountErrorType InErrorType, FDreamAccountUser InUser, FString InToken)
		: ResultType(InResultType), ErrorType(InErrorType), User(InUser), Token(InToken), bIsValidResult(true)
	{
	}

public:
	/** 操作结果类型，标识账户操作的具体结果 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDreamAccountResultType ResultType = EDreamAccountResultType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDreamAccountErrorType ErrorType = EDreamAccountErrorType::UNKNOWN;

	/** 账户用户信息对象，包含用户的基本信息 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDreamAccountUser User = FDreamAccountUser();

	/** 访问令牌字符串，用于用户身份验证 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Token = TEXT("");

	/** 结果有效性标志，标识该结果对象是否包含有效数据 */
	bool bIsValidResult;
};

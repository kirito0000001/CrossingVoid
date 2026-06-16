# DreamAccount

DreamAccount 是一个用于 Unreal Engine 的账号系统插件，提供账号注册、登录、登出等基础功能。

## 依赖服务端

本插件需要配合 [64hzAccountServer](https://github.com/TypeDreamMoon/64hzAccountServer) 服务端使用。

请先部署并启动服务端，详见其仓库说明。

## 安装方法

1. 将本插件放入你的 Unreal 项目的 `Plugins` 目录下。
2. 在 Unreal Editor 中启用 DreamAccount 插件。
3. 根据需要在项目设置中配置 DreamAccount。

## 使用说明

### 主要可用函数一览

#### UDreamAccountSubsystem（蓝图/代码调用）

- `void UserRegister(FDreamAccountInfo User, FOnAccountResult OnResult)`  用户注册
- `void UserLogin(FDreamAccountInfo User, FOnAccountResult OnResult)`  用户登录
- `void AuthenticationToken(FOnAccountResult OnResult)`  Token认证
- `void UserLogout()`  用户登出
- `void ClearToken()`  清除本地Token
- `FString GetToken() const`  获取当前Token

#### UDreamAccountAsyncAction（蓝图异步节点）

- `static UDreamAccountAsyncAction_UserRegister* UserRegister(UObject* WorldContextObject, FDreamAccountInfo User)`  异步注册
- `static UDreamAccountAsyncAction_UserLogin* UserLogin(UObject* WorldContextObject, FDreamAccountInfo User)`  异步登录
- `static UDreamAccountAsyncAction_UserAuthentication* UserAuthentication(UObject* WorldContextObject)`  异步Token认证
- `static UDreamPingServer* PingServer(UObject* WorldContextObject, const FString& InURL)`  Ping服务器

#### FDreamAccountUtil（静态工具函数，C++调用）

- `static void SendHttpRequest(...)`  发送HTTP请求（重载）
- `static TSharedPtr<FJsonObject> ParseJsonFromResponse(FHttpResponsePtr Response)`  解析HTTP响应为JSON
- `static FDreamAccountUser ParseAccountUserFromJson(TSharedPtr<FJsonObject> JsonObject)`  解析用户信息
- `static FString ParseTokenFromJson(TSharedPtr<FJsonObject> JsonObject)`  解析Token
- `static void HandleCommonErrorResponse(...)`  处理通用错误
- `static EDreamAccountErrorType GetErrorTypeFromString(const FString& ErrorString)`  错误类型字符串转枚举

#### UDreamAccountSettings（插件设置）

- `static UDreamAccountSettings* Get()`  获取设置单例
- `FString AccountServerURL`  账号服务端API地址
- `float TimeoutTime`  超时时间

#### 主要数据结构

- `FDreamAccountInfo`  用户名/密码结构体
- `FDreamAccountUser`  用户信息结构体
- `FDreamAccountResult`  账号操作结果结构体
- `EDreamAccountResultType`  账号操作类型枚举
- `EDreamAccountErrorType`  错误类型枚举

## 贡献与反馈

如有建议或问题，欢迎提交 Issue 或 PR。

> Powered By Dream Moon

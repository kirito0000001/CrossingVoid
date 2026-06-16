// Copyright 2025 Dream Moon. All Rights Reserved.

#include "DreamAccountUtil.h"

#include "DreamAccountSettings.h"
#include "HttpModule.h"
#include "Http.h"

void FDreamAccountUtil::SendHttpRequest(const FString& URL, const FString& Verb, const FString& Content, const TMap<FString, FString>& Headers, const TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)>& OnComplete)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(Verb);
	HttpRequest->SetContentAsString(Content);
	HttpRequest->SetTimeout(UDreamAccountSettings::Get()->TimeoutTime); // 30秒超时

	// 设置所有Header
	for (const TPair<FString, FString>& Pair : Headers)
	{
		HttpRequest->SetHeader(Pair.Key, Pair.Value);
	}

	HttpRequest->OnProcessRequestComplete().BindLambda(OnComplete);

	HttpRequest->ProcessRequest();
}

void FDreamAccountUtil::SendHttpRequest(const FString& URL, const FString& Verb, const TMap<FString, FString>& Headers, const TFunction<void(FHttpRequestPtr, FHttpResponsePtr, bool)>& OnComplete)
{
	SendHttpRequest(URL, Verb, TEXT(""), Headers, OnComplete);
}

TSharedPtr<FJsonObject> FDreamAccountUtil::ParseJsonFromResponse(FHttpResponsePtr Response)
{
	FString Content = Response->GetContentAsString();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		return JsonObject;
	}
	return nullptr;
}

FDreamAccountUser FDreamAccountUtil::ParseAccountUserFromJson(TSharedPtr<FJsonObject> JsonObject)
{
	FDreamAccountUser AuthUser;

	if (JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* UserObject;
		JsonObject->TryGetObjectField(TEXT("user"), UserObject);

		UserObject->Get()->TryGetStringField(FDreamAccountFields::FIELD_USER_NAME, AuthUser.UserInfo.Name);
		UserObject->Get()->TryGetNumberField(FDreamAccountFields::FIELD_USER_ID, AuthUser.UserID);
	}

	return AuthUser;
}

FString FDreamAccountUtil::ParseTokenFromJson(TSharedPtr<FJsonObject> JsonObject)
{
	FString Token;

	if (JsonObject.IsValid())
	{
		JsonObject->TryGetStringField(FDreamAccountFields::FIELD_TOKEN, Token);
	}

	return Token;
}

void FDreamAccountUtil::HandleCommonErrorResponse(FHttpResponsePtr Response, EDreamAccountResultType Type, const FDreamAccountResultCallback& OnResult)
{
	TSharedPtr<FJsonObject> Json = ParseJsonFromResponse(Response);
	FString Error = Json.IsValid() ? Json->GetStringField(TEXT("error")) : TEXT("UNKNOWN_ERROR");

	OnResult(FDreamAccountResult(Type, GetErrorTypeFromString(Error), FDreamAccountUser()));
}

EDreamAccountErrorType FDreamAccountUtil::GetErrorTypeFromString(const FString& ErrorString)
{
	if (ErrorString.IsEmpty())
	{
		return EDreamAccountErrorType::UNKNOWN;
	}

	static TMap<FString, EDreamAccountErrorType> ErrorTypeMap = {
		{TEXT("NORMAL"), EDreamAccountErrorType::NORMAL},
		{TEXT("MISSING_FIELDS"), EDreamAccountErrorType::NETWORK_MISSING_FIELDS},
		{TEXT("INVALID_USERNAME"), EDreamAccountErrorType::NETWORK_INVALID_USERNAME},
		{TEXT("INVALID_PASSWORD"), EDreamAccountErrorType::NETWORK_INVALID_PASSWORD},
		{TEXT("USERNAME_EXISTS"), EDreamAccountErrorType::NETWORK_USERNAME_EXISTS},
		{TEXT("TOO_MANY_REQUESTS"), EDreamAccountErrorType::NETWORK_TOO_MANY_REQUESTS},
		{TEXT("USER_NOT_FOUND"), EDreamAccountErrorType::NETWORK_USER_NOT_FOUND},
		{TEXT("INVALID_CREDENTIALS"), EDreamAccountErrorType::NETWORK_INVALID_CREDENTIALS},
		{TEXT("USER_BANNED"), EDreamAccountErrorType::NETWORK_USER_BANNED},
		{TEXT("BAN_NOT_FOUND"), EDreamAccountErrorType::NETWORK_BAN_NOT_FOUND},
		{TEXT("USER_NOT_AUTHENTICATED"), EDreamAccountErrorType::NETWORK_USER_NOT_AUTHENTICATED},
		{TEXT("INVALID_AUTH_HEADER"), EDreamAccountErrorType::NETWORK_INVALID_AUTH_HEADER},
		{TEXT("INVALID_TOKEN"), EDreamAccountErrorType::NETWORK_INVALID_TOKEN},
		{TEXT("INTERNAL_ERROR"), EDreamAccountErrorType::NETWORK_INTERNAL_ERROR},
		{TEXT("VALIDATION_ERROR"), EDreamAccountErrorType::NETWORK_VALIDATION_ERROR}
	};

	EDreamAccountErrorType* FoundErrorType = ErrorTypeMap.Find(ErrorString);
	if (FoundErrorType)
	{
		return *FoundErrorType;
	}

	return EDreamAccountErrorType::UNKNOWN;
}

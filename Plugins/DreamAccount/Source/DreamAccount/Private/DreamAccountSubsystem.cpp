// Copyright 2025 Dream Moon. All Rights Reserved.


#include "DreamAccountSubsystem.h"

#include "DreamAccountSettings.h"
#include "DreamAccountUtil.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

using namespace FDreamAccountAPI;
using namespace FDreamAccountFields;

void UDreamAccountSubsystem::UserRegister(FDreamAccountInfo User, FOnAccountResult OnResult)
{
	auto Callback = [OnResult](const FDreamAccountResult& Result)
	{
		if (OnResult.IsBound())
		{
			OnResult.Execute(Result);
		}
	};

	UserRegister_Internal(User, Callback);
}


void UDreamAccountSubsystem::UserRegister_Internal(FDreamAccountInfo User, FDreamAccountResultCallback Callback)
{
	if (User.Name.IsEmpty() || User.Password.IsEmpty())
	{
		Callback(FDreamAccountResult(
			EDreamAccountResultType::Register,
			EDreamAccountErrorType::LOCAL_INPUT_DATA_NOT_VALID,
			FDreamAccountUser()));
		return;
	}

	const UDreamAccountSettings* Settings = UDreamAccountSettings::Get();
	if (!Settings)
	{
		Callback(FDreamAccountResult(
			EDreamAccountResultType::Register,
			EDreamAccountErrorType::LOCAL_INPUT_DATA_NOT_VALID,
			FDreamAccountUser()));
		return;
	}

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/json;charset=UTF-8"));

	FDreamAccountUtil::SendHttpRequest(
		API_REGISTER,
		TEXT("POST"),
		User.Serialize(),
		Headers,
		[Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!bWasSuccessful || !Response.IsValid())
			{
				Callback(FDreamAccountResult(EDreamAccountResultType::Register, EDreamAccountErrorType::NETWORK_ERROR, FDreamAccountUser()));
				return;
			}

			if (Response->GetResponseCode() != 200 && Response->GetResponseCode() != 201)
			{
				FDreamAccountUtil::HandleCommonErrorResponse(Response, EDreamAccountResultType::Register, Callback);
				return;
			}

			TSharedPtr<FJsonObject> Json = FDreamAccountUtil::ParseJsonFromResponse(Response);
			FDreamAccountUser ResultUser = FDreamAccountUtil::ParseAccountUserFromJson(Json);

			Callback(FDreamAccountResult(EDreamAccountResultType::Register, EDreamAccountErrorType::NORMAL, ResultUser));
		});
}


void UDreamAccountSubsystem::UserLogin(FDreamAccountInfo User, FOnAccountResult OnResult)
{
	auto Callback = [OnResult](const FDreamAccountResult& Result)
	{
		if (OnResult.IsBound())
		{
			OnResult.Execute(Result);
		}
	};

	UserLogin_Internal(User, Callback);
}


void UDreamAccountSubsystem::UserLogin_Internal(FDreamAccountInfo User, FDreamAccountResultCallback Callback)
{
	if (User.Name.IsEmpty() || User.Password.IsEmpty())
	{
		Callback(FDreamAccountResult(EDreamAccountResultType::Login, EDreamAccountErrorType::LOCAL_INPUT_DATA_NOT_VALID, FDreamAccountUser()));
		return;
	}

	const UDreamAccountSettings* Settings = UDreamAccountSettings::Get();
	if (!Settings)
	{
		Callback(FDreamAccountResult(EDreamAccountResultType::Login, EDreamAccountErrorType::LOCAL_INPUT_DATA_NOT_VALID, FDreamAccountUser()));
		return;
	}

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/json;charset=UTF-8"));

	FDreamAccountUtil::SendHttpRequest(
		API_LOGIN,
		TEXT("POST"),
		User.Serialize(),
		Headers,
		[this, Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!bWasSuccessful || !Response.IsValid())
			{
				Callback(FDreamAccountResult(EDreamAccountResultType::Login, EDreamAccountErrorType::NETWORK_ERROR, FDreamAccountUser()));
				return;
			}

			if (Response->GetResponseCode() != 200 && Response->GetResponseCode() != 201)
			{
				FDreamAccountUtil::HandleCommonErrorResponse(Response, EDreamAccountResultType::Login, Callback);
				return;
			}

			TSharedPtr<FJsonObject> Json = FDreamAccountUtil::ParseJsonFromResponse(Response);

			FDreamAccountUser LoggedInUser = FDreamAccountUtil::ParseAccountUserFromJson(Json);
			FString mToken = FDreamAccountUtil::ParseTokenFromJson(Json);

			if (!mToken.IsEmpty())
			{
				SetToken(mToken);
			}

			Callback(FDreamAccountResult(EDreamAccountResultType::Login, EDreamAccountErrorType::NORMAL, LoggedInUser, Token));
		});
}


void UDreamAccountSubsystem::AuthenticationToken(FOnAccountResult OnResult)
{
	auto Callback = [OnResult](const FDreamAccountResult& Result)
	{
		if (OnResult.IsBound())
		{
			OnResult.Execute(Result);
		}
	};

	AuthenticationToken_Internal(Callback);
}


void UDreamAccountSubsystem::AuthenticationToken_Internal(FDreamAccountResultCallback Callback)
{
	if (Token.IsEmpty())
	{
		Callback(FDreamAccountResult(
			EDreamAccountResultType::Auth,
			EDreamAccountErrorType::LOCAL_TOKEN_NOT_VALID,
			FDreamAccountUser()));
		return;
	}

	const UDreamAccountSettings* Settings = UDreamAccountSettings::Get();
	if (!Settings)
	{
		Callback(FDreamAccountResult(
			EDreamAccountResultType::Auth,
			EDreamAccountErrorType::LOCAL_TOKEN_NOT_VALID,
			FDreamAccountUser()));
		return;
	}

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));

	FDreamAccountUtil::SendHttpRequest(
		API_AUTH,
		TEXT("GET"),
		Headers,
		[Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!bWasSuccessful || !Response.IsValid())
			{
				Callback(FDreamAccountResult(EDreamAccountResultType::Auth, EDreamAccountErrorType::NETWORK_ERROR, FDreamAccountUser()));
				return;
			}

			if (Response->GetResponseCode() != 200 && Response->GetResponseCode() != 201)
			{
				FDreamAccountUtil::HandleCommonErrorResponse(Response, EDreamAccountResultType::Auth, Callback);
				return;
			}

			TSharedPtr<FJsonObject> Json = FDreamAccountUtil::ParseJsonFromResponse(Response);

			FDreamAccountUser AuthUser = FDreamAccountUtil::ParseAccountUserFromJson(Json);

			Callback(FDreamAccountResult(EDreamAccountResultType::Auth, EDreamAccountErrorType::NORMAL, AuthUser));
		});
}


void UDreamAccountSubsystem::UserLogout()
{
	ClearToken();
}


void UDreamAccountSubsystem::ClearToken()
{
	SetToken(FString());
}


void UDreamAccountSubsystem::SetToken(FString NewToken)
{
	Token = NewToken;

	OnTokenChanged.Broadcast();
}

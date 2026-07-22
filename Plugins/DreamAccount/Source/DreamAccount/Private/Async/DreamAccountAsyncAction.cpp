// Copyright 2025 Dream Moon. All Rights Reserved.


#include "Async/DreamAccountAsyncAction.h"

#include "DreamAccountSettings.h"
#include "HttpModule.h"
#include "Kismet/GameplayStatics.h"

#define CREATE_NODE() ThisClass* Node = NewObject<ThisClass>();

UDreamAccountAsyncAction_UserRegister* UDreamAccountAsyncAction_UserRegister::UserRegister(UObject* WorldContextObject, FDreamAccountInfo User)
{
	CREATE_NODE()
	Node->Info = User;
	Node->Subsystem = GEngine->GetEngineSubsystem<UDreamAccountSubsystem>();
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UDreamAccountAsyncAction_UserRegister::Activate()
{
	if (Subsystem)
	{
		Subsystem->UserRegister_Internal(Info, [this](const FDreamAccountResult& Result)
		{
			if (Result.ErrorType == EDreamAccountErrorType::NORMAL)
			{
				OnSuccess.Broadcast(Result);
			}
			else
			{
				OnFailure.Broadcast(Result);
			}
		});
	}
	else
	{
		OnFailure.Broadcast(FDreamAccountResult());
	}

	SetReadyToDestroy();
}

UDreamAccountAsyncAction_UserLogin* UDreamAccountAsyncAction_UserLogin::UserLogin(UObject* WorldContextObject, FDreamAccountInfo User)
{
	CREATE_NODE()
	Node->Info = User;
	Node->Subsystem = GEngine->GetEngineSubsystem<UDreamAccountSubsystem>();
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UDreamAccountAsyncAction_UserLogin::Activate()
{
	if (Subsystem)
	{
		Subsystem->UserLogin_Internal(Info, [this](const FDreamAccountResult& Result)
		{
			if (Result.ErrorType == EDreamAccountErrorType::NORMAL)
			{
				OnSuccess.Broadcast(Result);
			}
			else
			{
				OnFailure.Broadcast(Result);
			}
		});
	}
	else
	{
		OnFailure.Broadcast(FDreamAccountResult());	
	}

	SetReadyToDestroy();
}

UDreamAccountAsyncAction_UserAuthentication* UDreamAccountAsyncAction_UserAuthentication::UserAuthentication(UObject* WorldContextObject)
{
	CREATE_NODE()
	Node->Subsystem =GEngine->GetEngineSubsystem<UDreamAccountSubsystem>();
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UDreamAccountAsyncAction_UserAuthentication::Activate()
{
	if (Subsystem)
	{
		Subsystem->AuthenticationToken_Internal([this](const FDreamAccountResult& Result)
		{
			if (Result.ErrorType == EDreamAccountErrorType::NORMAL)
			{
				OnSuccess.Broadcast(Result);
			}
			else
			{
				OnFailure.Broadcast(Result);
			}
		});
	}
	else
	{
		OnFailure.Broadcast(FDreamAccountResult());
	}

	SetReadyToDestroy();
}

UDreamPingServer* UDreamPingServer::PingServer(UObject* WorldContextObject, const FString& InURL)
{
	UDreamPingServer* Node = NewObject<UDreamPingServer>();
	Node->URL = InURL;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UDreamPingServer::Activate()
{
	StartTime = FPlatformTime::Seconds();

	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(UDreamAccountSettings::Get()->TimeoutTime);

	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr, bool bSuccess)
	{
		float PingMs = -1.0f;
		if (bSuccess)
		{
			double EndTime = FPlatformTime::Seconds();
			PingMs = static_cast<float>((EndTime - StartTime) * 1000.0);
			OnSuccess.Broadcast(PingMs);
		}
		else
		{
			OnFailure.Broadcast(-1.f);
		}

		SetReadyToDestroy();
	});

	Request->ProcessRequest();
}

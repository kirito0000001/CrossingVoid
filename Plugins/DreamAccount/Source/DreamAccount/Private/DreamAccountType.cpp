// Copyright 2025 Dream Moon. All Rights Reserved.

#include "DreamAccountTypes.h"
#include "DreamAccountUtil.h"

FString FDreamAccountInfo::Serialize() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(FDreamAccountFields::FIELD_USER_NAME, Name);
	JsonObject->SetStringField(FDreamAccountFields::FIELD_USER_PASSWORD, Password);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return JsonString;
}

FDreamAccountUser::FDreamAccountUser(const TSharedRef<FJsonObject>& InUserJsonObject)
{
	FString JsonUserName;
	bool bHasUserName = InUserJsonObject->TryGetStringField(FDreamAccountFields::FIELD_USER_NAME, JsonUserName);
	int32 JsonUserId;
	bool bHasUserId = InUserJsonObject->TryGetNumberField(FDreamAccountFields::FIELD_USER_ID, JsonUserId);

	UserInfo.Name = bHasUserName ? JsonUserName : FString();
	UserID = bHasUserId ? JsonUserId : 9999;
}

FDreamAccountUser::FDreamAccountUser(const TSharedPtr<FJsonObject>* InUserJsonObject)
{
	FString JsonUserName;
	bool bHasUserName = InUserJsonObject->Get()->TryGetStringField(FDreamAccountFields::FIELD_USER_NAME, JsonUserName);
	int32 JsonUserId;
	bool bHasUserId = InUserJsonObject->Get()->TryGetNumberField(FDreamAccountFields::FIELD_USER_ID, JsonUserId);

	UserInfo.Name = bHasUserName ? JsonUserName : FString();
	UserID = bHasUserId ? JsonUserId : 9999;
}

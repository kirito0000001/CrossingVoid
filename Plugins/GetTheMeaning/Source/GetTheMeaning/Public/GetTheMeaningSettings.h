#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GetTheMeaningSettings.generated.h"

UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, meta=(DisplayName="GetTheMeaning"))
class GETTHEMEANING_API UGetTheMeaningSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGetTheMeaningSettings();

	UPROPERTY(Config, EditAnywhere, Category="Risk Analysis", meta=(DisplayName="Identity Name Patterns"))
	TArray<FString> IdentityNamePatterns;

	virtual FName GetCategoryName() const override;
};

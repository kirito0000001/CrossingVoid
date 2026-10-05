#include "GetTheMeaningSettings.h"

UGetTheMeaningSettings::UGetTheMeaningSettings()
{
	IdentityNamePatterns = {
		TEXT("Guid"),
		TEXT("Uid"),
		TEXT("Id"),
		TEXT("Identifier"),
	};
}

FName UGetTheMeaningSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#pragma once

#include "CoreMinimal.h"
#include "ProjectRiskReport.h"

class UBlueprint;

class FBlueprintRiskAnalyzer
{
public:
	static FBlueprintRiskFacts Collect(UBlueprint* Blueprint, const TArray<FString>& IdentityNamePatterns);
	static bool IsIdentityLikeName(const FString& Name, const TArray<FString>& IdentityNamePatterns);
	static FString NormalizeTypeName(const FString& TypeName);
	static bool IsTrustedValidationFunction(const FString& FunctionName, const FString& OwnerClassName);
	static bool IsGuardTopologyProtected(bool bBranchReachable, bool bThenReachesConsumer, bool bElseReachesConsumer, bool bBypassExists);
	static bool IsContainerValueDefault(bool bValueConnected, bool bHasFieldPin, bool bFieldConnected);
};

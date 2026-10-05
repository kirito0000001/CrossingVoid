#pragma once

#include "CoreMinimal.h"

enum class EProjectRiskAccessKind : uint8
{
	Read,
	ExplicitWrite,
	DefaultWrite,
	ExternalSource,
};

struct FStructFieldAccessFact
{
	FString AssetPath;
	FString GraphName;
	FString NodeId;
	FString NodeTitle;
	FString StructPath;
	FString FieldName;
	FString FieldType;
	FString SourceExpression;
	EProjectRiskAccessKind AccessKind = EProjectRiskAccessKind::Read;
	bool bIdentityLike = false;
	bool bCriticalSink = false;
};

struct FContainerIdentityFact
{
	FString AssetPath;
	FString GraphName;
	FString NodeId;
	FString NodeTitle;
	FString ContainerExpression;
	FString KeyType;
	FString ValueStructPath;
	FString ValueFieldName;
	FString ValueFieldType;
	FString KeySource;
	FString ValueSource;
	bool bIdentityLike = false;
	bool bValueUsesDefault = false;
};

struct FMapFindFact
{
	FString AssetPath;
	FString GraphName;
	FString NodeId;
	FString NodeTitle;
	FString MapExpression;
	FString KeyExpression;
	FString ValueType;
	TArray<FString> ValueConsumers;
	bool bResultChecked = false;
};

struct FRpcIdentityFact
{
	FString AssetPath;
	FString GraphName;
	FString NodeId;
	FString NodeTitle;
	FString RpcName;
	FString ParameterName;
	FString ParameterType;
	FString SourceExpression;
	bool bRunOnServer = false;
	bool bCallSite = false;
	bool bIdentityLike = false;
	bool bCallerValidated = false;
	bool bServerValidated = false;
};

struct FBlueprintRiskFacts
{
	FString AssetPath;
	TArray<FStructFieldAccessFact> StructFieldAccesses;
	TArray<FContainerIdentityFact> ContainerIdentities;
	TArray<FMapFindFact> MapFinds;
	TArray<FRpcIdentityFact> RpcIdentities;
};

struct FProjectRiskFinding
{
	FString Severity;
	FString Confidence;
	FString Category;
	FString Message;
	FString Details;
	FString AssetPath;
	FString GraphName;
	FString NodeId;
	FString NodeTitle;
};

class FProjectRiskReport
{
public:
	static TArray<FProjectRiskFinding> Analyze(const TArray<FBlueprintRiskFacts>& Facts);
	static bool WriteReports(const TArray<FBlueprintRiskFacts>& Facts, const FString& OutputDirectory);
};

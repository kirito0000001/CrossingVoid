// 2025 Copyright Pandores Marketplace

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Document.generated.h"

UENUM(BlueprintType)
enum class EDocumentValueType : uint8
{
	Int32,
	Int64,
	Float,
	Double,
	String,
	Boolean,
	Array,
	ObjectId,
	// Unparsed 
	Json,
	Map,
	Null,
	Undefined
};

#define MONGODB_UE_4_26_OR_LATER ((ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION >= 5)

struct FDocumentValue;
struct FNullValue {};
struct FUndefinedValue {};
struct FNotSet {};

// We don't parse JSON as we have to convert
// it to string later anyway.
struct MONGODBDRIVER_API FJsonString
{
	FJsonString() = default;
	FJsonString(FString&&);
	FJsonString(const FString&);
	FJsonString(const FJsonString&);
	FJsonString(FJsonString&&);

	FJsonString& operator=(const FJsonString&);
	FJsonString& operator=(FJsonString&&);

	// Raw JSON string.
	FString Json;
};

USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FObjectId
{
	GENERATED_BODY()
public:
	FObjectId();

	FObjectId(const FString& ObjectId);

	FObjectId(FString&& ObjectId);

	FObjectId(const FObjectId& Other);
	FObjectId(FObjectId&& Other);

	FObjectId& operator=(const FString& Other);
	FObjectId& operator=(FString&& Other);
	FObjectId& operator=(const FObjectId& Other);
	FObjectId& operator=(FObjectId&& Other);

	friend bool operator==(const FObjectId& A, const FObjectId& B);
	friend bool operator!=(const FObjectId& A, const FObjectId& B);

	operator const FString& () const;

	FString ToString() const;

private:
	FString _ObjectId;
};

USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FDocumentValue
{
	GENERATED_BODY()
public:
	FDocumentValue();
	FDocumentValue(const int32&		InValue);
	FDocumentValue(const int64&		InValue);
	FDocumentValue(const uint8&		InValue);
	FDocumentValue(const double&	InValue);
	FDocumentValue(const float&		InValue);
	FDocumentValue(const FString&   InValue);
	FDocumentValue(const bool&	   bInValue);
	FDocumentValue(const TCHAR* const InValue);
	FDocumentValue(const TArray<FDocumentValue>&	InValue);
	FDocumentValue(const TMap<FString, FDocumentValue>&	InValue);
	FDocumentValue(const FObjectId& ObjectId);


	FDocumentValue(const FDocumentValue&);
	FDocumentValue(FDocumentValue&&) noexcept;

	FDocumentValue operator=(const FDocumentValue&);
	FDocumentValue operator=(FDocumentValue&&) noexcept;

	~FDocumentValue();

	operator int32()	const noexcept;
	operator int64()	const noexcept;
	operator uint8()	const noexcept;
	operator double()	const noexcept;
	operator float()	const noexcept;
	operator bool()		const noexcept;
	operator FString()	const noexcept;
	operator FObjectId()const noexcept;
	operator TArray<FDocumentValue>()	     const noexcept;
	operator TMap<FString, FDocumentValue>() const noexcept;

	bool IsNull() const;
	bool IsUndefined() const;

	static FDocumentValue Null();
	static FDocumentValue Undefined();
	static FDocumentValue FromJSON(FString JSON);

	void Visit(class FDocumentValueVisitor* const Visitor) const;

	FString ToJson() const;

	EDocumentValueType GetType() const;

private:
	FDocumentValue(FNullValue);
	FDocumentValue(FUndefinedValue);

private:
	TUniquePtr<class FDocumentValueInternalWrapper> Value;

private:
	friend class FDocumentValueInternalWrapper;
};

using FDocumentValueMap		= TMap<FString, FDocumentValue>;
using FDocumentValueArray	= TArray<FDocumentValue>;

/**
 * Visitor to get the exact value of a DocumentValue.
*/
class MONGODBDRIVER_API FDocumentValueVisitor
{
public:
	virtual void operator()(const bool    bValue) = 0;
	virtual void operator()(const int32    Value) = 0;
	virtual void operator()(const int64    Value) = 0;
	virtual void operator()(const uint8    Value) = 0;
	virtual void operator()(const double   Value) = 0;
	virtual void operator()(const float    Value) = 0;
	virtual void operator()(const FString& Value) = 0;
	virtual void operator()(const FObjectId&)	    = 0;
	virtual void operator()(const FUndefinedValue&) = 0;
	virtual void operator()(const FNullValue&)		= 0;
	virtual void operator()(const FNotSet&)			= 0;
	virtual void operator()(const FJsonString&)	    = 0;
	virtual void operator()(const FDocumentValueMap&	Value) = 0;
	virtual void operator()(const FDocumentValueArray&	Value) = 0;
};



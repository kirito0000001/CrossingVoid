// 2025 Copyright Pandores Marketplace

#include "MongoDB/Document.h"
#include "MongoDBDriver.h"

#include "DocumentBuilderHelper.h"

#if PLATFORM_LINUX
namespace std
{
	template <class _Arg, class _Result>
	using unary_function = __unary_function_keep_layout_base<_Arg, _Result>;
}
#endif

THIRD_PARTY_INCLUDES_START
#	pragma push_macro("check") 
#	undef check
#	define check BOOST_check
#	include <variant>
#	include "bsoncxx/json.hpp"
#	pragma pop_macro("check")
THIRD_PARTY_INCLUDES_END

using FDocumentValueInternal = std::variant<FNotSet, bool, int32, int64, uint8, double, float, FString, FUndefinedValue, FJsonString, 
	TArray<FDocumentValue>, FDocumentValueMap, FNullValue, FObjectId> ;

#define MAKE_DOCUMENT_VALUE_CTOR(Type)										\
	FDocumentValue::FDocumentValue(Type InValue) : FDocumentValue()			\
	{  Value->Value = InValue; }


#define MAKE_DOCUMENT_VALUE_OPERATOR(Type, Default)					\
	FDocumentValue::operator Type()	const noexcept					\
	{																\
		return GetInternalType<Type>(Value->Value, Default);		\
	}

/**
 * Wrapper to hide the variant as well as boost library to the header.
*/
class FDocumentValueInternalWrapper final
{
public:
	FDocumentValueInternalWrapper() {}

	FDocumentValueInternalWrapper(const FDocumentValueInternalWrapper& Other)
		: Value(Other.Value)
	{
	}

	FDocumentValueInternalWrapper(FDocumentValueInternalWrapper&& Other) 
		: Value(MoveTemp(Other.Value))
	{
	}

	FDocumentValueInternalWrapper& operator=(const FDocumentValueInternalWrapper& Other)
	{
		Value = Other.Value;
		return *this;
	}

	FDocumentValueInternalWrapper& operator=(FDocumentValueInternalWrapper&& Other)
	{
		Value = MoveTemp(Other.Value);
		return *this;
	}

	FDocumentValueInternalWrapper& operator=(FDocumentValue&& Other)
	{
		Value = MoveTemp(Other.Value->Value);
		return *this;
	}

	FDocumentValueInternal Value;

	operator FDocumentValueInternal& ()
	{
		return Value;
	}

	operator const FDocumentValueInternal& ()
	{
		return Value;
	}
};

class FDocumentValueTypeVisitor final : public FDocumentValueVisitor
{
private:
	EDocumentValueType Type = EDocumentValueType::Undefined;
public:
	EDocumentValueType Get()
	{
		return Type;
	}
public:
	virtual void operator()(const bool    bValue)
	{
		Type = EDocumentValueType::Boolean;
	}
	virtual void operator()(const int32    Value)
	{
		Type = EDocumentValueType::Int32;
	}
	virtual void operator()(const int64    Value)
	{
		Type = EDocumentValueType::Int64;
	}
	virtual void operator()(const uint8    Value)
	{
		Type = EDocumentValueType::Int32;
	}
	virtual void operator()(const double   Value)
	{
		Type = EDocumentValueType::Double;
	}
	virtual void operator()(const float    Value)
	{
		Type = EDocumentValueType::Float;
	}
	virtual void operator()(const FString& Value)
	{
		Type = EDocumentValueType::String;
	}
	virtual void operator()(const FDocumentValueArray& Value)
	{
		Type = EDocumentValueType::Array;
	}
	virtual void operator()(const FDocumentValueMap& Value)
	{
		Type = EDocumentValueType::Map;
	}
	virtual void operator()(const FUndefinedValue&)
	{
		Type = EDocumentValueType::Undefined;
	}
	virtual void operator()(const FNullValue&)
	{
		Type = EDocumentValueType::Null;
	}
	virtual void operator()(const FNotSet&)
	{
		Type = EDocumentValueType::Undefined;
	}
	virtual void operator()(const FJsonString&)
	{
		Type = EDocumentValueType::Json;
	}
	virtual void operator()(const FObjectId&)
	{
		Type = EDocumentValueType::ObjectId;
	}
};

FJsonString::FJsonString(FString&& Value)
	: Json(MoveTemp(Value))
{
}

FJsonString::FJsonString(const FString& Value)
	: Json(Value)
{
}

FJsonString::FJsonString(const FJsonString& JsonString)
	: Json(JsonString.Json)
{
}

FJsonString::FJsonString(FJsonString&& Other)
	: Json(MoveTemp(Other.Json))
{
}

FJsonString& FJsonString::operator=(const FJsonString& Other)
{
	Json = Other.Json;
	return *this;
}

FJsonString& FJsonString::operator=(FJsonString&& Other)
{
	Json = MoveTemp(Other.Json);
	return *this;
}

FObjectId::FObjectId() = default;

FObjectId::FObjectId(const FString& ObjectId)
	: _ObjectId(ObjectId)
{
}

FObjectId::FObjectId(FString&& ObjectId)
	: _ObjectId(MoveTemp(ObjectId))
{
}

FObjectId::FObjectId(const FObjectId& Other)
	: _ObjectId(Other._ObjectId)
{
}

FObjectId::FObjectId(FObjectId&& Other)
	: _ObjectId(MoveTemp(Other._ObjectId))
{
}

FObjectId& FObjectId::operator=(const FString& Other)
{
	_ObjectId = Other;
	return *this;
}

FObjectId& FObjectId::operator=(FString&& Other)
{
	_ObjectId = MoveTemp(Other);
	return *this;
}

FObjectId& FObjectId::operator=(const FObjectId& Other)
{
	_ObjectId = Other._ObjectId;
	
	return *this;
}

FObjectId& FObjectId::operator=(FObjectId&& Other)
{
	_ObjectId = MoveTemp(Other._ObjectId);

	return *this;
}

bool operator==(const FObjectId& A, const FObjectId& B)
{
	return A._ObjectId == B._ObjectId;
}

bool operator!=(const FObjectId& A, const FObjectId& B)
{
	return A._ObjectId != B._ObjectId;
}

FObjectId::operator const FString& () const
{
	return _ObjectId;
}

FString FObjectId::ToString() const
{
	return (FString)*this;
}

EDocumentValueType FDocumentValue::GetType() const
{
	FDocumentValueTypeVisitor Visitor;

	std::visit(Visitor, Value->Value);

	return Visitor.Get();
}

void FDocumentValue::Visit(FDocumentValueVisitor* const Visitor) const
{
	std::visit(*Visitor, Value->Value);
}

FString FDocumentValue::ToJson() const
{
	return UTF8_TO_TCHAR(NDocumentValueConverter::ToString(*this).c_str());
}

/* static */ FDocumentValue FDocumentValue::Null()
{
	return FDocumentValue(FNullValue());
}

/* static */ FDocumentValue FDocumentValue::Undefined()
{
	return FDocumentValue(FUndefinedValue());
}

/* static */ FDocumentValue FDocumentValue::FromJSON(FString JSON)
{
	FDocumentValue Value;

	Value.Value->Value = FJsonString(MoveTemp(JSON));

	return Value;
}

FDocumentValue::~FDocumentValue() = default;

FDocumentValue::FDocumentValue()
	: Value(MakeUnique<FDocumentValueInternalWrapper>())
{}

MAKE_DOCUMENT_VALUE_CTOR(const int32&);
MAKE_DOCUMENT_VALUE_CTOR(const int64&);
MAKE_DOCUMENT_VALUE_CTOR(const uint8&);
MAKE_DOCUMENT_VALUE_CTOR(const double&);
MAKE_DOCUMENT_VALUE_CTOR(const float&);
MAKE_DOCUMENT_VALUE_CTOR(const bool&);
MAKE_DOCUMENT_VALUE_CTOR(const FString&);
MAKE_DOCUMENT_VALUE_CTOR(const FObjectId&);
MAKE_DOCUMENT_VALUE_CTOR(const TArray<FDocumentValue>&);
MAKE_DOCUMENT_VALUE_CTOR(const FDocumentValueMap&);
MAKE_DOCUMENT_VALUE_CTOR(FUndefinedValue);
MAKE_DOCUMENT_VALUE_CTOR(FNullValue);

FDocumentValue::FDocumentValue(const TCHAR* const InValue) 
	: FDocumentValue(FString(InValue))
{
}

FDocumentValue::FDocumentValue(const FDocumentValue& Other)
	: Value(MakeUnique<FDocumentValueInternalWrapper>(*Other.Value))
{
}

FDocumentValue::FDocumentValue(FDocumentValue&& Other) noexcept
	: Value(MoveTemp(Other.Value))
{
}

FDocumentValue FDocumentValue::operator=(const FDocumentValue& Other)
{
	*Value = *Other.Value;
	return *this;
}

FDocumentValue FDocumentValue::operator=(FDocumentValue&& Other) noexcept
{
	Value = MoveTemp(Other.Value);
	return *this;
}

bool FDocumentValue::IsNull() const
{
	return GetType() == EDocumentValueType::Null;
}

bool FDocumentValue::IsUndefined() const
{
	return GetType() == EDocumentValueType::Undefined;
}

template<class T>
T GetInternalType(const FDocumentValueInternal& Value, const T & Default) noexcept
{
	try
	{
		return std::get<T>(Value);
	}
	catch (const std::exception&)
	{
		UE_LOG(LogMongoDB, Warning, TEXT("Tried to get DocumentValue as an invalid type."));
	}

	return Default;
}

FDocumentValue::operator float()	const noexcept
{
	try
	{
		return std::get<float>(Value->Value);
	}
	catch (const std::exception&)
	{
		return GetInternalType<double>(Value->Value,  0.);
	}

	return 0.f;
}

FDocumentValue::operator double()	const noexcept
{
	try
	{
		return std::get<double>(Value->Value);
	}
	catch (const std::exception&)
	{
		return GetInternalType<float>(Value->Value, 0.);
	}

	return 0.f;
}

MAKE_DOCUMENT_VALUE_OPERATOR(int32, 0);
MAKE_DOCUMENT_VALUE_OPERATOR(int64, 0);
MAKE_DOCUMENT_VALUE_OPERATOR(uint8, 0);
MAKE_DOCUMENT_VALUE_OPERATOR(bool, false);
MAKE_DOCUMENT_VALUE_OPERATOR(FString, TEXT(""));
MAKE_DOCUMENT_VALUE_OPERATOR(TArray<FDocumentValue>, TArray<FDocumentValue>());
MAKE_DOCUMENT_VALUE_OPERATOR(FDocumentValueMap, FDocumentValueMap());
MAKE_DOCUMENT_VALUE_OPERATOR(FObjectId, FObjectId());

#undef MAKE_DOCUMENT_VALUE_OPERATOR
#undef MAKE_DOCUMENT_VALUE_CTOR




// 2025 Copyright Pandores Marketplace

#include "DocumentBuilderHelper.h"

#include <list>

void FBuilderVisitor::operator()(const FDocumentValueArray& Value)
{
	Json << "[";

	uint32 ElementCount = Value.Num();

	if (Value.Num() > 0)
	{
		// Unwrapp the first to ommit comma comparison.
		Visit(Value[0]);

		for (int32 i = 1; i < Value.Num(); ++i)
		{
			Json << ",";
			Visit(Value[i]);
		}
	}

	Json << "]";
}

void FBuilderVisitor::operator()(const FDocumentValueMap& Value)
{
	Json << "{";

	uint32 Index = 0;

	const uint32 MemberCount = Value.Num();

	for (const auto& Member : Value)
	{
		Json << "\"" << TCHAR_TO_UTF8(*Member.Key) << "\":";

		Visit(Member.Value);

		if (Index < MemberCount - 1)
		{
			++Index;
			Json << ",";
		}
	}

	Json << "}";
}

bsoncxx::v_noabi::document::value NDocumentValueConverter::Convert(const FDocumentValue& Value)
{
	if (Value.IsUndefined())
	{
		return bsoncxx::from_json("{}");
	}

	return bsoncxx::from_json(NDocumentValueConverter::ToString(Value));
}

FDocumentValue NDocumentValueConverter::Convert(TCursor& Cursor)
{
	TArray<FDocumentValue> Values;
	
	{
		std::list<FDocumentValue> ValuesList;

		for (const TDocumentView& Document : Cursor)
		{
			TMap<FString, FDocumentValue> Doc;

			for (const bsoncxx::v_noabi::document::element& Elem : Document)
			{
				Doc.Emplace(UTF8_TO_TCHAR(Elem.key().data()), NDocumentValueConverter::ConvertElement(Elem));
			}

			ValuesList.emplace_back(MoveTemp(Doc));
		}

		Values.Reserve(ValuesList.size());
		for (auto&& Value : ValuesList)
		{
			Values.Emplace(MoveTemp(Value));
		}
	}


	return Values;
}

FDocumentValue NDocumentValueConverter::Convert(const TDocumentView& Element)
{
	TMap<FString, FDocumentValue> Document;

	for (const bsoncxx::v_noabi::document::element& Elem : Element)
	{
		Document.Emplace(UTF8_TO_TCHAR(Elem.key().data()), NDocumentValueConverter::ConvertElement(Elem));
	}

	return Document;
}

FDocumentValue NDocumentValueConverter::Convert(const TDocumentValue& Element)
{
	return Convert(Element.view());
}

FString NDocumentValueConverter::EscapeJsonString(const FString& Input)
{
	FString Output;
	for (auto C : Input)
	{
		switch (C)
		{
		case TEXT('\"'): Output += TEXT("\\\""); break;
		case TEXT('\\'): Output += TEXT("\\\\"); break;
		case TEXT('\b'): Output += TEXT("\\b");  break;
		case TEXT('\f'): Output += TEXT("\\f");  break;
		case TEXT('\n'): Output += TEXT("\\n");  break;
		case TEXT('\r'): Output += TEXT("\\r");  break;
		case TEXT('\t'): Output += TEXT("\\t");  break;
		default: Output += C;
		}
	}
	return Output;
}

std::string NDocumentValueConverter::ToString(const FDocumentValue& Value)
{
	FBuilderVisitor Visitor;

	Visitor.Visit(Value);

	return Visitor.Get();
}

FDocumentValue NDocumentValueConverter::ConvertElement(const bsoncxx::v_noabi::array::element& Element)
{
	switch (Element.type())
	{

	case TType::k_array:
	{
		const bsoncxx::v_noabi::array::view& Array = Element.get_array().value;

		TArray<FDocumentValue> DocumentValues;

		DocumentValues.Reserve((int32)Array.length());

		for (const bsoncxx::v_noabi::array::element& Elem : Array)
		{
			DocumentValues.Emplace(NDocumentValueConverter::ConvertElement(Elem));
		}

		return DocumentValues;
	}

	case TType::k_bool:
	{
		return (bool)Element.get_bool();
	}

	case TType::k_date:
	{
		return FDocumentValue(static_cast<int64>(Element.get_date().value.count()));
	}

	case TType::k_document:
	{
		TMap<FString, FDocumentValue> DocumentValues;

		for (const bsoncxx::v_noabi::document::element& Elem : Element.get_document().view())
		{
			DocumentValues.Emplace(UTF8_TO_TCHAR(Elem.key().data()), NDocumentValueConverter::ConvertElement(Elem));
		}

		return DocumentValues;
	}

	case TType::k_double:
	{
		return Element.get_double().value;
	}

	case TType::k_int32:
	{
		return (int32)Element.get_int32().value;
	}

	case TType::k_int64:
	{
		return (int64)Element.get_int64().value;
	}

	case TType::k_oid:
	{
		return BytesToHex((uint8*)Element.get_oid().value.bytes(), 12 * sizeof(uint8)).ToLower();
	}

	case TType::k_code:
	{
		return UTF8_TO_TCHAR(Element.get_code().code.data());
	}

	case TType::k_null:
	{
		return FDocumentValue::Null();
	}

	case TType::k_decimal128:
	{
		return UTF8_TO_TCHAR(Element.get_decimal128().value.to_string().c_str());
	}

	case TType::k_timestamp:
	{
		return (int64)Element.get_timestamp().timestamp;
	}
	case TType::k_string:
	{
		auto Value = Element.get_string().value;
			return FString(Value.size(), FUTF8ToTCHAR(Value.data(), Value.size()).Get());
	}
	}

	// We should add support for this type.
	return TEXT("TYPE_NOT_SUPPORTED");
}

FDocumentValue NDocumentValueConverter::ConvertElement(const bsoncxx::v_noabi::document::element& Element)
{
	switch (Element.type())
	{

	case TType::k_array:
	{
		const bsoncxx::v_noabi::array::view& Array = Element.get_array().value;

		TArray<FDocumentValue> DocumentValues;

		DocumentValues.Reserve((int32)Array.length());

		for (const bsoncxx::v_noabi::array::element& Elem : Array)
		{
			DocumentValues.Emplace(ConvertElement(Elem));
		}

		return DocumentValues;
	}

	case TType::k_bool:
	{
		return (bool)Element.get_bool();
	}

	case TType::k_date:
	{
		return FDocumentValue(static_cast<int64>(Element.get_date().value.count()));
	}

	case TType::k_document:
	{
		TMap<FString, FDocumentValue> DocumentValues;

		for (const bsoncxx::v_noabi::document::element& Elem : Element.get_document().view())
		{
			DocumentValues.Emplace(UTF8_TO_TCHAR(Elem.key().data()), NDocumentValueConverter::ConvertElement(Elem));
		}

		return DocumentValues;
	}

	case TType::k_double:
	{
		return Element.get_double().value;
	}

	case TType::k_int32:
	{
		return (int32)Element.get_int32().value;
	}

	case TType::k_int64:
	{
		return (int64)Element.get_int64().value;
	}

	case TType::k_oid:
	{
		return FObjectId(BytesToHex((uint8*)Element.get_oid().value.bytes(), 12 * sizeof(uint8)).ToLower());
	}

	case TType::k_code:
	{
		return UTF8_TO_TCHAR(Element.get_code().code.data());
	}

	case TType::k_null:
	{
		return FDocumentValue::Null();
	}

	case TType::k_decimal128:
	{
		return UTF8_TO_TCHAR(Element.get_decimal128().value.to_string().c_str());
	}

	case TType::k_timestamp:
	{
		return (int64)Element.get_timestamp().timestamp;
	}
	case TType::k_string:
	{
		auto Value = Element.get_string().value;
		return FUTF8ToTCHAR(Value.data(), Value.size()).Get();
	}
	}

	// We should add support for this type.
	return TEXT("TYPE_NOT_SUPPORTED");
}

mongocxx::options::find NDocumentValueConverter::Convert(const FMongoFindOptions& InOptions)
{
	mongocxx::options::find Options;

	Options.allow_partial_results(InOptions.bAllowPartialResults);
	Options.no_cursor_timeout(InOptions.bNoCursorTimeout);
	Options.show_record_id(InOptions.bShowRecordId);
	Options.return_key(InOptions.bReturnKey);

	if (!InOptions.Projection.IsUndefined())
	{
		Options.projection(Convert(InOptions.Projection));
	}

	if (!InOptions.Sort.IsUndefined())
	{
		Options.sort(Convert(InOptions.Sort));
	}

	if (InOptions.Skip >= 0)
	{
		Options.skip(InOptions.Skip);
	}

	if (!InOptions.Min.IsUndefined())
	{
		Options.min(Convert(InOptions.Min));
	}

	if (InOptions.MaxTime >= 0)
	{
		Options.max_time(std::chrono::milliseconds(InOptions.MaxTime));
	}

	if (InOptions.MaxAwaitTime >= 0)
	{
		Options.max_await_time(std::chrono::milliseconds(InOptions.MaxAwaitTime));
	}

	if (!InOptions.Max.IsUndefined())
	{
		Options.max(Convert(InOptions.Max));
	}

	if (InOptions.Limit >= 0)
	{
		Options.limit(InOptions.Limit);
	}

	if (InOptions.Comment.Len() > 0)
	{
		Options.comment(TCHAR_TO_UTF8(*InOptions.Comment));
	}

	if (!InOptions.Collation.IsUndefined())
	{
		Options.collation(Convert(InOptions.Collation));
	}

	if (InOptions.BatchSize >= 0)
	{
		Options.batch_size(InOptions.BatchSize);
	}

	return Options;
}


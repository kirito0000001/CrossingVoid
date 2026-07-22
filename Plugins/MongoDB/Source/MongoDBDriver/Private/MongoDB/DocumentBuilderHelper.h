// 2025 Copyright Pandores Marketplace

#pragma once

#ifndef __STDC_WANT_SECURE_LIB__
#define __STDC_WANT_SECURE_LIB__ 1
#endif

#include <sstream>
#include <string>

#include "CoreMinimal.h"
#include "MongoDB/Document.h"
#include "MongoDB/DatabaseConnector.h"

THIRD_PARTY_INCLUDES_START
#	include "bsoncxx/types.hpp"
#	include "bsoncxx/json.hpp"
#	include "mongocxx/cursor.hpp"
#	include "mongocxx/options/find.hpp"
THIRD_PARTY_INCLUDES_END

using TDocumentValue	= bsoncxx::v_noabi::document::value;
using TDocumentView		= bsoncxx::v_noabi::document::view;
using TCursor			= mongocxx::v_noabi::cursor;
using TElement			= bsoncxx::v_noabi::document::element;
using TType				= bsoncxx::type;

class FBuilderVisitor final : public FDocumentValueVisitor
{
private:
	std::stringstream Json;

public:
	
	FBuilderVisitor()
	{
	}

	void Visit(const FDocumentValue& Value)
	{
		Value.Visit(this);
	}

	std::string Get()
	{
		return Json.str();
	}

public:
	virtual void operator()(const bool bValue)
	{
		Json << (bValue ? "true" : "false");
	}

	virtual void operator()(const int32 Value)
	{
		Json << Value;
	}

	virtual void operator()(const int64 Value)
	{
		Json << "{\"$numberLong\":\"" << Value << "\"}";
	}

	virtual void operator()(const uint8 Value)
	{
		Json << Value;
	}

	virtual void operator()(const double Value)
	{
		Json << Value;
	}

	virtual void operator()(const float Value)
	{
		Json << Value;
	}

	virtual void operator()(const FString& Value)
	{
		Json << "\"" << TCHAR_TO_UTF8(*Value.Replace(TEXT("\""), TEXT("\\\""))) << "\"";
	}

	virtual void operator()(const FDocumentValueArray& Value);

	virtual void operator()(const FDocumentValueMap& Value);

	virtual void operator()(const FUndefinedValue&)
	{
		Json << "{\"$undefined\":true}";
	}

	virtual void operator()(const FNullValue&)
	{
		Json << "null";
	}

	virtual void operator()(const FNotSet&) {}

	virtual void operator()(const FJsonString& Value) 
	{
		Json << TCHAR_TO_UTF8(*Value.Json);
	}

	virtual void operator()(const FObjectId& Value)
	{
		Json << "{\"$oid\":\"" << TCHAR_TO_UTF8(*(const FString&)Value) << "\"}";
	}
};

namespace NDocumentValueConverter
{
	FString EscapeJsonString(const FString& Input);
	
	std::string ToString(const FDocumentValue& Value);

	bsoncxx::v_noabi::document::value Convert(const FDocumentValue& Value);

	FDocumentValue Convert(TCursor& Cursor);
	FDocumentValue Convert(const TDocumentView& Element);
	FDocumentValue Convert(const TDocumentValue& Element);

	FDocumentValue ConvertElement(const bsoncxx::v_noabi::document::element& Element);
	FDocumentValue ConvertElement(const bsoncxx::v_noabi::array::element& Element);

	mongocxx::options::find Convert(const FMongoFindOptions& Options);
};


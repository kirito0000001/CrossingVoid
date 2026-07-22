// 2025 Copyright Pandores Marketplace

#include "MongoDB/Client.h"

THIRD_PARTY_INCLUDES_START
#	include <bsoncxx/exception/exception.hpp>
#	include <mongocxx/client.hpp>
#	include <mongocxx/exception/query_exception.hpp>
#	include <mongocxx/exception/operation_exception.hpp>
THIRD_PARTY_INCLUDES_END

#include "Async/Async.h"

#include "MongoDBDriver.h"
#include "DocumentBuilderHelper.h"

#if PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4101)
#endif

using TUri			= mongocxx::v_noabi::uri;
using TClient		= mongocxx::v_noabi::client;
using TDatabase		= mongocxx::v_noabi::database;
using TCollection	= mongocxx::v_noabi::collection;
using TCursor		= mongocxx::v_noabi::cursor;
using TException	= mongocxx::v_noabi::exception;

#define MONGO_GET_CLIENT()		NClientHelper::GetClient(Protocole, Address, Port, AdditionalParameters)
#define MONGO_GET_DATABASE()	Client	.database(TCHAR_TO_UTF8(*DatabaseName))
#define MONGO_GET_COLLECTION()	Database.collection(TCHAR_TO_UTF8(*CollectionName))

#define MONGO_FAIL_TASK() bSuccess = false

#define THREAD_MOVE_ELEM_COPY(...) , ## __VA_ARGS__

#define THREAD_MOVE_ELEM(Elem, ...) Elem = MoveTemp(Elem) THREAD_MOVE_ELEM_COPY(__VA_ARGS__)

#define START_LEAVE_MAIN_THREAD(...)								\
	FString _Protocole = Protocole,									\
		_Address = Address,											\
		_AdditionalParameters = AdditionalParameters;				\
	int32 _Port = Port;												\
	AsyncTask(ENamedThreads::AnyThread, [							\
		Protocole = MoveTemp(_Protocole),							\
		Address = MoveTemp(_Address),								\
		AdditionalParameters = MoveTemp(_AdditionalParameters),		\
		Port = _Port,												\
		Callback = MoveTemp(Callback)								\
		THREAD_MOVE_ELEM_COPY(__VA_ARGS__)							\
	]() mutable -> void												\
	{ bool bSuccess = true

#define END_LEAVE_MAIN_THREAD(...) })

#define CALL_CALLBACK_GAME_THREAD(...)								\
	if (Callback.IsBound())											\
	{																\
		AsyncTask(ENamedThreads::GameThread, [						\
			Callback = MoveTemp(Callback),							\
			bSuccess												\
			THREAD_MOVE_ELEM_COPY(__VA_ARGS__)						\
		]() -> void													\
		{															\
			Callback.ExecuteIfBound(bSuccess, ## __VA_ARGS__);		\
		});															\
	}

namespace NClientHelper
{
	TClient GetClient(const FString& Protocole, const FString& Address, const int32 Port, const FString& AdditionalParameters)
	{
		TUri Uri(TCHAR_TO_UTF8(*FString::Printf(TEXT("%s://%s:%d?%s"), *Protocole, *Address, Port, *AdditionalParameters)));
		TClient Client(Uri);

		return Client;
	}
};

UMongoClient* UMongoClient::CreateClient(FString Protocole, FString Address, int32 Port, FString AdditionalParameters)
{
	UMongoClient* const Client = NewObject<UMongoClient>();

	Client->Protocole			 = MoveTemp(Protocole);
	Client->Address				 = MoveTemp(Address);
	Client->AdditionalParameters = MoveTemp(AdditionalParameters);
	Client->Port				 = Port;

	return Client;
}

void UMongoClient::ListDatabases(FMongoDatabasesCallback Callback)
{
	START_LEAVE_MAIN_THREAD();

	TArray<FDatabaseData> Databases;

	try
	{
		TClient Client = MONGO_GET_CLIENT();
		
		TCursor Cursor = Client.list_databases();

		for (const auto& Database : Cursor)
		{
			FDatabaseData& Data = Databases.Emplace_GetRef();
			for (const auto& Property : Database)
			{
				switch (Property.type())
				{
				case bsoncxx::v_noabi::type::k_double: Data.SizeOnDisk	= (float)Property.get_double();						break;
#if PLATFORM_LINUX
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
				case bsoncxx::v_noabi::type::k_string: Data.Name			= UTF8_TO_TCHAR(Property.get_string().value.data());	break;
#if PLATFORM_LINUX
#	pragma clang diagnostic push
#endif
				case bsoncxx::v_noabi::type::k_bool: Data.bHasAnyData	= Property.get_bool();								break;
				}
			}
		}
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to list databases: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Databases);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::DropDatabase(FString DatabaseName, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName);

	try
	{
		TClient		Client   = MONGO_GET_CLIENT();
		TDatabase	Database = MONGO_GET_DATABASE();
		
		Database.drop();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to drop database: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::CreateCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();

		Database.create_collection(TCHAR_TO_UTF8(*CollectionName));
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to create collection: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::DropCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		Collection.drop();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to drop collection: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD(); 	
}

void UMongoClient::InsertOne(FString DatabaseName, FString CollectionName, FDocumentValue Document, FMongoInsertCallback Callback) 
{
#if !PLATFORM_LINUX
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Document);

	FMongoInsertResult InsertResult;

	try
	{
		TClient	    Client		= MONGO_GET_CLIENT();
		TDatabase   Database	= MONGO_GET_DATABASE();
		TCollection Collection	= MONGO_GET_COLLECTION();

		try
		{
			const auto Result = Collection.insert_one(NDocumentValueConverter::Convert(Document));

			if (Result && Result.has_value())
			{
				InsertResult.InsertedId		= UTF8_TO_TCHAR(Result.value().inserted_id().get_oid().value.to_string().c_str());
				InsertResult.InsertedCount	= Result.value().result().inserted_count();
			}
		}
		catch (const bsoncxx::exception& _Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("InsertMany() failed. Reason: %s"), UTF8_TO_TCHAR(_Exception.what()));

			MONGO_FAIL_TASK();
		}
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to insert one: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(InsertResult);

	END_LEAVE_MAIN_THREAD();
#else
	Callback.ExecuteIfBound(false, {});
#endif
}

void UMongoClient::InsertMany(FString DatabaseName, FString CollectionName, TArray<FDocumentValue> Documents, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Documents);

	try
	{
		TClient	    Client     = MONGO_GET_CLIENT();
		TDatabase   Database   = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		try
		{
			std::vector<bsoncxx::document::value> RawDocuments;

			RawDocuments.reserve(Documents.Num());

			for (const auto& Document : Documents)
			{
				RawDocuments.push_back(NDocumentValueConverter::Convert(Document));
			}

			Collection.insert_many(RawDocuments);
		}
		catch (const bsoncxx::exception& _Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("InsertMany() failed. Reason: %s"), UTF8_TO_TCHAR(_Exception.what()));

			MONGO_FAIL_TASK();
		}
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to insert many. Rason: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::ListIndexes(FString DatabaseName, FString CollectionName, FMongoIndexesCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName);

	TArray<FDatabaseIndex> Indexes;

	try 
	{
		TClient	    Client     = MONGO_GET_CLIENT();
		TDatabase   Database   = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		TCursor RawIndexes = Collection.list_indexes();

		for (const auto& Index : RawIndexes)
		{
			FDatabaseIndex& CurrentIndex = Indexes.Emplace_GetRef();

			for (const auto& Info : Index)
			{
				switch (Info.type())
				{
				case bsoncxx::v_noabi::type::k_string:   
					CurrentIndex.Name = UTF8_TO_TCHAR(Info.get_value().get_string().value.data()); 
					break;
				case bsoncxx::v_noabi::type::k_int32:  
					CurrentIndex.ID = Info.get_int32(); 
					break;
				}
			}
		}
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to list indexes: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Indexes);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::RenameCollection(FString DatabaseName, FString CollectionName, FString NewCollectionName, bool bOverwriteExisting, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, NewCollectionName, bOverwriteExisting);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		Collection.rename(TCHAR_TO_UTF8(*NewCollectionName), bOverwriteExisting);
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to rename collection: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::ReplaceOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Replacement);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		try
		{
			Collection.replace_one(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Replacement));
		}
		catch (const bsoncxx::exception& _Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("ReplaceOne() failed. Reason: %s"), UTF8_TO_TCHAR(_Exception.what()));

			MONGO_FAIL_TASK();
		}
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to replace one: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::UpdateMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Update);

	try
	{
		TClient	    Client		= MONGO_GET_CLIENT();
		TDatabase   Database	= MONGO_GET_DATABASE();
		TCollection Collection	= MONGO_GET_COLLECTION();

		Collection.update_many(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Update));
	}
	catch (const bsoncxx::exception& _Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("UpdateMany() failed. Reason: %s"), UTF8_TO_TCHAR(_Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to update many: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::UpdateOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Update);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		Collection.update_one(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Update));
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to update one: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}
	catch (const bsoncxx::exception& _Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("UpdateOne() failed. Reason: %s"), UTF8_TO_TCHAR(_Exception.what()));

		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::CountDocuments(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCountCallback Callback) 
{
#if PLATFORM_LINUX
	Callback.ExecuteIfBound(false, 0);
#else
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter);

	int32 DocumentCount = 0;

	try 
	{
		TClient	    Client		= MONGO_GET_CLIENT();
		TDatabase   Database	= MONGO_GET_DATABASE();
		TCollection Collection	= MONGO_GET_COLLECTION();

		try
		{
			DocumentCount = Collection.count_documents(NDocumentValueConverter::Convert(Filter));
		}
		catch (const TException& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("CountDocuments() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

			MONGO_FAIL_TASK();
		}
		catch (const bsoncxx::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("CountDocuments() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

			MONGO_FAIL_TASK();
		}
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to count documents: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(DocumentCount);

	END_LEAVE_MAIN_THREAD();
#endif
}

void UMongoClient::GetEstimatedDocumentCount(FString DatabaseName, FString CollectionName, FMongoCountCallback Callback)
{
#if PLATFORM_LINUX
	Callback.ExecuteIfBound(false, 0);
#else // !PLATFORM_LINUX
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName);

	int32 DocumentCount = 0;

	try
	{
		TClient	    Client     = MONGO_GET_CLIENT();
		TDatabase   Database   = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		DocumentCount = Collection.estimated_document_count();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to get estimated document count: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(DocumentCount);

	END_LEAVE_MAIN_THREAD();
#endif // !PLATFORM_LINUX
}

void UMongoClient::CreateIndex(FString DatabaseName, FString CollectionName, FDocumentValue Keys, FDocumentValue IndexOptions, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Keys, IndexOptions);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		Collection.create_index(NDocumentValueConverter::Convert(Keys)
#if !PLATFORM_LINUX
			, NDocumentValueConverter::Convert(IndexOptions)
#endif
		);
	}
	catch (const bsoncxx::exception& _Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("CreateIndex() failed. Reason: %s"), UTF8_TO_TCHAR(_Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to create index: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::DeleteMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		Collection.delete_many(NDocumentValueConverter::Convert(Filter));
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("DeleteMany() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to delete many: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::DeleteOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter);

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		Collection.delete_one(NDocumentValueConverter::Convert(Filter));
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("DeleteOne() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to delete one: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD();

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::FindWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback)
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Options = MoveTemp(Options));

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Options));

		Result = NDocumentValueConverter::Convert(Found);
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Find() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::Find(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter);

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find(NDocumentValueConverter::Convert(Filter));

		Result = NDocumentValueConverter::Convert(Found);
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Find() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::FindOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback)
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Options = MoveTemp(Options));

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find_one(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Options));

		if (Found)
		{
			Result = NDocumentValueConverter::Convert(*Found);
		}
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("FindOne() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find one: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::FindOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter);

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find_one(NDocumentValueConverter::Convert(Filter));

		if (Found)
		{
			Result = NDocumentValueConverter::Convert(*Found);
		}
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("FindOne() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find one: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::FindOneAndDelete(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter);

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find_one_and_delete(NDocumentValueConverter::Convert(Filter));

		if (Found)
		{
			Result = NDocumentValueConverter::Convert(*Found);
		}
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("FindOneAndDelete() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find one and delete: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::FindOneAndReplace(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoDocumentCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Replacement);

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find_one_and_replace(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Replacement));

		if (Found)
		{
			Result = NDocumentValueConverter::Convert(*Found);
		}
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("FindOneAndReplace() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find one and replace: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::FindOneAndUpdate(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoDocumentCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, CollectionName, Filter, Update);

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();
		TCollection Collection = MONGO_GET_COLLECTION();

		auto Found = Collection.find_one_and_replace(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Update));

		if (Found)
		{
			Result = NDocumentValueConverter::Convert(*Found);
		}
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("FindOneAndUpdate() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to find one and update: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::RunCommand(FString DatabaseName, FDocumentValue Command, FMongoDocumentCallback Callback) 
{
	START_LEAVE_MAIN_THREAD(DatabaseName, Command);

	FDocumentValue Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();

		auto Return = Database.run_command(NDocumentValueConverter::Convert(Command));

		Result = NDocumentValueConverter::Convert(Return);
	}
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("RunCommand() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to run command: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
}

void UMongoClient::ListCollectionNames(FString DatabaseName, FMongoStringsCallback Callback) 
{
#if PLATFORM_LINUX
	UE_LOG(LogMongoDB, Error, TEXT("ListCollectionNames() is not available on Linux."));
	Callback.ExecuteIfBound(false, TArray<FString>());
#else // !PLATFORM_LINUX
	START_LEAVE_MAIN_THREAD(DatabaseName);

	TArray<FString> Result;

	try
	{
		TClient	    Client = MONGO_GET_CLIENT();
		TDatabase   Database = MONGO_GET_DATABASE();

		const std::vector<std::string> Collections = Database.list_collection_names();

		Result.Reserve(Collections.size());

		for (const std::string& Collection : Collections)
		{
			Result.Add(UTF8_TO_TCHAR(Collection.c_str()));
		}
	}	
	catch (const bsoncxx::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("ListCollectionNames() failed. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

		MONGO_FAIL_TASK();
	}
	catch (const TException& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to list collections: %s"), UTF8_TO_TCHAR(Exception.what()));
		MONGO_FAIL_TASK();
	}

	CALL_CALLBACK_GAME_THREAD(Result);

	END_LEAVE_MAIN_THREAD();
#endif // !PLATFORM_LINUX
}

#if PLATFORM_WINDOWS
#pragma warning(pop)
#endif

#undef START_LEAVE_MAIN_THREAD
#undef END_LEAVE_MAIN_THREAD
#undef THREAD_MOVE_ELEM
#undef THREAD_MOVE_ELEM_COPY
#undef MONGO_FAIL_TASK
#undef CALL_CALLBACK_GAME_THREAD
#undef MONGO_GET_COLLECTION
#undef MONGO_GET_DATABASE
#undef MONGO_GET_CLIENT



// 2025 Copyright Pandores Marketplace

#include "MongoDB/Pool.h"

THIRD_PARTY_INCLUDES_START
#	include "mongocxx/uri.hpp"
#	include "mongocxx/pool.hpp"
#	include "mongocxx/client.hpp"
#	include "mongocxx/exception/logic_error.hpp"
#	include "mongocxx/exception/operation_exception.hpp"
#	include "mongocxx/exception/bulk_write_exception.hpp"
#	include "mongocxx/exception/query_exception.hpp"
#	include "bsoncxx/builder/stream/document.hpp"
#	include "bsoncxx/builder/stream/array.hpp"
#	include "bsoncxx/types.hpp"
#	include "bsoncxx/json.hpp"
#	include "bsoncxx/exception/exception.hpp"
#	include "bsoncxx/document/value.hpp"
#	include "mongocxx/config/version.hpp"
#	if PLATFORM_WINDOWS
#		include "mongocxx/options/tls.hpp"
#	endif
THIRD_PARTY_INCLUDES_END

#include "Async/Async.h"
#include "MongoDBDriver.h"

#include "DocumentBuilderHelper.h"
#include "Misc/Paths.h"

#include <vector>

#if PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4101)
#endif

#define START_THREAD_POOL_EXECUTION(...)				\
	check(m_ThreadPool)									\
	NThreadPoolInternal::AsyncTask(m_ThreadPool.Get(),	\
	[													\
		Pool = this->m_Pool								\
		, ## __VA_ARGS__								\
	]() mutable -> void {

#define END_THREAD_POOL_EXECUTION(...) })

#define START_GAME_THREAD_EXECUTION(...)				\
	AsyncTask(ENamedThreads::GameThread,				\
	[													\
		__VA_ARGS__										\
	]() mutable -> void {

#define END_GAME_THREAD_EXECUTION(...) })

#define LAMBDA_MOVE_TEMP(Var) Var = MoveTemp(Var)

#define LAMBDA_FSTRING_TO_STD(Var) Var = std::string(TCHAR_TO_UTF8(*Var))

using FPoolClient = const mongocxx::pool::entry&;

static void PerformOperationWithClient(const FPoolPtr& Pool, TUniqueFunction<void(FPoolClient)> Function)
{
	auto Client = Pool->acquire();

	if (Client)
	{
		try
		{
			Function(Client);
		}
		catch (const mongocxx::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Operation leaked a MongoDB exception. Exception reason: %s"), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const std::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Operation leaked a standard exception. Exception reason: %s"), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Operation leaked an unknown exception."));
		}
	}
	else
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to get a valid client from the pool, operation canceled."));
	}
}

namespace NThreadPoolInternal
{
	class FMongoTask final : public IQueuedWork
	{
	public:
		FMongoTask(TUniqueFunction<void()>&& Work)
			: Function(MoveTemp(Work))
		{}

		virtual void DoThreadedWork() override
		{
			Function();

			delete this;
		}

		virtual void Abandon() override
		{}

	private:
		TUniqueFunction<void()> Function;
	};

	void AsyncTask(FQueuedThreadPool* const Pool, TUniqueFunction<void()>&& Work)
	{
		check(Pool);

		Pool->AddQueuedWork(new FMongoTask(MoveTemp(Work)));
	}
};

namespace NPoolHelper
{
	void ExecuteCallbackOnGameThread(FMongoCallback& Callback, const bool bSuccess)
	{
		if (Callback.IsBound())
		{
			START_GAME_THREAD_EXECUTION(bSuccess, LAMBDA_MOVE_TEMP(Callback));
			
			Callback.ExecuteIfBound(bSuccess);
			
			END_GAME_THREAD_EXECUTION();
		}
	}

	void ExecuteCallbackOnGameThread(FMongoDocumentCallback& Callback, const bool bSuccess, FDocumentValue& Value)
	{
		if (Callback.IsBound())
		{
			START_GAME_THREAD_EXECUTION(LAMBDA_MOVE_TEMP(Callback), bSuccess, LAMBDA_MOVE_TEMP(Value));

			Callback.ExecuteIfBound(bSuccess, Value);

			END_GAME_THREAD_EXECUTION();
		}
	}
};

/* static */ UMongoPool* UMongoPool::CreatePool(const FString& Protocole, const FString& Address, const int32 Port, 
	const int32 MinPoolSize, const int32 MaxPoolSize, const FString AdditionalParameters)
{
	const FString UriStr = FString::Printf(TEXT("%s://%s:%d/?%s"),
		*Protocole, *Address, Port, *AdditionalParameters);
	return CreatePoolFromURI(UriStr, MaxPoolSize);
}

/* static */ UMongoPool* UMongoPool::CreatePoolForAtlas(const FString& Protocole, const FString& Domain, const FString& DatabaseName, 
	const FString& Username, const FString& Password, const int32 MinPoolSize, const int32 MaxPoolSize, const FString& AdditionalParameters)
{
	const FString UriStr = FString::Printf(TEXT("%s://%s:%s@%s/%s?%s"),
		*Protocole, *Username, *Password, *Domain, *DatabaseName, *AdditionalParameters);

	return CreatePoolFromURI(UriStr, MaxPoolSize);
}

/* static */ UMongoPool* UMongoPool::CreatePoolFromURI(FString URI, const int32 PoolSize, 
	const FString& PemFile,
	const FString& PemPassword,
	const FString& CaFile,
	const FString& CaDir,
	const FString& CrlFile,
	bool bAllowInvalidCertificate,
	bool bSslEnabled)
{
	if (PoolSize <= 0)
	{
		UE_LOG(LogMongoDB, Error, TEXT("MaxPoolSize must be strictly greater than 0."));

		return nullptr;
	}

	if (PoolSize > 100)
	{
		UE_LOG(LogMongoDB, Error, TEXT("MaxPoolSize is too big (%d)."), PoolSize);

		return nullptr;
	}

	if (URI.Contains(TEXT("?")))
	{
		const int32 LastCharLoc = URI.Len() - 1;
		const TCHAR& LastChar = URI[LastCharLoc];
		
		if (LastChar != TEXT('?') && LastChar != TEXT('&'))
		{
			URI += "&";
		}

		URI += FString::Printf(TEXT("maxPoolSize=%d"), PoolSize);
	}
	else
	{
		URI += FString::Printf(TEXT("?maxPoolSize=%d"), PoolSize);
	}

	if (bSslEnabled)
	{
		URI += TEXT("&ssl=true");
	}
	
	UE_LOG(LogMongoDB, Log, TEXT("Creating Pool with URI '%s'."), *URI);

	mongocxx::v_noabi::uri Uri;

	FTCHARToUTF8 UriConverter(*URI);

	try
	{
		Uri = mongocxx::v_noabi::uri(UriConverter.Get());
	}

	catch (const std::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to create URI with parameter `%s`. Reason: %s."),
			*URI, UTF8_TO_TCHAR(Exception.what()));
		return nullptr;
	}

	catch (...)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to create URI with parameter `%s`. Reason: Unknown."), *URI);
		return nullptr;
	}

	using SSLOptions = mongocxx::v_noabi::options::tls;

	mongocxx::v_noabi::options::client ClientOptions;
	SSLOptions SslOptions;

	if (bSslEnabled)
	{
		if (!PemFile.IsEmpty())
		{
			if (!FPaths::FileExists(PemFile))
			{
				UE_LOG(LogMongoDB, Warning, TEXT("Pem File \"%s\" doesn't exist. Skipping."), *PemFile);
			}
			else SslOptions.pem_file(TCHAR_TO_UTF8(*PemFile));
		}

		if (!PemPassword.IsEmpty())
		{
			SslOptions.pem_password(TCHAR_TO_UTF8(*PemPassword));
		}

		if (!CaFile.IsEmpty())
		{
			if (!FPaths::FileExists(CaFile))
			{
				UE_LOG(LogMongoDB, Warning, TEXT("Ca File \"%s\" doesn't exist. Skipping."), *CaFile);
			}
			else SslOptions.ca_file(TCHAR_TO_UTF8(*CaFile));
		}

		if (!CaDir.IsEmpty())
		{
			if (!FPaths::DirectoryExists(CaDir))
			{
				UE_LOG(LogMongoDB, Warning, TEXT("Ca Dir \"%s\" doesn't exist. Skipping."), *CaDir);
			}
			else SslOptions.ca_dir(TCHAR_TO_UTF8(*CaDir));
		}

		if (!CrlFile.IsEmpty())
		{
			if (!FPaths::FileExists(CrlFile))
			{
				UE_LOG(LogMongoDB, Warning, TEXT("Crl File \"%s\" doesn't exist. Skipping."), *CrlFile);
			}
			else SslOptions.ca_dir(TCHAR_TO_UTF8(*CrlFile));
		}

		SslOptions.allow_invalid_certificates(bAllowInvalidCertificate);


		ClientOptions.tls_opts(MoveTemp(SslOptions));
	}

	UMongoPool* const Pool = NewObject<UMongoPool>();

	try
	{
		Pool->m_Pool = MakeShared<mongocxx::v_noabi::pool, ESPMode::ThreadSafe>(MoveTemp(Uri), MoveTemp(ClientOptions));
	}
	
	catch (const std::exception& Exception)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to create pool. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));
		return nullptr;
	}

	catch (...)
	{
		UE_LOG(LogMongoDB, Error, TEXT("Failed to create pool. Reason: Unknown."));
		return nullptr;
	}

	const bool bPoolCreated = Pool->m_ThreadPool->Create(PoolSize, 32678U, TPri_Normal, TEXT("MongoDB Thread Pool"));

	ensureMsgf(bPoolCreated, TEXT("Failed to create Thread Pool."));

	UE_LOG(LogMongoDB, Log, TEXT("New pool created."));

	return Pool;
}

UMongoPool::UMongoPool() 
	: Super()
	, m_Pool(nullptr)
	, m_ThreadPool(FQueuedThreadPool::Allocate())
{
}

UMongoPool::~UMongoPool()
{
	if (m_ThreadPool)
	{
		m_ThreadPool->Destroy();
	}
}

static bool ListDatabases(const FPoolPtr& Pool, TArray<FDatabaseData>& OutDatabases)
{
	using namespace mongocxx::v_noabi;

	bool bSuccess = true;
	TArray<FDatabaseData> DatabasesArray;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		try
		{
			cursor Databases = Client->list_databases();

			for (const auto& Database : Databases)
			{
				FDatabaseData& Current = DatabasesArray.Emplace_GetRef();
				for (const bsoncxx::document::element& Info : Database)
				{
					switch (Info.type())
					{
					case bsoncxx::v_noabi::type::k_string: Current.Name = UTF8_TO_TCHAR(Info.get_string().value.data());		break;
					case bsoncxx::v_noabi::type::k_bool:   Current.bHasAnyData = Info.get_bool();								break;
					case bsoncxx::v_noabi::type::k_double: Current.SizeOnDisk = (float)Info.get_double();						break;
					default:
						UE_LOG(LogMongoDB, Warning, TEXT("Unexptected type %d for Database info."), Info.type());
					}
				}
			}
		}

		catch (const std::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Failed to list databases. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

			bSuccess = false;
		}

		catch (...)
		{
			bSuccess = false;
		}
	});

	return bSuccess;
}


void UMongoPool::ListDatabases(FMongoDatabasesCallback Callback)
{
	// No need to work if nobody's gonna check.
	if (!Callback.IsBound())
	{
		UE_LOG(LogMongoDB, Warning, TEXT("Called ListDatabases() without a bound callback."));
		return;
	}

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback));

	TArray<FDatabaseData> DatabasesArray;
	
	const bool bSuccess = ::ListDatabases(Pool, DatabasesArray);

	START_GAME_THREAD_EXECUTION(bSuccess, LAMBDA_MOVE_TEMP(Callback), LAMBDA_MOVE_TEMP(DatabasesArray));

	Callback.ExecuteIfBound(bSuccess, DatabasesArray);

	END_GAME_THREAD_EXECUTION();	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::DropDatabase(FString DatabaseName, FMongoCallback Callback)
{
	using namespace mongocxx::v_noabi;

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName));

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void 
	{
		bool bSuccess = true;

		auto Database = Client->database(DatabaseName);

		try
		{
			Database.drop();
		}
		catch (const std::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Failed to drop database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));

			bSuccess = false;
		}
		catch (...)
		{
			bSuccess = false;
		}

		NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	});
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::CreateCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback)
{
	using namespace mongocxx::v_noabi;

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);

		try
		{
			Database.create_collection(CollectionName);
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to create collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});
	
	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::DropCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback)
{
	using namespace mongocxx::v_noabi;

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);

		auto Collection = Database.collection(CollectionName);

		try
		{
			Collection.drop();
		}
		catch (const std::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Failed to drop collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));

			bSuccess = false;
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::InsertOne(FString DatabaseName, FString CollectionName, FDocumentValue Document, FMongoInsertCallback Callback)
{
	using namespace mongocxx::v_noabi;
	using namespace bsoncxx ::v_noabi;
	using namespace bsoncxx::builder::stream;

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Document));

	FMongoInsertResult InsertResult;
	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		
		auto Collection = Database.collection(CollectionName);
		
		FBuilderVisitor BuilderVisitor;

		BuilderVisitor.Visit(Document);

		try
		{
			const auto Value = bsoncxx::from_json(BuilderVisitor.Get());

			const auto Result = Collection.insert_one(Value.view());

			if (Result)
			{
				InsertResult.InsertedId = UTF8_TO_TCHAR(Result->inserted_id().get_oid().value.to_string().c_str());
				InsertResult.InsertedCount = Result->result().inserted_count();
			}
		}
		catch (const bsoncxx::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to parse JSON, your string probably contains an invalid character. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to insert one in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	if (Callback.IsBound())
	{
		AsyncTask(ENamedThreads::GameThread, [Callback = MoveTemp(Callback), bSuccess, InsertResult = MoveTemp(InsertResult)]() -> void
		{
			Callback.ExecuteIfBound(bSuccess, InsertResult);
		});
	}
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::InsertMany(FString DatabaseName, FString CollectionName, TArray<FDocumentValue> Documents, FMongoCallback Callback)
{
	using namespace mongocxx::v_noabi;
	using namespace bsoncxx::v_noabi;
	using namespace bsoncxx::builder::stream;

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Documents));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		database	Database	= Client->database(DatabaseName);
		collection	Collection	= Database.collection(CollectionName);

		std::vector<bsoncxx::document::value> RawDocuments;
		RawDocuments.reserve(Documents.Num());

		for (const auto& Document : Documents)
		{
			FBuilderVisitor BuilderVisitor;

			BuilderVisitor.Visit(Document);

			try
			{
				RawDocuments.push_back(bsoncxx::from_json(BuilderVisitor.Get()));
			}
			catch (const std::exception& Exception)
			{
				UE_LOG(LogMongoDB, Error, TEXT("Failed to parse JSON, your string probably contains an invalid character. Reason: %s"), UTF8_TO_TCHAR(Exception.what()));

				bSuccess = false;
				return;
			}
			catch (...)
			{
				bSuccess = false;
			}
		}

		try
		{
			Collection.insert_many(MoveTemp(RawDocuments));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to insert many in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::ListIndexes(FString DatabaseName, FString CollectionName, FMongoIndexesCallback Callback)
{
	if (!Callback.IsBound())
	{
		return;
	}

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName));

	TArray<FDatabaseIndex> IndexesRecupered;
	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			auto Indexes = Collection.list_indexes();

			for (const auto& Index : Indexes)
			{
				FDatabaseIndex& CurrentIndex = IndexesRecupered.Emplace_GetRef();

				for (const auto& Info : Index)
				{
					switch (Info.type())
					{
					case bsoncxx::v_noabi::type::k_string: CurrentIndex.Name = UTF8_TO_TCHAR(Info.get_string().value.data()); break;
					case bsoncxx::v_noabi::type::k_int32:  CurrentIndex.ID = Info.get_int32();				  				  break;
					}
				}
			}
		}
		catch (const std::exception& Exception)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Failed to list indexes in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));

			bSuccess = false;
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	START_GAME_THREAD_EXECUTION(bSuccess, LAMBDA_MOVE_TEMP(IndexesRecupered), LAMBDA_MOVE_TEMP(Callback));
		
	Callback.ExecuteIfBound(bSuccess, IndexesRecupered);
		
	END_GAME_THREAD_EXECUTION();	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::RenameCollection(FString DatabaseName, FString CollectionName, FString NewCollectionName, bool bOverwriteExisting, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_FSTRING_TO_STD(NewCollectionName), bOverwriteExisting);

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Collection.rename(NewCollectionName, bOverwriteExisting);
		}

		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to rema,e collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::ReplaceOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoCallback Callback)
{
	using namespace bsoncxx::builder::stream;

	START_THREAD_POOL_EXECUTION(LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Filter),
		LAMBDA_MOVE_TEMP(Replacement), LAMBDA_MOVE_TEMP(Callback));
		
	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		FBuilderVisitor FilterVisitor;
		FBuilderVisitor ReplacementVisitor;

		FilterVisitor.Visit(Filter);
		ReplacementVisitor.Visit(Replacement);

		std::string FilterStr = FilterVisitor.Get();
		std::string ReplacementStr = ReplacementVisitor.Get();

		try
		{
			Collection.replace_one(bsoncxx::from_json(MoveTemp(FilterStr)),
				bsoncxx::from_json(MoveTemp(ReplacementStr)));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to replace document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::UpdateManyWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Filter),
		LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Update), LAMBDA_MOVE_TEMP(Options));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		mongocxx::options::update RawOptions;

		if (Options.ArrayFilter.Num() > 0)
		{
			bsoncxx::builder::stream::array ArrayFilters{};

			for (const auto& ArrFilter : Options.ArrayFilter)
			{
				ArrayFilters << NDocumentValueConverter::Convert(ArrFilter);
			}

			RawOptions.array_filters(ArrayFilters.extract());
		}

		RawOptions.upsert(Options.bUpsert);
		RawOptions.bypass_document_validation(Options.bBypassDocumentValidation);

		try
		{
			Collection.update_many(
				NDocumentValueConverter::Convert(Filter),
				NDocumentValueConverter::Convert(Update),
				RawOptions);
		}

		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to replace document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::UpdateMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Filter),
		LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Update));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Collection.update_many(
				NDocumentValueConverter::Convert(Filter),
				NDocumentValueConverter::Convert(Update));
		}

		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to replace document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::UpdateOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Filter),
		LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Update));

	bool bSuccess = true;
	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		using namespace bsoncxx::builder::basic;
		
		try
		{
			Collection.update_one(NDocumentValueConverter::Convert(Filter),
				make_document(kvp("$set", NDocumentValueConverter::Convert(Update))).view());
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to update document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);

	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::UpdateOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Filter),
		LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Update), LAMBDA_MOVE_TEMP(Options));

	bool bSuccess = true;
	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		mongocxx::options::update RawOptions;

		if (Options.ArrayFilter.Num() > 0)
		{	
			bsoncxx::builder::stream::array ArrayFilters{};

			for (const auto& ArrFilter : Options.ArrayFilter)
			{
				ArrayFilters << NDocumentValueConverter::Convert(ArrFilter);
			}

			RawOptions.array_filters(ArrayFilters.extract());
		}

		RawOptions.upsert(Options.bUpsert);
		RawOptions.bypass_document_validation(Options.bBypassDocumentValidation);

		try
		{
			Collection.update_one(NDocumentValueConverter::Convert(Filter),
				NDocumentValueConverter::Convert(Update), RawOptions);
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to update document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);

	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::CountDocuments(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCountCallback Callback)
{
	if (!Callback.IsBound())
	{
		return;
	}

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Filter),
		LAMBDA_FSTRING_TO_STD(CollectionName));

	bool bSuccess = true;
	int64 Count = -1;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Count = Collection.count_documents(NDocumentValueConverter::Convert(Filter));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to count documents in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	AsyncTask(ENamedThreads::GameThread, [bSuccess, Count, Callback = MoveTemp(Callback)]() -> void
	{
		Callback.ExecuteIfBound(bSuccess, Count);
	});

	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::GetEstimatedDocumentCount(FString DatabaseName, FString CollectionName, FMongoCountCallback Callback)
{
#if PLATFORM_LINUX
	UE_LOG(LogMongoDB, Warning, TEXT("GetEstimatedDocumentCount() is not available on Linux."));

	Callback.ExecuteIfBound(false, 0);
#else

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName));

	bool bSuccess = true;
	int64 Count = -1;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Count = Collection.estimated_document_count();
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to estimated documents count in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	START_GAME_THREAD_EXECUTION(LAMBDA_MOVE_TEMP(Callback), bSuccess, Count);

	Callback.ExecuteIfBound(bSuccess, Count);

	END_GAME_THREAD_EXECUTION();
	
	END_THREAD_POOL_EXECUTION();
#endif // !PLATFORM_LINUX
}

void UMongoPool::CreateIndex(FString DatabaseName, FString CollectionName, FDocumentValue Keys, FDocumentValue IndexOptions, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Keys), LAMBDA_MOVE_TEMP(IndexOptions));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Collection.create_index(
				(NDocumentValueConverter::Convert(Keys))
#if	!PLATFORM_LINUX
				, (NDocumentValueConverter::Convert(IndexOptions))
#endif
			);
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to create index in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);

	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::DeleteMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter));

	bool bSuccess = true;
	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Collection.delete_many((NDocumentValueConverter::Convert(Filter)));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to delete many in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::DeleteOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter));

	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			Collection.delete_one((NDocumentValueConverter::Convert(Filter)));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to delete many in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::FindWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback)
{
	if (!Callback.IsBound())
	{
		return;
	}

	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter), LAMBDA_MOVE_TEMP(Options));
	
	bool bSuccess = true;
	FDocumentValue Value;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			TCursor Documents = Collection.find(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Options));

			Value = NDocumentValueConverter::Convert(Documents);
		}
		catch (const mongocxx::query_exception&)
		{
			// It's empty.
			Value = false;
		}
		catch (const mongocxx::operation_exception& Exception)
		{
			bSuccess = false;

			if (Exception.raw_server_error())
			{
				UE_LOG(LogMongoDB, Error, TEXT("Raw server error: %s"), UTF8_TO_TCHAR(bsoncxx::to_json(*Exception.raw_server_error()).c_str()));
			}

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Code: %s. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.code().message().c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const bsoncxx::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Conversion failed. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const mongocxx::logic_error& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Logic Error. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Exception thrown. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	
	START_GAME_THREAD_EXECUTION(LAMBDA_MOVE_TEMP(Callback), bSuccess, LAMBDA_MOVE_TEMP(Value));

	Callback.ExecuteIfBound(bSuccess, Value);

	END_GAME_THREAD_EXECUTION();
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::Find(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback)
{	
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter));

	bool bSuccess = true;
	FDocumentValue Value;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			TCursor Documents = Collection.find(NDocumentValueConverter::Convert(Filter));

			Value = NDocumentValueConverter::Convert(Documents);
		}
		catch (const mongocxx::query_exception&)
		{
			// It's empty.
			Value = false;
		}
		catch (const mongocxx::operation_exception& Exception)
		{
			bSuccess = false;

			if (Exception.raw_server_error())
			{
				UE_LOG(LogMongoDB, Error, TEXT("Raw server error: %s"), UTF8_TO_TCHAR(bsoncxx::to_json(*Exception.raw_server_error()).c_str()));
			}

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Code: %s. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.code().message().c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const bsoncxx::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Conversion failed. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (const mongocxx::logic_error& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Logic Error. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	START_GAME_THREAD_EXECUTION(LAMBDA_MOVE_TEMP(Callback), bSuccess, LAMBDA_MOVE_TEMP(Value));
	
	Callback.ExecuteIfBound(bSuccess, Value);
		
	END_GAME_THREAD_EXECUTION();
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::FindOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter), LAMBDA_MOVE_TEMP(Options));

	bool bSuccess = true;
	FDocumentValue Value;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			auto Document = Collection.find_one(NDocumentValueConverter::Convert(Filter), NDocumentValueConverter::Convert(Options));

			if (Document)
			{
				Value = NDocumentValueConverter::Convert(*Document);
			}
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess, Value);

	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::FindOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback)
{
	struct FMongoFindOneParameters
	{
		std::string DatabaseName;
		std::string CollectionName; 
		std::string Filter; 
		FMongoDocumentCallback Callback;
	};
	
	START_THREAD_POOL_EXECUTION(Parameters = MakeUnique<FMongoFindOneParameters>(FMongoFindOneParameters{ 
		TCHAR_TO_UTF8(*DatabaseName), TCHAR_TO_UTF8(*CollectionName), Filter.IsUndefined() ? "{}" : NDocumentValueConverter::ToString(Filter), LAMBDA_MOVE_TEMP(Callback)}));

	FDocumentValue Value;
	bool bSuccess = true;
	bool bFoundClient = false;

	{
		auto Client = Pool->acquire();

		if (!Client)
		{
			UE_LOG(LogMongoDB, Error, TEXT("Pool failed to return a valid client."));
			return;
		}

		auto Database = Client->database(Parameters->DatabaseName);

		auto Collection = Database.collection(Parameters->CollectionName);

		try
		{
			mongocxx::v_noabi::options::find Options;
			Options.limit(1);

			auto Document = Collection.find(bsoncxx::from_json(Parameters->Filter), Options);

			if (Document.begin() != Document.end())
			{
				Value = NDocumentValueConverter::Convert(*Document.begin());
			}
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find document in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(Parameters->CollectionName.c_str()), UTF8_TO_TCHAR(Parameters->DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	}

	NPoolHelper::ExecuteCallbackOnGameThread(Parameters->Callback, bSuccess, Value);
	
	END_THREAD_POOL_EXECUTION();
}


void UMongoPool::FindOneAndDelete(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter));

	FDocumentValue Value;
	bool bSuccess = true;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			auto Document = Collection.find_one_and_delete(NDocumentValueConverter::Convert(Filter));

			if (Document)
			{
				Value = NDocumentValueConverter::Convert(*Document);
			}
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find and delete document in collection `%s` on database `%s`. Reason: %s"), 
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});
	
	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess, Value);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::FindOneAndReplace(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoDocumentCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_MOVE_TEMP(Callback), LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName),
		LAMBDA_MOVE_TEMP(Filter), LAMBDA_MOVE_TEMP(Replacement));

	bool bSuccess = true;
	FDocumentValue Value;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			auto Document = Collection.find_one_and_replace(
				(NDocumentValueConverter::Convert(Filter)),
				(NDocumentValueConverter::Convert(Replacement)));

			if (Document)
			{
				Value = NDocumentValueConverter::Convert(*Document);
			}
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find and replace document in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess, Value);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::FindOneAndUpdate(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoDocumentCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_FSTRING_TO_STD(CollectionName), LAMBDA_MOVE_TEMP(Filter), LAMBDA_MOVE_TEMP(Update), 
		LAMBDA_MOVE_TEMP(Callback));

	bool bSuccess = true;
	FDocumentValue Value;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		auto Collection = Database.collection(CollectionName);

		try
		{
			auto Document = Collection.find_one_and_update(
				NDocumentValueConverter::Convert(Filter),
				NDocumentValueConverter::Convert(Update));

			if (Document)
			{
				Value = NDocumentValueConverter::Convert(*Document);
			}
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to find and replace document in collection `%s` on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(CollectionName.c_str()), UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess, Value);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::RunCommand(FString DatabaseName, FDocumentValue Command, FMongoDocumentCallback Callback)
{
	START_THREAD_POOL_EXECUTION(LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Command), LAMBDA_MOVE_TEMP(Callback));

	bool bSuccess = true;
	FDocumentValue Result;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);

		try
		{
			Result = NDocumentValueConverter::Convert(Database.run_command(NDocumentValueConverter::Convert(Command)));
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to run command on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	NPoolHelper::ExecuteCallbackOnGameThread(Callback, bSuccess, Result);
	
	END_THREAD_POOL_EXECUTION();
}

void UMongoPool::ListCollectionNames(FString DatabaseName, FMongoStringsCallback Callback)
{
#if !PLATFORM_LINUX
	if (!Callback.IsBound())
	{
		return;
	}

	START_THREAD_POOL_EXECUTION(LAMBDA_FSTRING_TO_STD(DatabaseName), LAMBDA_MOVE_TEMP(Callback));

	bool bSuccess = true;

	std::vector<std::string> Collections;

	PerformOperationWithClient(Pool, [&](FPoolClient Client) -> void
	{
		auto Database = Client->database(DatabaseName);
		try
		{
			Collections = Database.list_collection_names();
		}
		catch (const std::exception& Exception)
		{
			bSuccess = false;

			UE_LOG(LogMongoDB, Error, TEXT("Failed to list collections on database `%s`. Reason: %s"),
				UTF8_TO_TCHAR(DatabaseName.c_str()), UTF8_TO_TCHAR(Exception.what()));
		}
		catch (...)
		{
			bSuccess = false;
		}
	});

	START_GAME_THREAD_EXECUTION(LAMBDA_MOVE_TEMP(Callback), bSuccess, LAMBDA_MOVE_TEMP(Collections));
	
	TArray<FString> Result;
	Result.Reserve(Collections.size());

	for (const std::string& Collection : Collections)
	{
		Result.Add(UTF8_TO_TCHAR(Collection.c_str()));
	}

	Callback.ExecuteIfBound(bSuccess, Result);
	
	END_GAME_THREAD_EXECUTION();	
	END_THREAD_POOL_EXECUTION();
#endif // !PLATFORM_LINUX
}

static void SetExec(EMongoDBOperationResult& Exec, const bool bResult)
{
	Exec = bResult ? EMongoDBOperationResult::Success : EMongoDBOperationResult::Failed;
}


#if PLATFORM_WINDOWS
#pragma warning(pop)
#endif

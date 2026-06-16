// 2025 Copyright Pandores Marketplace

#include "MongodbNodes.h"

#define CHECK_CONNECTOR(Func, ...)															\
	if (!m_Connector)																		\
	{																						\
		FFrame::KismetExecutionMessage(														\
			TEXT("Database Connector passed to function ") TEXT(#Func) TEXT(" was NULL."),	\
			ELogVerbosity::Error);															\
		OnTaskOver(false, ## __VA_ARGS__);													\
		return;																				\
	}

#define BROADCAST_EVENT(...)								\
	do {													\
		(bSuccess ? Done : Failed).Broadcast(__VA_ARGS__);	\
		SetReadyToDestroy();								\
	} while(0)

UMongoDbListDatabases* UMongoDbListDatabases::ListDatabases(TScriptInterface<IDatabaseConnector> Connector)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;

	return Proxy;
}

void UMongoDbListDatabases::Activate()
{
	CHECK_CONNECTOR(ListDatabases, TArray<FDatabaseData>());

	m_Connector->ListDatabases(FMongoDatabasesCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbListDatabases::OnTaskOver(bool bSuccess, const TArray<FDatabaseData>& Databases)
{
	BROADCAST_EVENT(Databases);
}

UMongoDbCreateCollection* UMongoDbCreateCollection::CreateCollection(TScriptInterface<IDatabaseConnector> Connector, const FString& InDatabaseName, const FString& InCollectionName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName		= InDatabaseName;
	Proxy->CollectionName	= InCollectionName;

	return Proxy;
}

void UMongoDbCreateCollection::Activate()
{
	CHECK_CONNECTOR(CreateCollection);

	m_Connector->CreateCollection(DatabaseName, CollectionName, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbCreateCollection::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}


UMongoDbDropDatabase* UMongoDbDropDatabase::DropDatabase(TScriptInterface<IDatabaseConnector> Connector, const FString& InDatabaseName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector			= Connector;
	Proxy->DatabaseName		= InDatabaseName;

	return Proxy;
}

void UMongoDbDropDatabase::Activate()
{
	CHECK_CONNECTOR(DropDatabase);

	m_Connector->DropDatabase(DatabaseName, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbDropDatabase::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}

UMongoDbDropCollection* UMongoDbDropCollection::DropCollection(TScriptInterface<IDatabaseConnector> Connector, const FString& InDatabaseName, const FString& InCollectionName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector			= Connector;
	Proxy->DatabaseName		= InDatabaseName;
	Proxy->CollectionName	= InCollectionName;

	return Proxy;
}

void UMongoDbDropCollection::Activate()
{
	CHECK_CONNECTOR(DropCollection);

	m_Connector->DropCollection(DatabaseName, CollectionName, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbDropCollection::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}


UMongoDbInsertOne* UMongoDbInsertOne::InsertOne(TScriptInterface<IDatabaseConnector> Connector, FString InDatabaseName, FString InCollectionName, FDocumentValue Value)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(InDatabaseName);
	Proxy->CollectionName = MoveTemp(InCollectionName);
	Proxy->Value = MoveTemp(Value);

	return Proxy;
}

void UMongoDbInsertOne::Activate()
{
	CHECK_CONNECTOR(InsertOne, FMongoInsertResult());

	m_Connector->InsertOne(DatabaseName, CollectionName, MoveTemp(Value), FMongoInsertCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbInsertOne::OnTaskOver(bool bSuccess, const FMongoInsertResult& Result)
{
	BROADCAST_EVENT(Result);
}


UMongoDbInsertMany* UMongoDbInsertMany::InsertMany(TScriptInterface<IDatabaseConnector> Connector, FString InDatabaseName, FString InCollectionName, TArray<FDocumentValue> Value)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(InDatabaseName);
	Proxy->CollectionName = MoveTemp(InCollectionName);
	Proxy->Value = MoveTemp(Value);

	return Proxy;
}

void UMongoDbInsertMany::Activate()
{
	CHECK_CONNECTOR(InsertMany);

	m_Connector->InsertMany(DatabaseName, CollectionName, MoveTemp(Value), FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbInsertMany::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}

UMongoDbListIndexes* UMongoDbListIndexes::ListIndexes(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = DatabaseName;
	Proxy->CollectionName = CollectionName;

	return Proxy;
}

void UMongoDbListIndexes::Activate()
{
	CHECK_CONNECTOR(ListIndexes, TArray<FDatabaseIndex>());

	m_Connector->ListIndexes(DatabaseName, CollectionName, FMongoIndexesCallback::CreateUObject(this, &UMongoDbListIndexes::OnTaskOver));
}

void UMongoDbListIndexes::OnTaskOver(bool bSuccess, const TArray<FDatabaseIndex>& Indexes)
{
	BROADCAST_EVENT(Indexes);
}

UMongoDbRenameCollection* UMongoDbRenameCollection::RenameCollection(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FString NewCollectionName, bool bOverwriteExisting)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector			= Connector;
	Proxy->DatabaseName			= MoveTemp(DatabaseName);
	Proxy->CollectionName		= MoveTemp(CollectionName);
	Proxy->NewCollectionName	= MoveTemp(NewCollectionName);
	Proxy->bOverwriteExisting	= bOverwriteExisting;

	return Proxy;
}

void UMongoDbRenameCollection::Activate()
{
	CHECK_CONNECTOR(RenameCollection);

	m_Connector->RenameCollection(DatabaseName, CollectionName, NewCollectionName, bOverwriteExisting, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbRenameCollection::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}


UMongoDbReplaceOne* UMongoDbReplaceOne::ReplaceOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);

	return Proxy;
}

void UMongoDbReplaceOne::Activate()
{
	CHECK_CONNECTOR(ReplaceOne);

	m_Connector->ReplaceOne(DatabaseName, CollectionName, Filter, Replacement, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbReplaceOne::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}

UMongoDbUpdateMany* UMongoDbUpdateMany::UpdateManyWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoUpdateOptions Options)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);
	Proxy->Options = MoveTemp(Options);

	return Proxy;
}

UMongoDbUpdateMany* UMongoDbUpdateMany::UpdateMany(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);

	return Proxy;
}

void UMongoDbUpdateMany::Activate()
{
	CHECK_CONNECTOR(UpdateMany);

	if (!Options)
	{
		m_Connector->UpdateMany(DatabaseName, CollectionName, Filter, Replacement, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
	}
	else
	{
		m_Connector->UpdateManyWithOptions(DatabaseName, CollectionName, Filter, Replacement, Options.GetValue(), FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
	}
}

void UMongoDbUpdateMany::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}

UMongoDbUpdateOne* UMongoDbUpdateOne::UpdateOneWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoUpdateOptions Options)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);
	Proxy->Options = MoveTemp(Options);

	return Proxy;
}

UMongoDbUpdateOne* UMongoDbUpdateOne::UpdateOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);

	return Proxy;
}

void UMongoDbUpdateOne::Activate()
{
	CHECK_CONNECTOR(UpdateOne);

	if (!Options)
	{
		m_Connector->UpdateOne(DatabaseName, CollectionName, Filter, Replacement, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
	}
	else
	{
		m_Connector->UpdateOneWithOptions(DatabaseName, CollectionName, Filter, Replacement, Options.GetValue(), FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
	}
}

void UMongoDbUpdateOne::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}

UMongoDbCountDocuments* UMongoDbCountDocuments::CountDocuments(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbCountDocuments::Activate()
{
	CHECK_CONNECTOR(CountDocuments, 0);

	m_Connector->CountDocuments(DatabaseName, CollectionName, Filter, FMongoCountCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbCountDocuments::OnTaskOver(bool bSuccess, int64 Count)
{
	BROADCAST_EVENT(Count);
}

UMongoDbGetEstimatedDocumentCount* UMongoDbGetEstimatedDocumentCount::GetEstimatedDocumentCount(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);

	return Proxy;
}

void UMongoDbGetEstimatedDocumentCount::Activate()
{
	CHECK_CONNECTOR(GetEstimatedDocumentCount, 0);

	m_Connector->GetEstimatedDocumentCount(DatabaseName, CollectionName, FMongoCountCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbGetEstimatedDocumentCount::OnTaskOver(bool bSuccess, int64 Count)
{
	BROADCAST_EVENT(Count);
}

UMongoDbCreateIndex* UMongoDbCreateIndex::CreateIndex(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Keys, FDocumentValue IndexOptions)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->IndexOptions = MoveTemp(IndexOptions);
	Proxy->Keys = MoveTemp(Keys);

	return Proxy;
}

void UMongoDbCreateIndex::Activate()
{
	CHECK_CONNECTOR(CreateIndex);

	m_Connector->CreateIndex(DatabaseName, CollectionName, Keys, IndexOptions, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbCreateIndex::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}


UMongoDbDeleteMany* UMongoDbDeleteMany::DeleteMany(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbDeleteMany::Activate()
{
	CHECK_CONNECTOR(DeleteMany);

	m_Connector->DeleteMany(DatabaseName, CollectionName, Filter, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbDeleteMany::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}


UMongoDbDeleteOne* UMongoDbDeleteOne::DeleteOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbDeleteOne::Activate()
{
	CHECK_CONNECTOR(DeleteOne);

	m_Connector->DeleteOne(DatabaseName, CollectionName, Filter, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbDeleteOne::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}


UMongoDbFind* UMongoDbFind::Find(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbFind::Activate()
{
	CHECK_CONNECTOR(Find, FDocumentValue::Undefined());

	m_Connector->Find(DatabaseName, CollectionName, Filter, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFind::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}

UMongoDbFindWithOptions* UMongoDbFindWithOptions::FindWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Options = MoveTemp(Options);

	return Proxy;
}

void UMongoDbFindWithOptions::Activate()
{
	CHECK_CONNECTOR(FindWithOptions, FDocumentValue::Undefined());

	m_Connector->FindWithOptions(DatabaseName, CollectionName, Filter, Options, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFindWithOptions::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}


UMongoDbFindOne* UMongoDbFindOne::FindOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbFindOne::Activate()
{
	CHECK_CONNECTOR(FindOne, FDocumentValue::Undefined());

	m_Connector->FindOne(DatabaseName, CollectionName, Filter, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFindOne::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}


UMongoDbFindOneWithOptions* UMongoDbFindOneWithOptions::FindOneWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbFindOneWithOptions::Activate()
{
	CHECK_CONNECTOR(FindOneWithOptions, FDocumentValue::Undefined());

	m_Connector->FindOneWithOptions(DatabaseName, CollectionName, Filter, Options, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFindOneWithOptions::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}


UMongoDbFindOneAndDelete* UMongoDbFindOneAndDelete::FindOneAndDelete(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);

	return Proxy;
}

void UMongoDbFindOneAndDelete::Activate()
{
	CHECK_CONNECTOR(FindOneAndDelete, FDocumentValue::Undefined());

	m_Connector->FindOneAndDelete(DatabaseName, CollectionName, Filter, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFindOneAndDelete::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}


UMongoDbFindOneAndReplace* UMongoDbFindOneAndReplace::FindOneAndReplace(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);

	return Proxy;
}

void UMongoDbFindOneAndReplace::Activate()
{
	CHECK_CONNECTOR(FindOneAndReplace, FDocumentValue::Undefined());

	m_Connector->FindOneAndReplace(DatabaseName, CollectionName, Filter, Replacement, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFindOneAndReplace::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}


UMongoDbFindOneAndUpdate* UMongoDbFindOneAndUpdate::FindOneAndUpdate(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->CollectionName = MoveTemp(CollectionName);
	Proxy->Filter = MoveTemp(Filter);
	Proxy->Replacement = MoveTemp(Replacement);

	return Proxy;
}

void UMongoDbFindOneAndUpdate::Activate()
{
	CHECK_CONNECTOR(FindOneAndUpdate, FDocumentValue::Undefined());

	m_Connector->FindOneAndUpdate(DatabaseName, CollectionName, Filter, Replacement, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbFindOneAndUpdate::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}

UMongoDbPing* UMongoDbPing::Ping(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);

	return Proxy;
}

void UMongoDbPing::Activate()
{
	CHECK_CONNECTOR(Ping);

	m_Connector->Ping(DatabaseName, FMongoCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbPing::OnTaskOver(bool bSuccess)
{
	BROADCAST_EVENT();
}

UMongoDbRunCommand* UMongoDbRunCommand::RunCommand(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FDocumentValue Command)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector	= Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);
	Proxy->Command		= MoveTemp(Command);

	return Proxy;
}

void UMongoDbRunCommand::Activate()
{
	CHECK_CONNECTOR(RunCommand, FDocumentValue::Undefined());

	m_Connector->RunCommand(DatabaseName, Command, FMongoDocumentCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbRunCommand::OnTaskOver(bool bSuccess, const FDocumentValue& Value)
{
	BROADCAST_EVENT(Value);
}

UMongoDbListCollections* UMongoDbListCollections::ListCollectionNames(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->m_Connector = Connector;
	Proxy->DatabaseName = MoveTemp(DatabaseName);

	return Proxy;
}

void UMongoDbListCollections::Activate()
{
	CHECK_CONNECTOR(ListCollections, TArray<FString>());

	m_Connector->ListCollectionNames(DatabaseName, FMongoStringsCallback::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UMongoDbListCollections::OnTaskOver(bool bSuccess, const TArray<FString>& Value)
{
	BROADCAST_EVENT(Value);
}




#undef CHECK_CONNECTOR
#undef BROADCAST_EVENT

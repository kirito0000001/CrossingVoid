// 2025 Copyright Pandores Marketplace

#include "MongoDB/DatabaseConnector.h"

FDatabaseData::FDatabaseData()
	: Name()
	, SizeOnDisk(0.f)
	, bHasAnyData(false)
{}

FDatabaseData::FDatabaseData(const FDatabaseData& Other)
	: Name(Other.Name)
	, SizeOnDisk(Other.SizeOnDisk)
	, bHasAnyData(Other.bHasAnyData)
{}

FDatabaseData::FDatabaseData(FDatabaseData&& Other)
	: Name(MoveTemp(Other.Name))
	, SizeOnDisk(Other.SizeOnDisk)
	, bHasAnyData(Other.bHasAnyData)
{}

FDatabaseData& FDatabaseData::operator=(const FDatabaseData& Other)
{
	Name = Other.Name;
	SizeOnDisk = Other.SizeOnDisk;
	bHasAnyData = Other.bHasAnyData;
	
	return *this;
}

FDatabaseData& FDatabaseData::operator=(FDatabaseData&& Other)
{
	Name = MoveTemp(Other.Name);
	SizeOnDisk = Other.SizeOnDisk;
	bHasAnyData = Other.bHasAnyData;

	return *this;
}


FMongoFindOptions::FMongoFindOptions() = default;

FMongoFindOptions::FMongoFindOptions(const FMongoFindOptions& Other)
{
	*this = Other;
}

FMongoFindOptions::FMongoFindOptions(FMongoFindOptions&& Other)
{
	*this = MoveTemp(Other);
}

FMongoFindOptions& FMongoFindOptions::operator=(const FMongoFindOptions& Other)
{
	bAllowPartialResults = Other.bAllowPartialResults;
	BatchSize = Other.BatchSize;
	Collation = (Other.Collation);
	Comment = (Other.Comment);
	Limit = Other.Limit;
	Max = (Other.Max);
	Min = (Other.Min);
	MaxAwaitTime = Other.MaxAwaitTime;
	MaxTime = Other.MaxTime;
	bNoCursorTimeout = Other.bNoCursorTimeout;
	Projection = (Other.Projection);
	bReturnKey = Other.bReturnKey;
	bShowRecordId = Other.bShowRecordId;
	Skip = Other.Skip;
	Sort = (Other.Sort);

	return *this;
}

FMongoFindOptions& FMongoFindOptions::operator=(FMongoFindOptions&& Other)
{
	bAllowPartialResults = Other.bAllowPartialResults;
	BatchSize = Other.BatchSize;
	Collation = MoveTemp(Other.Collation);
	Comment = MoveTemp(Other.Comment);
	Limit = Other.Limit;
	Max = MoveTemp(Other.Max);
	Min = MoveTemp(Other.Min);
	MaxAwaitTime = Other.MaxAwaitTime;
	MaxTime = Other.MaxTime;
	bNoCursorTimeout = Other.bNoCursorTimeout;
	Projection = MoveTemp(Other.Projection);
	bReturnKey = Other.bReturnKey;
	bShowRecordId = Other.bShowRecordId;
	Skip = Other.Skip;
	Sort = MoveTemp(Other.Sort);

	return *this;
}


FDatabaseIndex::FDatabaseIndex()
	: ID(-1)
{}

FDatabaseIndex::FDatabaseIndex(const FDatabaseIndex& Other)
	: ID(Other.ID)
	, Name(Other.Name)
{}

FDatabaseIndex::FDatabaseIndex(FDatabaseIndex&& Other)
	: ID(Other.ID)
	, Name(MoveTemp(Other.Name))
{}

FDatabaseIndex& FDatabaseIndex::operator=(const FDatabaseIndex& Other)
{
	ID = Other.ID;
	Name = Other.Name;
	return *this;
}

FDatabaseIndex& FDatabaseIndex::operator=(FDatabaseIndex&& Other)
{
	ID = Other.ID;
	Name = MoveTemp(Other.Name);
	return *this;
}

// Can't use initializer list because of Clang.
static FDocumentValue CreatePingCommand()
{
	FDocumentValueMap Map;

	Map.Add(TEXT("ping"), 1);

	return FDocumentValue(Map);
}

void IDatabaseConnector::Ping(FString DatabaseName, FMongoCallback Callback)
{
	static FDocumentValue Command = CreatePingCommand();

	RunCommand(MoveTemp(DatabaseName), Command,
		FMongoDocumentCallback::CreateLambda([Callback = MoveTemp(Callback)](bool bSuccess, const FDocumentValue&) -> void
	{
		Callback.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::ListDatabases_Blueprints_Implementation(const FMongoDatabasesDelegate& Callback)
{
	ListDatabases(FMongoDatabasesCallback::CreateLambda([Callback](bool bSuccess, const TArray<FDatabaseData>& Databases) -> void
	{
		Callback.ExecuteIfBound(bSuccess, Databases);
	}));
}

void IDatabaseConnector::DropDatabase_Blueprints_Implementation(const FString& DatabaseName, const FMongoDelegate& Delegate)
{
	DropDatabase(DatabaseName, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::CreateCollection_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoDelegate& Delegate)
{
	CreateCollection(DatabaseName, CollectionName, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::DropCollection_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoDelegate& Delegate)
{
	DropCollection(DatabaseName, CollectionName, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::InsertOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Document, const FMongoInsertDelegate& Delegate)
{
	InsertOne(DatabaseName, CollectionName, Document, FMongoInsertCallback::CreateLambda([Delegate](bool bSuccess, const FMongoInsertResult& Result) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Result);
	}));
}

void IDatabaseConnector::InsertMany_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const TArray<FDocumentValue>& Document, const FMongoDelegate& Delegate)
{
	InsertMany(DatabaseName, CollectionName, Document, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::ListIndexes_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoIndexesDelegate& Delegate)
{
	ListIndexes(DatabaseName, CollectionName, FMongoIndexesCallback::CreateLambda([Delegate](bool bSuccess, const TArray<FDatabaseIndex>& Indexes) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Indexes);
	}));
}

void IDatabaseConnector::RenameCollection_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FString& NewCollectionName, bool bOverwriteExisting, const FMongoDelegate& Delegate)
{
	RenameCollection(DatabaseName, CollectionName, NewCollectionName, bOverwriteExisting, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::ReplaceOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Replacement, const FMongoDelegate& Delegate)
{
	ReplaceOne(DatabaseName, CollectionName, Filter, Replacement, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::UpdateMany_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDelegate& Delegate)
{
	UpdateMany(DatabaseName, CollectionName, Filter, Update, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::UpdateManyWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoUpdateOptions& Options, const FMongoDelegate& Delegate)
{
	UpdateManyWithOptions(DatabaseName, CollectionName, Filter, Update, Options, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::UpdateOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDelegate& Delegate)
{
	UpdateOne(DatabaseName, CollectionName, Filter, Update, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::UpdateOneWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoUpdateOptions& Options, const FMongoDelegate& Delegate)
{
	UpdateOneWithOptions(DatabaseName, CollectionName, Filter, Update, Options, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::CountDocuments_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoCountDelegate& Delegate)
{
	CountDocuments(DatabaseName, CollectionName, Filter, FMongoCountCallback::CreateLambda([Delegate](bool bSuccess, int64 Count) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Count);
	}));
}

void IDatabaseConnector::GetEstimatedDocumentCount_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoCountDelegate& Delegate)
{
	GetEstimatedDocumentCount(DatabaseName, CollectionName, FMongoCountCallback::CreateLambda([Delegate](bool bSuccess, int64 Count) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Count);
	}));	
}

void IDatabaseConnector::CreateIndex_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Keys, const FDocumentValue& IndexOptions, const FMongoDelegate& Delegate)
{
	CreateIndex(DatabaseName, CollectionName, Keys, IndexOptions, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::DeleteMany_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDelegate& Delegate)
{
	DeleteMany(DatabaseName, CollectionName, Filter, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::DeleteOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDelegate& Delegate)
{
	DeleteOne(DatabaseName, CollectionName, Filter, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}

void IDatabaseConnector::Find_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate)
{
	Find(DatabaseName, CollectionName, Filter, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::FindWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoFindOptions& Options, const FMongoDocumentDelegate& Delegate)
{
	FindWithOptions(DatabaseName, CollectionName, Filter, Options, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::FindOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate)
{
	FindOne(DatabaseName, CollectionName, Filter, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::FindOneWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoFindOptions& Options, const FMongoDocumentDelegate& Delegate)
{
	FindOneWithOptions(DatabaseName, CollectionName, Filter, Options, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::FindOneAndDelete_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate)
{
	FindOneAndDelete(DatabaseName, CollectionName, Filter, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::FindOneAndReplace_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Replacement, const FMongoDocumentDelegate& Delegate)
{
	FindOneAndReplace(DatabaseName, CollectionName, Filter, Replacement, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::FindOneAndUpdate_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDocumentDelegate& Delegate)
{
	FindOneAndUpdate(DatabaseName, CollectionName, Filter, Update, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::RunCommand_Blueprints_Implementation(const FString& DatabaseName, const FDocumentValue& Command, const FMongoDocumentDelegate& Delegate)
{
	RunCommand(DatabaseName, Command, FMongoDocumentCallback::CreateLambda([Delegate](bool bSuccess, const FDocumentValue& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::ListCollectionNames_Blueprints_Implementation(const FString& DatabaseName, const FMongoStringsDelegate& Delegate)
{
	ListCollectionNames(DatabaseName, FMongoStringsCallback::CreateLambda([Delegate](bool bSuccess, const TArray<FString>& Doc) -> void
	{
		Delegate.ExecuteIfBound(bSuccess, Doc);
	}));
}

void IDatabaseConnector::Ping_Blueprints_Implementation(const FString& DatabaseName, const FMongoDelegate& Delegate)
{
	Ping(DatabaseName, FMongoCallback::CreateLambda([Delegate](bool bSuccess) -> void
	{
		Delegate.ExecuteIfBound(bSuccess);
	}));
}



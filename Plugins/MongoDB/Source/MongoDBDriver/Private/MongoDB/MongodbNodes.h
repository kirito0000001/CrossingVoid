// 2025 Copyright Pandores Marketplace

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MongoDB/DatabaseConnector.h"
#include "MongoDB/Document.h"
#include "MongodbNodes.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynMultListDatabases, const TArray<FDatabaseData>&,	Databases);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynMutlMongoIndexes,  const TArray<FDatabaseIndex>&,	Indexes);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynMutlMongoStrings,  const TArray<FString>&,			Collections);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynMutlMongoCounts,   const int64,						Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynMutlMongoDocument, const FDocumentValue,			Document);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDynMutlMongo);

UCLASS()
class UMongoBlueprintLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromInt32(int32 Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromInt64(int64 Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromUint8(uint8 Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromString(FString Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromFloat(float Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromDouble(double Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromBool(bool bValue) { return bValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromArray(const TArray<FDocumentValue>& Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromMap(const TMap<FString, FDocumentValue>& Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FDocumentValue FromObjectId(const FObjectId& Value) { return Value; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "NULL", BlueprintAutocast))
	static FDocumentValue FromNull() { return FDocumentValue::Null(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "OID", BlueprintAutocast, Keywords = "OID from string object id"))
	static FDocumentValue FromOIDString(FString OID) { return FDocumentValue(FObjectId(MoveTemp(OID))); }

	/** Creates a new document from JSON directly. Doesn't parse the json and just stores it internally for further use. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "JSON", BlueprintAutocast))
	static FDocumentValue FromJSON(FString Value) { return FDocumentValue::FromJSON(MoveTemp(Value)); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "JSON", BlueprintAutocast))
	static FString ToJson(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue.ToJson(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "OID", BlueprintAutocast))
	static FObjectId ToObjectId(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static int32 ToInt32(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static int64 ToInt64(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FString ToString(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static uint8 ToUint8(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static float ToFloat(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static double ToDouble(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static bool ToBool(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static TArray<FDocumentValue> ToArray(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static TMap<FString, FDocumentValue> ToMap(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue; }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "IS NULL"))
	static bool IsNull(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue.IsNull(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "IS UNDEFINED"))
	static bool IsUndefined(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue.IsUndefined(); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "IS TYPE"))
	static bool IsType(UPARAM(ref) const FDocumentValue& DocumentValue, EDocumentValueType Type) { return DocumentValue.GetType() == Type; }

	UFUNCTION(BlueprintCallable, Category = "MongoDB|Document|Value", meta = (ExpandEnumAsExecs = "Branches"))
	static void SwitchOnType(UPARAM(ref) const FDocumentValue& DocumentValue, EDocumentValueType& Branches) { Branches = DocumentValue.GetType(); }
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "TYPE"))
	static EDocumentValueType GetType(UPARAM(ref) const FDocumentValue& DocumentValue) { return DocumentValue.GetType(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "OID", Keywords = "OID from string object id make"))
	static FObjectId MakeObjectId(const FString& Value) { return Value; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Document|Value", meta = (CompactNodeTitle = "->"))
	static FString ObjectIdToString(const FObjectId& ObjectId) { return ObjectId.ToString(); }
};

UCLASS()
class UMongoDbAsyncNodeBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
protected:
	UPROPERTY()
	TScriptInterface<IDatabaseConnector> m_Connector;
};

UCLASS()
class UMongoDbListDatabases final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMultListDatabases Done;


	UPROPERTY(BlueprintAssignable)
	FDynMultListDatabases Failed;

public:

	/**
	 * List all databases.
	 * @param Connector The connector to use to communicate with the database.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Databases", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbListDatabases* ListDatabases(TScriptInterface<IDatabaseConnector> Connector);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const TArray<FDatabaseData>& Databases);
};

UCLASS()
class UMongoDbCreateCollection final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Creates a new collection on the specified database.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database to create the collection into.
	 * @param CollectionName The name of the collection to create.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbCreateCollection* CreateCollection(TScriptInterface<IDatabaseConnector> Connector, const FString& DatabaseName, const FString& CollectionName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
};

UCLASS()
class UMongoDbDropDatabase final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:

	/**
	 * Drops the specified database.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database to drop.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Database", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbDropDatabase* DropDatabase(TScriptInterface<IDatabaseConnector> Connector, const FString& DatabaseName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName;
};

UCLASS()
class UMongoDbDropCollection final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Drops a collection in the specified database.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to drop.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbDropCollection* DropCollection(TScriptInterface<IDatabaseConnector> Connector, const FString& DatabaseName, const FString& CollectionName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynMultInsert, const FMongoInsertResult&, Result);


UCLASS()
class UMongoDbInsertOne final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMultInsert Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMultInsert Failed;

public:
	/**
	 * Insert a document into the collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbInsertOne* InsertOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Value);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FMongoInsertResult& Result);

	FString DatabaseName, CollectionName;
	FDocumentValue Value;
};


UCLASS()
class UMongoDbInsertMany final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Insert many documents into the collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbInsertMany* InsertMany(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, TArray<FDocumentValue> Value);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
	TArray<FDocumentValue> Value;
};


UCLASS()
class UMongoDbListIndexes final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoIndexes Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoIndexes Failed;

public:
	/**
	 * Lists the indexes in the collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database name where the collection is.
	 * @param CollectionName The collection we target.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbListIndexes* ListIndexes(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const TArray<FDatabaseIndex>& Indexes);

	FString DatabaseName, CollectionName;
};


UCLASS()
class UMongoDbRenameCollection final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;
	
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Rename a collection
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to rename.
	 * @param NewCollectionName The new name of the collection.
	 * @param bOverwriteExisting If we should overwrite the collection that already has
	 *							 this name.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbRenameCollection* RenameCollection(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FString NewCollectionName, bool bOverwriteExisting);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName, NewCollectionName;
	bool bOverwriteExisting;
};


UCLASS()
class UMongoDbReplaceOne final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Replaces a single document matching the provided filter in this collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbReplaceOne* ReplaceOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter, Replacement;
};

UCLASS()
class UMongoDbUpdateMany final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbUpdateMany* UpdateMany(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update);

	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbUpdateMany* UpdateManyWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	TOptional<FMongoUpdateOptions> Options;
	FString DatabaseName, CollectionName;
	FDocumentValue Filter, Replacement;
};

UCLASS()
class UMongoDbUpdateOne final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbUpdateOne* UpdateOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update);

	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbUpdateOne* UpdateOneWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	TOptional<FMongoUpdateOptions> Options;
	FString DatabaseName, CollectionName;
	FDocumentValue Filter, Replacement;
};


UCLASS()
class UMongoDbCountDocuments final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoCounts Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoCounts Failed;

public:
	/**
	 * Counts the number of documents matching the criteria.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Filter The filter for the documents to count.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbCountDocuments* CountDocuments(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, int64 Count);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};


UCLASS()
class UMongoDbGetEstimatedDocumentCount final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoCounts Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoCounts Failed;

public:
	/**
	 * Get the estimated count of documents in the collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbGetEstimatedDocumentCount* GetEstimatedDocumentCount(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, int64 Count);

	FString DatabaseName, CollectionName;
};

UCLASS()
class UMongoDbCreateIndex final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Creates an index over the collection for the provided keys with the provided options.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database containing the taget collection.
	 * @param CollectionName The collection to add the index to.
	 * @param Keys The keys for the index: @c {a: 1, b: -1}
	 * @param IndexOptions A document containing optional arguments for creating the index.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbCreateIndex* CreateIndex(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Keys, FDocumentValue IndexOptions);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
	FDocumentValue IndexOptions, Keys;
};

UCLASS()
class UMongoDbDeleteMany final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Deletes all matching documents from the collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where we want to delete documents.
	 * @param CollectionName The collection where we want to delete documents.
	 * @param Filter DocumentValue representing the data to be deleted.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbDeleteMany* DeleteMany(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};

UCLASS()
class UMongoDbDeleteOne final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Deletes a matching document from the collection.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where we want to delete the document.
	 * @param CollectionName The collection where we want to delete the document.
	 * @param Filter DocumentValue representing the data to be deleted.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbDeleteOne* DeleteOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};


UCLASS()
class UMongoDbFind final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFind* Find(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};

UCLASS()
class UMongoDbFindWithOptions final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFindWithOptions* FindWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FMongoFindOptions Options;
	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};


UCLASS()
class UMongoDbFindOne final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFindOne* FindOne(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};

UCLASS()
class UMongoDbFindOneWithOptions final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFindOneWithOptions* FindOneWithOptions(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);
	
	FMongoFindOptions Options;
	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};


UCLASS()
class UMongoDbFindOneAndDelete final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds a single document matching the filter, deletes it, and returns the original.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be deleted.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFindOneAndDelete* FindOneAndDelete(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter;
};

UCLASS()
class UMongoDbFindOneAndReplace final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds a single document matching the filter, replaces it, and returns either the original
	 * or the replacement document.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Replacement Document representing the replacement to apply to a matching document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFindOneAndReplace* FindOneAndReplace(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter, Replacement;
};

UCLASS()
class UMongoDbFindOneAndUpdate final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Finds a single document matching the filter, updates it, and returns either the original
	 * or the newly-updated document.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Update Document representing the update to apply to a matching document.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbFindOneAndUpdate* FindOneAndUpdate(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FString DatabaseName, CollectionName;
	FDocumentValue Filter, Replacement;
};

UCLASS()
class UMongoDbPing final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongo Failed;

public:
	/**
	 * Pings the database.
	 * @param Connector The connector to use to communicate with the database.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbPing* Ping(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess);

	FString DatabaseName;
};

UCLASS()
class UMongoDbRunCommand final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoDocument Failed;

public:
	/**
	 * Runs a command against the database.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database to run the command against.
	 * @param Command The command to run.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbRunCommand* RunCommand(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName, FDocumentValue Command);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const FDocumentValue& Value);

	FString DatabaseName;
	FDocumentValue Command;
};


UCLASS()
class UMongoDbListCollections final : public UMongoDbAsyncNodeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoStrings Done;

	UPROPERTY(BlueprintAssignable)
	FDynMutlMongoStrings Failed;

public:
	/**
	 * List the names of the collections in the database.
	 * @param Connector The connector to use to communicate with the database.
	 * @param DatabaseName The database name we want to list the collections.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Collection", meta = (BlueprintInternalUseOnly = "true"))
	static UMongoDbListCollections* ListCollectionNames(TScriptInterface<IDatabaseConnector> Connector, FString DatabaseName);

	virtual void Activate();

private:
	void OnTaskOver(bool bSuccess, const TArray<FString>& Value);

	FString DatabaseName;
};






// 2025 Copyright Pandores Marketplace

#pragma once

#include "CoreMinimal.h"
#include "MongoDB/Document.h"
#include "UObject/Interface.h"
#include "DatabaseConnector.generated.h"


UENUM(BlueprintType)
enum class EMongoReadConcernLevel : uint8
{
	Local,
	Majority,
	Linearizable,
	ServerDefault,
	Unknown,
	Available,
	Snapshot
};

/**
 *	Options for a Find or FindOne query.
 **/
USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FMongoFindOptions
{
	GENERATED_BODY()
public:
	FMongoFindOptions();
	
	FMongoFindOptions(const FMongoFindOptions&);
	FMongoFindOptions(FMongoFindOptions&&);

	FMongoFindOptions& operator=(const FMongoFindOptions&);
	FMongoFindOptions& operator=(FMongoFindOptions&&);

	/**
	 * Sets whether to allow partial results from a mongos if some shards are down (instead of
	 * throwing an error).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	bool bAllowPartialResults = false;

	/**
	 * Sets the number of documents to return per batch.
	 * -1 for unset
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	int32 BatchSize = -1;

	/**
	 * Sets the collation for this operation.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	FDocumentValue Collation;

	/**
	 * Attaches a comment to the query. If $comment also exists in the modifiers document then
	 * the comment field overwrites $comment.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	FString Comment;

	/**
	 * Sets maximum number of documents to return.
	 * -1 for unset.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	int32 Limit = -1;
	
	/**
	 * Sets the current exclusive upper bound for a specific index.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	FDocumentValue Max;

	/** 
	 * Sets the current inclusive lower bound for a specific index.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	FDocumentValue Min;

	/**
	 * The maximum amount of time for the server to wait on new documents to satisfy a tailable
	 * cursor query. This only applies to a TAILABLE_AWAIT cursor. When the cursor is not a
	 * TAILABLE_AWAIT cursor, this option is ignored. The default on the server is to wait for one
	 * second.
	 * -1 for unset.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	int64 MaxAwaitTime = -1;
	
	/**
	 * Sets the maximum amount of time for this operation to run (server-side) in milliseconds.
	 * -1 for unset.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	int64 MaxTime = -1;

	/**
	 * Sets the cursor flag to prevent cursor from timing out server-side due to a period of
	 * inactivity.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	bool bNoCursorTimeout = false;

	/**
	 * Sets a projection which limits the returned fields for all matching documents.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	FDocumentValue Projection;

	/**
	 * Sets whether to return the index keys associated with the query results, instead of the
	 * actual query results themselves.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	bool bReturnKey = false;

	/**
	 * Sets whether to include the record identifier for each document in the query results.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	bool bShowRecordId = false;

	/**
	 * Sets the number of documents to skip before returning results.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	int64 Skip = -1;

	/**
	 * The order in which to return matching documents. If $orderby also exists in the modifiers
	 * document, the sort field takes precedence over $orderby.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	FDocumentValue Sort;
};


/**
 *	Options for a Update or UpdateOne query.
 **/
USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FMongoUpdateOptions
{
	GENERATED_BODY()
public:

	///
	/// Sets the bypass_document_validation option.
	/// If true, allows the write to opt-out of document level validation.
	///
	/// @note
	///   On servers >= 3.2, the server applies validation by default. On servers < 3.2, this option
	///   is ignored.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	bool bBypassDocumentValidation = false;

	///
	/// Sets the upsert option.
	///
	/// By default, if no document matches the filter, the update operation does nothing.
	/// However, by specifying upsert as @c true, this operation either updates matching documents
	/// or inserts a new document using the update specification if no matching document exists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	bool bUpsert = false;

	///
	/// Set array filters for this operation.
	///
	/// @param array_filters
	///   Array representing filters determining which array elements to modify.
	///
	/// @return
	///   A reference to the object on which this member function is being called.  This facilitates
	///   method chaining.
	///
	/// @see https://docs.mongodb.com/manual/reference/command/update/
	///
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Options")
	TArray<FDocumentValue> ArrayFilter;
};

/**
 * Information about a database.
*/
USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FDatabaseData
{
	GENERATED_BODY()
public:
	FDatabaseData();
	FDatabaseData(const FDatabaseData&);
	FDatabaseData(FDatabaseData&&);

	FDatabaseData& operator=(const FDatabaseData&);
	FDatabaseData& operator=(FDatabaseData&&);

	/**
	 * The database's name.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	FString Name;
	
	/**
	 * The database's size on disk.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	float SizeOnDisk;

	/**
	 * If the database contains data.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	bool bHasAnyData;
};

USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FDatabaseIndex
{
	GENERATED_BODY()
public:
	FDatabaseIndex();
	FDatabaseIndex(const FDatabaseIndex&);
	FDatabaseIndex(FDatabaseIndex&&);

	FDatabaseIndex& operator=(const FDatabaseIndex&);
	FDatabaseIndex& operator=(FDatabaseIndex&&);

	/**
	 * The index's ID.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	int32 ID;

	/**
	 * The index's name.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	FString Name;
};

USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FDatabaseReadConcern
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	FString String;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	EMongoReadConcernLevel Level = EMongoReadConcernLevel::Unknown;
};

USTRUCT(BlueprintType)
struct MONGODBDRIVER_API FMongoInsertResult
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	FObjectId InsertedId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MongoDB|Database")
	int32 InsertedCount = 0;
};

DECLARE_DELEGATE_OneParam (FMongoCallback,			bool /* bSuccess */);
DECLARE_DELEGATE_TwoParams(FMongoDatabasesCallback, bool /* bSuccess */, const TArray<FDatabaseData>& /* Databases */);
DECLARE_DELEGATE_TwoParams(FMongoIndexesCallback,	bool /* bSuccess */, const TArray<FDatabaseIndex>& /* Indexes */);
DECLARE_DELEGATE_TwoParams(FMongoCountCallback,		bool /* bSuccess */, int64 /* Count */);
DECLARE_DELEGATE_TwoParams(FMongoDocumentCallback,	bool /* bSuccess */, const FDocumentValue& /* Document */);
DECLARE_DELEGATE_TwoParams(FMongoStringsCallback,	bool /* bSuccess */, const TArray<FString>& /* Values */);
DECLARE_DELEGATE_TwoParams(FMongoInsertCallback,    bool /* bSuccess */, const FMongoInsertResult& /* Result */);

DECLARE_DYNAMIC_DELEGATE_OneParam(FMongoDelegate, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMongoDatabasesDelegate, bool, bSuccess, const TArray<FDatabaseData>&, Databases);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMongoIndexesDelegate, bool, bSuccess, const TArray<FDatabaseIndex>&, Indexes);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMongoCountDelegate, bool, bSuccess, int64, Count);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMongoDocumentDelegate, bool, bSuccess, const FDocumentValue&, Document);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMongoStringsDelegate, bool, bSuccess, const TArray<FString>&, Values);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMongoInsertDelegate, bool, bSuccess, const FMongoInsertResult&, Result);

UINTERFACE(MinimalAPI, Blueprintable)
class UDatabaseConnector : public UInterface
{
    GENERATED_BODY()
};

class MONGODBDRIVER_API IDatabaseConnector
{
    GENERATED_BODY()

public:
	/**
	 * List all databases.
	 * @param Callback Callback called when we received the list of databases.
	*/
	virtual void ListDatabases(FMongoDatabasesCallback Callback) {}

	/**
	 * Drops the specified database.
	 * @param DatabaseName The database to drop.
	 * @param Callback Callback called when the database has been dropped.
	*/
	virtual void DropDatabase(FString DatabaseName, FMongoCallback Callback) {}

	/**
	 * Creates a new collection on the specified database.
	 * @param DatabaseName The database to create the collection into.
	 * @param CollectionName The name of the collection to create.
	 * @param Callback Callback called when the collection has been created.
	*/
	virtual void CreateCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback) {}

	/**
	 * Drops a collection in the specified database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to drop.
	 * @param Callback Callback called when we dropped the collection.
	*/
	virtual void DropCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback) {}

	/**
	 * Insert a document into the collection.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	 * @param Callback Callback called when we inserted the document.
	*/
	virtual void InsertOne(FString DatabaseName, FString CollectionName, FDocumentValue Document, FMongoInsertCallback Callback) {}

	/**
	 * Insert many documents into the collection.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	 * @param Callback Callback called when we inserted the document.
	*/
	virtual void InsertMany(FString DatabaseName, FString CollectionName, TArray<FDocumentValue> Document, FMongoCallback Callback) {}

	/**
	 * Lists the indexes in the collection.
	 * @param DatabaseName The database name where the collection is.
	 * @param CollectionName The collection we target.
	 * @param Callback Called when we received the indexes.
	*/
	virtual void ListIndexes(FString DatabaseName, FString CollectionName, FMongoIndexesCallback Callback) {}

	/**
	 * Rename a collection
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to rename.
	 * @param NewCollectionName The new name of the collection.
	 * @param bOverwriteExisting If we should overwrite the collection that already has
	 *							 this name.
	 * @param Callback Callback called when the renaming operation has been done.
	*/
	virtual void RenameCollection(FString DatabaseName, FString CollectionName, FString NewCollectionName, bool bOverwriteExisting, FMongoCallback Callback) {}

	/**
	 * Replaces a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void ReplaceOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoCallback Callback) {}


	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) {}

	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateManyWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options, FMongoCallback Callback) {}

	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) {}

	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options, FMongoCallback Callback) {}

	/**
	 * Counts the number of documents matching the criteria.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Filter The filter for the documents to count.
	 * @param Callback Called when the documents have been counted.
	*/
	virtual void CountDocuments(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCountCallback Callback) {}

	/**
	 * Get the estimated count of documents in the collection.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Callback Called when we estimated the number of documents.
	*/
	virtual void GetEstimatedDocumentCount(FString DatabaseName, FString CollectionName, FMongoCountCallback Callback) {}

	/**
	 * Creates an index over the collection for the provided keys with the provided options.
	 * @param DatabaseName The database containing the taget collection.
	 * @param CollectionName The collection to add the index to.
	 * @param Keys The keys for the index: @c {a: 1, b: -1}
	 * @param IndexOptions A document containing optional arguments for creating the index.
	 * @param Callback Called when the index has been created.
	*/
	virtual void CreateIndex(FString DatabaseName, FString CollectionName, FDocumentValue Keys, FDocumentValue IndexOptions, FMongoCallback Callback) {}

	/**
	 * Deletes all matching documents from the collection.
	 * @param DatabaseName The database where we want to delete documents.
	 * @param CollectionName The collection where we want to delete documents.
	 * @param Filter DocumentValue representing the data to be deleted.
	 * @param Callback Called when the items have been deleted.
	*/
	virtual void DeleteMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback) {}

	/**
	 * Deletes a matching document from the collection.
	 * @param DatabaseName The database where we want to delete the document.
	 * @param CollectionName The collection where we want to delete the document.
	 * @param Filter DocumentValue representing the data to be deleted.
	 * @param Callback Called when the items have been deleted.
	*/
	virtual void DeleteOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback) {}

	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void Find(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) {}

	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Options The options applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void FindWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback) {}

	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void FindOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) {}

	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Options The options for the query.
	 * @param Callback Called when we found the documents.
	*/
	virtual void FindOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback) {}

	/**
	 * Finds a single document matching the filter, deletes it, and returns the original.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be deleted.
	 * @param Callback Called when we found and deleted the document.
	*/
	virtual void FindOneAndDelete(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) {}

	/**
	 * Finds a single document matching the filter, replaces it, and returns either the original
	 * or the replacement document.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Replacement Document representing the replacement to apply to a matching document.
	 * @param Callback Called when we found and replaced the document.
	*/
	virtual void FindOneAndReplace(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoDocumentCallback Callback) {}

	/**
	 * Finds a single document matching the filter, updates it, and returns either the original
	 * or the newly-updated document.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Update Document representing the update to apply to a matching document.
	 * @param Callback Called when we found and updated the document.
	*/
	virtual void FindOneAndUpdate(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoDocumentCallback Callback) {}

	/**
	 * Runs a command against the database.
	 * @param DatabaseName The database to run the command against.
	 * @param Command The command to run.
	 * @param Callback Called when the command was run.
	*/
	virtual void RunCommand(FString DatabaseName, FDocumentValue Command, FMongoDocumentCallback Callback) {}

	/**
	 * List the names of the collections in the database.
	 * @param DatabaseName The database name we want to list the collections.
	 * @param Callback Called when we received the collections' names.
	*/
	virtual void ListCollectionNames(FString DatabaseName, FMongoStringsCallback Callback) {}

	/**
	 * Pings the database.
	 * @param Callback Called when we received the result.
	*/
	virtual void Ping(FString DatabaseName, FMongoCallback Callback);

public:

	/**
	 * List all databases.
	 * @param Callback Callback called when we received the list of databases.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Databases", Meta = (DisplayName = "List Databases", AutoCreateRefTerm = "Delegate"))
	void ListDatabases_Blueprints(const FMongoDatabasesDelegate& Delegate);
	void ListDatabases_Blueprints_Implementation(const FMongoDatabasesDelegate& Delegate);

	/**
	 * Drops the specified database.
	 * @param DatabaseName The database to drop.
	 * @param Callback Callback called when the database has been dropped.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Databases", Meta = (DisplayName = "Drop Databases", AutoCreateRefTerm = "Delegate"))
	void DropDatabase_Blueprints(const FString& DatabaseName, const FMongoDelegate& Delegate);
	void DropDatabase_Blueprints_Implementation(const FString& DatabaseName, const FMongoDelegate& Delegate);

	/**
	 * Creates a new collection on the specified database.
	 * @param DatabaseName The database to create the collection into.
	 * @param CollectionName The name of the collection to create.
	 * @param Delegate Delegate called when the collection has been created.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Create Collection", AutoCreateRefTerm = "Delegate"))
	void CreateCollection_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FMongoDelegate& Delegate) ;
	void CreateCollection_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoDelegate& Delegate) ;

	/**
	 * Drops a collection in the specified database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to drop.
	 * @param Delegate Delegate called when we dropped the collection.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Drop Collection", AutoCreateRefTerm = "Delegate"))
	void DropCollection_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FMongoDelegate& Delegate) ;
	void DropCollection_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoDelegate& Delegate) ;

	/**
	 * Insert a document into the collection.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	 * @param Delegate Delegate called when we inserted the document.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Insert One", AutoCreateRefTerm = "Delegate"))
	void InsertOne_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Document, const FMongoInsertDelegate& Delegate);
	void InsertOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Document, const FMongoInsertDelegate& Delegate);

	/**
	 * Insert many documents into the collection.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	 * @param Delegate Delegate called when we inserted the document.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Insert Many", AutoCreateRefTerm = "Delegate"))
	void InsertMany_Blueprints(const FString& DatabaseName, const FString& CollectionName, const TArray<FDocumentValue>& Documents, const FMongoDelegate& Delegate) ;
	void InsertMany_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const TArray<FDocumentValue>& Documents, const FMongoDelegate& Delegate) ;

	/**
	 * Lists the indexes in the collection.
	 * @param DatabaseName The database name where the collection is.
	 * @param CollectionName The collection we target.
	 * @param Delegate Called when we received the indexes.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "List Indexes", AutoCreateRefTerm = "Delegate"))
	void ListIndexes_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FMongoIndexesDelegate& Delegate);
	void ListIndexes_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoIndexesDelegate& Delegate);

	/**
	 * Rename a collection
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to rename.
	 * @param NewCollectionName The new name of the collection.
	 * @param bOverwriteExisting If we should overwrite the collection that already has
	 *							 this name.
	 * @param Delegate Delegate called when the renaming operation has been done.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Rename Collection", AutoCreateRefTerm = "Delegate"))
	void RenameCollection_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FString& NewCollectionName, bool bOverwriteExisting, const FMongoDelegate& Delegate) ;
	void RenameCollection_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FString& NewCollectionName, bool bOverwriteExisting, const FMongoDelegate& Delegate) ;

	/**
	 * Replaces a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Delegate Called when the replacment has been done.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Replace One", AutoCreateRefTerm = "Delegate"))
	void ReplaceOne_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Replacement, const FMongoDelegate& Delegate) ;
	void ReplaceOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Replacement, const FMongoDelegate& Delegate) ;


	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Delegate Called when the replacment has been done.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Update Many", AutoCreateRefTerm = "Delegate"))
	void UpdateMany_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDelegate& Delegate) ;
	void UpdateMany_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDelegate& Delegate);

	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Delegate Called when the replacment has been done.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Update Many with Options", AutoCreateRefTerm = "Delegate"))
	void UpdateManyWithOptions_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoUpdateOptions& Options, const FMongoDelegate& Delegate) ;
	void UpdateManyWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoUpdateOptions& Options, const FMongoDelegate& Delegate);

	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Delegate Called when the replacment has been done.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Update One", AutoCreateRefTerm = "Delegate"))
	void UpdateOne_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDelegate& Delegate) ;
	void UpdateOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDelegate& Delegate) ;

	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Delegate Called when the replacment has been done.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Update One with Options", AutoCreateRefTerm = "Delegate"))
	void UpdateOneWithOptions_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoUpdateOptions& Options, const FMongoDelegate& Delegate) ;
	void UpdateOneWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoUpdateOptions& Options, const FMongoDelegate& Delegate);

	/**
	 * Counts the number of documents matching the criteria.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Filter The filter for the documents to count.
	 * @param Delegate Called when the documents have been counted.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Count Documents", AutoCreateRefTerm = "Delegate"))
	void CountDocuments_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoCountDelegate& Delegate) ;
	void CountDocuments_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoCountDelegate& Delegate) ;

	/**
	 * Get the estimated count of documents in the collection.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Delegate Called when we estimated the number of documents.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Get Estimated Document Count", AutoCreateRefTerm = "Delegate"))
	void GetEstimatedDocumentCount_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FMongoCountDelegate& Delegate) ;
	void GetEstimatedDocumentCount_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FMongoCountDelegate& Delegate) ;

	/**
	 * Creates an index over the collection for the provided keys with the provided options.
	 * @param DatabaseName The database containing the taget collection.
	 * @param CollectionName The collection to add the index to.
	 * @param Keys The keys for the index: @c {a: 1, b: -1}
	 * @param IndexOptions A document containing optional arguments for creating the index.
	 * @param Delegate Called when the index has been created.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Create Index", AutoCreateRefTerm = "Delegate"))
	void CreateIndex_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Keys, const FDocumentValue& IndexOptions, const FMongoDelegate& Delegate) ;
	void CreateIndex_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Keys, const FDocumentValue& IndexOptions, const FMongoDelegate& Delegate) ;

	/**
	 * Deletes all matching documents from the collection.
	 * @param DatabaseName The database where we want to delete documents.
	 * @param CollectionName The collection where we want to delete documents.
	 * @param Filter DocumentValue representing the data to be deleted.
	 * @param Delegate Called when the items have been deleted.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Delete Many", AutoCreateRefTerm = "Delegate"))
	void DeleteMany_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDelegate& Delegate) ;
	void DeleteMany_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDelegate& Delegate) ;

	/**
	 * Deletes a matching document from the collection.
	 * @param DatabaseName The database where we want to delete the document.
	 * @param CollectionName The collection where we want to delete the document.
	 * @param Filter DocumentValue representing the data to be deleted.
	 * @param Delegate Called when the items have been deleted.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Delete One", AutoCreateRefTerm = "Delegate"))
	void DeleteOne_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDelegate& Delegate) ;
	void DeleteOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDelegate& Delegate) ;

	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Delegate Called when we found the documents.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find", AutoCreateRefTerm = "Delegate"))
	void Find_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate) ;
	void Find_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Options The options applied.
	 * @param Delegate Called when we found the documents.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find with Options", AutoCreateRefTerm = "Delegate"))
	void FindWithOptions_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoFindOptions& Options, const FMongoDocumentDelegate& Delegate) ;
	void FindWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoFindOptions& Options, const FMongoDocumentDelegate& Delegate);

	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Delegate Called when we found the documents.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find One", AutoCreateRefTerm = "Delegate"))
	void FindOne_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate) ;
	void FindOne_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Options The options for the query.
	 * @param Delegate Called when we found the documents.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find One with Options", AutoCreateRefTerm = "Delegate"))
	void FindOneWithOptions_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoFindOptions& Options, const FMongoDocumentDelegate& Delegate) ;
	void FindOneWithOptions_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoFindOptions& Options, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * Finds a single document matching the filter, deletes it, and returns the original.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be deleted.
	 * @param Delegate Called when we found and deleted the document.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find One and Delete", AutoCreateRefTerm = "Delegate"))
	void FindOneAndDelete_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate) ;
	void FindOneAndDelete_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * Finds a single document matching the filter, replaces it, and returns either the original
	 * or the replacement document.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Replacement Document representing the replacement to apply to a matching document.
	 * @param Delegate Called when we found and replaced the document.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find One and Replace", AutoCreateRefTerm = "Delegate"))
	void FindOneAndReplace_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Replacement, const FMongoDocumentDelegate& Delegate) ;
	void FindOneAndReplace_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Replacement, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * Finds a single document matching the filter, updates it, and returns either the original
	 * or the newly-updated document.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Update Document representing the update to apply to a matching document.
	 * @param Delegate Called when we found and updated the document.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Find One and Update", AutoCreateRefTerm = "Delegate"))
	void FindOneAndUpdate_Blueprints(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDocumentDelegate& Delegate) ;
	void FindOneAndUpdate_Blueprints_Implementation(const FString& DatabaseName, const FString& CollectionName, const FDocumentValue& Filter, const FDocumentValue& Update, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * Runs a command against the database.
	 * @param DatabaseName The database to run the command against.
	 * @param Command The command to run.
	 * @param Delegate Called when the command was run.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Run Command", AutoCreateRefTerm = "Delegate"))
	void RunCommand_Blueprints(const FString& DatabaseName, const FDocumentValue& Command, const FMongoDocumentDelegate& Delegate) ;
	void RunCommand_Blueprints_Implementation(const FString& DatabaseName, const FDocumentValue& Command, const FMongoDocumentDelegate& Delegate) ;

	/**
	 * List the names of the collections in the database.
	 * @param DatabaseName The database name we want to list the collections.
	 * @param Delegate Called when we received the collections' names.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "List Collection Names", AutoCreateRefTerm = "Delegate"))
	void ListCollectionNames_Blueprints(const FString& DatabaseName, const FMongoStringsDelegate& Delegate) ;
	void ListCollectionNames_Blueprints_Implementation(const FString& DatabaseName, const FMongoStringsDelegate& Delegate) ;

	/**
	 * Pings the database.
	 * @param Delegate Called when we received the result.
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MongoDB|Collection", Meta = (DisplayName = "Ping"))
	void Ping_Blueprints(const FString& DatabaseName, const FMongoDelegate& Delegate);
	void Ping_Blueprints_Implementation(const FString& DatabaseName, const FMongoDelegate& Delegate);
};



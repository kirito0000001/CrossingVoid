// 2025 Copyright Pandores Marketplace

#pragma once

#include "CoreMinimal.h"
#include "Document.h"
#include "MongoDB/DatabaseConnector.h"
#include "Misc/QueuedThreadPool.h"
#include "Pool.generated.h"

UENUM()
enum class EMongoDBOperationResult : uint8
{
	Success,
	Failed
};

namespace mongocxx { namespace v_noabi { class pool; }; };

using FPoolPtr = TSharedPtr<mongocxx::v_noabi::pool, ESPMode::ThreadSafe>;

/**
 * A pool connected to a MongoDB database.
 * Call `CreatePool` to create a pool instead of the default constructor.
*/
UCLASS(BlueprintType)
class MONGODBDRIVER_API UMongoPool final : public UObject, public IDatabaseConnector
{
	GENERATED_BODY()
public:

	UMongoPool();
	~UMongoPool();

	/**
	 * Creates a new pool with the specified parameters.
	 * @param Protocole The protocole to use. (should be mongodb or mongodb+server)
	 * @param Address The database's address.
	 * @param Port The database's port.
	 * @param MinPoolSize The minimum size of the pool.
	 * @param MaxPoolSize The maximum size of the pool.
	 * @param AdditionalParameters Addiotional parameters to pass to the URI. It will be concatened at the end of the URI.
	 * @return A new pool.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Pool")
	static UPARAM(DisplayName = "Pool") UMongoPool* CreatePool
	(
		const FString&	Protocole			 = TEXT("mongodb"), 
		const FString&	Address				 = TEXT(""), 
		const int32		Port				 = 27017, 
		const int32		MinPoolSize			 = 0, 
		const int32		MaxPoolSize			 = 10, 
		const FString	AdditionalParameters = TEXT("")
	);

	/**
	 * Creates a new pool with the specified parameters.
	 * @param URI             The URI to connect to.
	 * @param PoolSize 		  The size of the pool.
	 * @param PemFile		  The path to the .pem file containing a public key certificate and its associated private key.
	 * @param PemPassword	  The pass phrase used to decrypt an encrypted PEM file.
	 * @param CaFile		  The path to the .pem file that contains the root certificate chain from the Certificate Authority.
	 * @param CaDir			  The path to the Certificate Authority directory.
	 * @param CrlFile		  The path to the .pem file that contains revoked certificates.
	 * @param bAllowInvalidCertificate If true, the driver will not verify the server's CA file.
	 * @return A new pool.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Pool")
	static UPARAM(DisplayName = "Pool") UMongoPool* CreatePoolFromURI(FString URI, 
		const int32    PoolSize       = 10,
		const FString& PemFile		  = TEXT(""),
		const FString& PemPassword	  = TEXT(""),
		const FString& CaFile		  = TEXT(""),
		const FString& CaDir		  = TEXT(""),
		const FString& CrlFile		  = TEXT(""),
		bool bAllowInvalidCertificate = false,
		UPARAM(DisplayName = "SSL Enabled") bool bSslEnabled = false);

	/**
	 * Creates a new pool for MongoDB Atlas.
	 * @param Protocole The protocole to use.
	 * @param Domain The domain of your atlas.
	 * @param DatabaseName The name of the database.
	 * @param Username The username of the atlas.
	 * @param Password The password of the atlas.
	 * @param MinPoolSize The min pool size.
	 * @param MaxPoolSize The max pool size.
	 * @param AdditionalParameters Additional parameters added at the end of the URI.
	 * @return A new pool.
	*/
	UFUNCTION(BlueprintCallable, Category = "MongoDB|Po1ol")
	static UPARAM(DisplayName = "Pool") UMongoPool* CreatePoolForAtlas(const FString& Protocole, const FString& Domain, const FString& DatabaseName,
		const FString& Username, const FString& Password, const int32 MinPoolSize, const int32 MaxPoolSize, const FString& AdditionalParameters);

	/**
	 * List all databases.
	 * @param Callback Callback called when we received the list of databases.
	*/
	virtual void ListDatabases(FMongoDatabasesCallback Callback) override;

	/**
	 * Drops the specified database.
	 * @param DatabaseName The database to drop.
	 * @param Callback Callback called when the database has been dropped.
	*/
	virtual void DropDatabase(FString DatabaseName, FMongoCallback Callback) override;

	/**
	 * Creates a new collection on the specified database.
	 * @param DatabaseName The database to create the collection into.
	 * @param CollectionName The name of the collection to create.
	 * @param Callback Callback called when the collection has been created.
	*/
	virtual void CreateCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback) override;

	/**
	 * Drops a collection in the specified database.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to drop.
	 * @param Callback Callback called when we dropped the collection.
	*/
	virtual void DropCollection(FString DatabaseName, FString CollectionName, FMongoCallback Callback) override;

	/**
	 * Insert a document into the collection.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	 * @param Callback Callback called when we inserted the document.
	*/
	virtual void InsertOne(FString DatabaseName, FString CollectionName, FDocumentValue Document, FMongoInsertCallback Callback) override;

	/**
	 * Insert many documents into the collection.
	 * @param DatabaseName The database containing the collection.
	 * @param CollectionName The collection to insert into.
	 * @param Document The document we want to insert into the collection.
	 * @param Callback Callback called when we inserted the document.
	*/
	virtual void InsertMany(FString DatabaseName, FString CollectionName, TArray<FDocumentValue> Document, FMongoCallback Callback) override;

	/**
	 * Lists the indexes in the collection.
	 * @param DatabaseName The database name where the collection is.
	 * @param CollectionName The collection we target.
	 * @param Callback Called when we received the indexes.
	*/
	virtual void ListIndexes(FString DatabaseName, FString CollectionName, FMongoIndexesCallback Callback) override;

	/**
	 * Rename a collection
	 * @param DatabaseName The database containing the collection. 
	 * @param CollectionName The collection to rename.
	 * @param NewCollectionName The new name of the collection.
	 * @param bOverwriteExisting If we should overwrite the collection that already has
	 *							 this name.
	 * @param Callback Callback called when the renaming operation has been done.
	*/
	virtual void RenameCollection(FString DatabaseName, FString CollectionName, FString NewCollectionName, bool bOverwriteExisting, FMongoCallback Callback) override;

	/**
	 * Replaces a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void ReplaceOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoCallback Callback) override;

	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) override;

	/**
	 * Updates multiple documents matching the provided filter in this collection.
	 * @param DatabaseName The database where the documents are.
	 * @param CollectionName The collection where the documents are.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateManyWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options, FMongoCallback Callback) override;
	
	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) override;

	/**
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoUpdateOptions Options, FMongoCallback Callback) override;

	/**
	 * Counts the number of documents matching the criteria.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Filter The filter for the documents to count.
	 * @param Callback Called when the documents have been counted.
	*/
	virtual void CountDocuments(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCountCallback Callback) override;

	/**
	 * Get the estimated count of documents in the collection.
	 * @param DatabaseName The name of the database where we want to count.
	 * @param CollectionName The name of the collection where we want to count.
	 * @param Callback Called when we estimated the number of documents.
	*/
	virtual void GetEstimatedDocumentCount(FString DatabaseName, FString CollectionName, FMongoCountCallback Callback) override;

	/**
	 * Creates an index over the collection for the provided keys with the provided options.
	 * @param DatabaseName The database containing the taget collection.
	 * @param CollectionName The collection to add the index to.
	 * @param Keys The keys for the index: @c {a: 1, b: -1}
	 * @param IndexOptions A document containing optional arguments for creating the index.
	 * @param Callback Called when the index has been created.
	*/
	virtual void CreateIndex(FString DatabaseName, FString CollectionName, FDocumentValue Keys, FDocumentValue IndexOptions, FMongoCallback Callback) override;

	/**
	 * Deletes all matching documents from the collection.
	 * @param DatabaseName The database where we want to delete documents.
	 * @param CollectionName The collection where we want to delete documents.
	 * @param Filter DocumentValue representing the data to be deleted.
	 * @param Callback Called when the items have been deleted.
	*/
	virtual void DeleteMany(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback) override;

	/**
	 * Deletes a matching document from the collection.
	 * @param DatabaseName The database where we want to delete the document.
	 * @param CollectionName The collection where we want to delete the document.
	 * @param Filter DocumentValue representing the data to be deleted.
	 * @param Callback Called when the items have been deleted.
	*/
	virtual void DeleteOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoCallback Callback) override;

	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void Find(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) override;

	/**
	 * Finds the documents in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Options The options applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void FindWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback) override;

	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void FindOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) override;

	/**
	 * Finds the document in the collection which match the provided filter.
	 * @param DatabaseName The database where the collection is.
	 * @param CollectionName The collection where we want the find to run on.
	 * @param Filter The filter applied.
	 * @param Options The options applied.
	 * @param Callback Called when we found the documents.
	*/
	virtual void FindOneWithOptions(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoFindOptions Options, FMongoDocumentCallback Callback) override;

	/**
	 * Finds a single document matching the filter, deletes it, and returns the original.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be deleted.
	 * @param Callback Called when we found and deleted the document.
	*/
	virtual void FindOneAndDelete(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FMongoDocumentCallback Callback) override;

	/**
	 * Finds a single document matching the filter, replaces it, and returns either the original
     * or the replacement document.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Replacement Document representing the replacement to apply to a matching document.
	 * @param Callback Called when we found and replaced the document.
	*/
	virtual void FindOneAndReplace(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Replacement, FMongoDocumentCallback Callback) override;

	/**
	 * Finds a single document matching the filter, updates it, and returns either the original
     * or the newly-updated document.
	 * @param DatabaseName Database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing a document that should be updated.
	 * @param Update Document representing the update to apply to a matching document.
	 * @param Callback Called when we found and updated the document.
	*/
	virtual void FindOneAndUpdate(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoDocumentCallback Callback) override;

	/**
	 * Runs a command against the database.
	 * @param DatabaseName The database to run the command against.
	 * @param Command The command to run.
	 * @param Callback Called when the command was run.
	*/
	virtual void RunCommand(FString DatabaseName, FDocumentValue Command, FMongoDocumentCallback Callback) override;

	/**
	 * List the names of the collections in the database.
	 * @param DatabaseName The database name we want to list the collections.
	 * @param Callback Called when we received the collections' names.
	*/
	virtual void ListCollectionNames(FString DatabaseName, FMongoStringsCallback Callback) override;

private:
	/**
	 * The mongocxx pool pointer.
	*/
	FPoolPtr m_Pool;

	/**
	 * The thread pool to run tasks in.
	*/
	TUniquePtr<class FQueuedThreadPool> m_ThreadPool;
};





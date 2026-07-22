// 2025 Copyright Pandores Marketplace

#pragma once

#include "CoreMinimal.h"
#include "MongoDB/DatabaseConnector.h"
#include "Client.generated.h"

UCLASS(BlueprintType, meta = (DeprecatedFunction, DeprecationMessage = "Use the UMongoPool class instead."))
class MONGODBDRIVER_API UMongoClient : public UObject, public IDatabaseConnector
{
	GENERATED_BODY()
public:

	UMongoClient()  = default;
	~UMongoClient() = default;

	/**
	 * Creates a new client with the specified parameters.
	 * @param Protocole The protocole to use. (should be mongodb or mongodb+server)
	 * @param Address The database's address.
	 * @param Port The database's port.
	 * @param AdditionalParameters Addiotional parameters to pass to the URI. It will be concatened at the end of the URI.
	 * @return A new client.
	*/
	[[ deprecated ]]
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MongoDB|Client", meta = (DeprecatedFunction, DeprecationMessage = "Use the UMongoPool class instead."))
	static UPARAM(DisplayName = "Client") UMongoClient* CreateClient(FString Protocole, FString Address, int32 Port, FString AdditionalParameters = TEXT(""));

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
	 * Updates a single document matching the provided filter in this collection.
	 * @param DatabaseName The database where the document is.
	 * @param CollectionName The collection where the document is.
	 * @param Filter Document representing the match criteria.
	 * @param Replacement The replacement document.
	 * @param Callback Called when the replacment has been done.
	*/
	virtual void UpdateOne(FString DatabaseName, FString CollectionName, FDocumentValue Filter, FDocumentValue Update, FMongoCallback Callback) override;

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
	FString Protocole;
	FString Address;
	int32 Port;
	FString AdditionalParameters;
};


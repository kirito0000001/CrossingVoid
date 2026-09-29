// Fill out your copyright notice in the Description page of Project Settings.


#include "Async/DreamTaskAsyncListener.h"

#include "Classes/DreamTaskComponent.h"
#include "Classes/DreamTask.h"
#include "DreamGameplayTaskSpecHandle.h"

UDreamTaskAsyncListener* UDreamTaskAsyncListener::CreateListenerAndGiveTask(UObject* WorldContextObject, UDreamTaskComponent* InTaskComponent, TSubclassOf<UDreamTask> InTaskClass)
{
	UDreamTaskAsyncListener* Node = NewObject<UDreamTaskAsyncListener>();
	Node->Task = InTaskComponent->GiveTaskByClass(InTaskClass).GetTask();
	Node->WorldContext = WorldContextObject;
	Node->RegisterWithGameInstance(WorldContextObject);

	return Node;
}

UDreamTaskAsyncListener* UDreamTaskAsyncListener::CreateListener(UObject* WorldContextObject, UDreamTask* InTask)
{
	UDreamTaskAsyncListener* Node = NewObject<UDreamTaskAsyncListener>();
	Node->Task = InTask;
	Node->WorldContext = WorldContextObject;
	Node->RegisterWithGameInstance(WorldContextObject);
	
	return Node;
}

UDreamTaskAsyncListener* UDreamTaskAsyncListener::CreateListenerWithHandle(UObject* WorldContextObject, const FDreamTaskSpecHandle& SpecHandle)
{
	UDreamTaskAsyncListener* Node = NewObject<UDreamTaskAsyncListener>();
	Node->Task = SpecHandle.GetTask();
	Node->WorldContext = WorldContextObject;
	Node->RegisterWithGameInstance(WorldContextObject);

	return Node;
}

void UDreamTaskAsyncListener::Activate()
{
	Task->OnTaskCompleted.AddDynamic(this, &UDreamTaskAsyncListener::HandleInitialize);
	Task->OnTaskUpdate.AddDynamic(this, &UDreamTaskAsyncListener::HandleUpdate);
	Task->OnTaskCompleted.AddDynamic(this, &UDreamTaskAsyncListener::HandleCompleted);
	Task->OnTaskConditionUpdate.AddDynamic(this, &UDreamTaskAsyncListener::HandleConditionUpdate);
}

void UDreamTaskAsyncListener::HandleInitialize(UDreamTask* InTask)
{
	OnTaskInitialize.Broadcast(InTask);
}

void UDreamTaskAsyncListener::HandleUpdate(UDreamTask* InTask)
{
	OnTaskUpdate.Broadcast(InTask);
}

void UDreamTaskAsyncListener::HandleCompleted(UDreamTask* InTask)
{
	OnTaskCompleted.Broadcast(InTask);
}

void UDreamTaskAsyncListener::HandleConditionUpdate(UDreamTask* InTask)
{
	OnTaskConditionUpdate.Broadcast(InTask);
}

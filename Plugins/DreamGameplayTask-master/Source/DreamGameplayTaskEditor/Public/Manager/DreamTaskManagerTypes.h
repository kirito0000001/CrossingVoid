#pragma once
#include "Classes/DreamTaskComponent.h"

struct FDreamTaskManagerRowData;
struct FDreamTaskSpecHandle;
class UDreamTaskComponent;

typedef TSharedPtr<TWeakObjectPtr<UDreamTaskComponent>> FDreamManagerTaskComponent;
typedef TSharedPtr<FDreamTaskSpecHandle> FDreamTaskSpecHandlePtr;
typedef TSharedPtr<FDreamTaskManagerRowData> FDreamTaskManagerRowDataPtr;


struct FDreamManagerRowKey
{
	TWeakObjectPtr<UDreamTaskComponent> Component;
	int32 HandleIndex;

	FDreamManagerRowKey(TWeakObjectPtr<UDreamTaskComponent> InComponent, int32 InHandleIndex)
		: Component(InComponent)
		, HandleIndex(InHandleIndex)
	{
	}

	const FDreamTaskSpecHandle& GetHandle() const;
};

inline const FDreamTaskSpecHandle& FDreamManagerRowKey::GetHandle() const
{
	return Component.Get()->TaskData.GetHandles()[HandleIndex]; 
}

typedef TSharedPtr<FDreamManagerRowKey> FDreamManagerAccessKey;

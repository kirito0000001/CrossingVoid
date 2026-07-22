#include "InventoryBaseItem.h"
#include "CP_Inventory.h"

UInventoryBaseItem::UInventoryBaseItem()
{
	ItemData.Time = FDateTime::Now();
	ROC_OnConstruct();
}

UInventoryBaseItem::~UInventoryBaseItem()
{
}

void UInventoryBaseItem::ChangeItemData()
{
	ROC_OnItemDataChanged();
	OnItemDataChanged.Broadcast();
}

void UInventoryBaseItem::DestroyItem()
{
	OnItemRemove.Broadcast();
	ConditionalBeginDestroy();
}

void UInventoryBaseItem::SetNewComponent(UCP_Inventory* NewComp)
{
	if (!NewComp)
	{
		return;
	}
	if (OwnerComponent)
	{
		OwnerComponent->ItemDatas.Remove(this);
		OwnerComponent->OnInventoryDataRefresh.Broadcast();
	}
	OwnerComponent = NewComp;
	NewComp->ItemDatas.Add(this);
	NewComp->OnInventoryDataRefresh.Broadcast();
}

FItemInformation UInventoryBaseItem::ExtractData()
{
	return ItemData;
}

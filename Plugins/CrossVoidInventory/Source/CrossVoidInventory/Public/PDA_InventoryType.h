#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_InventoryType.generated.h"

UCLASS()
class CROSSVOIDINVENTORY_API UPDA_InventoryType : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Type", DisplayName = "名称")
	FText Name = {};
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Type", DisplayName = "图标")
	TSoftObjectPtr<UTexture2D> Icon = {};
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Type", DisplayName = "描述")
	FText Description = {};
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Type", DisplayName = "优先级")
	int Priority = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Type", DisplayName = "主题色")
	FLinearColor ThemeColor = {};
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Type", DisplayName = "子类别")
	TArray<TSubclassOf<UPDA_InventoryType>> SubType = {};
};

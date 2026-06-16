#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CP_Inventory.h"
#include "InventoryFUClibrary.generated.h"

UCLASS()
class CROSSVOIDINVENTORY_API UInventoryFUClibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//函数
	/**物品合集筛选名称
	 * 可选择部分字查找物品
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param Name 名字
	 * @param Precise 是否精确查找
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选名称")
	static TArray<UInventoryBaseItem*> FilterItemSBYName(TArray<UInventoryBaseItem*> MapsIN, FText Name,
	                                                     bool Precise = false);

	/**物品合集筛选类型
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param Class 类型
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选类型")
	static TArray<UInventoryBaseItem*> FilterItemSBYClass(TArray<UInventoryBaseItem*> MapsIN,
	                                                      TSubclassOf<UInventoryBaseItem> Class);

	/**物品合集筛选关键词
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param KeyWord 关键字
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选关键词")
	static TArray<UInventoryBaseItem*>
	FilterItemSBYKeyWord(TArray<UInventoryBaseItem*> MapsIN, TArray<FString> KeyWord);

	/**物品合集筛选物品数量范围内
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param CountRange 数量范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选数量范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYCount(TArray<UInventoryBaseItem*> MapsIN, FIntPoint CountRange);


	/**物品合集筛选行列位置
	 * -1是不使用那个行或列进行查找
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param Location 行列查找
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选行列位置")
	static TArray<UInventoryBaseItem*> FilterItemSBYLocation(TArray<UInventoryBaseItem*> MapsIN,
	                                                         FVector2D Location = FVector2D(-1, -1));

	/**物品合集筛选品质
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param QualityRange 品质范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选品质范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYQuality(TArray<UInventoryBaseItem*> MapsIN, FIntPoint QualityRange);

	/**物品合集筛选重量
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param WeightRange 重量范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选重量范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYWeight(TArray<UInventoryBaseItem*> MapsIN, FVector2D WeightRange);

	/**物品合集筛选价格
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param PriceRange 价格范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选价格范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYPrice(TArray<UInventoryBaseItem*> MapsIN, FIntPoint PriceRange);

	/**物品合集筛选物品类型
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param Type 类型
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选自定义种类范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYType(TArray<UInventoryBaseItem*> MapsIN, UPDA_InventoryType* Type);

	/**物品合集筛选物品耐久
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param DurabilityRange 耐久范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选耐久范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYDurability(TArray<UInventoryBaseItem*> MapsIN,
	                                                           FIntPoint DurabilityRange);

	/**物品合集筛选物品等级
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param LevelRange 等级范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选等级范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYLevel(TArray<UInventoryBaseItem*> MapsIN, FIntPoint LevelRange);

	/**物品合集筛选日期内
	 * 根据月份和日期进行筛选
	 * -1就是关闭筛选选项
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param YearRange 年份范围（-1就是无限，全部-1就是不使用）
	 * @param MounthRange 月份范围（-1就是无限，全部-1就是不使用）
	 * @param DayRange 日期范围（-1就是无限，全部-1就是不使用）
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选日期范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYDate(TArray<UInventoryBaseItem*> MapsIN,
	                                                     FVector2D YearRange = FVector2D(-1, -1),
	                                                     FVector2D MounthRange = FVector2D(-1, -1),
	                                                     FVector2D DayRange = FVector2D(-1, -1));

	/**物品合集筛选优先级内
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param PriorityRange 优先级范围
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选优先级范围内")
	static TArray<UInventoryBaseItem*> FilterItemSBYPriority(TArray<UInventoryBaseItem*> MapsIN,
	                                                         FIntPoint PriorityRange);

	/**物品合集筛选相关物品
	 * @return 筛选后的物品合集
	 * @param MapsIN 物品合集
	 * @param Class 相关物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|筛选", DisplayName="物品合集筛选相关物品")
	static TArray<UInventoryBaseItem*> FilterItemSBYRelatedItem(TArray<UInventoryBaseItem*> MapsIN,
	                                                            TArray<TSubclassOf<UInventoryBaseItem>> Class);

	//物品交换位置
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item|操作", DisplayName="物品交换位置")
	static void SwapItemXY(UInventoryBaseItem* Item1, UInventoryBaseItem* Item2);
};

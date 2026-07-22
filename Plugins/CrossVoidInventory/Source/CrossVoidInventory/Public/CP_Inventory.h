#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryBaseItem.h"
#include "CP_Inventory.generated.h"


//单个物品数据结构体
USTRUCT(BlueprintType)
struct FItemSaveSingleData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="类型_基础")
	TSubclassOf<UInventoryBaseItem> Class = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="名称")
	FText Name = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="介绍")
	FText Description = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="数量")
	int Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="堆叠数量")
	int MaxCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="关键词")
	TArray<FString> KeyWords = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="位置")
	FIntPoint Location = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="品质")
	int Quality = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="重量")
	float Weight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="价格")
	int Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="耐久")
	int Durability = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="等级")
	int Level = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="优先级")
	int Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="页数")
	int Pages = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="额外词条")
	TArray<FString> ExtraAttributes = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="物品额外介绍")
	TArray<FText> ExtraDescription = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="当前技能")
	TArray<int> SkillNow = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="拥有的技能")
	TArray<bool> SkillHave = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="技能等级")
	TArray<int> SkillLevel = {};

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="当前幻形")
	int CharShapeNow = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component|Save", DisplayName="拥有的幻形")
	TArray<bool> CharShapeHas = {};

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="速度值")
	int Speed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="生命值")
	int Health = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="攻击力")
	int Attack = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="防御力")
	int PhyDefense = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="异能防御值")
	int MagDefense = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="暴击率")
	int Critical = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="暴击伤害")
	int CriticalC = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Component|Save", DisplayName="同步率")
	int Synchronize = 0;
	
	FItemSaveSingleData()
	{
		Class = UInventoryBaseItem::StaticClass();
		Name = FText::FromString("物品");
		Description = FText::FromString("物品");
		Count = 1;
		MaxCount = 1;
		KeyWords = {};
		Location = FIntPoint(0, 0);
		Quality = 0;
		Weight = 0.0f;
		Price = 0;
		Durability = 0;
		Level = 0;
		Priority = 0;
		Pages = 0;
		ExtraAttributes = {};
		ExtraDescription = {};
		SkillNow = {};
		SkillHave = {};
		SkillLevel = {1,1,1,1};
		CharShapeNow = 0;
		CharShapeHas = {};
		Speed = 0;
		Health = 0;
		Attack = 0;
		PhyDefense = 0;
		MagDefense = 0;
		Critical = 0;
		CriticalC = 0;
		Synchronize = 0;
	}
};

//单个物品数据枚举
UENUM(BlueprintType)
enum class EItemSaveSingleEnum : uint8
{
	Name = 0 UMETA(DisplayName="名称"),
	Description = 1 UMETA(DisplayName="介绍"),
	Count = 2 UMETA(DisplayName="数量"),
	MaxCount = 3 UMETA(DisplayName="堆叠数量"),
	KeyWords = 4 UMETA(DisplayName="关键词"),
	Location = 5 UMETA(DisplayName="位置"),
	Quality = 6 UMETA(DisplayName="品质"),
	Weight = 7 UMETA(DisplayName="重量"),
	Price = 8 UMETA(DisplayName="价格"),
	Durability = 9 UMETA(DisplayName="耐久"),
	Level = 10 UMETA(DisplayName="等级"),
	Priority = 11 UMETA(DisplayName="优先级"),
	Pages = 12 UMETA(DisplayName="页数"),
	ExtraAttributes = 13 UMETA(DisplayName="额外词条"),
	ExtraDescription = 14 UMETA(DisplayName="额外介绍"),
	
	SkillNow = 15 UMETA(DisplayName="当前技能"),
	SkillHave = 16 UMETA(DisplayName="拥有的技能"),
	CharShapeNow = 17 UMETA(DisplayName="当前幻形"),
	CharShapeHas = 18 UMETA(DisplayName="拥有的幻形"),
	Speed = 19 UMETA(DisplayName="速度值"),
	Health = 20 UMETA(DisplayName="生命值"),
	Attack = 21 UMETA(DisplayName="攻击力"),
	PhyDefense = 22 UMETA(DisplayName="物防"),
	MagDefense = 23 UMETA(DisplayName="异防"),
	Critical = 24 UMETA(DisplayName="暴击率"),
	CriticalC = 25 UMETA(DisplayName="暴击伤害"),
	Synchronize = 26 UMETA(DisplayName="同步率")
};

//库存物品合集数据
USTRUCT(BlueprintType)
struct FInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Component|Save", DisplayName="物品合集数据")
	TArray<FItemSaveSingleData> ItemSSaveData;
};


UCLASS(ClassGroup=(CrossingVoid), meta=(BlueprintSpawnableComponent))
class CROSSVOIDINVENTORY_API UCP_Inventory : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCP_Inventory();

	//委托
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FComponentDelegaet);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComponentDelegaetSort, const TArray<UInventoryBaseItem*>&, OutItem);

	

	
	//物品数量改变/刷新时
	UPROPERTY(BlueprintAssignable, Category="Inventory|Component", DisplayName="物品数量改变/刷新时")
	FComponentDelegaet OnInventoryDataRefresh = {};

	//筛选后排序
	UPROPERTY(BlueprintAssignable, Category="Inventory|Component", DisplayName="筛选后排序")
	FComponentDelegaetSort OnInventoryDataSort = {};

public: //变量，数据

	//物品合集
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Component")
	TArray<UInventoryBaseItem*> ItemDatas = {};

	//库存的XY
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory|Component", DisplayName="库存的XY大小")
	FIntPoint InventorySize = {};

	//是否启用翻页功能
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory|Component", DisplayName="是否启用翻页功能")
	bool IsEnablePage = false;

	//库存存档数据
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Component", DisplayName="库存存档数据")
	FInventorySaveData SaveData = {};

public: //函数

	
	//手动触发刷新/变更委托
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="手动触发刷新/变更委托")
	void RefreshInventoryDataByFuc();

	/**从类给予道具
	@param Residue 溢出数量
	@param InClass 物品的类
	@param Count 给的数量
	@param UseStack 是否使用堆叠
	@return 生成的道具
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="从类给予道具")
	UInventoryBaseItem* GiveItemByClass(int& Residue, TSubclassOf<UInventoryBaseItem> InClass, int Count = 1,
	                                    bool UseStack = true);

	/**从类给予道具_批量
	 * @param InClass 物品的类
	 * @param Count 统一物品数量，0则不使用
	 * @param AllCount 各种对应数量，优先级低
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="从类给予道具_批量",meta=(AutoCreateRefTerm="AllCount"))
	bool GiveItemByClassBatch(TArray<TSubclassOf<UInventoryBaseItem>> InClass,TArray<int> AllCount,int Count);

	/**获取物品合集
	 * @return 物品合集
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="获取物品合集")
	TArray<UInventoryBaseItem*> GetItemDatas();

	/**构建库存存档数据
	 * @return 库存存档数据(可网络传递)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="构建库存存档数据")
	FInventorySaveData BuildInventorySaveData();

	/**加载库存存档数据
	 * @param InSaveData 存档数据(配合构建存档使用)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="加载库存存档数据")
	void LoadInventorySaveData(FInventorySaveData InSaveData);

	//清空库存数据，在重复读取存档的时候使用
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="清空库存数据")
	void ClearInventoryData();

	/**移除物品_类型
	 * 直接移除类的所有物品
	 * @param InClass 物品类型
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="移除物品_类型")
	void RemoveItemByClass(TSubclassOf<UInventoryBaseItem> InClass);

	/**移除物品_指定
	 * @param InItem 要销毁的物品
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="移除物品_指定")
	void RemoveItemByItem(UInventoryBaseItem* InItem);

	/**使用物品_消耗数量
	 *可以消耗其他组的,也是可选
	 * @return 是否成功使用
	 * @param InItem 目标物品
	 * @param Count 使用数量
	 * @param UseOther 是否使用同类其他组的
	 * @param ResidueNeed 缺少多少数量
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="使用物品_消耗数量")
	bool UseItem(int& ResidueNeed, UInventoryBaseItem* InItem, int Count = 0, bool UseOther = true);

	/**使用物品_按类触发
	 * @return 是否成功使用
	 * @param InClass 目标物品类
	 * @param Count 使用数量
	 * @param ResidueNeed 缺少多少数量
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="使用物品_按类触发")
	bool UseItemByClass(int& ResidueNeed, TSubclassOf<UInventoryBaseItem> InClass, int Count = 0);

	/**计算位置还能放下吗
	 *核心函数，一般蓝图不使用，可以用来测试下个物品还有没有地方放
	 * @return 是否能放
	 * @param LocationOut 可以放的位置是
	 * @param PageOut 可以放进哪个页
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="计算位置还能放下吗")
	bool CanPutItem(FIntPoint& LocationOut, int& PageOut);

	/**重置默认XY排序
	 *按物品给予的顺序为默认顺序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="重置默认XY排序")
	void ResetItemXY();

	/**按筛选合集修正XY排序
	 *物品给予的顺序为默认顺序
	 *根据输入的合集进行排序，同时触发 筛选后排序 委托
	 *也可以直接放进未经筛选的合集，进行倒序操作
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="按筛选合集修正XY排序")
	void SortItemXYByInput(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按数量排序XY
	 * 按数量排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按数量排序XY")
	void SortItemXYByCount(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按品质排序XY
	 * 按品质排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按品质排序XY")
	void SortItemXYByQuality(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按重量排序XY
	 * 按重量排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按重量排序XY")
	void SortItemXYByWeight(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按价格排序XY
	 * 按价格排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按价格排序XY")
	void SortItemXYByPrice(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按耐久排序XY
	 * 按耐久排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按耐久排序XY")
	void SortItemXYByDurability(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按等级排序XY
	 * 按等级排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按等级排序XY")
	void SortItemXYByLevel(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**按优先级排序XY
	 * 按优先级排序输入进来的合集
	 * @param MapSin 物品合集
	 * @param Reverse 是否倒序
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component|排序", DisplayName="按优先级排序XY")
	void SortItemXYByPriority(TArray<UInventoryBaseItem*> MapSin, bool Reverse);

	/**使用物品_设置数值
	 * 直接设物品的数值
	 * 请完全符合对应的格式，不然不会生效
	 * 【文本：名称，介绍/文本数组：额外介绍文本
	 * /整数：数量，堆叠数量，品质，价格，耐久，等级，优先级，页数
	 * /浮点：重量/字符串数组：关键词，可拓展词条】
	 * @param InItem 目标物品
	 * @param Param 设置的变量
	 * @param Variable 设置的数值
	 */
	UFUNCTION(BlueprintCallable, CustomThunk,
		meta = (CustomStructureParam = "Variable"), Category = "Inventory|Component",
		DisplayName="使用物品_设置数值")
	void SetItemValue(UInventoryBaseItem* InItem, EItemSaveSingleEnum Param, const int32& Variable);
	DECLARE_FUNCTION(execSetItemValue)
	{
		P_GET_OBJECT(UInventoryBaseItem, Wildcard_InItem); //获取物品
		P_GET_ENUM(EItemSaveSingleEnum, Wildcard_Param); //获取枚举

		Stack.MostRecentProperty = nullptr; //获取通配符
		Stack.StepCompiledIn<FProperty>(nullptr); //获取通配符
		void* Wildcard_Variable = Stack.MostRecentPropertyAddress; //获取参数的内存地址
		FProperty* Wildcard_Property = Stack.MostRecentProperty; //获取参数的FProperty

		P_FINISH; //结束获取

		P_NATIVE_BEGIN; //函数开始
			//如果有物品
			if (Wildcard_InItem != nullptr)
			{
				//根据枚举来进行不同的设置
				switch (Wildcard_Param)
				{
				case EItemSaveSingleEnum::Name: //从名字开始
					if (Wildcard_Property->IsA(FTextProperty::StaticClass()))
					{
						FText* VarName = static_cast<FText*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Name = *VarName;
					}
					break;
				case EItemSaveSingleEnum::Description: //描述
					if (Wildcard_Property->IsA(FTextProperty::StaticClass()))
					{
						FText* VarDescription = static_cast<FText*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Description = *VarDescription;
					}
					break;
				case EItemSaveSingleEnum::Count:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarCount = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Count = *VarCount;
					}
					break;
				case EItemSaveSingleEnum::MaxCount:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarMaxCount = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.MaxCount = *VarMaxCount;
					}
					break;
				case EItemSaveSingleEnum::KeyWords:
					if (Wildcard_Property->IsA(FArrayProperty::StaticClass()))
					{
						FArrayProperty* ArrayProp = CastField<FArrayProperty>(Wildcard_Property);
						if (ArrayProp && ArrayProp->Inner->IsA(FStrProperty::StaticClass()))
						{
							TArray<FString>* VarKeyWords = static_cast<TArray<FString>*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.KeyWords = *VarKeyWords;
						}
					}
					break;
				case EItemSaveSingleEnum::Location:
					if (Wildcard_Property->IsA(FStructProperty::StaticClass()))
					{
						FStructProperty* StructProp = CastField<FStructProperty>(Wildcard_Property);
						if (StructProp && StructProp->Struct == TBaseStructure<FIntPoint>::Get())
						{
							FIntPoint* VarLoc = static_cast<FIntPoint*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.Location = *VarLoc;
						}
					}
					break;
				case EItemSaveSingleEnum::Quality:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarQuality = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Quality = *VarQuality;
					}
					break;
				case EItemSaveSingleEnum::Weight:
					if (Wildcard_Property->IsA(FFloatProperty::StaticClass()))
					{
						float* VarWeight = static_cast<float*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Weight = *VarWeight;
					}
					break;
				case EItemSaveSingleEnum::Price:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPrice = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Price = *VarPrice;
					}
					break;
				case EItemSaveSingleEnum::Durability:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarDurability = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Durability = *VarDurability;
					}
					break;
				case EItemSaveSingleEnum::Level:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarLevel = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Level = *VarLevel;
					}
					break;
				case EItemSaveSingleEnum::Priority:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Priority = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::Pages:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.Pages = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::ExtraAttributes:
					if (Wildcard_Property->IsA(FArrayProperty::StaticClass()))
					{
						FArrayProperty* ArrayProp = CastField<FArrayProperty>(Wildcard_Property);
						if (ArrayProp && ArrayProp->Inner->IsA(FStrProperty::StaticClass()))
						{
							TArray<FString>* VarExtraDescription = static_cast<TArray<FString>*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.ExtraAttributes = *VarExtraDescription;
						}
					}
					break;
				case EItemSaveSingleEnum::ExtraDescription:
					if (Wildcard_Property->IsA(FArrayProperty::StaticClass()))
					{
						FArrayProperty* ArrayProp = CastField<FArrayProperty>(Wildcard_Property);
						if (ArrayProp && ArrayProp->Inner->IsA(FTextProperty::StaticClass()))
						{
							TArray<FText>* VarExtraDescription = static_cast<TArray<FText>*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.ExtraDescription = *VarExtraDescription;
						}
					}
					break;
				case EItemSaveSingleEnum::SkillNow:
					if (Wildcard_Property->IsA(FArrayProperty::StaticClass()))
					{
						FArrayProperty* ArrayProp = CastField<FArrayProperty>(Wildcard_Property);
						if (ArrayProp && ArrayProp->Inner->IsA(FIntProperty::StaticClass()))
						{
							TArray<int>* VarKeyWords = static_cast<TArray<int>*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.CharData.SkillNow = *VarKeyWords;
						}
					}
					break;
				case EItemSaveSingleEnum::SkillHave:
					if (Wildcard_Property->IsA(FArrayProperty::StaticClass()))
					{
						FArrayProperty* ArrayProp = CastField<FArrayProperty>(Wildcard_Property);
						if (ArrayProp && ArrayProp->Inner->IsA(FBoolProperty::StaticClass()))
						{
							TArray<bool>* VarKeyWords = static_cast<TArray<bool>*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.CharData.SkillHave = *VarKeyWords;
						}
					}
					break;
				case EItemSaveSingleEnum::CharShapeNow:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.CharShapeNow = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::CharShapeHas:
					if (Wildcard_Property->IsA(FArrayProperty::StaticClass()))
					{
						FArrayProperty* ArrayProp = CastField<FArrayProperty>(Wildcard_Property);
						if (ArrayProp && ArrayProp->Inner->IsA(FBoolProperty::StaticClass()))
						{
							TArray<bool>* VarKeyWords = static_cast<TArray<bool>*>(Wildcard_Variable);
							Wildcard_InItem->ItemData.CharData.CharShapeHas = *VarKeyWords;
						}
					}
					break;
				case EItemSaveSingleEnum::Speed:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.Speed = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::Health:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.Health = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::Attack:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.Attack = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::PhyDefense:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.PhyDefense = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::MagDefense:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.MagDefense = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::Critical:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.Critical = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::CriticalC:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.CriticalC = *VarPriority;
					}
					break;
				case EItemSaveSingleEnum::Synchronize:
					if (Wildcard_Property->IsA(FIntProperty::StaticClass()))
					{
						int* VarPriority = static_cast<int*>(Wildcard_Variable);
						Wildcard_InItem->ItemData.CharData.Synchronize = *VarPriority;
					}
					break;
				}
				Wildcard_InItem->ChangeItemData(); //结束了后触发委托
			}
		P_NATIVE_END; //函数结束
	};

	/**一键合并类的物品
	 * 如果超过一组上限会自动分组
	 * @param Class 需要合并的类
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Component", DisplayName="一键合并类的物品")
	void MergeItemSBYClass(TSubclassOf<UInventoryBaseItem> Class);
};

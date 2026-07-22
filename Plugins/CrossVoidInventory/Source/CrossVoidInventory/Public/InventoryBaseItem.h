#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PDA_InventoryType.h"
#include "InventoryBaseItem.generated.h"


class UCP_Inventory;

USTRUCT(BlueprintType)
struct FSkillData
{
	GENERATED_BODY()
	//技能名字
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能名字")
	FText Name = {};
	//技能的绝对位置
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能的绝对位置")
	int Position = 0;
	//技能类型（0大招，1普通，2特殊）
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能类型")
	int Type = 0;
	//技能图标
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能图标")
	TSoftObjectPtr<UTexture2D> Icon = {};
	//技能介绍
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能介绍")
	FText Description = {};
};

USTRUCT(BlueprintType)
struct FCharInformation
{
	GENERATED_BODY()
	//零境组
	//当前幻形
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="当前幻形")
	int CharShapeNow = 0;
	//拥有的幻形合集
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="拥有的幻形合集")
	TArray<bool> CharShapeHas = {};
	//幻形头像合集
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="幻形头像合集")
	TArray<TSoftObjectPtr<UTexture2D>> CharShapeIcon = {};
	//幻形立绘合集，当前的要自己从数组获取
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="幻形立绘合集")
	TArray<TSoftObjectPtr<UTexture2D>> CharShapeGroup = {};
	//幻形完整立绘合集
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="幻形完整立绘合集")
	TArray<TSoftObjectPtr<UTexture2D>> CharShapeComplete = {};
	//当前技能
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="当前技能")
	TArray<int> SkillNow = {};
	//拥有的技能
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="拥有的技能合集")
	TArray<bool> SkillHave = {};
	//技能的各个详情
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="技能的各个详情")
	TArray<FSkillData> SkillData = {};
	//当前技能的等级
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="当前技能的等级")
	TArray<int> SkillLevel = {1,1,1,1};
	//被动介绍合集
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="被动介绍合集")
	TArray<FText> SkillDescription = {};
	//属性值
	//速度值
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="速度值")
	int Speed = 0;
	//生命值
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="生命值")
	int Health = 0;
	//攻击力
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="攻击力")
	int Attack = 0;
	//物理防御值
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="物理防御")
	int PhyDefense = 0;
	//异能防御值
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="异能防御")
	int MagDefense = 0;
	//暴击率
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="暴击率")
	int Critical = 0;
	//暴击伤害
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="暴击伤害")
	int CriticalC = 0;
	//同步率（熟练度）
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Char", DisplayName="同步率")
	int Synchronize = 0;
};

USTRUCT(BlueprintType)
struct FItemInformation
{
	GENERATED_BODY()
	//物品的基础信息
	//名称
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="名称")
	FText Name = {};
	//介绍
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="介绍", meta=(MultiLine=true))
	FText Description = {};
	//图标
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="图标")
	TSoftObjectPtr<UTexture2D> Icon = {};
	//类型
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="类型")
	UPDA_InventoryType* Type = nullptr;
	//数量
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="数量")
	int Count = 0;
	//堆叠数量上限
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="堆叠数量上限")
	int MaxCount = 0;
	//关键词
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="关键词")
	TArray<FString> KeyWords = {};
	//物品的位置
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="物品的位置")
	FIntPoint Location = {};
	
	//物品的额外属性
	//人物类型物品数据
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="人物类型物品数据")
	FCharInformation CharData = {};
	//品质
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="品质")
	int Quality = 0;
	//重量
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="重量")
	float Weight = 0.0f;
	//价格
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="价格")
	int Price = 0;
	//耐久
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="耐久")
	int Durability = 0;
	//等级
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="等级")
	int Level = 0;
	//获取时间
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="获取时间")
	FDateTime Time = {};
	//优先级
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="优先级")
	int Priority = 0;
	//页数
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="页数")
	int Pages = 0;
	//相关物品
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="相关物品")
	TMap<TSubclassOf<UInventoryBaseItem>, FString> RelatedItem = {};
	//可拓展词条
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="可拓展词条")
	TArray<FString> ExtraAttributes = {};
	//额外介绍文本
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|额外属性列表", DisplayName="额外介绍文本")
	TArray<FText> ExtraDescription = {};
};

UCLASS(Blueprintable)
class CROSSVOIDINVENTORY_API UInventoryBaseItem :  public UObject
{
	GENERATED_BODY()

public:
	UInventoryBaseItem();
	~UInventoryBaseItem();

public:
	//委托
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FItemDelegaet);

	//物品数值改变时
	UPROPERTY(BlueprintAssignable, Category="Inventory|Item", DisplayName="物品数值改变时")
	FItemDelegaet OnItemDataChanged;

	//物品从背包物品移除/使用时
	UPROPERTY(BlueprintAssignable, Category="Inventory|Item", DisplayName="物品移除/使用时")
	FItemDelegaet OnItemRemove;

public:
	//事件函数
	//物品构造时
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|Item", DisplayName = "物品构造时")
	void ROC_OnConstruct();

	//物品数值改变时
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|Item", DisplayName = "物品数值改变时")
	void ROC_OnItemDataChanged();

	//物品使用时
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|Item", DisplayName = "物品使用时")
	void ROC_OnItemUse();

public:
	//手动触发改变物品数值委托
	UFUNCTION(BlueprintCallable, Category="Inventory|Item", DisplayName = "手动触发改变物品数值委托")
	void ChangeItemData();

	//手动触发物品摧毁时的委托
	UFUNCTION(BlueprintCallable, Category="Inventory|Item", DisplayName = "手动触发物品摧毁时的委托")
	void DestroyItem();

	//物品基础信息
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="物品基础信息")
	FItemInformation ItemData = {};

	//父类组件
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item", DisplayName="父类组件")
	UCP_Inventory* OwnerComponent = nullptr;

	/**设置新的父类组件
	 * @param NewComp 新的父类组件
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory|Item", DisplayName = "设置新的父类组件")
	void SetNewComponent(UCP_Inventory* NewComp);

	/**提取数据
	 * 单个物品的数据，可以直接找obj的ItemData进行提取和覆盖
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory|Item", DisplayName = "提取数据")
	FItemInformation ExtractData();
};

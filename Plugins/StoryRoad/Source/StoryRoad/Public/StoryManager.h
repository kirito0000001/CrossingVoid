#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StoryManager.generated.h"

UENUM(BlueprintType) //用于故事切换的条件约定
enum class EConditionType : uint8
{
	All UMETA(DisplayName="全部完成", ToolTip="所有条件必须完成"),
	Any UMETA(DisplayName="任一完成", ToolTip="任一满足条件即可通过"),
	Count UMETA(DisplayName="满足数量", ToolTip="需要完成多少条件，在下方写上数量"),
};

UENUM(BlueprintType) //条件判断方式
enum class EJudjeMode : uint8
{
	Custom UMETA(DisplayName="自定义函数条件", ToolTip="在组件的父类创建函数，自由进行逻辑判断，成功时请设置[自定义判断结果]为true"),
	Unconditional UMETA(DisplayName="无条件放行", ToolTip="此条件直接计入完成"),
	Ban UMETA(DisplayName="禁止通行", ToolTip="此条件禁止通过"),
	Equal UMETA(DisplayName="等于", ToolTip="上方的变量数值=下方的数值名称[例如：布尔变量X->true，true]"),
	NotEqual UMETA(DisplayName="不等于", ToolTip="上方变量数值!=下方数值名称[例如：整数变量X->114，514]"),
	Greater UMETA(DisplayName="大于", ToolTip="上方变量数值>下方数值名称[例如：浮点变量X->514.0，114.0]"),
	GreaterEqual UMETA(DisplayName="大于等于", ToolTip="上方变量数值>=下方数值名称[例如：浮点变量X->114.0，114.0]"),
	Less UMETA(DisplayName="小于", ToolTip="上方变量数值<下方数值名称[例如：浮点变量X->114.0，514.0]"),
	LessEqual UMETA(DisplayName="小于等于", ToolTip="上方变量数值<=下方数值名称[例如：浮点变量X->114.0，114.0]"),
	Contain UMETA(DisplayName="包含", ToolTip="上方变量数值<下方数值名称[例如：字符串变量X->我是文字，文字]"),
	NotContain UMETA(DisplayName="不包含", ToolTip="上方变量数值不包含下方数值名称[例如：字符串变量X->我是文字，没有我]"),
	Valid UMETA(DisplayName="有效", ToolTip="上方的变量有效"),
	Invalid UMETA(DisplayName="无效", ToolTip="上方的变量无效"),
};

UENUM(BlueprintType) //变量类型
enum class EStoryPropertyType : uint8
{
	Unknown UMETA(DisplayName="未知类型"),
	Int UMETA(DisplayName="整数"),
	Float UMETA(DisplayName="浮点"),
	Bool UMETA(DisplayName="布尔"),
	String UMETA(DisplayName="字符串"),
	Name UMETA(DisplayName="命名"),
	CObject UMETA(DisplayName="Object类"),
};


USTRUCT(BlueprintType)
struct FStoryJudjeInfor
{
	GENERATED_BODY() //故事到达前的判断

	//要进行判断的变量，直接写名字就可以在组件的父类里寻找
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="要进行判断的变量",
		meta=(EditCondition="JudjeType!=EJudjeMode::Unconditional && JudjeType!=EJudjeMode::Ban"))
	FName PropName = "";
	/**
	 * 两个变量的判断条件，运算逻辑
	 * 支持的变量类：布尔，整数浮点数，字符串，名称，数据表格，Object类
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="判断条件")
	EJudjeMode JudjeType;
	//目标变量值，写字符串，会找到变量然后进行判断
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="目标变量值",
		meta=(EditCondition=
			"JudjeType!=EJudjeMode::Custom && JudjeType!=EJudjeMode::Unconditional && JudjeType!=EJudjeMode::Ban && JudjeType!=EJudjeMode::Valid && JudjeType!=EJudjeMode::Invalid"
		))
	FString TargetValue;
	FStoryJudjeInfor()
	{
		PropName = "";
		JudjeType = EJudjeMode::Equal;
		TargetValue = "";
	}
};


USTRUCT(BlueprintType)
struct FStoryGoon
{
	GENERATED_BODY() //准备去的故事块

	/**
	 * 要去的故事，如果不填，则代表故事结束
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="要去的故事")
	UDataTable* TargetStory;

	//判断条件合集
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="判断条件合集")
	TArray<FStoryJudjeInfor> Judjes;

	//或与非判断条件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="或与非判断条件")
	EConditionType ConditionType;

	//至少完成条件的数量，在判断条件为“满足数量”时生效
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="至少完成条件的数量",
		meta=(EditCondition="ConditionType==EConditionType::Count"))
	int32 CompleteCount;
	FStoryGoon()
	{
		TargetStory = nullptr;
		Judjes.Empty();
		ConditionType = EConditionType::All;
		CompleteCount = 0;
	}
};

USTRUCT(BlueprintType)
struct FStoryInfor
{
	GENERATED_BODY() //接下来的所有的故事

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story", DisplayName="接下来的所有的故事")
	TArray<FStoryGoon> StoryOnSetMaps;
	FStoryInfor()
	{
		StoryOnSetMaps.Empty();
	}
};

USTRUCT(BlueprintType)
struct FAllStoryManager : public FTableRowBase
{
	GENERATED_BODY() //用于表格制作不同的故事

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Setting", DisplayName="所有的故事")
	TMap<UDataTable*, FStoryInfor> StoryOnSetTables;
	FAllStoryManager()
	{
		StoryOnSetTables.Empty();
	}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STORYROAD_API UStoryManager : public UActorComponent
{
	GENERATED_BODY()

public: //变量
	// Sets default values for this component's properties
	UStoryManager();
	//委托
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FComponentDelegaet_Stroy); //普通触发
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FErroeMessage, FString, ErrorMessage); //错误信息

	UPROPERTY(BlueprintAssignable, Category="Story", DisplayName="故事切换了")
	FComponentDelegaet_Stroy OnStoryChange;

	UPROPERTY(BlueprintAssignable, Category="Story", DisplayName="故事条件未满足")
	FErroeMessage OnStoryDefeat;

	UPROPERTY(BlueprintAssignable, Category="Story", DisplayName="故事完结")
	FComponentDelegaet_Stroy OnStoryFinish;

	/**
	 * 所有的故事
	 * 在这里设置故事，然后设置下来会跳转的故事和条件
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Setting", DisplayName="所有的故事")
	TMap<UDataTable*, FStoryInfor> StoryOnSetMaps;

	/**
	 * 当前故事
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Setting", DisplayName="当前故事")
	UDataTable* CurrentStory;

	/**
	 * 自定义判断结果
	 * 配合自定义函数进行判断
	 */
	UPROPERTY(BlueprintReadWrite, Category="Story Setting", DisplayName="自定义判断结果")
	bool bCustomJudjeResult;

public: //函数

	//根据名字获取变量类型
	UFUNCTION(BlueprintCallable, Category="Story|System")
	EStoryPropertyType GetOwnerPropertyTypeByName(FName FieldName);
	//数值获取_整数
	UFUNCTION(BlueprintCallable, Category="Story|System")
	int32 GetIntPropertyByName(FName FieldName, bool& bSuccess);
	//数值获取_浮点
	UFUNCTION(BlueprintCallable, Category="Story|System")
	float GetFloatPropertyByName(FName FieldName, bool& bSuccess);
	//数值获取_布尔
	UFUNCTION(BlueprintCallable, Category="Story|System")
	bool GetBoolPropertyByName(FName FieldName, bool& bSuccess);
	//数值获取_字符串
	UFUNCTION(BlueprintCallable, Category="Story|System")
	FString GetStringPropertyByName(FName FieldName, bool& bSuccess);
	//数值获取_名称
	UFUNCTION(BlueprintCallable, Category="Story|System")
	FName GetNamePropertyByName(FName FieldName, bool& bSuccess);
	//数值获取_Object
	UFUNCTION(BlueprintCallable, Category="Story|System")
	UObject* GetObjectPropertyByName(FName FieldName, bool& bSuccess);


	/**
	* 根据名字调用 Owner 的函数
	* 自行创建判断，然后更改故事中转变量
	* 最后拿来直接判断是否跳转
	 */
	UFUNCTION(BlueprintCallable, Category="Story|System")
	bool CallFunctionByName(FName FunctionName);

	/**
	 * 故事的开篇，初始化
	 */
	UFUNCTION(BlueprintCallable, Category="Story|Function", DisplayName="故事初始化")
	void StoryInit(UDataTable* Story);

	/**
	 * 故事进行下一章
	 * 从组件里设置的条件判断下来要进行什么故事
	 * 如果成功切换，则会触发故事切换[委托]
	 * 如果没有顺利跳转，则会触发故事条件未满足[委托]
	 * 如果当前章节没有设置后续，则会触发故事完结[委托]
	 */
	UFUNCTION(BlueprintCallable, Category="Story|Function", DisplayName="故事进行下一章")
	void StoryNextChapter();
};

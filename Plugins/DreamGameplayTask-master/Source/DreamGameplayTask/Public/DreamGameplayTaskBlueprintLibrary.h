// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DreamGameplayTaskBlueprintLibrary.generated.h"

class UDreamTaskData;
class UDreamTaskComponent;
class UDreamTaskType;
class UDreamTask;
/**
 * 
 */
UCLASS()
class DREAMGAMEPLAYTASK_API UDreamGameplayTaskBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
#pragma region Common

public:
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Common")
	static UDreamTaskComponent* GetDreamTaskComponent(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Common")
	static TArray<UDreamTaskComponent*> GetDreamTaskComponents(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Common")
	static TSubclassOf<UDreamTask> GetDreamTaskClassByGUID(FGuid Guid);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Common")
	static FGuid GetDreamTaskGuid(TSubclassOf<UDreamTask> InTaskClass);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Common")
	static FDreamTaskSaveData ConstructDreamGameplayTaskSaveData(TArray<UDreamTask*> Tasks);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Common")
	static TArray<UDreamTask*> DestructDreamGameplayTaskSaveData(const FDreamTaskSaveData& Data);

	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Common", Meta = (DeterminesOutputType = "InTaskClass"))
	static UDreamTaskData* GetTaskData(UDreamTask* InTask, TSubclassOf<UDreamTaskData> InTaskClass);

#pragma endregion Common

#pragma region Filter

public:
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Filter")
	static TArray<UDreamTask*> FilterTasksByType(const TArray<UDreamTask*>& Tasks, UDreamTaskType* TaskType);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Filter")
	static TArray<UDreamTask*> FilterTasksByPriority(const TArray<UDreamTask*>& Tasks, EDreamTaskPriority Priority);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Filter")
	static TArray<UDreamTask*> FilterTasksByState(const TArray<UDreamTask*>& Tasks, EDreamTaskState State);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Filter")
	static TArray<FDreamTaskSpecHandle> FilterHandlesByType(TArray<FDreamTaskSpecHandle>& Handles, UDreamTaskType* TaskType);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Filter")
	static TArray<FDreamTaskSpecHandle> FilterHandlesByPriority(TArray<FDreamTaskSpecHandle>& Handles, EDreamTaskPriority Priority);

	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Filter")
	static TArray<FDreamTaskSpecHandle> FilterHandlesByState(TArray<FDreamTaskSpecHandle>& Handles, EDreamTaskState State);
#pragma endregion Filter

#pragma region Handles

public:
	/**
	 * 获取任务句柄对应的任务对象
	 * @param Handle 要查询的任务句柄
	 * @return 对应的任务对象指针，无效句柄返回nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static UDreamTask* GetHandleTask(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务句柄对应的任务条件对象
	 * @param Handle 要查询的任务句柄
	 * @param ConditionName 要查询的条件名称
	 * @return 对应的任务条件对象指针，无效句柄返回nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static UDreamTaskConditionTemplate* GetHandleTaskCondition(const FDreamTaskSpecHandle& Handle, FName ConditionName);

	/**
	 * 获取任务所属的任务组件
	 * @param Handle 要查询的任务句柄  
	 * @return 拥有该任务的任务组件
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static UDreamTaskComponent* GetHandleTaskOwnerComponent(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务的唯一标识GUID
	 * @param Handle 要查询的任务句柄
	 * @return 任务的GUID标识符
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static FGuid GetHandleGuid(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务的当前状态
	 * @param Handle 要查询的任务句柄
	 * @return 任务状态枚举值(未开始/进行中/已完成/已失败等)
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static EDreamTaskState GetHandleState(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务已运行时间
	 * @param Handle 要查询的任务句柄
	 * @return 任务已运行的时长(TimeSpan格式)
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static FTimespan GetHandleRunningTime(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务开始时间
	 * @param Handle 要查询的任务句柄  
	 * @return 任务的开始时间(DateTime格式)
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static FDateTime GetHandleStartTime(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务结束时间
	 * @param Handle 要查询的任务句柄
	 * @return 任务的结束时间(DateTime格式)，未结束返回默认值
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static FDateTime GetHandleEndTime(const FDreamTaskSpecHandle& Handle);

	/**
	 * 获取任务的条件映射表
	 * @param Handle 要查询的任务句柄(需非常量引用)
	 * @return 任务条件名称到条件模板的映射表引用
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static TMap<FName, UDreamTaskConditionTemplate*>& GetHandleConditions(UPARAM(Ref)
		FDreamTaskSpecHandle& Handle);

	/**
	 * 检查任务是否设置了最大时间限制
	 * @param Handle 要检查的任务句柄
	 * @return 如果有时间限制返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static bool IsHandleUseMaximumTime(const FDreamTaskSpecHandle& Handle);

	/**
	 * 检查任务是否已超时
	 * @param Handle 要检查的任务句柄
	 * @return 如果超过最大时间限制返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static bool IsHandleTimeout(const FDreamTaskSpecHandle& Handle);

	/**
	 * 检查任务是否已完成
	 * @param Handle 要检查的任务句柄
	 * @return 任务已完成返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static bool IsHandleCompleted(const FDreamTaskSpecHandle& Handle);

	/**
	 * 检查任务是否已失败
	 * @param Handle 要检查的任务句柄
	 * @return 任务已失败返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static bool IsHandleFailed(const FDreamTaskSpecHandle& Handle);

	/**
	 * 检查任务句柄是否有效
	 * @param Handle 要检查的任务句柄
	 * @return 句柄有效返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Handles")
	static bool IsHandleValid(const FDreamTaskSpecHandle& Handle);

	/**
	 * 设置任务的运行时间
	 * @param Handle 要设置的任务句柄(需非常量引用)
	 * @param InTime 要设置的运行时间(TimeSpan格式)
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Handles")
	static void SetHandleRunningTime(UPARAM(Ref)
	                                 FDreamTaskSpecHandle& Handle, FTimespan InTime);

	/**
	 * 设置任务的开始时间
	 * @param Handle 要设置的任务句柄(需非常量引用) 
	 * @param InTime 要设置的开始时间(DateTime格式)
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Handles")
	static void SetHandleStartTime(UPARAM(Ref)
	                               FDreamTaskSpecHandle& Handle, FDateTime InTime);

	/**
	 * 设置任务的结束时间
	 * @param Handle 要设置的任务句柄(需非常量引用)
	 * @param InTime 要设置的结束时间(DateTime格式)
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Handles")
	static void SetHandleEndTime(UPARAM(Ref)
	                             FDreamTaskSpecHandle& Handle, FDateTime InTime);

	/**
	 * 增加任务的运行时间
	 * @param Handle 要修改的任务句柄(需非常量引用)
	 * @param InTime 要增加的时间增量(TimeSpan格式)
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Handles")
	static void AddHandleTime(UPARAM(Ref)
	                          FDreamTaskSpecHandle& Handle, FTimespan InTime);

	/**
	 * 以秒为单位增加任务的运行时间
	 * @param Handle 要修改的任务句柄(需非常量引用)
	 * @param InSeconds 要增加的秒数
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Handles")
	static void AddHandleTimeWithSeconds(UPARAM(Ref)
	                                     FDreamTaskSpecHandle& Handle, float InSeconds);

	/**
	 * 更新任务状态(通常每帧调用)
	 * @param Handle 要更新的任务句柄(需非常量引用)
	 * @param DeltaTime 帧时间增量(秒)
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Handles")
	static void UpdateHandle(UPARAM(Ref)
	                         FDreamTaskSpecHandle& Handle, float DeltaTime);

	/**
	 * 比较两个任务句柄是否相等
	 * @param A 第一个任务句柄
	 * @param B 第二个任务句柄
	 * @return 如果两个句柄指向同一任务返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Equal (DreamTaskHandle)", CompactNodeTitle="==", Keywords="== equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool EqualEqual_TaskHandleTaskHandle(const FDreamTaskSpecHandle& A, const FDreamTaskSpecHandle& B);

	/**
	 * 比较两个任务句柄是否不等
	 * @param A 第一个任务句柄
	 * @param B 第二个任务句柄
	 * @return 如果两个句柄指向不同任务返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Not Equal (DreamTaskHandle)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool NotEqual_TaskHandleTaskHandle(const FDreamTaskSpecHandle& A, const FDreamTaskSpecHandle& B);

	/**
	 * 比较任务句柄与任务对象是否相等
	 * @param A 任务句柄
	 * @param B 任务对象
	 * @return 如果句柄指向该任务对象返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Equal (DreamTaskHandle)", CompactNodeTitle="==", Keywords="== equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool EqualEqual_TaskHandleTask(const FDreamTaskSpecHandle& A, UDreamTask* B);

	/**
	 * 比较任务句柄与任务对象是否不等
	 * @param A 任务句柄
	 * @param B 任务对象
	 * @return 如果句柄不指向该任务对象返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Not Equal (DreamTaskHandle)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool NotEqual_TaskHandleTask(const FDreamTaskSpecHandle& A, UDreamTask* B);

	/**
	 * 比较任务句柄与任务名称是否匹配
	 * @param A 任务句柄
	 * @param B 任务名称
	 * @return 如果句柄任务名称匹配返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Equal (DreamTaskHandle)", CompactNodeTitle="==", Keywords="== equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool EqualEqual_TaskHandleTaskName(const FDreamTaskSpecHandle& A, FName B);

	/**
	 * 比较任务句柄与任务名称是否不匹配
	 * @param A 任务句柄
	 * @param B 任务名称
	 * @return 如果句柄任务名称不匹配返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Not Equal (DreamTaskHandle)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool NotEqual_TaskHandleTaskName(const FDreamTaskSpecHandle& A, FName B);

	/**
	 * 比较任务句柄与任务类是否匹配
	 * @param A 任务句柄
	 * @param B 任务类
	 * @return 如果句柄任务类匹配返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Equal (DreamTaskHandle)", CompactNodeTitle="==", Keywords="== equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool EqualEqual_TaskHandleTaskClass(const FDreamTaskSpecHandle& A, TSubclassOf<UDreamTask> B);

	/**
	 * 比较任务句柄与任务类是否不匹配
	 * @param A 任务句柄
	 * @param B 任务类
	 * @return 如果句柄任务类不匹配返回true，否则false
	 */
	UFUNCTION(BlueprintPure, meta=(IgnoreTypePromotion, DisplayName="Not Equal (DreamTaskHandle)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="DreamGameplayTaskFunctions|Handles")
	static bool NotEqual_TaskHandleTaskClass(const FDreamTaskSpecHandle& A, TSubclassOf<UDreamTask> B);


#pragma endregion Handles

#pragma region HandleContainer

public:
	/**
	 * 根据任务类型过滤容器中的任务句柄
	 * @param Container [输入/输出] 要过滤的任务句柄容器（通过UPARAM(Ref)标记为引用参数，允许修改原容器）
	 * @param InTaskType [输入] 过滤依据的任务类型指针（UDreamTaskType*类型）
	 * @return 返回过滤后的任务句柄数组引用（TArray<FDreamTaskSpecHandle>&），可直接操作结果数组
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static TArray<FDreamTaskSpecHandle> FilterContainerHandlesByTaskType(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		UDreamTaskType* InTaskType);

	/**
	 * 根据任务优先级过滤容器中的任务句柄  
	 * @param Container [输入/输出] 要过滤的任务句柄容器（需非常量引用）
	 * @param InPriority [输入] 过滤依据的任务优先级（EDreamTaskPriority枚举值）
	 * @return 返回过滤后的任务句柄数组引用，包含所有匹配优先级的任务句柄
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static TArray<FDreamTaskSpecHandle> FilterContainerHandlesByTaskPriority(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		EDreamTaskPriority InPriority);

	/**
	 * 根据任务状态过滤容器中的任务句柄
	 * @param Container [输入/输出] 要过滤的任务句柄容器（通过引用直接修改）
	 * @param InState [输入] 过滤依据的任务状态（EDreamTaskState枚举值）
	 * @return 返回过滤后的任务句柄数组引用，包含所有匹配状态的任务句柄
	 * @note 典型状态包括：未开始(NotStarted)、进行中(InProgress)、已完成(Completed)、已失败(Failed)等
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static TArray<FDreamTaskSpecHandle> FilterContainerHandlesByTaskState(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		EDreamTaskState InState);

	/**
	 * 获取容器中所有的任务句柄
	 * @param Container 任务句柄容器
	 * @return 包含所有任务句柄的数组引用
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static TArray<FDreamTaskSpecHandle>& GetContainerHandles(UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);

	/**
	 * 向容器中添加新任务句柄
	 * @param Container 要添加到的容器
	 * @param InHandle 要添加的任务句柄
	 * @return 新添加的任务句柄引用
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static FDreamTaskSpecHandle& AddContainerHandle(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		FDreamTaskSpecHandle InHandle);

	/**
	 * 从容器中移除指定任务句柄
	 * @param Container 要操作的容器
	 * @param InHandle 要移除的任务句柄
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static bool RemoveContainerHandle(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		const FDreamTaskSpecHandle& InHandle);

	/**
	 * 根据任务对象查找对应的句柄
	 * @param Container 要查找的容器
	 * @param InTask 要查找的任务对象
	 * @return 找到的任务句柄常量引用
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static const FDreamTaskSpecHandle& FindContainerHandleByClass(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		UDreamTask* InTask);

	/**
	 * 根据任务类查找对应的句柄
	 * @param Container 要查找的容器
	 * @param InClass 要查找的任务类
	 * @return 找到的任务句柄常量引用
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static const FDreamTaskSpecHandle& FindContainerHandleByTaskClass(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		TSubclassOf<UDreamTask> InClass);

	/**
	 * 根据任务名称查找对应的句柄
	 * @param Container 要查找的容器
	 * @param InName 要查找的任务名称
	 * @return 找到的任务句柄常量引用
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static const FDreamTaskSpecHandle& FindContainerHandleByTaskName(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		FName InName);

	/**
	 * 查找指定句柄在容器中的索引位置
	 * @param Container 要查找的容器
	 * @param InHandle 要查找的任务句柄
	 * @return 找到的索引位置，未找到返回-1
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static int32 FindContainerHandleIndex(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		const FDreamTaskSpecHandle& InHandle);

	/**
	 * 清空容器中的所有任务句柄
	 * Warning!!! 警告!!! 使用此方法不会向Component发送任务列表更新消息 !!!
	 * @param Container 要清空的容器
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static void ClearContainerHandles(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);

	/**
	 * 设置容器中的任务句柄列表
	 * @param Container 要设置的容器
	 * @param InHandles 新的任务句柄列表
	 * @return 设置的任务句柄数量
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static int SetContainerHandles(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		const TArray<FDreamTaskSpecHandle>& InHandles);

	/**
	 * 构建包含容器中所有任务对象的数组
	 * @param Container 要构建的任务容器
	 * @return 包含所有任务对象的数组
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static TArray<UDreamTask*> BuildContainerTaskArray(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);

	/**
	 * 更新容器中所有任务句柄的状态
	 * @param Container 要更新的容器
	 * @param DeltaTime 帧时间增量
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static void UpdateContainerHandles(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container,
		float DeltaTime);

	/**
	 * 检查容器中所有任务是否都已完成
	 * @param Container 要检查的容器
	 * @return 所有任务都已完成返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static bool IsContainerAllCompleted(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);

	/**
	 * 检查容器中是否有部分任务已完成
	 * @param Container 要检查的容器
	 * @return 有任务已完成返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static bool IsContainerSomeCompleted(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);

	/**
	 * 检查容器中是否没有任务已完成
	 * @param Container 要检查的容器
	 * @return 没有任务已完成返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static bool IsContainerNoCompleted(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);

	/**
	 * 检查容器是否为空
	 * @param Container 要检查的容器
	 * @return 容器为空返回true，否则false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|HandleContainer")
	static bool IsContainerEmpty(
		UPARAM(Ref)
		FDreamTaskSpecHandleContainer& Container);


#pragma endregion HandleContainer

#pragma region ConditionContainer

	/**
	 * 通过条件名称从容器中获取对应的条件模板
	 * @param Container 要查询的条件容器
	 * @param InConditionName 要查找的条件名称
	 * @return 找到的条件模板指针，如果未找到则返回nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Condition")
	static UDreamTaskConditionTemplate* GetConditionByName(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container,
		FName InConditionName);

	/**
	 * 获取容器中所有的条件模板
	 * @param Container 要查询的条件容器
	 * @return 包含所有条件模板的数组
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Condition")
	static TArray<UDreamTaskConditionTemplate*> GetConditions(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container);

	/**
	 * 获取容器中条件名称到条件模板的映射表
	 * @param Container 要查询的条件容器 
	 * @return 条件名称与条件模板的映射表引用
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Condition")
	static TMap<FName, UDreamTaskConditionTemplate*>& GetConditionMapping(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container);

	/**
	 * 更新指定名称的条件状态
	 * @param Container 要操作的条件容器
	 * @param InConditionName 要更新的条件名称
	 * @return 是否成功找到并更新了该条件
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Condition")
	static bool UpdateConditionByName(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container,
		FName InConditionName);

	/**
	 * 统计容器中已完成的条件数量
	 * @param Container 要统计的条件容器
	 * @return 已完成条件的数量
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Condition")
	static int ConditionCompletedCount(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container);

	/**
	 * 检查容器中的所有条件是否都已完成
	 * @param Container 要检查的条件容器
	 * @return 如果所有条件都已完成则返回true，否则返回false
	 */
	UFUNCTION(BlueprintPure, Category = "DreamGameplayTaskFunctions|Condition")
	static bool IsConditionsCompleted(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container);

	/**
	 * 重置容器中的所有条件状态
	 * @param Container 要重置的条件容器
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Condition")
	static void ResetConditionContainer(
		UPARAM(Ref)
		FDreamTaskConditionContainer& Container);


#pragma endregion ConditionContainer
};

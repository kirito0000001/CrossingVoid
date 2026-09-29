// Copyright 2022 - 2024 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskTypes.h"
#include "DreamTaskData.h"
#include "UObject/Object.h"
#include "DreamTask.generated.h"

class UDreamTaskData;
class UDreamTaskConditionTemplate;
class UDreamTaskType;
class UDreamTaskComponent;

/**
 * Dream Task Object
 */
UCLASS(Blueprintable, Abstract)
class DREAMGAMEPLAYTASK_API UDreamTask : public UObject
{
	GENERATED_BODY()

public:
	UDreamTask(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	/**
	 * 初始化任务
	 * @param InOwnerComponent 任务的拥有组件
	 * @param InPayload 额外数据
	 */
	virtual void InitializeTask(UDreamTaskComponent* InOwnerComponent, UObject* InPayload = nullptr);

public:
	// 任务的管理组件
	UPROPERTY(BlueprintReadOnly, Category = System)
	TObjectPtr<UDreamTaskComponent> OwnerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Task)
	FName TaskName = FName(TEXT("Task"));

	// 任务的名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Task)
	FText TaskDisplayName = FText::FromString(TEXT("NewTask"));

	// 任务的描述
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Task)
	FText TaskDesc = FText::FromString(TEXT("This is a new task."));

	// 任务的类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Task)
	UDreamTaskType* TaskType = nullptr;

	// 子任务
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Task)
	TArray<TSubclassOf<UDreamTask>> SubTasks;

	// 是否自动给子任务
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Task)
	bool bCompletedAutoGiveSubTask = false;

	// 相关的Actor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Task)
	TArray<TSubclassOf<AActor>> RelatedActorsClasses;

	// 任务的优先级
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Task)
	EDreamTaskPriority TaskPriority = EDreamTaskPriority::EDTP_Normal;

	// 任务数据
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = Task)
	TObjectPtr<UDreamTaskData> TaskData;

	// 是否启用最大完成时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (InlineEditConditionToggle))
	bool bUseMaximumCompletionTime = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Task, Meta = (EditCondition = "bUseMaximumCompletionTime", ClampMin = "0.0", Units = "Seconds"))
	float MaximumCompletionTimeSeconds;

	// 任务的条件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= Condition)
	FDreamTaskConditionContainer TaskCompletedCondition;

	// 任务的状态
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = State)
	EDreamTaskState TaskState = EDreamTaskState::EDTS_Accept;

	// 任务的额外数据 此数据无法保存
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data)
	TObjectPtr<UObject> Payload = nullptr;

	UPROPERTY(Transient)
	TArray<AActor*> CachedRelatedActors;

public:
	/**
	 * 获取任务的唯一名称标识
	 * @return FName 返回任务的唯一名称标识符
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	FName GetTaskName() const { return TaskName; }

	/**
	 * 获取任务的显示名称(可本地化)
	 * @return FText 返回任务的显示名称
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	FText GetTaskDisplayName() const { return TaskDisplayName; }

	/**
	 * 获取任务的描述信息 
	 * @return FText 返回任务的描述文本
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	FText GetTaskDesc() const { return TaskDesc; }

	/**
	 * 获取任务所属的类型
	 * @return UDreamTaskType* 返回任务类型对象指针
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	UDreamTaskType* GetTaskType() const { return TaskType; }

	/**
	 * 获取任务的当前状态
	 * @return EDreamTaskState 返回任务状态枚举值
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	EDreamTaskState GetTaskState() const { return TaskState; }

	/**
	 * 获取任务的优先级
	 * @return EDreamTaskPriority 获取任务的优先级枚举值
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	EDreamTaskPriority GetTaskPriority() const { return TaskPriority; }

	/**
	 * 获取任务所属的任务组件
	 * @return UDreamTaskComponent* 返回拥有该任务的任务组件
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	UDreamTaskComponent* GetOwnerComponent() const { return OwnerComponent; }

	/**
	 * 获取任务的附加数据
	 * @return UObject* 返回任务的附加数据对象
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	UObject* GetPayload() const { return Payload; }


	// --------------- Condition ---------------

	// 获取任务条件列表
	UFUNCTION(BlueprintPure, Category = Functions)
	TArray<UDreamTaskConditionTemplate*> GetTaskConditions()
	{
		return TaskCompletedCondition.GetConditions();
	}

	UFUNCTION(BlueprintPure, Category = Functions)
	UDreamTaskConditionTemplate* GetTaskCondition(FName ConditionName);

	UFUNCTION(BlueprintPure, Category = Functions)
	FDreamTaskConditionContainer& GetTaskConditionContainer()
	{
		return TaskCompletedCondition;
	}

	// ---------------- SubTask ---------------

	// 获取子任务类
	UFUNCTION(BlueprintPure, Category = Functions)
	TArray<TSubclassOf<UDreamTask>> GetSubTasks() { return SubTasks; }

	// ---------------- Tools -----------------

	// 检查任务是否完成
	UFUNCTION(BlueprintPure, Category = Functions)
	bool CheckTaskCompleted();

	// 获取相关Actor
	UFUNCTION(BlueprintPure, Category = Functions)
	TArray<AActor*> GetRelatedActors();

	/**
	 * 更新任务的条件 通过名称
	 * @param ConditionNames 要更新的条件名称
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void UpdateTaskByName(TArray<FName> ConditionNames);

	/**
	 * 设置任务状态
	 * @param NewState 新的任务状态
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void SetTaskState(EDreamTaskState NewState);

	/**
	 * 设置任务进度
	 * @param InValue 任务进度 <任务条件名称, 新的进度>
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void SetTaskConditionProgress(const TMap<FName, int32>& InValue);

	/**
	 * 获取任务进度
	 * @return 任务进度 <任务条件名称, 当前任务进度>
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	TMap<FName, int32> GetTaskConditionProgress() const;

	/**
	 * 获取自定义任务数据
	 * @return 自定义的任务数据
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	UDreamTaskData* GetTaskData() const;

	/**
	 * 任务是否完成
	 * @return 任务是否完成
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	bool IsCompleted() const { return EnumHasAnyFlags(TaskState, EDreamTaskState::EDTS_Completed); }

	/**
	 * 任务是否失败
	 * @return 任务是否失败
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	bool IsFailed() const { return EnumHasAnyFlags(TaskState, EDreamTaskState::EDTS_Failed); }

	/**
	 * 手动刷新任务
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void RefreshTask();

	/**
	 * 重置任务
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void ResetTask();

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTaskDelegate, UDreamTask*, Task);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTaskSimpleDelegate);

public:
	// 任务更新时
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskDelegate OnTaskUpdate;

	// 任务条件更新时
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskDelegate OnTaskConditionUpdate;

	// 任务完成时 
	UPROPERTY(BlueprintAssignable, Category = Delegate)
	FTaskDelegate OnTaskCompleted;

	// 任务移除时
	UPROPERTY(BlueprintAssignable, Category = Delegate)
	FTaskDelegate OnTaskRemoved;

	// 任务重置时
	UPROPERTY(BlueprintAssignable, Category = Delegate)
	FTaskDelegate OnTaskReset;

public:
	// 任务初始化时
	UFUNCTION(BlueprintNativeEvent, Category = Events, meta = (DisplayName = "Task Initialize"))
	void BP_TaskInitialize();

	// 任务更新时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Update"))
	void BP_TaskUpdate();

	// 任务条件更新时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Condition Update"))
	void BP_TaskConditionUpdate();

	// 任务完成时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Completed"))
	void BP_TaskCompleted();

	// 任务失败时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Failed"))
	void BP_TaskFailed();

	// 任务超时时
	UFUNCTION(BlueprintNativeEvent, Category = Events, meta = (DisplayName = "Task Going"))
	void BP_TaskGoing();

	// 任务超时时
	UFUNCTION(BlueprintNativeEvent, Category = Events, meta = (DisplayName = "Task Timeout"))
	void BP_TaskTimeout();

	// 任务开始时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Accept"))
	void BP_TaskAccept();

	// 任务移除时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Remove"))
	void BP_TaskRemove();

	// 任务重置时
	UFUNCTION(BlueprintNativeEvent, Category = Events, Meta = (DisplayName = "Task Reset"))
	void BP_TaskReset();

public:
	static UDreamTask* Create(TSubclassOf<UDreamTask> Class, TMap<FName, int32> Progress);

public:
	virtual UWorld* GetWorld() const override;

	void DelegateCall_TaskUpdate(bool bCallCondition);
	void DelegateCall_TaskReset();
	void DelegateCall_TaskRemoved();

protected:
	virtual void CompletedTask_Internal();
	virtual void AcceptTask_Internal();
	virtual void GoingTask_Internal();
	virtual void TimeoutTask_Internal();
	virtual void FailedTask_Internal();
	virtual void UpdateTaskState_Internal(EDreamTaskState NewState);
};

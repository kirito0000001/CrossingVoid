// Copyright 2022 - 2024 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskTypes.h"
#include "DreamTask.h"
#include "Components/ActorComponent.h"
#include "DreamTaskComponent.generated.h"

/**
 * Dream Gameplay Task Manager Component
 */
UCLASS(ClassGroup=(DreamPlugin), meta=(BlueprintSpawnableComponent))
class DREAMGAMEPLAYTASK_API UDreamTaskComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDreamTaskComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~UDreamTaskComponent() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTaskListDynamicMulticastDelegate, const FDreamTaskSpecHandleContainer&, TaskData);

	DECLARE_MULTICAST_DELEGATE_OneParam(FTaskListDelegate, FDreamTaskSpecHandleContainer&);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTaskDelegate, const FDreamTaskSpecHandle&, Task);

public:
	// 任务列表更新时
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskListDynamicMulticastDelegate OnTaskListChanged;

	// 任务列表更新时 CPP
	FTaskListDelegate OnTaskListChangedDelegate;
	
	// 任务更新时
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskDelegate OnTaskUpdate;

	// 任务移除时
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskDelegate OnTaskRemoved;

	// 任务重置时
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskDelegate OnTaskReset;

public:
	// 任务列表
	UPROPERTY(BlueprintReadOnly)
	FDreamTaskSpecHandleContainer TaskData;

	// 定时器间隔
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimerDeltaTime = 0.01f;

public:
	/**
	 * 给予任务 (Class)
	 * @param InClass 要给予的任务类
	 * @param InPayload 任务的载荷
	 * @return 给予的任务
	 */
	UFUNCTION(BlueprintCallable, Category = Functions, meta=(DeterminesOutputType = InClass))
	FDreamTaskSpecHandle GiveTaskByClass(TSubclassOf<UDreamTask> InClass, UObject* InPayload = nullptr);

	/**
	 * 初始化任务列表
	 * @param NewList 新的任务列表
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void InitializeTaskList(FDreamTaskSpecHandleContainer NewList);

	/**
	 * 任务类是否在列表内
	 * @param InCheckTaskClass 要检测的任务类别
	 * @return 任务列表内是否有这个类
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	bool HasTaskByClass(TSubclassOf<UDreamTask> InCheckTaskClass);

	/**
	 * 任务名称是否在列表内
	 * @param InCheckTaskName 要检测的任务名称
	 * @return 任务列表内是否有这个任务名称
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	bool HasTaskByName(FName InCheckTaskName);

	/**
	 * 移除任务 (Class)
	 * @param InRemoveTaskClass 要移除的任务名称
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	bool RemoveTaskByClass(TSubclassOf<UDreamTask> InRemoveTaskClass);

	/**
	 * 移除任务 (Name)
	 * @param InRemoveTaskName 要移除的任务名称
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	bool RemoveTaskByName(FName InRemoveTaskName);
	
	/**
	 * 清空任务列表
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void ClearTasks();

	/**
	 * 更新任务 (Name) (ConditionNames)
	 * @param TaskName 要更新的任务名称
	 * @param InConditionNames 要更新的任务条件名称
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void UpdateTask(FName TaskName, const TArray<FName>& InConditionNames);

	/**
	 * 获取任务 (Class) (ConditionNames)
	 * @param InClass 要更新的任务类
	 * @param InConditionNames 要更新的任务条件名称
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void UpdateTaskByClass(TSubclassOf<UDreamTask> InClass, const TArray<FName>& InConditionNames);

	/**
	 * 获取任务列表
	 * @return 任务列表
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	TArray<FDreamTaskSpecHandle>& GetTaskList() { return TaskData.GetHandles(); }

	/**
	 * 获取任务 (Class)
	 * @param InTaskClass 要获取的任务类
	 * @return 获取到的任务
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	const FDreamTaskSpecHandle& GetTaskByClass(TSubclassOf<UDreamTask> InTaskClass);

	/**
	 * 获取任务 (Name)
	 * @param InTaskName 要获取的任务名称
	 * @return 获取到的任务
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	const FDreamTaskSpecHandle& GetTaskByName(FName InTaskName);

	/**
	 * 重置任务 (Name)
	 * @param InName 要重置的任务名称
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void ResetTaskByName(FName InName);

	/**
	 * 重置任务 (Class)
	 * @param InClass 要重置的任务类
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void ResetTaskByClass(TSubclassOf<UDreamTask> InClass);

	/**
	 * 重置任务 (Task)
	 * @param InTask 要重置的任务
	 */
	UFUNCTION(BlueprintCallable, Category = Functions)
	void ResetTask(UDreamTask* InTask);

	/**
	 * 获取任务列表数据
	 * @return 任务列表数据
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	FDreamTaskSpecHandleContainer& GetTaskData() { return TaskData; }

	/**
	 * 获取任务列表数据 (Array Task)
	 * @return 获取到的任务列表数据
	 */
	UFUNCTION(BlueprintPure, Category = Functions)
	TArray<UDreamTask*> GetTaskArray();

public:
	void ActiveTimer();
	void StopTimer();

	void DelegateCall_TaskListChanged(const FDreamTaskSpecHandleContainer& InTaskData);
	void DelegateCall_TaskUpdate(const FDreamTaskSpecHandle& InTask);
	void DelegateCall_TaskUpdate(UDreamTask* InTask);
	void DelegateCall_TaskRemoved(const FDreamTaskSpecHandle& InTask);
	void DelegateCall_TaskRemoved(UDreamTask* InTask);	
	void DelegateCall_TaskReset(const FDreamTaskSpecHandle& InTask);
	void DelegateCall_TaskReset(UDreamTask* InTask);

private:
	FTimerHandle TimerHandle;
	bool bTimerActive = false;
	void Updater();

public:
	template <typename T>
	T* NewTask(TSubclassOf<T> Class, UObject* Payload = nullptr)
	{
		if (Class->IsChildOf(UDreamTask::StaticClass()))
		{
			UDreamTask* Task = NewObject<UDreamTask>(this, Class);
			Task->InitializeTask(this, Payload);
			return Task;
		}
		else
		{
			return nullptr;
		}
	}
};

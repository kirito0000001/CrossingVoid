// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "DreamTaskAsyncListener.generated.h"

struct FDreamTaskSpecHandle;
enum class EDreamTaskState : uint8;
class UDreamTask;
class UDreamTaskComponent;
/**
 * 
 */
UCLASS()
class DREAMGAMEPLAYTASK_API UDreamTaskAsyncListener : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPinResult, UDreamTask*, Task);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPinUpdateResult, UDreamTask*, Task, EDreamTaskState, State);

public:
	/**
	 * 蓝图可分配的事件委托，用于任务初始化完成时触发
	 * 该委托在任务初始化阶段完成后被调用，可用于执行初始化相关的后续操作
	 */
	UPROPERTY(BlueprintAssignable)
	FPinResult OnTaskInitialize;

	/**
	 * 蓝图可分配的事件委托，用于任务更新时触发
	 * 该委托在任务每次更新时被调用，可用于处理任务运行时的逻辑更新
	 */
	UPROPERTY(BlueprintAssignable)
	FPinResult OnTaskUpdate;

	/**
	 * 蓝图可分配的事件委托，用于任务条件更新时触发
	 * 该委托在任务的条件发生变化时被调用，可用于处理条件变更的逻辑
	 */
	UPROPERTY(BlueprintAssignable)
	FPinResult OnTaskConditionUpdate;

	/**
	 * 蓝图可分配的事件委托，用于任务完成时触发
	 * 该委托在任务完成时被调用，可用于执行任务完成后的清理或回调操作
	 */
	UPROPERTY(BlueprintAssignable)
	FPinResult OnTaskCompleted;

public:
	/**
	 * 创建任务监听器并分配新任务
	 * 
	 * @param WorldContextObject 提供世界上下文的对象，用于安全的定时器管理和世界状态获取
	 * @param InTaskComponent 负责管理和执行任务的组件
	 * @param InTaskClass 要创建的任务类
	 * @return 返回配置好的任务监听器实例
	 * 
	 * @note  会自动绑定到任务的生命周期事件（初始化/更新/状态变更/完成等）
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Async", Meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UDreamTaskAsyncListener* CreateListenerAndGiveTask(UObject* WorldContextObject, UDreamTaskComponent* InTaskComponent, TSubclassOf<UDreamTask> InTaskClass);

	/**
	 * 为现有任务创建监听器
	 * 
	 * @param WorldContextObject 提供世界上下文的对象
	 * @param InTask 要监听的已存在任务实例
	 * @return 返回配置好的任务监听器实例
	 * 
	 * @note 适用于监控已运行中的任务，自动绑定任务的所有事件委托
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Async", Meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UDreamTaskAsyncListener* CreateListener(UObject* WorldContextObject, UDreamTask* InTask);

	/**
	 * 为现有任务句柄创建任务监听器
	 * 
	 * @param WorldContextObject 提供世界上下文的对象
	 * @param SpecHandle 任务句柄
	 * @return 配置好的任务监听器实例
	 * 
	 * @note 适用于从任务句柄中获取任务实例，并自动绑定任务事件委托
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamGameplayTaskFunctions|Async", Meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UDreamTaskAsyncListener* CreateListenerWithHandle(UObject* WorldContextObject, const FDreamTaskSpecHandle& SpecHandle);

protected:
	virtual void Activate() override;

	UFUNCTION()
	void HandleInitialize(UDreamTask* InTask);

	UFUNCTION()
	void HandleUpdate(UDreamTask* InTask);

	UFUNCTION()
	void HandleCompleted(UDreamTask* InTask);

	UFUNCTION()
	void HandleConditionUpdate(UDreamTask* InTask);

	UPROPERTY()
	TObjectPtr<UDreamTask> Task;

	UPROPERTY()
	TObjectPtr<UObject> WorldContext;
};

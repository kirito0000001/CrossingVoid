// Copyright 2022 - 2024 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DreamTaskInterface.generated.h"

class UDreamTask;
class UDreamTaskComponent;

UINTERFACE()
class UDreamTaskInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Dream Task Interface
 */
class DREAMGAMEPLAYTASK_API IDreamTaskInterface
{
	GENERATED_BODY()

public:
	/**
	 * 任务初始化
	 * @param Task 当前初始化的任务
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DreamTaskInterface|RelatedActor")
	void TaskInitialize(UDreamTask* Task);
		virtual void TaskInitialize_Implementation(UDreamTask* Task);

	/**
	 * 任务更新
	 * @param Task 当前更新的任务
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DreamTaskInterface|RelatedActor")
	void TaskUpdate(UDreamTask* Task);
		void TaskUpdate_Implementation(UDreamTask* Task);

	/**
	 * 任务完成
	 * @param Task 完成的任务
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DreamTaskInterface|RelatedActor")
	void TaskCompleted(UDreamTask* Task);
		void TaskCompleted_Implementation(UDreamTask* Task);
};

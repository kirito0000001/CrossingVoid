// Copyright 2022 - 2024 Dream Moon Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DreamTaskConditionTemplate.generated.h"

class UDreamTask;
/**
 * Dream Task Condition Template
 */
UCLASS(Blueprintable, Abstract, EditInlineNew)
class DREAMGAMEPLAYTASK_API UDreamTaskConditionTemplate : public UObject
{
	GENERATED_BODY()

public:
	UDreamTaskConditionTemplate(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTaskConditionNumDelegate, int, Num);

public:
	// 条件显示名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data)
	FText ConditionDisplayName = FText::FromString(TEXT("New Condition"));

	// 条件描述
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data)
	FText ConditionDesc = FText::FromString(TEXT("This is a new condition."));

	// 条件数量
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data)
	int32 Count = 0;

	// 完成数量
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data)
	int32 CompletedCount = 1;

	// 必完成
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data)
	bool bTaskMustBeCompleted = false;

	// 条件拥有者
	UPROPERTY(BlueprintReadOnly, Category = Data)
	UDreamTask* OwnerTask = nullptr;

public:
	// 条件完成实现事件
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, DisplayName = "CompletedCondition", Category = Functions)
	bool BP_CompletedCondition();
	bool BP_CompletedCondition_Implementation();

public:
	// 是否完成
	UFUNCTION(BlueprintPure, Category = Functions)
	bool IsCompleted() const;

	UFUNCTION(BlueprintPure, Category = Functions)
	bool MustTaskBeCompleted() const { return bTaskMustBeCompleted; }

	// 获取数量
	UFUNCTION(BlueprintPure, Category = Functions)
	int32 GetCount() const { return Count; }

	// 获取完成数量
	UFUNCTION(BlueprintPure, Category = Functions)
	int32 GetCompletedCount() const { return CompletedCount; }

	// 设置数量
	UFUNCTION(BlueprintCallable, Category = Functions)
	void SetCount(int32 InValue);

	// 设置完成数量
	UFUNCTION(BlueprintCallable, Category = Functions)
	void SetCompletedCount(int32 InValue);

	// 更新条件
	UFUNCTION(BlueprintCallable, Category = Functions)
	void Update();

	// 重置
	UFUNCTION(BlueprintCallable, Category = Functions)
	void Reset();

	UFUNCTION(BlueprintCallable, Category = Functions)
	void InitializeCondition(UDreamTask* InTask);
public:
	// 条件更新事件
	UPROPERTY(BlueprintAssignable, Category = Delegates)
	FTaskConditionNumDelegate OnConditionUpdate;
};

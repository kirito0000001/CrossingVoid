#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ForEachLoopDelay.generated.h"

/**
 * 打字机附赠的ForEachLoopDelay
 */
UCLASS(BlueprintType,meta = (ExposedAsyncProxy = AsyncAction))//公开自己
class TEXTTYPER_API UForEachLoopDelay : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	//创建回调
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoopDelegate, int, Index);
	//初始化和解构
	UForEachLoopDelay(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~UForEachLoopDelay();

	/**
	 * For Loop 延迟
	 * @param InStart 开始的索引
	 * @param InEnd 结束的索引
	 * @param InDuration 每次的延迟时间
	 * @param bInForce 略过第一次延迟
	 */
	UFUNCTION(BlueprintCallable,  Category = "Textyper",Meta = (WorldContext="WorldContextObject",BlueprintInternalUseOnly = true, DisplayName = "Dream Async For Loop Delay"))
	static UForEachLoopDelay* Async_ForLoopDelay(UObject* WorldContextObject,int InStart = 0, int InEnd = 1, float InDuration = 0.1f,
												bool bInForce = false);
	//终止循环
	UFUNCTION(BlueprintCallable, Category = "Textyper")
	void Break();

	//每一次循环触发
	UPROPERTY(BlueprintAssignable)
	FOnLoopDelegate OnLoop;
	//完成时
	UPROPERTY(BlueprintAssignable)
	FOnLoopDelegate OnFinish;

protected: //私有变量

	virtual void Activate() override;
	//激活函数，重写父类函数
	void LoopInternal();
	//循环函数，内部调用
	FTimerHandle Handle; //定时器句柄
	int Start = 0; //初始化输入参数
	int End = 0;
	float Duration = 0.f;
	bool bForce = false;
	TWeakObjectPtr<UObject> WorldContextObject;
};

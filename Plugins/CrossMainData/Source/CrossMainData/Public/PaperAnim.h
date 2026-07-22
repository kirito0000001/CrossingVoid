#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Components/Image.h"
#include "PaperAnim.generated.h"

UENUM(BlueprintType)
enum EStopMode : uint8
{
	CantSee = 0 UMETA(DisplayName = "不可见", ToolTip="不可见，但是占用布局"),
	NoAction = 1 UMETA(DisplayName = "无行为", ToolTip="无行为"),
	Hide = 2 UMETA(DisplayName = "已折叠", ToolTip="已折叠，不占用布局"),
};

/**
 * PaperAnim，UI上序列图播放
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class CROSSMAINDATA_API UPaperAnim : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	//制作委托回调，当每一次循环的时候触发
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoopDelegate, int, IndexNow);

	//构造函数和析构函数
	UPaperAnim(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~UPaperAnim();

	/**
	 * 播放序列图
	 * 当前函数只能在载体所在的蓝图调用，别的地方搜不到
	 * @param TexMaps 贴图数组
	 * @param Fps 帧数
	 * @param bLoop 是否循环播放
	 */
	UFUNCTION(BlueprintCallable,
		Meta = (WorldContext="WorldContextObject", BlueprintInternalUseOnly = true, DisplayName = "PaperAnim序列图播放"))
	static UPaperAnim* PapersPlay(UObject* WorldContextObject, UImage* Target, EStopMode EndType,
	                              TArray<TSoftObjectPtr<UTexture2D>> TexMaps, int Fps = 24, bool bLoop = false);

	/**
	 * 序列图停止
	 * @param bCanle 
	 */
	UFUNCTION(BlueprintCallable, Meta = ( DisplayName = "PaperAnim停止"))
	void PapersStop(bool bCanle = true);


	//每次循环触发
	UPROPERTY(BlueprintAssignable)
	FOnLoopDelegate OnLoop;
	//非循环下播放完时触发
	UPROPERTY(BlueprintAssignable)
	FOnLoopDelegate OnComplete;
	//使用取消函数时触发
	UPROPERTY(BlueprintAssignable)
	FOnLoopDelegate OnCanle;

protected: //局部变量，不可访问
	FTimerHandle LoopHandle; //定时器句柄
	TWeakObjectPtr<UImage> TargetLocal = nullptr;//图像目标
	TArray<TSoftObjectPtr<UTexture2D>> TexMapsLocal = {}; //贴图数组
	float FpsLocal = 0.04f; //帧数
	bool bLoopLocal = false; //是否循环播放
	int IndexLocalNow = 0; //当前播放的索引
	EStopMode EndTypeLocal = EStopMode::CantSee; //结束方式
	UWorld* WorldV;

	virtual void Activate() override; //当前节点激活时会触发
	void LoopInternal(); //当每次循环时触发
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
};

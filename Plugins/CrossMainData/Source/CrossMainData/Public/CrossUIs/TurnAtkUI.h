#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnAtkUI.generated.h"


UCLASS()
class CROSSMAINDATA_API UTurnAtkUI : public UUserWidget
{
	GENERATED_BODY()

protected: //初始化绑定和解绑UI
	virtual void NativeConstruct() override; //UMG的构造函数格式（用于绑定按钮和各种UI事件）
	virtual void NativeDestruct() override; //UMG的析构函数格式（用于清理UI事件，必须写解绑）
public: //控件绑定到变量
	/*UPROPERTY(Meta = (BindWidget))
	class UButton* Button111;*/
	UPROPERTY(Meta = (BindWidget))
	class UTextBlock* Round_text;

public: //变量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="当前回合数")
	int RoundIN = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="当前行动角色")
	ACrossvoid2DatkNew* NowA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn), DisplayName="角色合集")
	TArray<ACrossvoid2DatkNew*> MapsIN;

	 UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="自动模式开关")
	 bool AutoOpen;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="闪忆自动模式")
	 bool MemoryAuto;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="关卡字符串")
	FString LevelStrID;

	
public: //函数
	UFUNCTION(BlueprintCallable, DisplayName="设置当前回合数")
	void SetRound(int Round);

	UFUNCTION(BlueprintCallable)
	void Onclick();

	UFUNCTION(BlueprintCallable, DisplayName="切换鼠标输入")
	void SwitchMouseInput(bool On);

public: //自定义事件
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePadRealizeSubsystem.generated.h"

class UWidget;
UCLASS()
class GAMPADLIGHT_API UGamePadRealizeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public://变量
	
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="模拟鼠标控制器")
	APlayerController* NavPC = nullptr;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="可以聚焦的控件")
	TArray<UWidget*> NavCanFocus;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="X位置控件排序")
	TArray<UWidget*> NavLocationX;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="Y位置控件排序")
	TArray<UWidget*> NavLocationY;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="控件XY当前位置")
	FVector2D XYnow;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="模拟鼠标中")
	bool PluginsMouseON = false;
	UPROPERTY()
	bool PluginsAxisON = false;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="摇杆按下")
	bool Presson = false;
	UPROPERTY(BlueprintReadWrite,Category="手柄|内部",DisplayName="当前聚焦控件")
	UWidget* NavFocusNow = nullptr;

public:
	
	/** 切换手柄操控模式
	 * [直接控制/模拟鼠标]
	 * @param MouseON 是否开启模拟鼠标
	 */ 
	UFUNCTION(BlueprintCallable, Category = "手柄",DisplayName="切换手柄模式")
	void SwitchAnalogCursor(bool MouseON);

	/** 手柄模拟鼠标位置
	 * [根据手柄输入来设置鼠标位置]
	 * @param PlayerController 玩家控制器
	 * @param PadInput 手柄输入
	 * @param Speed 基础移动速度
	 * @param CanUse 移动是否开启
	*/
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "手柄模拟鼠标位置")
	void GamePadSetMouseLocation(APlayerController* PlayerController,FVector PadInput,float Speed,bool& CanUse);

	/** 默认左摇杆按下加速移动
	 * [可以直接选取“摇杆按下”变量进行设置]
	 * @param Press 任意键按下
	 */
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "手柄移动加速")
	void GamePadLeftThumbstickClick(bool Press);

	/** UI导航系统初始化
	 * [控件构造/切换时必用]
	 * @param PC 玩家控制器
	 * @param CanFocus 可以聚焦的控件合集
	 */
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "UI导航系统初始化_自动")
	void NewNavigationInitialize(APlayerController* PC,TArray<UWidget*> CanFocus);

	/** UI导航系统初始化
	 * [控件构造/切换时必用]
	 * @param PC 玩家控制器
	 * @param FocusX 横向可以聚焦的控件顺序（左到右）
	 * @param FocusY 竖向可以聚焦的控件顺序（上到下）
	 */
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "UI导航系统初始化_手动")
	void NewNavigationInitializeHM(APlayerController* PC,TArray<UWidget*> FocusX,TArray<UWidget*> FocusY);

	/** 指定新导航[聚焦控件]
	 * [根据整数导航至新控件]
	 * @param X 横向坐标
	 * @param Y 竖向坐标
	 * @param AddMode 添加模式/设置模式
	 * @param Success 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "指定新导航_单整数")
	void AssignNewNavigationInt(int X,int Y,bool AddMode,bool& Success);

	/** 指定新导航[聚焦控件]
	 * [根据手柄摇杆输入导航至新控件]
	 * @param Axis 摇杆输入
	 * @param Success 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "指定新导航_摇杆")
	void AssignNewNavigationRocker(FVector Axis,bool& Success);

	/** 清空手柄导航*/
	UFUNCTION(BlueprintCallable, Category = "手柄", DisplayName = "清空手柄导航")
	void NavigationClear();
};

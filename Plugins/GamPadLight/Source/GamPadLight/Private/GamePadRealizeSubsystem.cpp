// Fill out your copyright notice in the Description page of Project Settings.

#include "GamePadRealizeSubsystem.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Framework/Application/NavigationConfig.h"
#include "Components/Widget.h"



void UGamePadRealizeSubsystem::SwitchAnalogCursor(bool MouseON)
{
	TSharedRef<FNavigationConfig> navigation_config = MakeShared<FNavigationConfig>();
	navigation_config->bTabNavigation = false;
	navigation_config->bKeyNavigation = false;
	navigation_config->bAnalogNavigation = false;
	FSlateApplication::Get().SetNavigationConfig(navigation_config);
	PluginsMouseON = MouseON;
}

void UGamePadRealizeSubsystem::GamePadSetMouseLocation(APlayerController* PlayerController, FVector PadInput,
	float Speed, bool& CanUse)
{
	if (PluginsMouseON == true)
	{
		//判断X输入是否有效
		bool XAxisIsvalid = FMath::Abs(PadInput.X) >= 0.4f;
		if (XAxisIsvalid)
		{
			float SpeedIN = (Presson)? Speed * 1.6 : Speed;
			float SpeedLast =FMath::RoundToInt((PadInput.X > 0.0f) ? SpeedIN : -SpeedIN); //根据输入值判断方向
			//拿出鼠标位置
			float VarX;
			float VarY; 
			PlayerController->GetMousePosition(VarX,VarY);
			//设置鼠标位置
			PlayerController->SetMouseLocation(VarX + SpeedLast, VarY);
		}
		//判断Y输入是否有效
		bool YAxisIsvalid = FMath::Abs(PadInput.Y) >= 0.4f;
		if (YAxisIsvalid)
		{
			float SpeedIN = (Presson)? Speed * 1.6 : Speed;
			float SpeedLast =FMath::RoundToInt((PadInput.Y < 0.0f) ? SpeedIN : -SpeedIN);//根据输入值判断方向
			//拿出鼠标位置
			float VarX;
			float VarY; 
			PlayerController->GetMousePosition(VarX,VarY);
			//设置鼠标位置
			PlayerController->SetMouseLocation(VarX, VarY+SpeedLast);
		}
	}
	else
	{
		CanUse = false;
	}
}

void UGamePadRealizeSubsystem::GamePadLeftThumbstickClick(bool Press)
{
	Presson = Press;
}

void UGamePadRealizeSubsystem::NewNavigationInitialize(APlayerController* PC, TArray<UWidget*> CanFocus)
{
	NavigationClear();//清空导航合集
	//把变量初始
	NavPC = PC;
	NavCanFocus = CanFocus;
	//创建局部变量，用于判断XY的最终结果
	int Xjudge = -1;
	int Yjudge = -1;
	FVector2D For1Vector(0.0f,0.0f);
	FVector2D ForXVector(0.0f,0.0f);
	FVector2D ForYVector(0.0f,0.0f);
	FVector2D UselessVar;
	
	for (const auto& For1 : NavCanFocus)
	{
		if (NavLocationX.IsEmpty())
		{
			NavLocationX.AddUnique(For1);
			NavLocationY.AddUnique(For1);
		}//如果是空合集就先设置一个
		else
		{
			//每次loop初始化判断值
			Xjudge = -1;
			Yjudge = -1;
			USlateBlueprintLibrary::LocalToViewport(GWorld,For1->GetCachedGeometry(),FVector2D(0,0),For1Vector,UselessVar);
			//循环判断当前UI在X轴合集的什么地方
			for (const auto& ForX : NavLocationX)
			{
				USlateBlueprintLibrary::LocalToViewport(GWorld,ForX->GetCachedGeometry(),FVector2D(0,0),ForXVector,UselessVar);

				if (For1Vector.X < ForXVector.X)
				{
					Xjudge = NavLocationX.Find(ForX);
					break;
				}
			}
			if (Xjudge >= 0)
			{
				NavLocationX.Insert(For1,Xjudge);
			}//如果有就插入到X轴合集的指定位置
			else
			{
				NavLocationX.AddUnique(For1);
			}//如果没有就插入到X轴合集的末尾
			
			//判断Y的位置
			for (const auto& ForY : NavLocationY)
			{
				USlateBlueprintLibrary::LocalToViewport(GWorld,ForY->GetCachedGeometry(),FVector2D(0,0),ForYVector,UselessVar);
				if (For1Vector.Y < ForYVector.Y)
				{
					Yjudge = NavLocationY.Find(ForY);
					break;
				}
			}
			if (Yjudge >= 0)
			{
				NavLocationY.Insert(For1,Yjudge);
			}
			else
			{
				NavLocationY.AddUnique(For1);
			}
		}
	}
}

void UGamePadRealizeSubsystem::NewNavigationInitializeHM(APlayerController* PC, TArray<UWidget*> FocusX,
	TArray<UWidget*> FocusY)
{
	NavigationClear();//清空导航合集
	NavLocationX = FocusX;
	NavLocationY = FocusY;
}


void UGamePadRealizeSubsystem::AssignNewNavigationInt(int X, int Y, bool AddMode, bool& Success)
{
	if (AddMode)//走添加流程
		{
			if (X != 0)//X轴输入有效
			{
				if (NavLocationX.IsValidIndex(X+XYnow.X))//当前指定位置有效
				{
					NavFocusNow = NavLocationX[X+XYnow.X];//记录当前有效控件
					NavFocusNow->SetFocus();//设置聚焦
					XYnow = FVector2D(X+XYnow.X,NavLocationY.Find(NavLocationX[X+XYnow.X]));//记录当前有效控件位置
					Success = true;
				}
				else
				{
					Success = false;
				}
			
			}
			else if (Y != 0)
			{
				if (NavLocationY.IsValidIndex(Y+XYnow.Y))
				{
					NavFocusNow = NavLocationY[Y+XYnow.Y];//记录当前有效控件
					NavFocusNow->SetFocus();//设置聚焦
					XYnow = FVector2D(NavLocationX.Find(NavLocationY[Y+XYnow.Y]),Y+XYnow.Y);
					Success = true;
				}
				else
				{
					Success = false;
				}
			}//Y轴输入有效
		}
	else
	{
		if (X >= 0)
		{
			if(NavLocationX.IsValidIndex(X))
			{
				NavFocusNow = NavLocationX[X];//记录当前有效控件
				NavFocusNow->SetFocus();//设置聚焦
				XYnow = FVector2D(X,NavLocationY.Find(NavLocationX[X]));
				Success = true;
			}
			else
			{
				Success = false;
			}
		}
		else
		{
			if (Y >= 0)
			{
				if(NavLocationY.IsValidIndex(Y))
				{
					NavFocusNow = NavLocationY[Y];//记录当前有效控件
					NavFocusNow->SetFocus();//设置聚焦
					XYnow = FVector2D(NavLocationX.Find(NavLocationY[Y]),Y);
					Success = true;
				}
				else
				{
					Success = false;
				}
			}
			else
			{
				Success = false;
			}
		}
	}//走直接设置流程
	
}

void UGamePadRealizeSubsystem::AssignNewNavigationRocker(FVector Axis,bool& Success)
{
	if (PluginsMouseON)
	{
		Success = false;
	}
	else
	{
		float Xvalid = FMath::Abs(Axis.X);
		float Yvalid = FMath::Abs(Axis.Y);
		if (Xvalid >= 0.8f)
		{
			if (PluginsAxisON)
			{
				Success = false;
			}
			else
			{
				AssignNewNavigationInt((Axis.X < 0)?-1 : 1,0,true,Success);
				PluginsAxisON = true;
				Success = true;
			}
		}
		else if(Yvalid >= 0.8f)
		{
			if (PluginsAxisON)
			{
				Success = false;
			}
			else
			{
				AssignNewNavigationInt(0,(Axis.Y > 0)? -1 : 1,true,Success);
				PluginsAxisON = true;
				Success = true;
			}
		}
		else
		{
			PluginsAxisON = false;
			Success = false;
		}
	}
}

void UGamePadRealizeSubsystem::NavigationClear()
{
	NavPC = nullptr;
	NavFocusNow = nullptr;
	NavCanFocus = {};
	NavLocationX = {};
	NavLocationY = {};
	XYnow = FVector2D(0.0f,0.0f);
	
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}
#include "CrossUIs/TurnAtkUI.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"


void UTurnAtkUI::NativeConstruct()
{
	Super::NativeConstruct();
	//开始绑定
	//Button111->OnClicked.AddDynamic(this, &UTurnAtkUI::Onclick);
}

void UTurnAtkUI::NativeDestruct()
{
	Super::NativeDestruct();
	//销毁绑定，必须做
	//Button111->OnClicked.RemoveDynamic(this, &UTurnAtkUI::Onclick);
}


void UTurnAtkUI::SetRound(int Round)
{
	RoundIN = Round;//设置变量
	Round_text->SetText(FText::FromString(FString::FromInt(RoundIN)));
}

void UTurnAtkUI::Onclick()
{
	
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Hello, Screen!"));
}

void UTurnAtkUI::SwitchMouseInput(bool On)
{
	if (On)
	{
		//设置输入模式游戏和UI
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(GetOwningPlayer(), this, EMouseLockMode::DoNotLock,false,false);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();//将聚焦设置到游戏窗口
		SetUserFocus(GetOwningPlayer());//设置用户聚焦
		SetKeyboardFocus();//设置键盘聚焦
		GetOwningPlayer()->bShowMouseCursor = true;//显示鼠标
	}
	else
	{
		if (AutoOpen == false)
		{
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(GetOwningPlayer(), false);
			GetOwningPlayer()->bShowMouseCursor = false;//显示鼠标
		}
	}
}




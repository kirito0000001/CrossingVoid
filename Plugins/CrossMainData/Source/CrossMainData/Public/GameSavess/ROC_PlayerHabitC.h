#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CrossMainData/Public/FUCRealize.h"
#include "ROC_PlayerHabitC.generated.h"


/**
 * 
 */
UCLASS()
class CROSSMAINDATA_API UROC_PlayerHabitC : public USaveGame
{
	GENERATED_BODY()
public://变量
	//自定义对话颜色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="自定义对话颜色")
	FSlateColor CustomColor = {};
	//2D入场自动战斗模式
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="2D入场自动战斗模式")
	int AtkAutoMode2D = 0;
	//2D上次队伍选择
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="2D上次队伍选择")
	int AtkTeamList2D = 1;
	//2D战斗速度
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="2D战斗速度")
	float AtkSpeedMode2D = 1.0f;
	//设置类
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="设置类")
	FVoidSetting Settings = {};
	//开启UI特效（交互动效）
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="开启UI动效")
	bool UIEffect = false;
	//开启控件特效（序列帧）
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="开启控件序列帧动画")
	bool WidgetEffect = false;
	//UI样式索引
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="UI样式索引")
	int UIStyleIndex = 0;
	//玩家剧情已读
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="玩家剧情已读")
	TMap<FString,bool> StoryHasRead = {};
	//好友备注
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友备注")
	TMap<FGuid,FName> FriendRemark = {};
	//上一次私聊对象
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="上次私聊对象")
	TMap<FGuid,FGuid> LastPrivateChatFriend = {};
};

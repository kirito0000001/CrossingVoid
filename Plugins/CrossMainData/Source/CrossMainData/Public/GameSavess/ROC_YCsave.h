#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CrossVoidInventory/Public/InventoryBaseItem.h"
#include "CrossMainData/Public/FUCRealize.h"
#include "ROC_YCsave.generated.h"

/**
 * 养成中的暂存数据
 */
UCLASS()
class CROSSMAINDATA_API UROC_YCsave : public USaveGame
{
	GENERATED_BODY()
public://变量
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|角色", DisplayName="角色道具")
	TSoftClassPtr<UInventoryBaseItem> CharReady = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值",DisplayName="纯提升属性")
	F2DAtkDataStruct CharData = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值",DisplayName="助战SUB角色数值")
	F2DAtkDataStruct CharSub = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值", DisplayName="额外属性加成")
	TMap<FString,int> CharExtra = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值", DisplayName="技能等级")
	TArray<int> CharSkillLevel = {1,1,1,1};
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值", DisplayName="每个技能的加点情况")
	TArray<int> CharSkup = {0,0,0,0};
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值", DisplayName="剩余的技能点")
	int CharSkillPoint = 10;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|数值", DisplayName="提取的迷子")
	int MiziGet = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="当前Plot")
	FString PlotName = "";
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="锚定的属性")
	int AbNow = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="角色Rank")
	int CharRank = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="角色行动值")
	int CharAction = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="当前回合数")
	int RoundNow = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="总回合数")
	int RoundMax = 91;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="养成是否结束")
	bool IsEnd = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category="养成中|系统", DisplayName="养成成功")
	bool IsSuccess = false;
};

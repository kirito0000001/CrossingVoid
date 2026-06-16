#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrossMainData/Public/FUCRealize.h"
#include "SubTS.generated.h"

/**
 * 游戏开启后，用于暂存数据的地方
 * 与实例存的不同，这里的数据是可信的，会用于后续逻辑判断，实例的数据是临时的，不可信
 */
UCLASS()
class CROSSMAINDATA_API USubTS : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public://变量

	//角色属性数据，用于养成进入Gal/战斗界面时保存数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="角色属性数据")
	F2DAtkDataStruct CharDataTS = {};
};

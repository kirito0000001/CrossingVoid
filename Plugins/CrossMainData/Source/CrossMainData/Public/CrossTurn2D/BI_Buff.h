#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BI_Buff.generated.h"


UINTERFACE()
class UBI_Buff : public UInterface
{
	GENERATED_BODY()
};

class CROSSMAINDATA_API IBI_Buff
{
	GENERATED_BODY()
public://函数
	/**可以在C++或蓝图中实现的React To Trigger版本。*/
	//属性类BUFF
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Property_Get(float& PhyAdd, float& MagAdd, float& PhyMulti, float& MagMulti);

	//最终伤害BUFF
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void LastGet(float& lastAdd,  float& lastMulti);
};

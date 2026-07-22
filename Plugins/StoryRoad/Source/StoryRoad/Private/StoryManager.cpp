#include "StoryManager.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

UStoryManager::UStoryManager(): CurrentStory(nullptr), bCustomJudjeResult(false)
{
	PrimaryComponentTick.bCanEverTick = false;
	OnStoryChange.Clear(); //清空委托
	OnStoryDefeat.Clear();
	OnStoryFinish.Clear();
}


EStoryPropertyType UStoryManager::GetOwnerPropertyTypeByName(FName FieldName)
{
	if (!GetOwner())//你到底把组件放到谁身上了，怎么会没有
	return EStoryPropertyType::Unknown;
	
	FProperty* Property = GetOwner()->GetClass()->FindPropertyByName(FieldName);
	if (FieldName == "None")
	{
		OnStoryDefeat.Broadcast(TEXT("变量未设置"));
		return EStoryPropertyType::Unknown;
	}
	if (!Property)
	{
		OnStoryDefeat.Broadcast(TEXT("找不到需要的变量，请确认是否创建，以及名称一致"));
		return EStoryPropertyType::Unknown;
	}
	if (Property->IsA(FIntProperty::StaticClass()))
		return EStoryPropertyType::Int;
	if (Property->IsA(FFloatProperty::StaticClass()))
		return EStoryPropertyType::Float;
	if (Property->IsA(FBoolProperty::StaticClass()))
		return EStoryPropertyType::Bool;
	if (Property->IsA(FStrProperty::StaticClass()))
		return EStoryPropertyType::String;
	if (Property->IsA(FNameProperty::StaticClass()))
		return EStoryPropertyType::Name;

	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		if (ObjProp->PropertyClass->IsChildOf(UObject::StaticClass()))
			return EStoryPropertyType::CObject;
		return EStoryPropertyType::Unknown;
	}
	return EStoryPropertyType::Unknown;
}

int32 UStoryManager::GetIntPropertyByName(FName FieldName, bool& bSuccess)
{
	bSuccess = false;
	if (!GetOwner()) return 0;

	if (FIntProperty* Prop = CastField<FIntProperty>(GetOwner()->GetClass()->FindPropertyByName(FieldName)))
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(GetOwner());
		bSuccess = true;
		return Prop->GetSignedIntPropertyValue(ValuePtr);
	}
	return 0;
}

float UStoryManager::GetFloatPropertyByName(FName FieldName, bool& bSuccess)
{
	bSuccess = false;
	if (!GetOwner()) return 0.0f;

	if (FFloatProperty* Prop = CastField<FFloatProperty>(GetOwner()->GetClass()->FindPropertyByName(FieldName)))
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(GetOwner());
		bSuccess = true;
		return Prop->GetFloatingPointPropertyValue(ValuePtr);
	}
	return 0.0f;
}

bool UStoryManager::GetBoolPropertyByName(FName FieldName, bool& bSuccess)
{
	bSuccess = false;
	if (!GetOwner()) return false;

	if (FBoolProperty* Prop = CastField<FBoolProperty>(GetOwner()->GetClass()->FindPropertyByName(FieldName)))
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(GetOwner());
		bSuccess = true;
		return Prop->GetPropertyValue(ValuePtr);
	}
	return false;
}

FString UStoryManager::GetStringPropertyByName(FName FieldName, bool& bSuccess)
{
	bSuccess = false;
	if (!GetOwner()) return TEXT("");

	if (FStrProperty* Prop = CastField<FStrProperty>(GetOwner()->GetClass()->FindPropertyByName(FieldName)))
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(GetOwner());
		bSuccess = true;
		return Prop->GetPropertyValue(ValuePtr);
	}
	return TEXT("");
}

FName UStoryManager::GetNamePropertyByName(FName FieldName, bool& bSuccess)
{
	bSuccess = false;
	if (!GetOwner()) return NAME_None;

	if (FNameProperty* Prop = CastField<FNameProperty>(GetOwner()->GetClass()->FindPropertyByName(FieldName)))
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(GetOwner());
		bSuccess = true;
		return Prop->GetPropertyValue(ValuePtr);
	}
	return NAME_None;
}

UObject* UStoryManager::GetObjectPropertyByName(FName FieldName, bool& bSuccess)
{
	bSuccess = false;
	if (!GetOwner()) return nullptr;

	if (FObjectProperty* Prop = CastField<FObjectProperty>(GetOwner()->GetClass()->FindPropertyByName(FieldName)))
	{
		if (Prop->PropertyClass->IsChildOf(UObject::StaticClass()))
		{
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(GetOwner());
			bSuccess = true;
			return Cast<UObject>(Prop->GetObjectPropertyValue(ValuePtr));
		}
	}
	return nullptr;
}


bool UStoryManager::CallFunctionByName(FName FunctionName)
{
	bCustomJudjeResult = false;
	if (!GetOwner())
	{
		OnStoryDefeat.Broadcast(TEXT("自定义函数未找到，请确认函数是否存在，以及名字是否一致"));
		return false;
	}
	UFunction* Func = GetOwner()->FindFunction(FunctionName);
	if (Func)
	{
		GetOwner()->ProcessEvent(Func, nullptr); // 没有参数时传 nullptr
	}
	if (bCustomJudjeResult)
	{
		return true;
	}
	return false;
}


void UStoryManager::StoryInit(UDataTable* Story)
{
	CurrentStory = Story;
	OnStoryChange.Broadcast(); //故事切换委托
}

void UStoryManager::StoryNextChapter()
{
	if (!CurrentStory)
	{
		OnStoryDefeat.Broadcast(TEXT("当前没有故事，请确认是否初始化"));
		return; //当前没有剧情
	}

	TArray<FStoryGoon> InforNow; //当前剧情对应的所有分支暂存

	//判断有没有这个故事，没有就完结撒花（单独的小片段完结在后面）
	if (StoryOnSetMaps.Find(CurrentStory))
	{
		InforNow = StoryOnSetMaps.Find(CurrentStory)->StoryOnSetMaps; //获取一下信息
		if (InforNow.Num() == 0)//如果这个故事没有分支
		{
			OnStoryFinish.Broadcast(); //完结撒花
			return;
		}
	}
	else
	{
		OnStoryFinish.Broadcast(); //完结撒花
		return;
	}

	bool bSuccess = false; //成功与否变量(不用)

	//开始判断所有故事
	for (const auto& SingleStoryInfor : InforNow) //遍历，开始判断故事进行
	{
		int CompleteCondition = 0; //完成的条件数量变量
		//从条件组开始遍历
		for (const auto& SingleJudje : SingleStoryInfor.Judjes)
		{
			//获取当前条件变量的属性类型
			EStoryPropertyType VarType = GetOwnerPropertyTypeByName(SingleJudje.PropName);
			//获取当前条件变量的值（布尔变为字符串）
			FString BoolValue = GetBoolPropertyByName(SingleJudje.PropName, bSuccess) ? TEXT("true") : TEXT("false");
			//从判断条件开始进行分支
			switch (SingleJudje.JudjeType)
			{
			case EJudjeMode::Custom: //自定义判断
				CallFunctionByName(SingleJudje.PropName); //先触发自定义函数
			//然后根据结果判断后续切换
				if (bCustomJudjeResult)
				{
					CompleteCondition++;
				}
				break;
			case EJudjeMode::Unconditional: //无条件放行
				CompleteCondition++;
				break;
			case EJudjeMode::Ban: //禁止通行
				break;
			case EJudjeMode::Equal: //[等于]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) ==
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) == FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					if (BoolValue == SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::String:
					if (GetStringPropertyByName(SingleJudje.PropName, bSuccess) == SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Name:
					if (GetNamePropertyByName(SingleJudje.PropName, bSuccess) == SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::CObject:
					if (GetObjectPropertyByName(SingleJudje.PropName, bSuccess)->GetName() == SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
				}
				break;
			case EJudjeMode::NotEqual: //[不等于]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) !=
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) != FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					if (BoolValue != SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::String:
					if (GetStringPropertyByName(SingleJudje.PropName, bSuccess) != SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Name:
					if (GetNamePropertyByName(SingleJudje.PropName, bSuccess) != SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::CObject:
					if (GetObjectPropertyByName(SingleJudje.PropName, bSuccess)->GetName() != SingleJudje.TargetValue)
					{
						CompleteCondition++;
					}
				}
				break;
			case EJudjeMode::Greater: //[大于]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) >
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) > FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					break;
				case EStoryPropertyType::String:
					break;
				case EStoryPropertyType::Name:
					break;
				case EStoryPropertyType::CObject:
					break;
				}
				break;
			case EJudjeMode::GreaterEqual: //[大于等于]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) >=
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) >= FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					break;
				case EStoryPropertyType::String:
					break;
				case EStoryPropertyType::Name:
					break;
				case EStoryPropertyType::CObject:
					break;
				}
				break;
			case EJudjeMode::Less: //[小于]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) <
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) < FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					break;
				case EStoryPropertyType::String:
					break;
				case EStoryPropertyType::Name:
					break;
				case EStoryPropertyType::CObject:
					break;
				}
				break;
			case EJudjeMode::LessEqual: //[小于等于]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) <=
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) <= FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					break;
				case EStoryPropertyType::String:
					break;
				case EStoryPropertyType::Name:
					break;
				case EStoryPropertyType::CObject:
					break;
				}
				break;
			case EJudjeMode::Contain: //[包含]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) >=
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) >= FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					break;
				case EStoryPropertyType::String:
					if (SingleJudje.TargetValue.Contains(GetStringPropertyByName(SingleJudje.PropName, bSuccess)))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Name:
					if (SingleJudje.TargetValue.Contains(
						GetNamePropertyByName(SingleJudje.PropName, bSuccess).ToString()))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::CObject:
					if (GetObjectPropertyByName(SingleJudje.PropName, bSuccess)->GetName().Contains(
						SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				}
				break;
			case EJudjeMode::NotContain: //[不包含]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) <
						FCString::Atoi(*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) < FCString::Atof(
						*SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					break;
				case EStoryPropertyType::String:
					if (!SingleJudje.TargetValue.Contains(GetStringPropertyByName(SingleJudje.PropName, bSuccess)))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Name:
					if (!SingleJudje.TargetValue.Contains(
						GetNamePropertyByName(SingleJudje.PropName, bSuccess).ToString()))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::CObject:
					if (!GetObjectPropertyByName(SingleJudje.PropName, bSuccess)->GetName().Contains(
						SingleJudje.TargetValue))
					{
						CompleteCondition++;
					}
					break;
				}
				break;
			case EJudjeMode::Valid: //[有效]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) != 0)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) != 0)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					if (GetBoolPropertyByName(SingleJudje.PropName, bSuccess))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::String:
					if (!GetStringPropertyByName(SingleJudje.PropName, bSuccess).IsEmpty())
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Name:
					if (!GetNamePropertyByName(SingleJudje.PropName, bSuccess).IsNone())
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::CObject:
					if (IsValid(GetObjectPropertyByName(SingleJudje.PropName, bSuccess)))
					{
						CompleteCondition++;
					}
					break;
				}
				break;
			case EJudjeMode::Invalid: //[无效]
				switch (VarType) //各种分支
				{
				default: break;
				case EStoryPropertyType::Unknown:
					break;
				case EStoryPropertyType::Int:
					if (GetIntPropertyByName(SingleJudje.PropName, bSuccess) == 0)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Float:
					if (GetFloatPropertyByName(SingleJudje.PropName, bSuccess) == 0)
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Bool:
					if (!GetBoolPropertyByName(SingleJudje.PropName, bSuccess))
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::String:
					if (GetStringPropertyByName(SingleJudje.PropName, bSuccess).IsEmpty())
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::Name:
					if (GetNamePropertyByName(SingleJudje.PropName, bSuccess).IsNone())
					{
						CompleteCondition++;
					}
					break;
				case EStoryPropertyType::CObject:
					if (!IsValid(GetObjectPropertyByName(SingleJudje.PropName, bSuccess)))
					{
						CompleteCondition++;
					}
					break;
				}
				break;
			}
		}

		//先把所有条件判断完，计数，最后再根据这个返回结果
		switch (SingleStoryInfor.ConditionType)
		{
		default: break;
		case EConditionType::All:

			if (CompleteCondition == SingleStoryInfor.Judjes.Num())
			{
				CurrentStory = SingleStoryInfor.TargetStory;
				OnStoryChange.Broadcast();//故事切换
				return;
			}
			break;
		case EConditionType::Any:
			if (CompleteCondition >= 1)
			{
				CurrentStory = SingleStoryInfor.TargetStory;
				OnStoryChange.Broadcast();//故事切换
				return;
			}
			break;
		case EConditionType::Count:
			if (CompleteCondition == SingleStoryInfor.CompleteCount)
			{
				CurrentStory = SingleStoryInfor.TargetStory;
				OnStoryChange.Broadcast();//故事切换
				return;
			}
			break;
		}
	}

	//故事转换失败，所有条件都没通过
	OnStoryDefeat.Broadcast(TEXT("所有故事都没有通过条件，BADEND"));
}

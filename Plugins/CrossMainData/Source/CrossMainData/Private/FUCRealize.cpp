#include "FUCRealize.h"
#include "CrossTurn2D/Crossvoid2DatkNew.h"
#include "Kismet/KismetStringLibrary.h"


EMainUIStartType UFUCRealize::Opitions_MainMap(FString Input, FString& MainStartEx)
{
	FString PerfixFront = "";//前缀
	FString PerfixBack = "";//后缀
	
	//为空正常去主界面
	if (Input.IsEmpty())
	{
		return EMainUIStartType::Default;
	}

	//打开UI界面
	if(UKismetStringLibrary::Split(Input, "UI_", PerfixFront, PerfixBack, ESearchCase::IgnoreCase,
										  ESearchDir::FromStart))
	{
		MainStartEx = PerfixBack + "_C";//返回后缀，找到的UI名字，要打开还要把对应的类放进数组去找
		return EMainUIStartType::UIopen;
	}
	return EMainUIStartType::Default;//默认正常去主界面
}

F2DAtkDataStruct UFUCRealize::GetCharater2DData(UInventoryBaseItem* MainChar, UInventoryBaseItem* SubChar,
                                                TSoftClassPtr<ACrossvoid2DatkNew> SpawnChar,TSoftClassPtr<ACrossvoid2DatkNew> SpawnSub)
{
	if (MainChar == nullptr)
	{
		return F2DAtkDataStruct();
	}
	FString MainNameIN = MainChar->ItemData.Name.ToString();

	FItemInformation SubDataBuffer = FItemInformation(); //初始化助战缓存，这时候都默认为空
	SubDataBuffer.CharData.SkillLevel = {0, 0, 0, 0};//技能等级初始化
	FString SubNameIN = FString();
	int SkillLevelOver = 0;
	if (SubChar) //获取助战数据，如果有助战就覆盖
	{
		SubDataBuffer = SubChar->ItemData;
		SubNameIN = SubDataBuffer.Name.ToString();
		for (auto i : SubChar->ItemData.CharData.SkillLevel)
		{
			SkillLevelOver += i;
		}
	}

	F2DAtkDataStruct ReturnV = F2DAtkDataStruct(); //初始化返回值
	ReturnV.GenMain = SpawnChar; //生成的人物写入
	ReturnV.GenSub = SpawnSub; //生成助战
	ReturnV.Char1 = MainNameIN; //主战名字写入
	ReturnV.Char2 = SubNameIN;
	ReturnV.Speed = MainChar->ItemData.CharData.Speed + SubDataBuffer.CharData.Speed; //速度合并
	ReturnV.Health = MainChar->ItemData.CharData.Health + SubDataBuffer.CharData.Health; //生命合并
	ReturnV.Attack = MainChar->ItemData.CharData.Attack + SubDataBuffer.CharData.Attack; //攻击合并
	ReturnV.SkillLevel = MainChar->ItemData.CharData.SkillLevel; //主站人物等级
	//设置护援技能等级
	ReturnV.SkillLevel.SetNum(5); //先扩容到5个技能5
	ReturnV.SkillLevel[0] = MainChar->ItemData.CharData.SkillLevel[0] < 1?1:MainChar->ItemData.CharData.SkillLevel[0];
	ReturnV.SkillLevel[1] = MainChar->ItemData.CharData.SkillLevel[1] < 1?1:MainChar->ItemData.CharData.SkillLevel[1];
	ReturnV.SkillLevel[2] = MainChar->ItemData.CharData.SkillLevel[2] < 1?1:MainChar->ItemData.CharData.SkillLevel[2];
	ReturnV.SkillLevel[3] = SubDataBuffer.CharData.SkillLevel[3];
	//计算连携等级，按所有人物来算
	for (auto i : MainChar->ItemData.CharData.SkillLevel)
	{
		SkillLevelOver += i;
	}
	ReturnV.SkillLevel[4] = SkillLevelOver / 8;

	//合并被动技能介绍
	ReturnV.SkillDescription.SetNum(
		MainChar->ItemData.CharData.SkillDescription.Num() + SubDataBuffer.CharData.SkillDescription.Num()); //先扩容
	ReturnV.SkillDescription.Append(MainChar->ItemData.CharData.SkillDescription);
	ReturnV.SkillDescription.Append(SubDataBuffer.CharData.SkillDescription);

	// 物理防御计算合并，首先，这里合并的是分数，然后根据函数来计算50%，所以最后要使用值的时候还要*0.01
	int PhyBuffer = MainChar->ItemData.CharData.PhyDefense + SubDataBuffer.CharData.PhyDefense;
	if (PhyBuffer > 200)
	{
		if (PhyBuffer > 18200)
		{
			ReturnV.PhyDefense = ((PhyBuffer * 0.01) - 102) * 0.01;
		}
		else
		{
			ReturnV.PhyDefense = ((PhyBuffer * 0.004) + 7.2) * 0.01;
		}
	}
	else
	{
		ReturnV.PhyDefense = (PhyBuffer * 0.04) * 0.01;
	}

	// 异能防御计算合并
	int MagBuffer = MainChar->ItemData.CharData.MagDefense + SubDataBuffer.CharData.MagDefense;
	if (MagBuffer > 200)
	{
		if (MagBuffer > 18200)
		{
			ReturnV.MagDefense = ((MagBuffer * 0.01) - 102) *0.01;
		}
		else
		{
			ReturnV.MagDefense = ((MagBuffer * 0.004) + 7.2) *0.01;
		}
	}
	else
	{
		ReturnV.MagDefense = (MagBuffer * 0.04) *0.01;
	}

	// 暴击率计算合并
	int CriticalBuffer = MainChar->ItemData.CharData.Critical + SubDataBuffer.CharData.Critical;
	if (CriticalBuffer > 200)
	{
		if (CriticalBuffer > 18200)
		{
			ReturnV.Critical = ((CriticalBuffer * 0.011) - 142.2) *0.01;
		}
		else
		{
			ReturnV.Critical = ((CriticalBuffer * 0.003) + 3.4) *0.01;
		}
	}
	else
	{
		ReturnV.Critical = (CriticalBuffer * 0.02) *0.01;
	}

	// 暴击率计算合并
	int CriticalCBuffer = MainChar->ItemData.CharData.CriticalC + SubDataBuffer.CharData.CriticalC;
	if (CriticalCBuffer > 200)
	{
		if (CriticalCBuffer > 18200)
		{
			ReturnV.CriticalC = ((CriticalCBuffer * 0.011) - 142.2) *0.01;
		}
		else
		{
			ReturnV.CriticalC = ((CriticalCBuffer * 0.003) + 3.4) *0.01;
		}
	}
	else
	{
		ReturnV.CriticalC = (CriticalCBuffer * 0.02) *0.01;
	}

	return ReturnV;
}

void UFUCRealize::SetTagPosition(TArray<AActor*> MapsIN)
{
	//等会加个判断，数量是否够6个
	if (MapsIN.Num() != 6)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("GM_PlotTurn/ActorMap出现错误：数量不为6个"));
	}
	else if (MapsIN[0] == nullptr && MapsIN[1] == nullptr && MapsIN[2] == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("GM_PlotTurn/ActorMap出现错误：我方一个人都没有"));
	}
	else if (MapsIN[3] == nullptr && MapsIN[4] == nullptr && MapsIN[5] == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("GM_PlotTurn/ActorMap出现错误：敌方一个人都没有"));
	}
	else
	{
		TArray<int> ForEach = {0, 1, 2, 3, 4, 5};
		for (auto i : ForEach)
		{
			switch (i)
			{
			case 0: //第一个人的标签
				{
					if (MapsIN[0] != nullptr) //一号位有人，添加F标签
					{
						MapsIN[0]->Tags[1] = FName("F");
						break;
					}
					else
					{
						if (MapsIN[1] != nullptr)
						{
							MapsIN[1]->Tags[1] = FName("F");
							break;
						} //二号位有人，添加F标签
						else //给三号位添加F标签
						{
							MapsIN[2]->Tags[1] = FName("F");
							break;
						}
					}
				}
			case 1: //第二个人的标签
				{
					if (MapsIN[1] != nullptr) //二号位有人
					{
						if (MapsIN[1]->Tags.Contains(FName("F"))) //二号位是不是已经是前排了，是了就不加M标签
						{
							if (MapsIN[2] != nullptr) //三号位有人，加M标签
							{
								MapsIN[2]->Tags[2] = FName("M");
								break;
							}
							else //三号没人，就自己一个，加M标签
							{
								MapsIN[1]->Tags[2] = FName("M");
								break;
							}
						}
						else //二号位不是前排，正常加M标签
						{
							MapsIN[1]->Tags[2] = FName("M");
							break;
						}
					}
					else //二号位没人，让三号位来检测中排
					{
						if (MapsIN[2] != nullptr) //三号有人就加M标签
						{
							MapsIN[2]->Tags[2] = FName("M");
							break;
						}
						else //三号位没人，让一号把标签吃满
						{
							MapsIN[0]->Tags[2] = FName("M");
							break;
						}
					}
				}
			case 2: //友方的第三位
				{
					if (MapsIN[2] != nullptr) //三号有人，就加B标签
					{
						MapsIN[2]->Tags[3] = FName("B");
						break;
					}
					else //三号位没人，让二号位来检测后排
					{
						if (MapsIN[1] != nullptr) //二号有人，就加B标签
						{
							MapsIN[1]->Tags[3] = FName("B");
							break;
						}
						else //只有前排一个人，添加B标签
						{
							MapsIN[0]->Tags[3] = FName("B");
							break;
						}
					}
				}
			case 3: //第一个人的标签
				{
					if (MapsIN[3] != nullptr) //一号位有人，添加F标签
					{
						MapsIN[3]->Tags[1] = FName("F");
						break;
					}
					else
					{
						if (MapsIN[4] != nullptr)
						{
							MapsIN[4]->Tags[1] = FName("F");
							break;
						} //二号位有人，添加F标签
						else //给三号位添加F标签
						{
							MapsIN[5]->Tags[1] = FName("F");
							break;
						}
					}
				}
			case 4: //第二个人的标签
				{
					if (MapsIN[4] != nullptr) //二号位有人
					{
						if (MapsIN[4]->Tags.Contains(FName("F"))) //二号位是不是已经是前排了，是了就不加M标签
						{
							if (MapsIN[5] != nullptr) //三号位有人，加M标签
							{
								MapsIN[5]->Tags[2] = FName("M");
								break;
							}
							else //三号没人，就自己一个，加M标签
							{
								MapsIN[4]->Tags[2] = FName("M");
								break;
							}
						}
						else //二号位不是前排，正常加M标签
						{
							MapsIN[4]->Tags[2] = FName("M");
							break;
						}
					}
					else //二号位没人，让三号位来检测中排
					{
						if (MapsIN[5] != nullptr) //三号有人就加M标签
						{
							MapsIN[5]->Tags[2] = FName("M");
							break;
						}
						else //三号位没人，让一号把标签吃满
						{
							MapsIN[3]->Tags[2] = FName("M");
							break;
						}
					}
				}
			case 5: //友方的第三位
				{
					if (MapsIN[5] != nullptr) //三号有人，就加B标签
					{
						MapsIN[5]->Tags[3] = FName("B");
						break;
					}
					else //三号位没人，让二号位来检测后排
					{
						if (MapsIN[4] != nullptr) //二号有人，就加B标签
						{
							MapsIN[4]->Tags[3] = FName("B");
							break;
						}
						else //只有前排一个人，添加B标签
						{
							MapsIN[3]->Tags[3] = FName("B");
							break;
						}
					}
				}
			default:
				{
					break;
				}
			}
		}
	}
}


int UFUCRealize::SortSpeed(TArray<ACrossvoid2DatkNew*> MapsIN)
{
	TArray<int> GetSpeed; //制作一个速度数组暂存
	GetSpeed.SetNum(6);//规整到6个
	for (auto Buffer : MapsIN) //遍历所有角色,获取所有角色的速度
	{
		if (Buffer != nullptr) //如果当前角色不为空
		{
			if (GetSpeed.Contains(Buffer->CharSelfData.Speed))//如果当前速度已存在
			{
				GetSpeed[MapsIN.Find(Buffer)] = Buffer->CharSelfData.Speed - 1; //获取当前角色偏移速度
			}
			else
			{
				GetSpeed[MapsIN.Find(Buffer)] = Buffer->CharSelfData.Speed; //获取当前角色真实速度
			}
		}
	}
	TArray<int> Action; //角色的行动顺序

	TArray<int> SortReturnV = SortInt(GetSpeed, true); //获取排序后的速度

	for (auto i : SortReturnV) //遍历排序后的速度
	{
		AActor* iActor = MapsIN[GetSpeed.Find(i)]; //获取当前角色
		if (iActor != nullptr) //如果当前角色不为空
		{
			iActor->Tags[4] = FName(*FString::FromInt(SortReturnV.Find(i) + 1));
		}
	}
	TArray<AActor*> MapsINB; //预留好一个数组，判断有效的Actor数量
	
	for (auto Buffer2 : MapsIN)
	{
		if (Buffer2 != nullptr)
		{
			MapsINB.Add(Buffer2);
		}
	}
	return MapsINB.Num(); //返回有效Actor数量
}

TArray<int> UFUCRealize::SortInt(TArray<int> Input, bool Descending)
{
	TArray<int> Buffer = Input; //将Dispose输入值赋值给Buffer（局部变量），两个都是自己起的名
	if (Descending) //判断是否降序
	{
		Buffer.Sort([](const int& A, const int& B) { return A > B; }); //降序,利用TArray的Sort函数
	}
	else
	{
		Buffer.Sort([](const int& A, const int& B) { return A < B; }); //升序
	}
	return Buffer; //返回排序后的值
}

TArray<float> UFUCRealize::SortFloat(TArray<float> Input, bool Descending)
{
	TArray<float> Buffer = Input;
	if (Descending)
	{
		Buffer.Sort([](const int& A, const int& B) { return A > B; });
	}
	else
	{
		Buffer.Sort([](const int& A, const int& B) { return A < B; });
	}
	return Buffer;
}

bool UFUCRealize::VictoryType1(TArray<AActor*> MapsIN, bool& WhoWin)
{
	bool Alive1p = false;
	bool Alive2P = false;
	//判断2P全部阵亡
	for (auto Char1P : MapsIN)
	{
		if (IsValid(Char1P))
		{
			if (Char1P->Tags[0] == FName("1P"))
			{
				Alive1p = true;
			}
		}
	}
	for (auto Char2P : MapsIN)
	{
		if (IsValid(Char2P))
		{
			if (Char2P->Tags[0] == FName("2P"))
			{
				Alive2P = true;
			}
		}
	}
	if (Alive1p)
	{
		if (Alive2P)
		{
			WhoWin = false;
			return false;
		}
		else
		{
			WhoWin = true;
			return true;
		}
	}
	else
	{
		WhoWin = false;
		return true;
	}
}

void UFUCRealize::MoveCalcu(AActor* Movea, AActor* Target, float Offset, float& TargetLoc, bool& Arrive, bool& MoveFace)
{
	if (IsValid(Target) && IsValid(Movea)) //修复 的让少报点错
	{
		check(IsInGameThread()); // 调试时检查线程
		float OffsetCorrect = Offset;
		if (OffsetCorrect >= 70)
		{
			if (Target->Tags.Contains(FName("F")))
			{
				OffsetCorrect = 70;
			}
		}
		//根据0来判断是向左还是向右，以此判断偏移量的增减
		TargetLoc = (Target->GetActorLocation().X > 0
			             ? Target->GetActorLocation().X - OffsetCorrect
			             : Target->GetActorLocation().X + OffsetCorrect);
		//根据0判断方位，是否到达，如果都在同一边了就是到了
		bool ju1 = Target->GetActorLocation().X > 0 == Movea->GetActorLocation().X > 0;
		//目标位置的绝对值-自己的，小于8就是到了，（懂了）
		int Distance2 = std::abs(TargetLoc) - std::abs(Movea->GetActorLocation().X);
		//如果是同一边，那么也要绝对值判断才不会出现双绝对值算出负数
		bool ju3 = std::abs(Distance2) <= 8;
		Arrive = (ju1 ? ju3 : false); //(旧版，根据0为分界线和剩余距离，会出现实际距离在另一边的情况)
		//Arrive = ju3;//现在只根据距离试试（会出现一种情况，因为两边是对称的，所以不换边直接到地方了，还是只能用原本的）
		MoveFace = TargetLoc - Movea->GetActorLocation().X > 0;
	}
}

float UFUCRealize::CardProbability(int Counted)
{
	if (Counted <= 34)
		return 0.01f;
	else if (Counted <= 50)
		return 0.005f + 0.01f * (Counted - 34);
	else if (Counted <= 70)
		return 0.01f + 0.02f * (Counted - 50);
	else if (Counted <= 90)
		return 0.02f + 0.06f * (Counted - 70);
	else
		return 1.0f; // 第 91 抽后强制出五星
}

bool UFUCRealize::CallFunctionByName(UObject* Object, FName FunctionName)
{
	if (IsValid(Object)) // 判断对象是否合法
	{
		UFunction* Func = Object->FindFunction(FunctionName); // 查找函数
		if (Func) // 判断函数是否合法
		{
			Object->ProcessEvent(Func, nullptr);
			return true;
		}
		if(FunctionName == "None")
		{
			return false;
		}
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("函数不存在: %s"), *FunctionName.ToString()));
		return false;
	}
	return false;
}

TArray<FName> UFUCRealize::SplitFunction(FString FunctionStr, TArray<FString>& ParameterArray)
{
	if (FunctionStr.IsEmpty())
	{
		//函数名为空
		ParameterArray = {};
		return {};
	}

	FString Front = "";//分割前缀暂存
	FString Back = "";//分割后缀暂存
	TArray<FName> IndsBuffers ={"",""};//函数名数组
	TArray<FString> ValuesBuffers ={"",""};//函数参数数组
	
	if (!FunctionStr.Contains("/"))//是否有多重函数
	{
		if (!FunctionStr.Contains("_"))//是否有参数
		{
			IndsBuffers.Add(*FunctionStr);
			ParameterArray = {};//空
			return IndsBuffers;//直接返回函数名
		}
		//开始分割函数和数值
		UKismetStringLibrary::Split(FunctionStr, TEXT("_"), Front, Back, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		IndsBuffers.Add(*Front);//添加函数名
		ValuesBuffers.Add(Back);//添加参数
		
		ParameterArray = ValuesBuffers;
		return IndsBuffers;
	}

	//如果有一个以及以上的分隔符，就会执行后续
	int IndIndex = -1;//函数指示器准备置入索引
	Back = FunctionStr;
	
	//获取单个字符数组
	TArray<FString> SingleStr = UKismetStringLibrary::GetCharacterArrayFromString(FunctionStr);
	for (auto Times : SingleStr)
	{
		if(Times == "/")//如果找到一次分隔符，就开始执行一次后续
		{
			IndIndex += 1;//置入索引+1
			IndsBuffers.SetNum(IndIndex + 2);//扩展数组
			ValuesBuffers.SetNum(IndIndex + 2);//扩展数组
			UKismetStringLibrary::Split(Back, TEXT("/"), Front, Back, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			IndsBuffers[IndIndex] = *Front;//添加分割出来的前缀函数（保证收录单个）
		}	
	}
	//前面都添加完毕，添加最后一个后缀
	IndsBuffers[IndIndex+1] = *Back;
	
	//准备提取参数
	for (auto Times2 : IndsBuffers)
	{
		FString ReadyToValue = Times2.ToString();//转换为字符串,后续分割
		int SelfIndex = IndsBuffers.Find(Times2);//获取当前的索引
		
		if (ReadyToValue.Contains("_"))//判断函数中是否有参数
		{
			UKismetStringLibrary::Split(ReadyToValue, TEXT("_"), Front, Back, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			IndsBuffers[SelfIndex] = *Front;//添加函数名
			ValuesBuffers[SelfIndex] = Back;//添加参数
		}
		else
		{
			ValuesBuffers[SelfIndex] = "";
		}
	}
	ParameterArray = ValuesBuffers;
	return IndsBuffers;//结束
}

int UFUCRealize::CultivateAttributeSingle(int RoundAll, bool IsSpecial, int AbMode)
{
	if (AbMode == 0)
	{
		return (IsSpecial ? 960 : 320) / (RoundAll/3);
	}
	else if (AbMode == 1)
	{
		return (IsSpecial ? 10000 : 8000) / (RoundAll/3);
	}
	else
	{
		return (IsSpecial ? 5000 : 3000) / (RoundAll/3);
	}
}

F2DAtkDataStruct UFUCRealize::CultivateAbAdd(F2DAtkDataStruct CharData, int Random1, int Ab2, int Random3, int Holding,
                                             int Ab1Value, int Ab2Value, int Ab3Value)
{
	F2DAtkDataStruct CharBuffer = CharData; //先存储角色的参数
	int ValueAddBuffer = 0; //增长多少数值
	int ValueMode = 0; //要增加的数值类型
	switch (Holding)
	{
	case 0:
		ValueAddBuffer = Ab1Value;
		ValueMode = Random1;
		break;
	case 1:
		ValueAddBuffer = Ab2Value;
		ValueMode = Ab2;
		break;
	case 2:
		ValueAddBuffer = Ab3Value;
		ValueMode = Random3;
		break;
	default:
		ValueAddBuffer = 0;
		ValueMode = 0;
		break;
	}
	switch (ValueMode)
	{
	default:
		break;
	case 0:
		CharBuffer.Attack += ValueAddBuffer;
		break;
	case 1:
		CharBuffer.Health += ValueAddBuffer;
		break;
	case 2:
		CharBuffer.Critical += ValueAddBuffer;
		break;
	case 3:
		CharBuffer.CriticalC += ValueAddBuffer;
		break;
	case 4:
		CharBuffer.PhyDefense += ValueAddBuffer;
		break;
	case 5:
		CharBuffer.MagDefense += ValueAddBuffer;
		break;
	}
	return CharBuffer;
}

FString UFUCRealize::HoldingValue(FYCPlotrpc PlotData, int Holding, bool UseAct, FName& FUC, FString& StoryResult,
                                  FString& ExAb)
{
	FString ChoiceBuffer = "Error"; //选项
	FString FUCName = "Error"; //函数名
	FString ChoiceTextBuffer = "Error"; //选项文本
	FString ResultBuffer = "Error"; //选项结果
	FString ExResult = "Error"; //额外属性
	bool HaveFuc = false; //有函数吗

	//获取选项，选项结果，额外属性
	switch (Holding)
	{
	default:
		break;
	case 0:
		ChoiceBuffer = (UseAct ? PlotData.Option1_EX : PlotData.Option1);
		ResultBuffer = (UseAct ? PlotData.Result1_EX : PlotData.Result1);
		ExResult = PlotData.Option1_EX_Attributes;
		break;
	case 1:
		ChoiceBuffer = (UseAct ? PlotData.Option2_EX : PlotData.Option2);
		ResultBuffer = (UseAct ? PlotData.Result2_EX : PlotData.Result2);
		ExResult = PlotData.Option2_EX_Attributes;
		break;
	case 2:
		ChoiceBuffer = (UseAct ? PlotData.Option3_EX : PlotData.Option3);
		ResultBuffer = (UseAct ? PlotData.Result3_EX : PlotData.Result3);
		ExResult = PlotData.Option3_EX_Attributes;
		break;
	}

	//拆开选项
	HaveFuc = UKismetStringLibrary::Split(ChoiceBuffer, "#", ChoiceTextBuffer, FUCName, ESearchCase::IgnoreCase,
	                                      ESearchDir::FromStart);

	FUC = FName(*FUCName); //函数指示器
	StoryResult = ResultBuffer; //选项结果
	ExAb = ExResult; //额外属性
	return (HaveFuc ? ChoiceTextBuffer : ChoiceBuffer); //选项文本
}

void UFUCRealize::GetCharacterSum(F2DAtkDataStruct CharData,F2DAtkDataStruct SubData, int Rank, F2DAtkDataStruct& CharSum)
{
	F2DAtkDataStruct CharBuffer = CharData; //角色参数缓存初始化

	CharBuffer.Speed = CharData.Speed + SubData.Speed;
	CharBuffer.SkillLevel = CharData.SkillLevel;
	CharBuffer.SkillLevel.SetNum(5);
	CharBuffer.SkillLevel[3] = SubData.SkillLevel[3];

	//准备计算角色技能等级，先获取角色技能等级总和
	int SkillLevelOver = 0;
	for (auto i : CharData.SkillLevel)
	{
		SkillLevelOver += i;
	}
	for (auto i : SubData.SkillLevel)
	{
		SkillLevelOver += i;
	}//最后除以8
	CharBuffer.SkillLevel[4] = SkillLevelOver > 8 ? SkillLevelOver/8:1;
	
	
	switch (Rank) //根据角色等级，增加基础数值
	{
	default:
		break;
	case 0:
		CharBuffer.Health = 2000 + CharData.Health + SubData.Health;
		CharBuffer.Attack = 50 + CharData.Attack + SubData.Attack;
		CharBuffer.Critical = 10 + CharData.Critical + SubData.Critical;
		CharBuffer.CriticalC = 10 + CharData.CriticalC + SubData.CriticalC;
		CharBuffer.PhyDefense = 50 + CharData.PhyDefense + SubData.PhyDefense;
		CharBuffer.MagDefense = 50 + CharData.MagDefense + SubData.MagDefense;
		break;
	case 1:
		CharBuffer.Health = 2500 + CharData.Health + SubData.Health;
		CharBuffer.Attack = 60 + CharData.Attack + SubData.Attack;
		CharBuffer.Critical = 30 + CharData.Critical + SubData.Critical;
		CharBuffer.CriticalC = 30 + CharData.CriticalC + SubData.CriticalC;
		CharBuffer.PhyDefense = 60 + CharData.PhyDefense + SubData.PhyDefense;
		CharBuffer.MagDefense = 60 + CharData.MagDefense + SubData.MagDefense;
		break;
	case 2:
		CharBuffer.Health = 3000 + CharData.Health + SubData.Health;
		CharBuffer.Attack = 70 + CharData.Attack + SubData.Attack;
		CharBuffer.Critical = 60 + CharData.Critical + SubData.Critical;
		CharBuffer.CriticalC = 60 + CharData.CriticalC + SubData.CriticalC;
		CharBuffer.PhyDefense = 80 + CharData.PhyDefense + SubData.PhyDefense;
		CharBuffer.MagDefense = 80 + CharData.MagDefense + SubData.MagDefense;
		break;
	case 3:
		CharBuffer.Health = 4000 + CharData.Health + SubData.Health;
		CharBuffer.Attack = 85 + CharData.Attack + SubData.Attack;
		CharBuffer.Critical = 80 + CharData.Critical + SubData.Critical;
		CharBuffer.CriticalC = 80 + CharData.CriticalC + SubData.CriticalC;
		CharBuffer.PhyDefense = 100 + CharData.PhyDefense + SubData.PhyDefense;
		CharBuffer.MagDefense = 100 + CharData.MagDefense + SubData.MagDefense;
		break;
	case 4:
		CharBuffer.Health = 4600 + CharData.Health + SubData.Health;
		CharBuffer.Attack = 100 + CharData.Attack + SubData.Attack;
		CharBuffer.Critical = 100 + CharData.Critical + SubData.Critical;
		CharBuffer.CriticalC = 100 + CharData.CriticalC + SubData.CriticalC;
		CharBuffer.PhyDefense = 200 + CharData.PhyDefense + SubData.PhyDefense;
		CharBuffer.MagDefense = 200 + CharData.MagDefense + SubData.MagDefense;
		break;
	}
	CharSum = CharBuffer;
}

F2DAtkDataStruct UFUCRealize::CharacterDataConversion(F2DAtkDataStruct CharData)
{
	F2DAtkDataStruct CharBuffer = CharData;

	//物理防御
	if (CharBuffer.PhyDefense > 200)
	{
		if (CharBuffer.PhyDefense > 18200)
		{
			CharBuffer.PhyDefense = ((CharBuffer.PhyDefense * 0.01) - 137) *0.01;
		}
		else
		{
			CharBuffer.PhyDefense = ((CharBuffer.PhyDefense * 0.004) + 7.2) *0.01;
		}
	}
	else
	{
		CharBuffer.PhyDefense = (CharBuffer.PhyDefense * 0.04) *0.01;
	}
	//异能防御
	if (CharBuffer.MagDefense > 200)
	{
		if (CharBuffer.MagDefense > 18200)
		{
			CharBuffer.MagDefense = ((CharBuffer.MagDefense * 0.01) - 137) *0.01;
		}
		else
		{
			CharBuffer.MagDefense = ((CharBuffer.MagDefense * 0.004) + 7.2) *0.01;
		}
	}
	else
	{
		CharBuffer.MagDefense = (CharBuffer.MagDefense * 0.04) *0.01;
	}
	//暴击率
	if (CharBuffer.Critical > 200)
	{
		if (CharBuffer.Critical > 18200)
		{
			CharBuffer.Critical = ((CharBuffer.Critical * 0.011) - 140.2) *0.01;
		}
		else
		{
			CharBuffer.Critical = ((CharBuffer.Critical * 0.003) + 3.4) *0.01;
		}
	}
	else
	{
		CharBuffer.Critical = (CharBuffer.Critical * 0.02) *0.01;
	}
	//暴击伤害
	if (CharBuffer.CriticalC > 200)
	{
		if (CharBuffer.CriticalC > 18200)
		{
			CharBuffer.CriticalC = ((CharBuffer.CriticalC * 0.011) - 140.2) *0.01;
		}
		else
		{
			CharBuffer.CriticalC = ((CharBuffer.CriticalC * 0.003) + 3.4) *0.01;
		}
	}
	else
	{
		CharBuffer.CriticalC = (CharBuffer.CriticalC * 0.02) *0.01;
	}

	return CharBuffer;
}

FString UFUCRealize::YCNextPlot(FName NowPlotName)
{
	FString Strbuffer = NowPlotName.ToString();//获取当前剧情名称str
	FString PerfixFront = "";//前缀
	FString PerfixBack = "";//后缀
	UKismetStringLibrary::Split(Strbuffer, "_", PerfixFront, PerfixBack, ESearchCase::IgnoreCase,
										  ESearchDir::FromEnd);
	int PlotNum = FCString::Atoi(*PerfixBack);//获取当前剧情编号int
	return PerfixFront + "_" + FString::FromInt(PlotNum + 1);
}

EYCFUCTriggerType UFUCRealize::YCFUCTriggerTypeFuc(FName FUC, FString& Type2, FString& Value1)
{
	if (FUC.IsNone())//先检测FUC是否空
	{
		Type2 = "FUC is None";
		Value1 = "FUC is None";
		return EYCFUCTriggerType::Default;
	}
	FString FUCStr = FUC.ToString();//转换为str
	FString Front = "";//前缀
	FString Back = "";//后缀
	
	if (FUCStr.Contains("AbAdd"))//属性增加大分支
	{
		UKismetStringLibrary::Split(FUCStr, "_", Front, Back, ESearchCase::IgnoreCase,
										  ESearchDir::FromStart);
		UKismetStringLibrary::Split(Back, "_", Front, Back, ESearchCase::IgnoreCase,
										  ESearchDir::FromStart);
		Type2 = Front;
		Value1 = Back;
		return EYCFUCTriggerType::AbAdd;
	}

	if (FUCStr.Contains("ExabAdd"))//额外属性增加大分支
	{
		UKismetStringLibrary::Split(FUCStr, "_", Front, Back, ESearchCase::IgnoreCase,
										  ESearchDir::FromStart);
		Type2 = "NotUse";
		Value1 = Back;
		return EYCFUCTriggerType::ExabAdd;
	}

	Type2 = "FUC not contrast";
	Value1 = "FUC not contrast";
	return EYCFUCTriggerType::Default;//默认
}

int UFUCRealize::YCScore(int Rank, TArray<int> Sklevel, int HP, int Attack, int Critical, int CriticalC, int PhyDefense,
                         int MagDefense)
{
	if (Sklevel.Num() != 4)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("技能等级数组长度错误"));
		return -1;
	}
	int M = Rank + 1 + (Sklevel[0] + Sklevel[1] + Sklevel[2] + Sklevel[3]);
	int B = (HP + Attack + Critical + CriticalC + PhyDefense + MagDefense) * 0.1;
	return M * B;
}



EBackPVPType UFUCRealize::BackPVPTypeFuc(FString Pvpback,bool Victory, FString& PvpbackEX)
{
	
	if (Pvpback.IsEmpty())//为空返回主界面
	{
		return EBackPVPType::MainM;
	}
	
	if (Pvpback.Contains("YC"))//返回养成界面
	{
		if (Victory)
		{
			PvpbackEX = Pvpback;
		}
		else
		{
			FString PerfixFront = "";//前缀
			FString PerfixBack = "";//后缀
			UKismetStringLibrary::Split(Pvpback, "_", PerfixFront, PerfixBack, ESearchCase::IgnoreCase,
												  ESearchDir::FromEnd);
			PvpbackEX =  PerfixFront + "_END";
		}
		return EBackPVPType::YC;
	}
	
	return EBackPVPType::MainM;//默认返回主界面
}

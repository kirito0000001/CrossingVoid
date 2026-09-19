#include "CrossTurn2D/Crossvoid2DatkNew.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values
ACrossvoid2DatkNew::ACrossvoid2DatkNew()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	//创建组件，.h去绑定
	BuffComponent = CreateDefaultSubobject<UDreamTaskComponent>(FName{TEXTVIEW("BuffComponent")});
	//创建组件
	DialogueComponent = CreateDefaultSubobject<UWidgetComponent>(FName{TEXTVIEW("DialogueComponent")});
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(FName{TEXTVIEW("SpringArm")});
	Camera = CreateDefaultSubobject<UCameraComponent>(FName{TEXTVIEW("Camera")});
	//相机附加到SpringArm
	SpringArm->SetupAttachment(RootComponent);
	Camera->SetupAttachment(SpringArm);
	
	//构造函数其余逻辑开始
	//扩容Tag数组
	this->Tags.SetNum(35);
	
}

// Called when the game starts or when spawned
void ACrossvoid2DatkNew::BeginPlay()
{
	Super::BeginPlay();
	//根据目标形态检查配置是否有效
	if (ClickSeq.Num() < TargetShape || DodgeSeq.Num() < TargetShape || DefSeq.Num() < TargetShape || DefAttackSeq.Num()
		< TargetShape || OnDamageSeq.Num() < TargetShape || VictorAnim.Num() < TargetShape || DefeatedAnim.Num() <
		TargetShape || DeathAnim.Num() < TargetShape)
	{
		GEngine->AddOnScreenDebugMessage(-1, 500.0f, FColor::Red, TEXT("动画没有完全配置好，会报错"));
	}
}

// Called every frame
void ACrossvoid2DatkNew::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ACrossvoid2DatkNew::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent); //只写了这个
}

void ACrossvoid2DatkNew::CheckSelfData()
{
	bool HasSub = !CharSelfData.Char2.IsEmpty();
	//sub角色是没有四个技能，只有三个
	//检查技能等级是否合规
	if (CharSelfData.SkillLevel.Num() < (HasSub? 4:3))
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("%s_技能等级数组数量不合规，会越界访问"), *CharSelfData.Char1));
		CharSelfData.SkillLevel = {1,1,1,1};
	}
	//检查等级是否有0（因为是从1开始，所以有0就是数据出错了）
	if (HasSub)
	{
		if (CharSelfData.SkillLevel.Contains(0))
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("%s_技能等级数组里出现了0级"), *CharSelfData.Char1));
			CharSelfData.SkillLevel = {1,1,1,1};
		}
	}
	else
	{
		if (CharSelfData.SkillLevel[0] == 0)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("%s_技能等级数组里出现了0级"), *CharSelfData.Char1));
			CharSelfData.SkillLevel = {1,1,1,1};
		}
	}
	//检查血量是否合规
	if (CharSelfData.Health == 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("%s_生命值等于0"), *CharSelfData.Char1));
	}
	//检查攻击力是否合规
	if (CharSelfData.Attack <= 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("%s_攻击力小于等于0"), *CharSelfData.Char1));
	}
}

bool ACrossvoid2DatkNew::PerformSpawn(FSkillData2D SkillPer)
{
	float HPspwan = 0.0f; //新建一个生成的盾
	float PhyDefenseRate = 0.0f; //物理护盾生成的比例
	float MagDefenseRate = 0.0f; //异能护盾生成的比例
	int CharShapeNow = SkillPer.SkillState.Num()-1 < CharShape ? SkillPer.SkillState.Num()-1 : CharShape;
	bool IsSkillAbandon = SkillPer.SkillState[CharShapeNow] == E2DSkillType::Abandon; //判断这个技能是被丢弃的吗
	//开始生成守备数值
	PreformType = SkillPer.PreformType[CharShapeNow]; //找到对应的守备类型并设置
	switch (PreformType)
	{
	case EPreformType::Air:
		return false;
	case EPreformType::Defense:
		if (IsSkillAbandon)
		{
			FSkillDiscard.Broadcast();//触发技能丢弃的委托
		}
		else
		{
			FSkillStart.Broadcast(); //触发技能开始委托
		}
		HPspwan = MaxHealth * (IsSkillAbandon ? 0.36f : 0.18f); //生成总盾值
		if (SkillPer.PreSkillValue.IsValidIndex(CharShapeNow))
		{
			PhyDefenseRate = SkillPer.PreSkillValue[CharShapeNow]; //设置物理比例
			MagDefenseRate = 1 - PhyDefenseRate; //设置异能比例
		}

		PhyDefense = PhyDefenseRate * HPspwan; //根据比例生成物理盾
		MagDefense = MagDefenseRate * HPspwan;

		DefenseUIInitialize(PreformType, PhyDefense, MagDefense); //触发守备UI初始化

		DefAtkPhy = PhyDefenseRate > MagDefenseRate; //使用哪种类型反击
		return IsSkillAbandon;
	case EPreformType::Attack:
		if (IsSkillAbandon)
		{
			FSkillDiscard.Broadcast();//触发技能丢弃的委托
		}
		else
		{
			FSkillStart.Broadcast(); //触发技能开始委托
		}
		HPspwan = MaxHealth * (IsSkillAbandon ? 0.10f : 0.05f); //生成总盾值
		if (SkillPer.PreSkillValue.IsValidIndex(CharShapeNow))
		{
			PhyDefenseRate = SkillPer.PreSkillValue[CharShapeNow]; //设置物理比例
			MagDefenseRate = 1 - PhyDefenseRate; //设置异能比例
		}

		PhyDefense = PhyDefenseRate * HPspwan; //根据比例生成物理盾
		MagDefense = MagDefenseRate * HPspwan;

		DefenseUIInitialize(PreformType, PhyDefense, MagDefense); //触发守备UI初始化

		DefAtkPhy = PhyDefenseRate > MagDefenseRate; //使用哪种类型反击
		return IsSkillAbandon;
	case EPreformType::Dodge:
		if (IsSkillAbandon)
		{
			FSkillDiscard.Broadcast();//触发技能丢弃的委托
		}
		else
		{
			FSkillStart.Broadcast(); //触发技能开始委托
		}
		if (SkillPer.PreSkillValue.IsValidIndex(CharShapeNow))
		{
			PhyDefenseRate = SkillPer.PreSkillValue[CharShapeNow];
		}
		Dodges = (IsSkillAbandon ? PhyDefenseRate * 2 : PhyDefenseRate);
		DefenseUIInitialize(PreformType, Dodges, 0); //触发守备UI初始化
		return IsSkillAbandon;
	default: return false;
	}
}

ACrossvoid2DatkNew* ACrossvoid2DatkNew::FindTarget(FName Who)
{
	ACrossvoid2DatkNew* SelfActor = nullptr; //新建敌方友方的暂时变量
	ACrossvoid2DatkNew* EnemyActor = nullptr;

	//根据合集开始找
	for (auto BufferActor : MapsIN)
	{
		if (BufferActor == nullptr) //先检测人物是否有效
		{
			continue;
		}
		else
		{
			if (BufferActor->Tags.Contains(Who)) //检测人物是否有这个tag
			{
				if (BufferActor->Tags.Contains(this->Tags[0])) //判断敌方友方
				{
					SelfActor = BufferActor; //这是友军的
				}
				else
				{
					EnemyActor = BufferActor; //这是敌方的
				}
			}
			else
			{
				continue;
			}
		}
	}
	//根据结果输出
	return (TargetTeam ? EnemyActor : SelfActor);
}

TArray<ACrossvoid2DatkNew*> ACrossvoid2DatkNew::FindTargets(TArray<FName> Who)
{
	TArray<ACrossvoid2DatkNew*> Selfs = {};
	TArray<ACrossvoid2DatkNew*> Enemys = {};

	for (auto Tagsin : Who)
	{
		for (auto BufferActor : MapsIN)
		{
			if (BufferActor != nullptr)
			{
				if (BufferActor->Tags.Contains(Tagsin))
				{
					if (BufferActor->Tags.Contains(this->Tags[0]))
					{
						Selfs.AddUnique(BufferActor);
					}
					else
					{
						Enemys.AddUnique(BufferActor);
					}
				}
			}
		}
	}
	return (TargetTeam ? Enemys : Selfs);
}


void ACrossvoid2DatkNew::MoveTikFuc(float InX, float InZ, bool OverRide)
{
	this->LaunchCharacter(FVector(((Face ? -InX : InX)), 0, InZ), OverRide, OverRide);
}

void ACrossvoid2DatkNew::AtkCalculate(int SkSel, float& Phy1, float& Mag1)
{
	//先获取技能
	FSkillData2D SkillBuffer;
	int Useless1 = 0;
	GetCharSkill(SkSel, SkillBuffer, Useless1);
	//获取倍率
	float PhyRate = 0;
	float MagRate = 0;
	PhyRate = SkillBuffer.SkillRate[CharShape].PhyLVrate[CharSelfData.SkillLevel[SkSel - 1]];
	MagRate = SkillBuffer.SkillRate[CharShape].MagLVrate[CharSelfData.SkillLevel[SkSel - 1]];
	//根据攻击力和倍率计算伤害
	Phy1 = CharSelfData.Attack * PhyRate;
	Mag1 = CharSelfData.Attack * MagRate;
}

void ACrossvoid2DatkNew::CriticalCalculate(float Phy2IN, float Mag2IN, float& Phy3Out, float& Mag3Out, bool& IsCritical)
{
	bool Buffer = UKismetMathLibrary::RandomBoolWithWeight(CharSelfData.Critical);
	Phy3Out = (Buffer ? Phy2IN * (CharSelfData.CriticalC + 1) : Phy2IN);
	Mag3Out = (Buffer ? Mag2IN * (CharSelfData.CriticalC + 1) : Mag2IN);
	IsCritical = Buffer;
}

void ACrossvoid2DatkNew::DefenseCalculate(float Phy2IN, float Mag2IN, float& Phy3Out, float& Mag3Out)
{
	if (Phy2IN <= 0 && Mag2IN <= 0) //判断是不是治疗
	{
		IsCure = true;
		Phy3Out = Phy2IN;
		Mag3Out = Mag2IN;
		return;
	}
	IsCure = false;
	Phy3Out = Phy2IN * (1 - CharSelfData.PhyDefense);
	Mag3Out = Mag2IN + (1 - CharSelfData.MagDefense);
}

bool ACrossvoid2DatkNew::PreformUse(float Phy, float Mag)
{
	if (Dmer == this) //如果打的是自己，就不触发守备
	{
		return false;
	}
	if (IsCure)
	{
		return false; //如果治疗，就不触发守备
	}
	switch (PreformType)
	{
	case EPreformType::Air:
		return false;
	case EPreformType::Defense:
		if (PhyDefense < Phy || MagDefense < Mag) //如果有任何一个盾破了，就直接碎守备
		{
			PhyDefense = MagDefense = Dodges = 0; //全部归零
			DefenseUIUpdate(0, 0, 0); //更新UI
			PreformType = EPreformType::Air; //守备状态归零
			return false; //返回没有使用守备
		}
	//减盾
		PhyDefense -= Phy;
		MagDefense -= Mag;
		DefenseTrigger(); //触发防御动画
		DefenseUIUpdate(PhyDefense, MagDefense, 0); //更新UI
		return true;
	case EPreformType::Attack:
		if (PhyDefense < Phy || MagDefense < Mag) //如果有任何一个盾破了，就直接碎守备
		{
			PhyDefense = MagDefense = Dodges = 0; //全部归零
			DefenseUIUpdate(0, 0, 0); //更新UI
			PreformType = EPreformType::Air; //守备状态归零
			return false; //返回没有使用守备
		}
		//减盾
		PhyDefense -= Phy;
		MagDefense -= Mag;
		DefenseTrigger(); //触发防御动画
		DefenseUIUpdate(PhyDefense, MagDefense, 0); //更新UI
		return true;
	case EPreformType::Dodge:
		if (Dodges <= 0) //还有闪避的次数吗
		{
			PhyDefense = MagDefense = Dodges = 0; //全部归零
			DefenseUIUpdate(0, 0, 0);
			PreformType = EPreformType::Air; //守备状态归零
			return false;
		}
		Dodges--; //消耗一次
		DefenseDodgeTrigger(); //触发闪避动画
		DefenseUIUpdate(PhyDefense, MagDefense, Dodges); //更新UI
		return true;
	default:
		return false;
	}
}

bool ACrossvoid2DatkNew::SkillOverFront(FSkillData2D Skillindex)
{
	//CustomTimeDilation = TargetDilation;//技能开始时的速度
	GetAnimInstance()->StopAllAnimationOverrides(); //先停止所有播放中的动画
	//以后什么时候会加一个死亡判断（现在还不做）
		
	if (PerformSpawn(Skillindex)) //触发守备生成
	{
		GetAnimInstance()->PlayAnimationOverride(ClickSeq[CharShape], "DefaultSlot", 1.0f, 0.0f);
		SkillDrop(); //触发丢弃技能
		return false;
	}
	else//如果正常攻击的话，就开始判断反击
	{
		if (NotBreak)//自己不能被打断
		{
			return true;
		}
		//自己是反击类型吗
		if(PreformType == EPreformType::Attack)
		{
		for (const auto& Target : MapsIN)//遍历所有目标
		{
			//判断目标有效
			if (IsValid(Target))
			{
				//判断要扫描哪些人
				if (TargetTeam ? this->Tags[0] != Target->Tags[0]: this->Tags[0] == Target->Tags[0])
				{
					if(Target->NotBreak)//对方不能被打断
					{
						continue;
					}
					if (Target->PreformType == EPreformType::Attack)//如果对方有一个反击的角色
					{
						DefatkTarget = Target;//设置反击目标
						//计算谁会赢
						DefWin = Anti ? MagDefense > DefatkTarget->MagDefense : PhyDefense > DefatkTarget->PhyDefense;

						//计算反击倍率
						float AtkRate = DefWin ? 1 - Health / MaxHealth + DefatkRateBase: 1 - DefatkTarget->Health / DefatkTarget->MaxHealth + DefatkTarget->DefatkRateBase;
						//计算反击伤害
						float PhyIN = DefWin ? (DefAtkPhy ? CharSelfData.Attack * AtkRate : 0):(DefAtkPhy ? DefatkTarget->CharSelfData.Attack * AtkRate : 0);
						float MagIN = DefWin ? (DefAtkPhy ? 0 : CharSelfData.Attack * AtkRate):(DefAtkPhy ? 0 : DefatkTarget->CharSelfData.Attack * AtkRate);
						//准备变量并计算暴击
						CriticalCalculate(PhyIN, MagIN, DefAtkPhyDM, DefAtkMagDM, DefCriticalis);

						//计算击飞程度
						DefAtkFlyout = AtkRate * 1500; 
					
						//清空双方的守备状态
						PhyDefense = MagDefense = Dodges = 0; //全部归零
						DefenseUIUpdate(0, 0, 0); //更新UI
						PreformType = EPreformType::Air; //守备状态归零
						DefatkTarget->PhyDefense = 0;
						DefatkTarget->MagDefense = 0;
						DefatkTarget->Dodges = 0;
						DefatkTarget->DefenseUIUpdate(0, 0, 0);
						DefatkTarget->PreformType = EPreformType::Air;

						//触发反击
						DefenseAttackTrigger();
						return false;
					}
				}
			}
			
				
			}
		}
		//敌方都没有反击可以触发，正常走
		return true;
	}
}



void ACrossvoid2DatkNew::CameraShow(FVector LocOffset, FRotator RotOffset, float Lenth, float Speed)
{
	SpringArm->TargetOffset = FVector((Face ? -LocOffset.X : LocOffset.X),LocOffset.Y,LocOffset.Z);//直接设置位置偏移
	CameraLerpProgress = Speed;//Lerp的进度
	CameraActiveSpeed = Speed;//获取执行速度
	SpringArm->CameraLagSpeed = CameraActiveSpeed * 8.0f;//同步给摄像机速度
	CameraLengthOffset = Lenth;//摄像机臂长度倍率
	FRotator Rot1 = (Face? RotOffset * -1:RotOffset);
	CameraRotOffset = FRotator(Rot1.Roll, Rot1.Pitch-(Face?-90:90), Rot1.Yaw);
	if (CameraLerpTimer.IsValid())
	{
		UKismetSystemLibrary::K2_UnPauseTimerHandle(this,CameraLerpTimer);//开启计时器s
	}
	else//创建计时器
	{
		CameraLerpTimer = UKismetSystemLibrary::K2_SetTimer(this,FString("CameraShowLerp"),0.01,true,false,0,0);
	}
}

void ACrossvoid2DatkNew::CameraShowLerp()
{
	CameraLerpProgress = CameraLerpProgress-0.01;
	if (CameraLerpProgress <= 0)
	{
		UKismetSystemLibrary::K2_PauseTimerHandle(this,CameraLerpTimer);
	}
	else
	{
		SpringArm->TargetArmLength = UKismetMathLibrary::Lerp(CameraLengthOffset*340, SpringArm->TargetArmLength, UKismetMathLibrary::NormalizeToRange(CameraLerpProgress, 0, CameraActiveSpeed));
		SpringArm->SetRelativeRotation(UKismetMathLibrary::RLerp(CameraRotOffset, SpringArm->GetRelativeRotation(), UKismetMathLibrary::NormalizeToRange(CameraLerpProgress, 0, CameraActiveSpeed), false));
	}
}

TArray<ACrossvoid2DatkNew*> ACrossvoid2DatkNew::EnemyOrSelf(bool IsSelf)
{
	TArray<ACrossvoid2DatkNew*> P1s = {};
	TArray<ACrossvoid2DatkNew*> P2s = {};
	bool SelfIsEnemy = Tags[0] == "2P";
	
	for (const auto& Buffer : MapsIN)
	{
		if (IsValid(Buffer))
		{
			if(Buffer->Tags.Num()<2)
			{
				GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Red, FString::Printf(TEXT("%s_Tag长度小于2，不准搜索"), *CharSelfData.Char1));
				return P1s;
			}
			if (Buffer->Tags[0] == "1P")
			{
				P1s.Add(Buffer);
			}
			else
			{
				P2s.Add(Buffer);
			}
		}
	}
	if(SelfIsEnemy)
	{
		return IsSelf ? P2s : P1s;
	}
	else
	{
		return IsSelf ? P1s : P2s;
	}
}

void ACrossvoid2DatkNew::ExtraAttack_Implementation()
{
}

void ACrossvoid2DatkNew::DefenseAttackTrigger_Implementation()
{
	//移动到反击的角色
	MoveTo("DefAtk","None",50,false,DefatkTarget);
}

void ACrossvoid2DatkNew::CharRealInit_Implementation()
{
}

void ACrossvoid2DatkNew::HealSuccess_Implementation(ACrossvoid2DatkNew* DmWho)
{
}

void ACrossvoid2DatkNew::HitSuccess_Implementation(ACrossvoid2DatkNew* DmWho)
{
}


void ACrossvoid2DatkNew::EndSubAction_Implementation()
{
}


void ACrossvoid2DatkNew::SK5SubStart_Implementation(ACrossvoid2DatkNew* Parent,int SubIndex12)
{
}

void ACrossvoid2DatkNew::SK4SubStart_Implementation(ACrossvoid2DatkNew* Parent)
{
}

void ACrossvoid2DatkNew::Death_Implementation()
{
}

void ACrossvoid2DatkNew::StandUp1_Implementation()
{
	//设置自己位置到默认位置
	SetActorLocation(DefaultPosition->GetActorLocation());
	//摄像机归位
	CameraShow(FVector(0, 0, 0), FRotator(0, 0, 0), 1, 1);
}

void ACrossvoid2DatkNew::DefenseDodgeTrigger_Implementation()
{
	//交给蓝图
}

void ACrossvoid2DatkNew::HitFeel_Implementation()
{
}

void ACrossvoid2DatkNew::FlyUp_Implementation(FIntPoint FlyXZ, bool Defense)
{
	GetCharacterMovement()->GravityScale = 3;//受击后恢复重力
	//弹飞
	this->LaunchCharacter(FVector((Face ? FlyXZ.X : -FlyXZ.X), 0, FlyXZ.Y), true, true);
	if(NotBreak)//如果不能被打断直接返回
	{
		return;
	}
	if (Defense) //如果是防御中的击飞，就不触发受击
	{
		return;
	}
	if (FlyXZ.Y != 0) //如果有击飞高度
	{
		this->GetAnimationComponent()->GetAnimInstance()->JumpToNode("Fly"); //触发起飞动画事件
	}
	else //平地就播放受击动画
	{
		GetAnimInstance()->PlayAnimationOverride(OnDamageSeq[CharShape], "DefaultSlot", 1.0f, 0.0f);
	}
}

void ACrossvoid2DatkNew::DefenseTrigger_Implementation()
{
	if (SelfActive == false) //自己是被打的一方
	{
		//播放防御动画
		GetAnimInstance()->PlayAnimationOverride(DefSeq[CharShape], "DefaultSlot", 1.0f, 0.0f);
		FDefDefenseTriggerOn.Broadcast(); //触发防御委托
		//受击位移
		FlyUp(FIntPoint(200, 0), true);
		Dmer->HitFeel(); //减帧
	}
}

void ACrossvoid2DatkNew::DefenseUIUpdate_Implementation(int PhyIn, int MagIn, int Dodge)
{
}


void ACrossvoid2DatkNew::DamageTarget_Implementation(float Phy, float Mag, FIntPoint FlyXZ, bool DmEnd, bool Critical,
                                                     ACrossvoid2DatkNew* DmerIn)
{
	Dmer = DmerIn;
}


void ACrossvoid2DatkNew::OtherRoundEnd_Implementation()
{
	//回合结束保险栓
	SelfActive = false;//我不在行动
	NotBreak = false;//去除不可打断
	CustomTimeDilation = 1.0f;//速度回到1
	DefatkTarget = nullptr;//清空反击目标
	FinalTargets = {};//清空被覆盖的最终目标
	ExtraTargets = {};//清空额外攻击容量
}

void ACrossvoid2DatkNew::OnMoveEnd_Implementation()
{
}


void ACrossvoid2DatkNew::MoveTo_Implementation(FName ActionCord, FName Move, float Offset, bool Teleport,ACrossvoid2DatkNew* JustTarget)
{
	Indicate = ActionCord.ToString(); //设置指示器
}

void ACrossvoid2DatkNew::IndCommd_Implementation()
{
}


void ACrossvoid2DatkNew::SK5StartMove_Implementation()
{
}

void ACrossvoid2DatkNew::SK4StartMove_Implementation()
{
}

void ACrossvoid2DatkNew::SK3StartMove_Implementation()
{
}

void ACrossvoid2DatkNew::SK2StartMove_Implementation()
{
}

void ACrossvoid2DatkNew::SK1StartMove_Implementation()
{
}


void ACrossvoid2DatkNew::EndSelfTurn_Implementation()
{
	// 设置角色的旋转角度
	K2_SetActorRotation(FRotator(0, (Face ? 180.0f : 0.0f), 0), false);
	NotBreak = false;
	CustomTimeDilation = 1.0f;
	FActionEnd.Broadcast(); //触发结束自身行动的委托
}

void ACrossvoid2DatkNew::SkillDrop_Implementation()
{
}

void ACrossvoid2DatkNew::DefenseUIInitialize_Implementation(EPreformType Type, int MaxPhyIN, int MaxMagIN)
{
}

void ACrossvoid2DatkNew::IsVictory_Implementation(bool IsWin)
{
}


void ACrossvoid2DatkNew::GetCharIcon_Implementation(bool Is1P, UTexture2D*& Main, UTexture2D*& Sub)
{
}


void ACrossvoid2DatkNew::AutoPriorityChange_Implementation(int SkSel, int NewPriority)
{
}


void ACrossvoid2DatkNew::SK5Start_Implementation()
{
	if (SkillOverFront(SkillSlot5))
	{
		SK5StartMove(); //触发移动
	}
}

void ACrossvoid2DatkNew::SK4Start_Implementation()
{
	if (SkillOverFront(SkillSlot4))
	{
		SK4StartMove(); //触发移动
	}
}

void ACrossvoid2DatkNew::SK3Start_Implementation()
{
	if (SkillOverFront(SkillSlot3))
	{
		SK3StartMove(); //触发移动
	}
}

void ACrossvoid2DatkNew::SK2Start_Implementation()
{
	if (SkillOverFront(SkillSlot2))
	{
		SK2StartMove(); //触发移动
	}
}

void ACrossvoid2DatkNew::SK1Start_Implementation()
{
	if (SkillOverFront(SkillSlot1))
	{
		SK1StartMove(); //触发移动
	}
}

void ACrossvoid2DatkNew::GetCharSkill_Implementation(int Loc, FSkillData2D& Infor, int& Shape)
{
}


void ACrossvoid2DatkNew::CharActionStart_Implementation()
{
	CheckSelfData();//检查自身数据
	FActionStart.Broadcast(); //触发回合开始时的委托
	DefatkTarget = nullptr;//清空反击目标
	SelfActive = true;//我在行动
	//播放站街动画
	GetAnimInstance()->PlayAnimationOverride(ClickSeq[CharShape], "DefaultSlot", 1.0f, 0.0f);
}

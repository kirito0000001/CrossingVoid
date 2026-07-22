#pragma once
#include "CoreMinimal.h"
#include "PaperZDCharacter.h"
#include "FUCRealize.h"
#include "Components/WidgetComponent.h"//对话组件
#include "DreamGameplayTask/Public/Classes/DreamTaskComponent.h"
#include "PaperZD/Public/AnimSequences/PaperZDAnimSequence_Flipbook.h"//Paper的动画序列
#include "Crossvoid2DatkNew.generated.h"


class UMediaSource;
class UCameraComponent;
class USpringArmComponent;
//2D守备类型
UENUM(BlueprintType)
enum class EPreformType: uint8
{
	Air = 0 UMETA(DisplayName="空"),
	Defense = 1 UMETA(DisplayName="防御"),
	Attack = 2 UMETA(DisplayName="反击"),
	Dodge = 3 UMETA(DisplayName="闪避"),
};


class UDreamTaskType;
//物理异能技能等级对应倍率
USTRUCT(BlueprintType)
struct FRateSubPhymag
{
	GENERATED_BODY()
	/* 物理技能等级对应倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<int, float> PhyLVrate = {{1, 1.00f}, {2, 1.00f}, {3, 1.00f}, {4, 1.00f}, {5, 1.00f}};
	/* 异能技能等级对应倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<int, float> MagLVrate = {{1, 1.00f}, {2, 1.00f}, {3, 1.00f}, {4, 1.00f}, {5, 1.00f}};
};


USTRUCT(Blueprintable)
struct FSkillData2D
{
	GENERATED_BODY()
	//技能图标
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能图标")
	TArray<UTexture2D*> Icon = {};
	//技能名字
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能名字")
	TArray<FText> Name = {};
	//技能介绍
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能介绍")
	TArray<FText> Description = {};
	//技能Point消耗
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能Point消耗")
	TArray<int> PointCost = {};
	//技能等级倍率
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能等级倍率")
	TArray<FRateSubPhymag> SkillRate = {};
	//技能自动优先级
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能自动优先级")
	TArray<int> AutoPriority = {};
	//技能状态
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能状态")
	TArray<E2DSkillType> SkillState = {};
	//守备技能
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="守备技能")
	TArray<EPreformType> PreformType = {};
	//守备技能数值
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="守备技能数值")
	TArray<float> PreSkillValue = {};
	//技能的真名
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能的真名")
	TArray<FText> SkillName = {};
	//攻击容量
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Inventory|Item|Skill", DisplayName="技能攻击容量")
	TArray<int> AttackCapacity = {};
};

USTRUCT(BlueprintType)
struct FINGameProData
{
	GENERATED_BODY()
	//主站角色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="主战角色")
	FString Char1 = {};
	//助战角色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="助战角色")
	FString Char2 = {};
	//助战角色类
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="助战角色类")
	TSubclassOf<APaperZDCharacter> Char2Class;
	//速度
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="速度")
	int Speed = 0;
	//血量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="血量")
	int Health = 0;
	//攻击力
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="攻击力")
	int Attack = 0;
	//物理防御
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="物理防御")
	float PhyDefense = 0.0f;
	//异能防御
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="异能防御")
	float MagDefense = 0.0f;
	//暴击率
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="暴击率")
	float Critical = 0.0f;
	//暴击伤害
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="暴击伤害")
	float CriticalC = 0.0f;
	//技能等级对应
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="技能等级对应")
	TArray<int> SkillLevel = {};
	//被动介绍合集
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="被动介绍合集")
	TArray<FText> SkillDescription = {};
};

USTRUCT(BlueprintType)
struct F12SkData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="连携技索引")
	int Skill12Index = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="连携技能数据")
	FSkillData2D SkillSlot5Data = {};
};

USTRUCT(BlueprintType)
struct F12SkData2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="对应护援人物")
	TMap<FString, F12SkData> SubCharName = {};
};



UCLASS()
class CROSSMAINDATA_API ACrossvoid2DatkNew : public APaperZDCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACrossvoid2DatkNew();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="Buff组件")
	UDreamTaskComponent* BuffComponent; //绑定Buff组件
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="气泡对话组件")
	UWidgetComponent* DialogueComponent;//绑定气泡对话组件

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="弹簧臂组件")
	USpringArmComponent* SpringArm;//绑定弹簧臂组件

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="相机组件")
	UCameraComponent* Camera;

	

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBuffUpdateDelegaet); //制作BUff通用的委托
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWhoAttackDelegaet,ACrossvoid2DatkNew*,Dmer);//制作被谁打
	//DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FDefAtkDelegaet,bool,SelfWin,float,PhyDM,float,MagDM,float,Dmflyout);

public: //变量
	//目标形态
	//用于检测配置是否有效，以防出现访问空引起的崩溃
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|System", DisplayName="目标形态上限")
	int TargetShape = 1;
	
	//自己的默认位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|System", DisplayName="自己的默认位置")
	AActor* DefaultPosition = nullptr;

	//自己的数据
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|System", DisplayName="自己的数据")
	FINGameProData CharSelfData = {};

	//技能槽1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|Skill", DisplayName="一技能")
	FSkillData2D SkillSlot1 = {};

	//技能槽2
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|Skill", DisplayName="二技能")
	FSkillData2D SkillSlot2 = {};

	//技能槽3
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|Skill", DisplayName="大招")
	FSkillData2D SkillSlot3 = {};

	//技能槽4
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|Skill", DisplayName="护援技")
	FSkillData2D SkillSlot4 = {};

	//技能槽5
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|Skill", DisplayName="连携技")
	FSkillData2D SkillSlot5 = {};

	//人物形态
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing|Data", DisplayName="人物形态")
	int CharShape = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="角色合集")
	TArray<ACrossvoid2DatkNew*> MapsIN;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="1P主站头像")
	TSoftObjectPtr<UTexture2D> Icon1P = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="2P主站头像")
	TSoftObjectPtr<UTexture2D> Icon2P = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="抗性类别(True是异能)")
	bool Anti = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="当前血量")
	int Health = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="最大血量上限")
	int MaxHealth = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色点击动作")
	TArray<UPaperZDAnimSequence*> ClickSeq = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色闪避动作")
	TArray<UPaperZDAnimSequence*> DodgeSeq = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色防御动作")
	TArray<UPaperZDAnimSequence*> DefSeq = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色反击动作")
	TArray<UPaperZDAnimSequence*> DefAttackSeq = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色受伤动作")
	TArray<UPaperZDAnimSequence*> OnDamageSeq = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="角色默认朝向")
	bool Face = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="角色移动中")
	bool Moveing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="当前守备状态")
	EPreformType PreformType = EPreformType::Air;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="守备闪避次数")
	int Dodges = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="守备物理防御值")
	int PhyDefense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="守备异能防御值")
	int MagDefense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="施暴者")
	TObjectPtr<ACrossvoid2DatkNew> Dmer = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="我在行动")
	bool SelfActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="关闭暴击")
	bool CriticalOFF = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="能否死亡")
	bool CanDie = true;

	//反击系列变量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="使用什么属性来反击")
	bool DefAtkPhy = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="反击赢了吗")
	bool DefWin = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="物理反击伤害")
	float DefAtkPhyDM = 0.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="异能反击伤害")
	float DefAtkMagDM = 0.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="反击触发暴击")
	bool DefCriticalis = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="反击飞出距离")
	float DefAtkFlyout = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="我是护援人物")
	bool Suber = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="不能被打断")
	bool NotBreak = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="治疗类伤害")
	bool IsCure = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="大招动画")
	TArray<UMediaSource*> KoAnimSource = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="连携技能索引")
	int Index12 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="行动指示器")
	FString Indicate = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|Data", DisplayName="目标队伍")
	bool TargetTeam = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色胜利动作")
	TArray<UPaperZDAnimSequence*> VictorAnim = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色失败动作")
	TArray<UPaperZDAnimSequence*> DefeatedAnim = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|ONset", DisplayName="角色死亡动作")
	TArray<UPaperZDAnimSequence*> DeathAnim = {};

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="角色目标速度")
	//float TargetDilation = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="移动计时器柄")
	FTimerHandle MoveTimer = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="反击目标")
	TObjectPtr<ACrossvoid2DatkNew> DefatkTarget = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="反击倍率")
	float DefatkRateBase = 0.8f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="额外攻击容量")
	TArray<FName> ExtraTargets = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="最终攻击目标")
	TArray<FName> FinalTargets = {};
	
	//摄像机演出相关
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="摄像机偏移目标旋转")
	FRotator CameraRotOffset = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="摄像机偏移目标长度")
	float CameraLengthOffset = 1.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="摄像机偏移执行速度")
	float CameraActiveSpeed = 1.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="摄像机Lerp句柄")
	FTimerHandle CameraLerpTimer = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category ="Crossing|System", DisplayName="摄像机Lerp进度")
	float CameraLerpProgress = {};

public: //委托
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="回合开始时")
	FBuffUpdateDelegaet FActionStart;//CharActionStart_Implementation触发

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="回合结束时")
	FBuffUpdateDelegaet FActionEnd;//EndSelfTurn_Implementation触发

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="技能开始时")
	FBuffUpdateDelegaet FSkillStart;//技能执行的触发

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="技能结束时")
	FBuffUpdateDelegaet FSkillEnd;//C里不写，蓝图子类去写

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="伤害打出时")
	FBuffUpdateDelegaet FDamageStart;//C里不写，蓝图父类里写了

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="受到伤害时")
	FWhoAttackDelegaet FOnDamage;//C里不写，蓝图父类里写了

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="委托成功打出伤害时")
	FBuffUpdateDelegaet FOnDamageSuccess;//C里不写，蓝图父类里写了
	
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="受到治疗时")
	FBuffUpdateDelegaet FOnHealth;//C里不写，蓝图父类里写了

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="成功治疗时")
	FBuffUpdateDelegaet FOnHealthSuccess;//C里不写，蓝图父类里写了

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="防御守备触发时")
	FBuffUpdateDelegaet FDefDefenseTriggerOn;//DefenseTrigger_Implementation，守备动作触发

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="反击守备触发时")
	FBuffUpdateDelegaet FAtkDefenseTriggerOn;//反击减帧加速触发

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="闪避守备触发时")
	FBuffUpdateDelegaet FDogDefenseTriggerOn;//C里不写，蓝图父类里写了

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="委托死亡时")
	FBuffUpdateDelegaet FOnDeath;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="成功击杀时")
	FBuffUpdateDelegaet FOnKill;//C里不写，对面死亡时触发

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Crossing|Delegaet", DisplayName="技能丢弃时")
	FBuffUpdateDelegaet FSkillDiscard;//C里不写，蓝图父类里写了

public: //函数
	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="检查自身数据")
	void CheckSelfData();
	
	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="生成守备数值")
	bool PerformSpawn(FSkillData2D SkillPer);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="寻找目标_单个")
	ACrossvoid2DatkNew* FindTarget(FName Who);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="寻找目标_多个")
	TArray<ACrossvoid2DatkNew*> FindTargets(TArray<FName> Who);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="自我位移")
	void MoveTikFuc(float InX, float InZ, bool OverRide);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="1基础倍率乘算")
	void AtkCalculate(int SkSel, float& Phy1, float& Mag1);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="3暴击计算函数")
	void CriticalCalculate(float Phy2IN, float Mag2IN, float& Phy3Out, float& Mag3Out, bool& IsCritical);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="5防御数值计算")
	void DefenseCalculate(float Phy2IN, float Mag2IN, float& Phy3Out, float& Mag3Out);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="触发守备防御")
	bool PreformUse(float Phy,  float Mag);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="技能释放前置")
	bool SkillOverFront(FSkillData2D Skillindex);
	
	/**
	 * 触发摄像机的位置移动和缩放旋转
	 * @param LocOffset 新的位置
	 * @param RotOffset 新的旋转
	 * @param Lenth 相机臂长度
	 * @param Speed 插值的速度
	 */
	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="摄像机演出")
	void CameraShow(FVector LocOffset, FRotator RotOffset, float Lenth = 1.0f, float Speed = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="摄像机演出Lerp核心")
	void CameraShowLerp();

	UFUNCTION(BlueprintCallable, Category = "Base|Fuc", DisplayName="获取一方角色")
	TArray<ACrossvoid2DatkNew*> EnemyOrSelf(bool IsSelf);

public: //自定义事件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="角色开始行动")
	void CharActionStart();
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="角色初始化_构造")
	void CharRealInit();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="获取技能")
	void GetCharSkill(int Loc, FSkillData2D& Infor, int& Shape);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="一技能开始")
	void SK1Start();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="一技能开始执行")
	void SK1StartMove();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="二技能开始")
	void SK2Start();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="二技能开始执行")
	void SK2StartMove();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="大招开始")
	void SK3Start();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="大招开始执行")
	void SK3StartMove();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="护援技开始")
	void SK4Start();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="护援技开始执行")
	void SK4StartMove();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="Sub护援技执行")
	void SK4SubStart(ACrossvoid2DatkNew* Parent);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="连携技开始")
	void SK5Start();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="连携技开始执行")
	void SK5StartMove();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="Sub连携技执行")
	void SK5SubStart(ACrossvoid2DatkNew* Parent,int SubIndex12);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="额外追击执行")
	void ExtraAttack();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="获取角色头像")
	void GetCharIcon(bool Is1P, UTexture2D* & Main, UTexture2D* & Sub);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="技能优先级更改")
	void AutoPriorityChange(int SkSel, int NewPriority);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="胜利结算")
	void IsVictory(bool IsWin);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="触发守备UI")
	void DefenseUIInitialize(EPreformType Type, int MaxPhyIN, int MaxMagIN);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="丢弃技能后")
	void SkillDrop();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="结束自己的回合")
	void EndSelfTurn();

	/*
	 * 角色的基础移动节点
	 * @param IsEmemy 是敌人
	 * @param Move 移动到的位置
	 * @param Offset 移动后的偏移量
	 * @param Teleport 是否瞬移
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="触发角色移动")
	void MoveTo(FName ActionCord, FName Move, float Offset, bool Teleport,ACrossvoid2DatkNew* JustTarget);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="行动指示器")
	void IndCommd();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="移动结束—触发指示器")
	void OnMoveEnd();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="回合结束保险")
	void OtherRoundEnd();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="打击目标")
	void DamageTarget(float Phy, float Mag, FIntPoint FlyXZ, bool DmEnd, bool Critical, ACrossvoid2DatkNew* DmerIn);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="守备UI更新")
	void DefenseUIUpdate(int PhyIn, int MagIn,int Dodge);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="守备_防御触发")
	void DefenseTrigger();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="守备_反击触发")
	void DefenseAttackTrigger();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="守备_闪避触发")
	void DefenseDodgeTrigger();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="受击弹飞")
	void FlyUp(FIntPoint FlyXZ,bool Defense);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="减帧")
	void HitFeel();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="触发站立")
	void StandUp1();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="死亡时")
	void Death();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="Sub结束行动时")
	void EndSubAction();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="成功打出伤害时")
	void HitSuccess(ACrossvoid2DatkNew* DmWho);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, DisplayName="成功治疗时")
	void HealSuccess(ACrossvoid2DatkNew* DmWho);
};

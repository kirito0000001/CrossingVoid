#pragma once
#include "CoreMinimal.h"
#include "CP_Inventory.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FUCRealize.generated.h"


class UNiagaraSystem;
//2D技能状态
UENUM(BlueprintType)
enum class E2DSkillType: uint8
{
	Air = 0 UMETA(DisplayName="空"),
	Normal = 1 UMETA(DisplayName="常规"),
	Disable = 2 UMETA(DisplayName="禁用"),
	Abandon = 3 UMETA(DisplayName="舍弃"),
};

//AST加载，卸载方案
UENUM(BlueprintType)
enum class EAST: uint8
{
	MainMap = 0 UMETA(DisplayName="主界面", ToolTip="主界面相关资源"),
	Plot = 1 UMETA(DisplayName="Plot关卡", ToolTip="Plot战斗资源"),
	PlotGAL = 2 UMETA(DisplayName="Plot剧情", ToolTip="Plot战斗资源，剧情资源"),
	PlotYC = 3 UMETA(DisplayName="Plot养成", ToolTip="Plot战斗资源，剧情资源，养成资源"),
};

//PVP返回方案
UENUM(BlueprintType)
enum class EBackPVPType: uint8
{
	MainM = 0 UMETA(DisplayName="主界面"),
	YC = 1 UMETA(DisplayName="养成"),
};

//养成指示器返回方案
UENUM(BlueprintType)
enum class EYCFUCTriggerType: uint8
{
	Default = 0 UMETA(DisplayName="默认", ToolTip="触发全名对应的函数"),
	AbAdd = 1 UMETA(DisplayName="属性增加", ToolTip="触发属性增加“AbAdd_Atk/HP/C/CC/Phy/Mag_114”"),
	ExabAdd = 2 UMETA(DisplayName="额外属性增加", ToolTip="触发额外属性增加“ExabAdd_属性名称”"),
};

//主界面选项字符串方案
UENUM(BlueprintType)
enum class EMainUIStartType: uint8
{
	Default = 0 UMETA(DisplayName="无操作", ToolTip="正常加载游戏"),
	UIopen = 1 UMETA(DisplayName="打开UI", ToolTip="根据字符串找对应的UI打开"),
};

//抽卡分类
UENUM(BlueprintType)
enum class ETransCardType: uint8
{
	Normal = 0 UMETA(DisplayName="常规"),
	UP = 1 UMETA(DisplayName="概率UP"),
	Wish = 2 UMETA(DisplayName="祈愿"),
};

//特写类型
UENUM(BlueprintType)
enum class ESkillFeature: uint8
{
	KO = 0 UMETA(DisplayName="大招"),
	Sub = 1 UMETA(DisplayName="护援技"),
	SKLink = 2 UMETA(DisplayName="连携技"),
};

//镜头震动等级
UENUM(BlueprintType)
enum class EShakeLevel: uint8
{
	Low = 0 UMETA(DisplayName="低"),
	Middle = 1 UMETA(DisplayName="中"),
	High = 2 UMETA(DisplayName="高"),
	Earthquake = 3 UMETA(DisplayName="地震")
};

//后处理类型
UENUM(BlueprintType)
enum class EPostProcessType: uint8
{
	T0Black = 0 UMETA(DisplayName="黑白滤镜"),
	T1DimLight = 1 UMETA(DisplayName="环境压暗"),
	T2ActorBlack = 2 UMETA(DisplayName="仅Actor黑"),
};

//当前加载类型
UENUM(BlueprintType)
enum class ELoadedNowType: uint8
{
	Uasset = 0 UMETA(DisplayName="资产"),
	Widget = 1 UMETA(DisplayName="控件"),
	Actor = 2 UMETA(DisplayName="Actor"),
};

//好友信息结构体
USTRUCT(BlueprintType)
struct FPlayerFriendInfor
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友GUID")
	FGuid FriendGUID = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友名称")
	FName Name = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友等级")
	int Level = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友所属公会GUID")
	FGuid AssociationGUID = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友签名")
	FName Texts = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友头像")
	int Icon = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友头像框")
	int IconFrame = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友助战角色")
	TArray<FItemInformation> SupportChars = {};
};

// 公告结构体
USTRUCT(BlueprintType)
struct FNoticetexts : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公告标题")
	FString TextTitle = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公告内容")
	FString Texts = {};
};

// 加载列表结构体
USTRUCT(BlueprintType)
struct FLoadedAssetListNeed : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="资产列表")
	TArray<TSoftObjectPtr<UObject>> LoadedList = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="控件类型列表")
	TArray<TSoftClassPtr<UUserWidget>> WidList = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="Actor类型列表")
	TArray<TSoftClassPtr<AActor>> ActorList = {};
};

// 用户基本数据结构（之后再不改变）
USTRUCT(BlueprintType)
struct FNormalUserInfor
{
	GENERATED_BODY()
	//玩家名字
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="玩家名字")
	FName PlayerName = {};
	//玩家等级
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="玩家等级")
	int PlayerLevel = 0;
	//等级经验值
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="等级经验值")
	int LevelExp = 0;
	//迷子
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="迷子")
	int Mizi = 0;
	//彩色迷子
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="彩色迷子")
	int PlusMizi = 0;
	//硬币
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="硬币")
	int Coin = 0;
	//能量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="能量")
	int Energy = 0;
	//能量充满时间
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="能量充满时间")
	FDateTime EnergyFillTime = {};
	//公会GUID
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会GUID")
	FGuid AssociationGUID = {};
	//普通池转境次数
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="普通池转境次数")
	int TRS1Count = 0;
	//UP池转境次数
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="UP池转境次数")
	int TRS2Count = 0;
	//祈愿池转境次数
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="祈愿池转境次数")
	int TRS3Count = 0;

	FNormalUserInfor()
	{
		PlayerName = "NoneNameTOResetDaTa";
		PlayerLevel = 0;
		LevelExp = 0;
		Mizi = 0;
		PlusMizi = 0;
		Coin = 0;
		Energy = 0;
		EnergyFillTime = FDateTime::Now();
		AssociationGUID = FGuid();
		TRS1Count = 0;
		TRS2Count = 0;
		TRS3Count = 0;
	}
};

// 用户个性化结构
USTRUCT(BlueprintType)
struct FIndiUserInfor
{
	GENERATED_BODY()
	//玩家签名
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="玩家签名")
	FName PlayerSign = {};
	//玩家头像
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="玩家头像")
	int PlayerIcon = 0;
	//玩家头像框
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="玩家头像框")
	int PlayerIconFrame = 0;
	//自定义背景音乐
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="自定义背景音乐")
	TArray<int> CustomMusic = {0};
	//自定义背景图
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="自定义背景图")
	TArray<int> CustomBackground = {0};
	//好友列表
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友列表")
	TArray<FGuid> FriendList = {};
	//队伍名称
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="队伍名称")
	TArray<FName> TeamNames = {};
	//助战角色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="助战角色")
	TArray<FItemInformation> SupportChars = {};
	//好友申请列表
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友申请列表")
	TArray<FGuid> FriendApplyList = {};
	//上次登录时间
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="上次登录时间")
	FDateTime LoginTime = {};

	FIndiUserInfor()
	{
		CustomMusic = {};
		CustomBackground = {};
		PlayerSign = "当最强烈的祈愿相交之时，零境便会诞生至此。";
		PlayerIconFrame = 0;
		PlayerIcon = 0;
		FriendList = {};
		TeamNames = {};
		SupportChars = {};
		FriendApplyList = {};
		LoginTime = FDateTime::Now();
	}
};

// 我的世界结构
USTRUCT(BlueprintType)
struct FMinecraftUserInfor
{
	GENERATED_BODY()
	//建材Materials
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="建材")
	int Materialsbase = 0;
	//食物
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="食物")
	int Food = 0;
	//电力
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="电力")
	int Electricity = 0;
	//药物
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="药物")
	int Drugs = 0;
	//氛围值
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="氛围值")
	int Atmosphere = 0;
	//地盘开放程度（每一片位置都对应一个索引位置，布尔判断有没有开放）
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="地盘开放程度")
	TArray<bool> OpenDegree = {};
	//建筑数据占位符
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="建筑数据占位符")
	//TArray<单个建筑的结构体> == 这里包含当前建筑的位置【整个地图的坐标，以原点为0，0】
	//包含建筑的工作进度，建筑进度，建筑等级，建筑图标，建模索引，建筑图标，里面工作的角色，每小时产出，每小时消耗，包含的UI控件

	FMinecraftUserInfor()
	{
		Materialsbase = 0;
		Food = 0;
		Electricity = 0;
		Drugs = 0;
		Atmosphere = 0;
		OpenDegree = {};
	}
};

//公会数据GM记录
USTRUCT(BlueprintType)
struct FAssociationData
{
	GENERATED_BODY()
	//公会名称
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会名称")
	FName AssociationName = {};
	//公会头像
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会头像")
	int AssociationIcon = 0;
	//拥有的成员数
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="拥有的成员数")
	TArray<FGuid> MemberCount = {};
	//公会描述
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会描述")
	FName AssociationDescription = {};
	//公会会长
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会会长")
	FGuid AssociationChairman = {};
	//公会等级
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会等级")
	int AssociationLevel = 0;
	//公会经验值
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会经验值")
	int AssociationExp = 0;
	//公会公告
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="公会公告")
	FName AssociationNotice = {};

	FAssociationData()
	{
		AssociationName = "None";
		AssociationIcon = 0;
		MemberCount = {};
		AssociationDescription = "Initialize";
		AssociationChairman = {};
		AssociationLevel = 0;
		AssociationExp = 0;
		AssociationNotice = "Initialize";
	}
};

//存档用数据合集
USTRUCT(BlueprintType)
struct FPlayerMainData
{
	GENERATED_BODY()
	//用户基本数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="用户基本数据")
	FNormalUserInfor UserInfor = {};
	//用户个性化数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="用户个性化数据")
	FIndiUserInfor UserIndi = {};
	//用户库存数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="用户库存数据")
	FInventorySaveData UserData = {};
	//用户Minecraft数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="用户Minecraft数据")
	FMinecraftUserInfor MinecraftUserInfor = {};

	FPlayerMainData()
	{
		UserInfor = {};
		UserIndi = {};
		UserData = {};
		MinecraftUserInfor = {};
	}
};

//聊天信息合集
USTRUCT(BlueprintType)
struct FMessageChatData
{
	GENERATED_BODY()
	//聊天内容
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天内容")
	FString ChatContent = {};
	//聊天人名字
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天人名字")
	FString ChatName = {};
	//聊天人头像
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天人头像")
	int ChatIcon = 0;
	//聊天人头像框
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天人头像框")
	int ChatIconFrame = 0;
	//聊天人文本颜色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天人文本颜色")
	FSlateColor ChatColor = {};
	//聊天表情
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天表情")
	int ChatEmoji = 0;
	//聊天信息的时间
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天信息时间")
	FDateTime ChatTime = {};

	FMessageChatData()
	{
		ChatContent = "Initialize";
		ChatName = "Initialize";
		ChatIcon = 0;
		ChatIconFrame = 0;
		ChatColor = {};
		ChatEmoji = 0;
		ChatTime = {};
	}
};


//小分支内确定聊天室
USTRUCT(BlueprintType)
struct FChatRoomFind
{
	GENERATED_BODY()
	//被联系人的对应索引
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="被联系人的对应索引")
	TMap<FGuid, int> PlayerChatRoomIndex = {};
};

//聊天室内容
USTRUCT(BlueprintType)
struct FChatRoomChats
{
	GENERATED_BODY()
	//当前聊天室成员
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="当前聊天室成员")
	TArray<FGuid> PlayerChatRoomHave = {};
	//当前聊天室内容合集
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="聊天室内容")
	TArray<FMessageChatData> PlayerChatRoomChats = {};
};

//GM_TemporaryInfor储存结构体
//临时数据，会在服务器关闭时清空
USTRUCT(BlueprintType)
struct FTemporaryInfor
{
	GENERATED_BODY()
	//BUG反馈列表
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="BUG列表")
	TArray<FName> BUGList = {};
	//世界聊天信息
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="世界聊天信息")
	TArray<FMessageChatData> WorldChats = {};
	//双GUID确定聊天室（以后群组可能按名字来，也可能单独给聊天室一个GUID（和公会一样））
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友私聊索引表")
	TMap<FGuid, FChatRoomFind> FriendChatRoomIndexMaps = {};
	//好友私聊对应聊天室
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="好友私聊对应聊天室")
	TArray<FChatRoomChats> FriendChatRoom = {};
};


//每个角色对应的立绘信息
USTRUCT(BlueprintType)
struct FLustrationStruct : public FTableRowBase
{
	GENERATED_BODY()
	/* 名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lustration", DisplayName="名称")
	FText Name = {};
	/* 代表色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lustration", DisplayName="代表色")
	FLinearColor Color = {};
	/* 服装 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lustration", DisplayName="服装")
	TArray<TSoftObjectPtr<UTexture2D>> Cloth = {};
	/* 表情 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lustration", DisplayName="表情")
	TArray<TSoftObjectPtr<UTexture2D>> Face = {};
	/* 装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lustration", DisplayName="装饰")
	TArray<TSoftObjectPtr<UTexture2D>> Adorn = {};
	/* 特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lustration", DisplayName="特效")
	TArray<TSoftObjectPtr<UMaterialInstance>> Vfx = {};

	FLustrationStruct()
	{
		Name = {};
		Color = {};
		Cloth = {};
		Face = {};
		Adorn = {};
		Vfx = {};
	}
};

//文本的结构体
USTRUCT(BlueprintType)
struct FStoryStruct : public FTableRowBase
{
	GENERATED_BODY()

	/* 文本 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="文本")
	FString Tesxt = {};
	/* 自定义函数或者事件 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="自定义事件")
	FString Custom = {};
	/* 背景图 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="背景图")
	int BGindex = 0;
	/* 背景音乐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="背景音乐")
	int BGM = 0;
	/* 环境音 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="环境音")
	int Scene = 0;
	/* 说话人|名字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="说话人|名字")
	FString TalkChar = {};
	/* 说话人|身体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="说话人|身体")
	int TalkBody = 0;
	/* 说话人|面部 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="说话人|面部")
	int TalkFace = 0;
	/* 说话人|装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="说话人|装饰")
	int TalkAdorn = 0;
	/* 说话人|特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="说话人|特效")
	int TalkVfx = 0;
	/* 人物1|名字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物1|名字")
	FString Chara1 = {};
	/* 人物1|身体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物1|身体")
	int Body1 = 0;
	/* 人物1|面部 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物1|面部")
	int Face1 = 0;
	/* 人物1|装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物1|装饰")
	int Adorn1 = 0;
	/* 人物1|特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物1|特效")
	int Vfx1 = 0;
	/* 人物2|名字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物2|名字")
	FString Chara2 = {};
	/* 人物2|身体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物2|身体")
	int Body2 = 0;
	/* 人物2|面部 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物2|面部")
	int Face2 = 0;
	/* 人物2|装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物2|装饰")
	int Adorn2 = 0;
	/* 人物2|特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物2|特效")
	int Vfx2 = 0;
	/* 人物3|名字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物3|名字")
	FString Chara3 = {};
	/* 人物3|身体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物3|身体")
	int Body3 = 0;
	/* 人物3|面部 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物3|面部")
	int Face3 = 0;
	/* 人物3|装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物3|装饰")
	int Adorn3 = 0;
	/* 人物3|特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物3|特效")
	int Vfx3 = 0;
	/* 人物4|名字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物4|名字")
	FString Chara4 = {};
	/* 人物4|身体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物4|身体")
	int Body4 = 0;
	/* 人物4|面部 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物4|面部")
	int Face4 = 0;
	/* 人物4|装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物4|装饰")
	int Adorn4 = 0;
	/* 人物4|特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物4|特效")
	int Vfx4 = 0;
	/* 人物5|名字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物5|名字")
	FString Chara5 = {};
	/* 人物5|身体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物5|身体")
	int Body5 = 0;
	/* 人物5|面部 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物5|面部")
	int Face5 = 0;
	/* 人物5|装饰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物5|装饰")
	int Adorn5 = 0;
	/* 人物5|特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GALDataTable", DisplayName="人物5|特效")
	int Vfx5 = 0;
};

//2D回合制人物数据
USTRUCT(BlueprintType)
struct F2DAtkDataStruct
{
	GENERATED_BODY()
	//主站生成人物
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="主战生成人物")
	TSoftClassPtr<ACrossvoid2DatkNew> GenMain = {};
	//护援生成人物
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="护援生成人物")
	TSoftClassPtr<ACrossvoid2DatkNew> GenSub = {};
	//主站角色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="主战角色")
	FString Char1 = {};
	//辅助角色
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="助战角色")
	FString Char2 = {};
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
	//技能的等级，从1开始
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="技能等级")
	TArray<int> SkillLevel = {1, 1, 1, 1};
	//被动介绍合集
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="被动介绍合集")
	TArray<FText> SkillDescription = {};

	F2DAtkDataStruct()
	{
		GenMain = nullptr;
		GenSub = nullptr;
		Char1 = {};
		Char2 = {};
		Speed = 1;
		Health = 0;
		Attack = 0;
		PhyDefense = 0.0f;
		MagDefense = 0.0f;
		Critical = 0.0f;
		CriticalC = 0.0f;
		SkillLevel = {1, 1, 1, 1};
	}
};

//地图的BGM数据结构体
USTRUCT(BlueprintType)
struct FMapBGMDataStruct
{
	GENERATED_BODY()
	//BGM索引
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="普通BGM索引范围")
	FIntPoint BGMIndex = {};
	//是阶段BGM
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="是阶段BGM")
	bool IsStageBGM = false;
	//阶段BGM列表
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="阶段BGM列表")
	TArray<TSoftObjectPtr<USoundWave>> StageBGMList = {};
	//阶段BGM开始循环的时间点
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="阶段BGM开始循环的时间点")
	TArray<float> StageBGMLoopStartTime = {};

	FMapBGMDataStruct()
	{
		BGMIndex = {};
		IsStageBGM = false;
		StageBGMList = {};
		StageBGMLoopStartTime = {};
	}
};

//地图数据结构体
USTRUCT(BlueprintType)
struct FPlotMapDataStruct : public FTableRowBase
{
	GENERATED_BODY()
	//Gal路线选择
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="Gal路线名")
	FName GalRoad = {};
	//Gal剧情点选择
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="Gal剧情点选择")
	FName GalPart = {};
	//2D地图选择
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="2D地图选择")
	UDataLayerAsset* MapSel = nullptr;
	//关卡开始提示语
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="关卡开始提示语")
	FText StartTalk = {};
	//关卡外部名字
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="关卡名称")
	FText MapName = {};
	//回合数
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="回合数")
	int Round = 0;
	//是玩家敌人
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="敌人是玩家")
	bool IsSystem = false;
	//敌人数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="敌人数据")
	TArray<F2DAtkDataStruct> EnemyDataS = {};
	//关卡指示器
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="关卡指示器")
	TSoftClassPtr<AActor> PlotIndicator = nullptr;
	//BGM数据
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="BGM数据")
	FMapBGMDataStruct BGMData = {};
	//关卡消耗能量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="关卡消耗能量")
	int Energy = 0;
	//关卡结算奖励
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="关卡结算奖励")
	TMap<TSoftClassPtr<UInventoryBaseItem>,int> AwardPrice = {};
};

//护援人物头像结构体
USTRUCT(BlueprintType)
struct FCharacterSupportData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="是谁的")
	FString CharName = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="1P头像")
	TSoftObjectPtr<UTexture2D> Sub1P = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="2P头像")
	TSoftObjectPtr<UTexture2D> Sub2P = {};
};

//游戏设置结构体
USTRUCT(BlueprintType)
struct FVoidSetting
{
	GENERATED_BODY()
	//BGM音量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="BGM音量")
	float BGMVolume = 100.0f;
	//战斗音效音量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="战斗音效音量")
	float SoundVolume = 100.0f;
	//UI音效音量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="UI音效音量")
	float SoundUIVolume = 100.0f;
	//语音音量
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="语音音量")
	float VoiceVolume = 100.0f;
	//环境音
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="环境音")
	float EnvironmentVolume = 100.0f;
};

// 养成模式路线介绍
USTRUCT(BlueprintType)
struct FCultivateIntroduces
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="路线名称")
	FText RoadName = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="路线起始节点")
	FString RoadStart = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="总回合数")
	int MaxRound = 91;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="BOSS数量")
	int MaxBosss = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="推荐属性")
	int RecommendAttributes = 0;
};

/**
 * 养成事件的选项
 * 这个是单个故事，一个回合内会有不同的故事
 * 额外属性就用计算来获取
 */
USTRUCT(BlueprintType)
struct FYCPlotrpc
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项1")
	FString Option1 = {}; //用字符串来判断后续触发什么，【故事#函数】
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项1_强化")
	FString Option1_EX = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="Result_1")
	FString Result1 = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="ExResult_1")
	FString Result1_EX = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项2")
	FString Option2 = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项2_强化")
	FString Option2_EX = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="Result_2")
	FString Result2 = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="ExResult_2")
	FString Result2_EX = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项3")
	FString Option3 = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项3_强化")
	FString Option3_EX = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="Result_3")
	FString Result3 = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="ExResult_3")
	FString Result3_EX = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项1_额外属性")
	FString Option1_EX_Attributes = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项2_额外属性")
	FString Option2_EX_Attributes = {};
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="选项3_额外属性")
	FString Option3_EX_Attributes = {};
};

/**
 * 养成单回合故事
 * 当前回合，拥有的数据和可能出现的故事
 * 属性就按总回合数平均分配
 */
USTRUCT(BlueprintType)
struct FYCPlots : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="是特殊事件")
	bool IsSpecial = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName="此处可以出现的故事")
	TMap<FString, FYCPlotrpc> RoundStory = {};

	FYCPlots()
	{
		IsSpecial = false;
		RoundStory = {};
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMaterialCompilationFinishedDelegate); //材质编译完成？用上了吗

UCLASS()
class CROSSMAINDATA_API UFUCRealize : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public: //变量

public: //函数

	/**
	 * 主界面选项字符串走向
	 */
	UFUNCTION(BlueprintCallable, Category="TFAC", DisplayName="地图起始_MainMap")
	static EMainUIStartType Opitions_MainMap(FString Input, FString& MainStartEx);

	//提取角色数值
	UFUNCTION(BlueprintCallable, Category="回合制", DisplayName ="提取角色数值")
	static F2DAtkDataStruct GetCharater2DData(UInventoryBaseItem* MainChar, UInventoryBaseItem* SubChar,
	                                          TSoftClassPtr<ACrossvoid2DatkNew> SpawnChar,
	                                          TSoftClassPtr<ACrossvoid2DatkNew> SpawnSub);
	//分配前后排
	UFUNCTION(BlueprintCallable, Category = "回合制", DisplayName="分配前后排")
	static void SetTagPosition(TArray<AActor*> MapsIN);
	/**
	 * 人物速度排序
	 * @return 有效的人数
	 */
	UFUNCTION(BlueprintCallable, Category = "回合制", DisplayName="人物速度排序")
	static int SortSpeed(TArray<ACrossvoid2DatkNew*> MapsIN);
	/**
	 * 对整形进行排序，默认升序
	 * @param Input 请放入整形数组
	 * @param Descending 是否降序
	 * @return 处理结果
	 */
	UFUNCTION(BlueprintCallable, Category="TFAC", DisplayName="整形数组排序") //整形排序函数,蓝图可调用
	static TArray<int> SortInt(TArray<int> Input, bool Descending); //最前面是返回值，后面是输入的参数，中间是函数名

	/**
	 * 对浮点进行排序，默认升序
	 * @param Input 请放入浮点数组
	 * @param Descending 是否降序
	 * @return 处理结果
	 */
	UFUNCTION(BlueprintCallable, Category="TFAC", DisplayName="浮点数组排序")
	static TArray<float> SortFloat(TArray<float> Input, bool Descending);

	//胜利条件1
	UFUNCTION(BlueprintCallable, Category="回合制", DisplayName="胜利条件1")
	static bool VictoryType1(TArray<AActor*> MapsIN, bool& WhoWin);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="回合制", DisplayName="2D计算移动")
	static void MoveCalcu(AActor* Movea, AActor* Target, float Offset, float& TargetLoc, bool& Arrive, bool& MoveFace);

	/**
	 * PVP返回方案
	 * @return 返回方案
	 * @param Pvpback GM的PVP返回指示器
	 * @param PvpbackEX PVP返回的额外信息
	 */
	UFUNCTION(BlueprintCallable, Category="回合制", DisplayName="PVP返回方案")
	static EBackPVPType BackPVPTypeFuc(FString Pvpback, bool Victory, FString& PvpbackEX);

	//金色抽卡概率计算
	UFUNCTION(BlueprintCallable, Category="TFAC", DisplayName="金色抽卡概率计算")
	static float CardProbability(int Counted);

	//Fisher-Yates完全随机
	//UFUNCTION(BlueprintCallable, Category="2D", DisplayName="完全随机")
	//static int FisherRandomArray(int Length, TArray<float> Weight);

	//函数触发器
	UFUNCTION(BlueprintCallable, meta=(DefaultToSelf), Category="TFAC", DisplayName="函数触发器")
	static bool CallFunctionByName(UObject* Object, FName FunctionName);

	/**
    	 * 字符串分割指示器
    	 * 根据“/”来分割出函数名称数组（命名）
    	 * 根据“_”来分割出参数数组（字符串）
    	 */
	UFUNCTION(BlueprintCallable, meta=(DefaultToSelf), Category="TFAC")
	static TArray<FName> SplitFunction(FString FunctionStr, TArray<FString>& ParameterArray);


	/**
	 * 养成模式随机属性数值
	 * 根据总回合数、特殊事件、属性模式来计算，这个选项要给多少数值
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="随机属性数值")
	static int CultivateAttributeSingle(int RoundAll, bool IsSpecial, int AbMode);

	/**
	 * 养成模式数值增加
	 * 对角色结构体进行数值增加
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="数值增加")
	static F2DAtkDataStruct CultivateAbAdd(F2DAtkDataStruct CharData, int Random1, int Ab2, int Random3, int Holding,
	                                       int Ab1Value, int Ab2Value, int Ab3Value);

	/**
	 * 根据Hold定下返回值
	 * @return 选项的文本
	 * @param PlotData 当前故事数据
	 * @param Holding 当前Hold位置
	 * @param UseAct 是否用了行动值
	 * @param FUC 函数触发指示器
	 * @param StoryResult 选择了后的结果
	 * @param ExAb 要加额外的属性
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="根据Hold定下返回值")
	static FString HoldingValue(FYCPlotrpc PlotData, int Holding, bool UseAct, FName& FUC, FString& StoryResult,
	                            FString& ExAb);

	/**
	 * 获取角色总和数值
	 * @param CharData 角色纯养成数据
	 * @param Rank 角色等级
	 * @param CharSum 角色总和数值
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="获取角色总和数值")
	static void GetCharacterSum(F2DAtkDataStruct CharData, F2DAtkDataStruct SubData, int Rank,
	                            F2DAtkDataStruct& CharSum);

	/**
	 * 角色数值从分数转换为数值
	 * @return 转换后的数据
	 * @param CharData 要转换的角色数据
	 */
	UFUNCTION(BlueprintCallable, Category="TFAC", DisplayName="角色数值分数转化")
	static F2DAtkDataStruct CharacterDataConversion(F2DAtkDataStruct CharData);

	/**
	 * 计算下一个要到的回合
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="计算下一个要到的回合")
	static FString YCNextPlot(FName NowPlotName);

	/**
	 * 养成中函数指示器走向
	 * @return 函数指示器大方向
	 * @param FUC 函数指示器进入
	 * @param Type2 小方向
	 * @param Value1 返回的参数
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="函数指示器走向YC")
	static EYCFUCTriggerType YCFUCTriggerTypeFuc(FName FUC, FString& Type2, FString& Value1);

	/**
	 * 养成分数
	 * 根据养成的数值总和/10，加上等级和rank的和/10
	 * @return 角色的养成分数
	 */
	UFUNCTION(BlueprintCallable, Category="养成模式", DisplayName="养成分数")
	static int YCScore(int Rank, TArray<int> Sklevel, int HP, int Attack, int Critical, int CriticalC, int PhyDefense,
	                   int MagDefense);
};

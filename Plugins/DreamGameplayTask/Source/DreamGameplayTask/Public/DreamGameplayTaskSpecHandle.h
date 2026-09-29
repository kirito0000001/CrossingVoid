#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskSpecHandle.generated.h"

enum class EDreamTaskPriority : uint8;
class UDreamTaskType;
class UDreamTaskConditionTemplate;
class UDreamTaskComponent;
class UDreamTask;

/**
 *  Task State
 */
UENUM(BlueprintType, Meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor=true))
enum class EDreamTaskState : uint8
{
	EDTS_None = 0 UMETA(DisplayName = "None"),
	EDTS_Accept = 1 << 0 UMETA(DisplayName = "Accept"),
	EDTS_Going = 1 << 1 UMETA(DisplayName = "Going"),
	EDTS_Completed = 1 << 2 UMETA(DisplayName = "Completed"),
	EDTS_Failed = 1 << 3 UMETA(DisplayName = "Failed"),
	EDTS_Timeout = 1 << 4 UMETA(DisplayName = "Timeout"),
	EDTS_Initialized = 1 << 5 UMETA(DisplayName = "Initialized"),
};

ENUM_CLASS_FLAGS(EDreamTaskState)

/* EnumHasAllFlags EnumHasAnyFlags  EnumAddFlags EnumRemoveFlags
 *	template<typename Enum>
	constexpr bool EnumHasAllFlags(Enum Flags, Enum Contains)
	{
		using UnderlyingType = __underlying_type(Enum);
		return ((UnderlyingType)Flags & (UnderlyingType)Contains) == (UnderlyingType)Contains;
	}

	template<typename Enum>
	constexpr bool EnumHasAnyFlags(Enum Flags, Enum Contains)
	{
		using UnderlyingType = __underlying_type(Enum);
		return ((UnderlyingType)Flags & (UnderlyingType)Contains) != 0;
	}

	template<typename Enum>
	void EnumAddFlags(Enum& Flags, Enum FlagsToAdd)
	{
		using UnderlyingType = __underlying_type(Enum);
		Flags = (Enum)((UnderlyingType)Flags | (UnderlyingType)FlagsToAdd);
	}

	template<typename Enum>
	void EnumRemoveFlags(Enum& Flags, Enum FlagsToRemove)
	{
		using UnderlyingType = __underlying_type(Enum);
		Flags = (Enum)((UnderlyingType)Flags & ~(UnderlyingType)FlagsToRemove);
	}
*/

/**
 * @struct FDreamTaskSpecHandle
 * @brief 用于管理DreamTask任务的结构体，包含任务相关信息和状态
 * @details 该结构体通过GUID标识任务，记录任务时间信息，并提供任务状态查询功能
 * @note BlueprintType标记使其可在蓝图中使用
 */
USTRUCT(BlueprintType)
struct DREAMGAMEPLAYTASK_API FDreamTaskSpecHandle
{
	GENERATED_BODY()

public:
	/**
	 * @brief 默认构造函数
	 */
	FDreamTaskSpecHandle();

	/**
	 * @brief 带参数构造函数
	 * @param InTask 要关联的UDreamTask对象
	 * @param InStartTime 任务开始时间
	 */
	FDreamTaskSpecHandle(UDreamTask* InTask, FDateTime InStartTime);

	/**
	 * @brief 获取无效的任务句柄
	 * @return 返回一个静态的无效任务句柄引用
	 */
	static const FDreamTaskSpecHandle& InvalidHandle();

public:
	// 用于标记此Handle是否有效
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid Guid;

	// 任务
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UDreamTask> Task = nullptr;

	// 拥有任务的组件
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UDreamTaskComponent> OwnerComponent = nullptr;

	// 任务开始时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDateTime StartTime = 0.f;

	// 任务运行时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTimespan RunningTime = 0.f;

	// 任务结束时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDateTime EndTime = 0.f;

public:
	/**
	 * @brief 获取关联的任务对象
	 * @return 返回UDreamTask指针
	 */
	UDreamTask* GetTask() const { return Task.Get(); }

	/**
	 * @brief 获取任务所属组件
	 * @return 返回UDreamTaskComponent指针
	 */
	UDreamTaskComponent* GetOwnerComponent() const { return OwnerComponent.Get(); }

	/**
	 * @brief 获取任务GUID
	 * @return 返回FGuid标识符
	 */
	FGuid GetGuid() const { return Guid; }

	/**
	 * @brief 获取任务当前状态
	 * @return 返回EDreamTaskState枚举值
	 */
	EDreamTaskState GetTaskState() const;

	/**
	 * @brief 获取任务已运行时间
	 * @return 返回FTimespan时间间隔
	 */
	FTimespan GetRunningTime() const;

	/**
	 * @brief 获取任务开始时间
	 * @return 返回FDateTime时间对象
	 */
	FDateTime GetStartTime() const;

	/**
	 * @brief 获取任务结束时间
	 * @return 返回FDateTime时间对象
	 */
	FDateTime GetEndTime() const;

	/**
	 * @brief 获取最大时间限制
	 * @return 获取最大时间限制
	 */
	FDateTime GetMaximumTime() const;

	/**
	 * @brief 获取任务条件映射
	 * @return 任务条件映射
	 */
	TMap<FName, UDreamTaskConditionTemplate*>& GetTaskConditions();

	TMap<FName, UDreamTaskConditionTemplate*>& GetTaskConditions() const;

	int32 GetTaskConditionsCount() const;

	/**
	 * @brief 检查是否使用最大时间限制
	 * @return 返回布尔值表示是否有限制
	 */
	bool IsUseMaximumTime() const;

	/**
	 * @brief 检查任务是否超时
	 * @return 返回布尔值表示是否超时
	 */
	bool IsTimeout() const;

	/**
	 * @brief 检查任务是否完成
	 * @return 返回布尔值表示是否完成
	 */
	bool IsCompleted() const;

	/**
	 * @brief 检查任务是否失败
	 * @return 返回布尔值表示是否失败
	 */
	bool IsFailed() const;

	/**
	 * @brief 检查任务句柄是否有效
	 * @return 返回布尔值表示是否有效
	 */
	bool IsValid() const;

	/**
	 * @brief 设置任务运行时间
	 * @param InTime 要设置的FTimespan时间间隔
	 */
	void SetRunningTime(FTimespan InTime);

	/**
	 * @brief 设置任务开始时间
	 * @param InTime 要设置的FDateTime时间对象
	 */
	void SetStartTime(FDateTime InTime);

	/**
	 * @brief 设置任务结束时间
	 * @param InTime 要设置的FDateTime时间对象
	 */
	void SetEndTime(FDateTime InTime);

	/**
	 * @brief 增加任务时间(秒为单位)
	 * @param InSeconds 要增加的秒数
	 */
	void AddTime(float InSeconds);

	/**
	 * @brief 增加任务时间
	 * @param InTime 要增加的FTimespan时间间隔
	 */
	void AddTime(FTimespan InTime);

	/**
	 * @brief 更新任务状态
	 * @param DeltaTime 帧时间间隔
	 */
	void Update(float DeltaTime);

	/**
	 * @brief 重置任务
	 */
	void Reset();

public:
	/**
	 * @brief 比较两个任务句柄是否相等
	 * @param Other 要比较的另一个任务句柄
	 * @return 返回布尔值表示是否相等
	 */
	bool operator==(const FDreamTaskSpecHandle& Other) const;

	/**
	 * @brief 比较任务句柄与任务对象是否匹配
	 * @param Other 要比较的UDreamTask对象
	 * @return 返回布尔值表示是否匹配
	 */
	bool operator==(const UDreamTask* Other) const;

	/**
	 * @brief 比较任务句柄与名称是否匹配
	 * @param InName 要比较的FName名称
	 * @return 返回布尔值表示是否匹配
	 */
	bool operator==(const FName& InName) const;

	/**
	 * @brief 比较任务句柄与任务类是否匹配
	 * @param Class 要比较的任务类
	 * @return 返回布尔值表示是否匹配
	 */
	bool operator==(const TSubclassOf<UDreamTask>& Class) const;

	bool operator==(const UDreamTaskType* InTaskType) const;

	bool operator==(EDreamTaskState InState) const;

	bool operator==(EDreamTaskPriority InPriority) const;
};

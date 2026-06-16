#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskSpecHandleContainer.generated.h"

class UDreamTaskType;
enum class EDreamTaskPriority : uint8;
enum class EDreamTaskState : uint8;
class UDreamTask;

UENUM(BlueprintType, Meta = (Bitflags))
enum class EDreamTaskSpecHandleContainerState : uint8
{
	None = 0, // 未定义
	Empty = 1 << 0, // 空
	NoCompleted = 1 << 1, // 全部未完成
	SomeCompleted = 1 << 2, // 部分完成
	AllCompleted = 1 << 3, // 全部
	AllUseMaximumTimeCompleted = 1 << 4,
};

ENUM_CLASS_FLAGS(EDreamTaskSpecHandleContainerState)

struct FDreamTaskSpecHandle;

/**
 * @struct FDreamTaskSpecHandleContainer
 * @brief 蓝图可用的结构体，用于管理一组FDreamTaskSpecHandle对象
 * 
 * 该结构体包含一个可编辑的FDreamTaskSpecHandle数组，并提供相关操作方法
 */
USTRUCT(BlueprintType)
struct DREAMGAMEPLAYTASK_API FDreamTaskSpecHandleContainer
{
	GENERATED_BODY()

	/**
	 * @brief 默认构造函数
	 */
	FDreamTaskSpecHandleContainer()
	{
	}

	/**
	 * @brief 拷贝构造函数
	 * @param Other 要拷贝的源容器对象
	 */
	FDreamTaskSpecHandleContainer(const FDreamTaskSpecHandleContainer& Other);

private:
	/** 存储所有任务句柄的数组 */
	UPROPERTY()
	TArray<FDreamTaskSpecHandle> Handles = {};

	/** 当前容器的状态 */
	EDreamTaskSpecHandleContainerState ContainerState = EDreamTaskSpecHandleContainerState::Empty;

public:
	/**
	 * @brief 获取所有任务句柄的可编辑引用
	 * @return 返回任务句柄数组的引用
	 */
	TArray<FDreamTaskSpecHandle>& GetHandles();

	/**
	 * @brief 添加新任务句柄到容器中
	 * @param InHandle 要添加的任务句柄
	 * @return 返回添加后的任务句柄引用
	 * @note 会自动初始化StartTime和Guid
	 */
	FDreamTaskSpecHandle& AddHandle(const FDreamTaskSpecHandle& InHandle);

	/**
	 * @brief 从容器中移除指定任务句柄
	 * @param InHandle 要移除的任务句柄
	 * @return 成功移除返回true，否则返回false
	 */
	bool RemoveHandle(const FDreamTaskSpecHandle& InHandle);

	/**
	 * @brief 根据任务对象查找对应的任务句柄
	 * @param InTask 要查找的任务对象指针
	 * @return 返回找到的任务句柄常量引用
	 */
	const FDreamTaskSpecHandle& FindHandle(UDreamTask* InTask);

	FDreamTaskSpecHandle* FindHandleMutable(UDreamTask* InTask);

	/**
	 * @brief 根据任务类查找对应的任务句柄
	 * @param InClass 要查找的任务类
	 * @return 返回找到的任务句柄常量引用
	 */
	const FDreamTaskSpecHandle& FindHandle(TSubclassOf<UDreamTask> InClass);

	FDreamTaskSpecHandle* FindHandleMutable(TSubclassOf<UDreamTask> InClass);

	/**
	 * @brief 根据名称查找对应的任务句柄
	 * @param InName 要查找的任务名称
	 * @return 返回找到的任务句柄常量引用
	 */
	const FDreamTaskSpecHandle& FindHandle(FName InName);

	FDreamTaskSpecHandle* FindHandleMutable(FName InName);

	/**
	 * @brief 查找指定任务句柄的索引
	 * @param InHandle 要查找的任务句柄
	 * @return 找到返回索引值，未找到返回INDEX_NONE
	 */
	int32 FindHandleIndex(const FDreamTaskSpecHandle& InHandle);

	/**
	 * @brief 获取需要使用最大时间限制的任务句柄数组
	 * 
	 * 该函数返回一个包含所有需要使用最大时间限制的任务句柄的数组。
	 * 这些任务句柄通常用于需要特殊时间管理的任务，可能用于超时处理或优先级调度。
	 * 
	 * @return TArray<FDreamTaskSpecHandle*> 包含FDreamTaskSpecHandle指针的数组，
	 *         每个指针指向一个需要使用最大时间限制的任务规格
	 */
	TArray<FDreamTaskSpecHandle*> GetUseMaximumTimeHandles();


	/**
	 * @brief 清空容器中的所有任务句柄
	 */
	void ClearHandles();

	/**
	 * @brief 设置容器中的任务句柄数组
	 * @param InHandles 要设置的新任务句柄数组
	 * @return 返回设置的任务句柄数量
	 */
	int SetHandles(const TArray<FDreamTaskSpecHandle>& InHandles);

	/**
	 * @brief 构建包含所有任务对象的数组
	 * @return 返回构建的任务对象数组
	 */
	TArray<UDreamTask*> BuildTaskArray();

public:
	/**
	 * @brief 更新所有任务句柄的状态
	 * @param DeltaTime 自上次更新以来的时间增量
	 */
	void UpdateHandles(float DeltaTime);

	/**
	 * @brief 检查所有任务是否都已完成
	 * @return 全部完成返回true，否则返回false
	 */
	bool IsAllCompleted() const;

	/**
	 * @brief 检查所有任务是否都使用最大时间限制
	 * @return 全部使用最大时间限制返回true，否则返回false
	 */
	bool IsAllUseMaximumTimeCompleted() const;

	/**
	 * @brief 检查是否有部分任务已完成
	 * @return 有任务完成返回true，否则返回false
	 */
	bool IsSomeCompleted() const;

	/**
	 * @brief 检查是否没有任务完成
	 * @return 没有任务完成返回true，否则返回false
	 */
	bool IsNoCompleted() const;

	/**
	 * @brief 检查容器是否为空
	 * @return 容器为空返回true，否则返回false
	 */
	bool IsEmpty() const;

	TArray<FDreamTaskSpecHandle> Filter(UDreamTaskType* InType);
	TArray<FDreamTaskSpecHandle> Filter(EDreamTaskPriority InPriority);
	TArray<FDreamTaskSpecHandle> Filter(EDreamTaskState InState);
	
private:
	/**
	 * @brief 根据当前任务状态更新容器状态
	 */
	void ChangeContainerState();
};

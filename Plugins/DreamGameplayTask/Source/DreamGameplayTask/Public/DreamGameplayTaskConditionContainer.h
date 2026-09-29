#pragma once

#include "CoreMinimal.h"
#include "DreamGameplayTaskConditionContainer.generated.h"

#define TASK_CONDITION_NOT_COMPLETED -1

/**
 *  Task Conditional Completion Mode
 */
UENUM(BlueprintType)
enum class EDreamTaskConditionalCompletionMode : uint8
{
	EDTCCM_All = 0 UMETA(DisplayName = "All"),
	EDTCCM_Any = 1 UMETA(DisplayName = "Any"),
	EDTCCM_Custom = 2 UMETA(DisplayName = "Custom"),
};

class UDreamTaskConditionTemplate;
/**
* @struct FDreamTaskConditionContainer
 * @brief 蓝图可用的任务完成条件容器结构体
 * 
 * 该结构体用于存储和管理任务完成的条件，支持多种条件完成模式
 */
USTRUCT(BlueprintType)
struct DREAMGAMEPLAYTASK_API FDreamTaskConditionContainer
{
	GENERATED_BODY()

public:
	/**
	 * @brief 任务条件映射表
	 * 
	 * 以FName为键，存储任务条件模板对象的指针
	 * 可编辑，蓝图可读写，支持实例化
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TMap<FName, UDreamTaskConditionTemplate*> Conditions = {};

	/**
	 * @brief 任务条件完成模式
	 * 
	 * 定义任务条件的完成判定方式（全部完成/任意完成/自定义数量完成）
	 * 可编辑，蓝图只读，归类为Condition类别
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Condition)
	EDreamTaskConditionalCompletionMode CompletionMode = EDreamTaskConditionalCompletionMode::EDTCCM_All;

	/**
	 * @brief 自定义条件数量
	 * 
	 * 当CompletionMode为EDTCCM_Custom时有效，指定需要完成的条件数量
	 * 可编辑，蓝图只读，归类为Condition类别
	 * 仅在CompletionMode为EDTCCM_Custom时显示编辑控件
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Condition,
		meta = (EditCondition = "CompletionMode == EDreamTaskConditionalCompletionMode::EDTCCM_Custom",
			EditConditionHides))
	int CustomConditionCount = 1;

public:
	/**
	 * @brief 通过名称获取任务条件
	 * @param InConditionName 要查找的条件名称
	 * @return 找到的任务条件模板对象指针，未找到返回nullptr
	 */
	UDreamTaskConditionTemplate* GetConditionByName(FName InConditionName) const;

	/**
	 * @brief 获取所有任务条件
	 * @return 包含所有任务条件模板对象的数组
	 */
	TArray<UDreamTaskConditionTemplate*> GetConditions();

	/**
	 * @brief 获取任务条件映射表
	 * @return 任务条件映射表
	 */
	TMap<FName, UDreamTaskConditionTemplate*>& GetConditionMapping();

	/**
	 * @brief 更新指定名称的任务条件
	 * @param InConditionName 要更新的条件名称
	 * @return 更新是否成功
	 */
	bool UpdateConditionByName(FName InConditionName);

	/**
	 * @brief 获取已完成的任务条件数量
	 * @return 已完成的条件数量，若返回 TASK_CONDITION_NOT_COMPLETED 则表示全部未完成
	 */
	int ConditionCompletedCount() const;

	/**
	 * @brief 检查任务条件是否全部完成
	 * @return 根据CompletionMode判断条件是否满足完成要求
	 */
	bool IsConditionsCompleted() const;

	/**
	 * @brief 重置所有任务条件
	 */
	void Reset();
};

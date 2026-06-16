#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Realize.generated.h"

UENUM(BlueprintType,DisplayName="路径模式")
enum class EPathMode:uint8
{
	EPM_Custom				= 0 UMETA(DisplayName = "电脑自定义路径"),
	EPM_ProjectDir			= 1 UMETA(DisplayName = "项目文件夹路径"),
	EPM_ProjectSaveDir		= 2 UMETA(DisplayName = "项目saved路径")
};

// 音频资产结构体,本来不属于这里的
USTRUCT(BlueprintType)
struct FMetaSoundMapsInside
{
	GENERATED_BODY()
	/* 每一个章节段落内的语音 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,DisplayName="章节内")
	TArray<USoundWave*> ChapterWaves;
};
USTRUCT(BlueprintType,DisplayName="音频资产结构体")
struct FMetaSoundMaps
{
	GENERATED_BODY()
	/* 每一个控件内的语音集合 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,DisplayName="控件内")
	TArray<FMetaSoundMapsInside> UIWaves;
};

UCLASS(DisplayName="DreamMoon")
class DTSCREENSHOT_API URealize : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 截屏为png
	 * @param ImageName 图片名字
	 * @param Path 图片路径，可以衔接路径，如"\content\任意文件夹\"
	 * @param SaveUI 是否截屏UI，控件
	 * @param PathMode 保存路径模式
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamMoon",DisplayName="获取视口截图")
	static void GetViewportScreenshot(FString ImageName,FString Path, bool SaveUI,EPathMode PathMode);

	/**
	 * 打开图片
	 * @param ImageName 图片名字
	 * @param Path 图片路径，可以衔接路径，如"content\任意文件夹"
	 * @param PathMode 读取路径模式
	 */
	UFUNCTION(BlueprintCallable, Category = "DreamMoon",DisplayName="打开图像")
	static UTexture2D* OpenImage(FString ImageName,FString Path, EPathMode PathMode);

	
};
class Realize
{
public:
	
};

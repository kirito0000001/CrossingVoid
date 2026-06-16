#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MusicPlayerC.generated.h"

//用于语音的结构体
USTRUCT(BlueprintType)
struct FTableVoices
{
	GENERATED_BODY()
	/* 每一个表格对应的语音合集 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="这表格包含的语音")
	TArray<TSoftObjectPtr<USoundWave>> TableWaves;
};

UCLASS(DisplayName="MusicActor")
class CROSSMAINDATA_API AMusicPlayerC : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMusicPlayerC();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="BGM1")
	UAudioComponent* BGMComponent1; //绑定组件

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="BGM2")
	UAudioComponent* BGMComponent2; //绑定组件

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="语音")
	UAudioComponent* VoiceComponent; //绑定组件

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MySceneComponent", DisplayName="环境音")
	UAudioComponent* SceneComponent; //绑定组件

public://变量

	//目前只作用固定BGM记录
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="BGM名称")
	FString BGMName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="BGM音频索引")
	int AudioIndex = -1;

	//所有音频淡入淡出的过渡时长
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="过渡时间")
	float MusicLerpLen = 0.8f;

	//此值越小越精准，消耗也会变大
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="无缝播放准度")
	float MusicLerpAccuracy = 0.8f;

	UPROPERTY(BlueprintReadOnly, Category = "MusicActor", DisplayName="Comp12")
	bool Is2 = false;

	UPROPERTY(BlueprintReadOnly, Category = "MusicActor", DisplayName="已播放时长_当前歌曲")
	float PlayedTimeSingle = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "MusicActor", DisplayName="音频计时器句柄")
	FTimerHandle AudioTimerHandle = {};

	UPROPERTY(BlueprintReadOnly, Category = "MusicActor", DisplayName="暂停中")
	bool Pauseing = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="BGM列表")
	TArray<TSoftObjectPtr<USoundWave>> BGMaps = {};

	//每个BGM的循环起始时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="BGM列表_循环起始")
	TArray<float> BGMapsLoopStart = {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="环境音频索引")
    int SceneIndex = -1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="环境音列表")
	TArray<TSoftObjectPtr<USoundWave>> SceneMaps = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MusicActor", DisplayName="语音列表")
	TMap<UDataTable*, FTableVoices> VoiceMaps = {};

public://函数

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="初始化BGM列表")
	bool InitBGMaps(TArray<TSoftObjectPtr<USoundWave>> BGMapsIN);

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="播放指定BGM")
	void PlayBGM(USoundWave* Wave, bool SyncSwitch);
	
	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="切换BGM")
	void SwitchBGM(int Index, bool SyncSwitch);

	UFUNCTION(Category = "MusicActor", DisplayName="BGM记录时长TM")
	void RecordBGMTime();

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="暂停BGM")
	void PauseBGM(bool Pause);

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="BGM循环")
	void BGMLoop(bool Loop);

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="播放环境音")
	void PlayScene(int Index,bool Pause);

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="环境音循环")
	void SceneLoop(bool Loop);

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="播放语音")
	void PlayVoice(UDataTable* Table, int Index);

	UFUNCTION(BlueprintCallable, Category = "MusicActor", DisplayName="停止语音")
	void StopVoice();
};

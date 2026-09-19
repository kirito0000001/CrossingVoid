#include "CrossMainData/Public/MusicPlayerC.h"
#include "Components/AudioComponent.h"
#include "Kismet/KismetSystemLibrary.h"


//请在此处初始化
AMusicPlayerC::AMusicPlayerC()
{
	//这个Actor能否使用tick
	PrimaryActorTick.bCanEverTick = true;
	//创建音频组件
	BGMComponent1 = CreateDefaultSubobject<UAudioComponent>(FName{TEXTVIEW("BGMComponent1")});
	BGMComponent2 = CreateDefaultSubobject<UAudioComponent>(FName{TEXTVIEW("BGMComponent2")});
	VoiceComponent = CreateDefaultSubobject<UAudioComponent>(FName{TEXTVIEW("VoiceComponent")});
	SceneComponent = CreateDefaultSubobject<UAudioComponent>(FName{TEXTVIEW("SceneComponent")});
}

//构造函数
void AMusicPlayerC::BeginPlay()
{
	Super::BeginPlay();
	//初始静音所有
	BGMComponent1->Stop();
	BGMComponent2->Stop();
	VoiceComponent->Stop();
	SceneComponent->Stop();
}

//Tick
void AMusicPlayerC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AMusicPlayerC::InitBGMaps(TArray<TSoftObjectPtr<USoundWave>> BGMapsIN)
{
	BGMaps = BGMapsIN;
	if (BGMaps.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("BGM列表为空"));
		return false;
	}
	return true;
}

void AMusicPlayerC::PlayBGM(USoundWave* Wave, bool SyncSwitch)
{
	if (IsValid(Wave))
	{
		if (Wave->GetName() != BGMName)
		{
			if (AudioTimerHandle.IsValid()) //音频计时器句柄有效
			{
				float MusicLen = Wave->GetDuration(); //获取音频长度
				if (PlayedTimeSingle >= MusicLen) //播放时长超过音频长度
				{
					PlayedTimeSingle -= MusicLen * PlayedTimeSingle / MusicLen; //减去音频长度（包含循环次数计算）
				}
			}
			else //创建一个音频计时器
			{
				AudioTimerHandle = UKismetSystemLibrary::K2_SetTimer(this, FString("RecordBGMTime"), MusicLerpAccuracy,
				                                                     true, false, 0, 0);
			}
			Is2 = !Is2; //切换组件
			BGMName = Wave->GetName();
			if (Is2) //切换组件2
			{
				//淡出组件1
				BGMComponent1->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
				//设置组件2
				BGMComponent2->SetWaveParameter(FName{TEXTVIEW("IN")}, Wave); //设置音频
				//设置播放时间
				BGMComponent2->SetFloatParameter(FName{TEXTVIEW("TimeIN")}, (SyncSwitch ? PlayedTimeSingle : 0.f));
				//设置当前音频的循环起始时间
				if (AudioIndex <= BGMapsLoopStart.Num() - 1)
				{
					BGMComponent2->SetFloatParameter(FName{TEXTVIEW("LoopStart")}, BGMapsLoopStart[AudioIndex]);
				}
				//淡入组件2
				BGMComponent2->FadeIn(MusicLerpLen, 1, 0, EAudioFaderCurve::Sin);
			}
			else
			{
				//淡出组件2
				BGMComponent2->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
				//设置组件1
				BGMComponent1->SetWaveParameter(FName{TEXTVIEW("IN")}, Wave); //设置音频
				//设置播放时间
				BGMComponent1->SetFloatParameter(FName{TEXTVIEW("TimeIN")}, (SyncSwitch ? PlayedTimeSingle : 0.f));
				//设置当前音频的循环起始时间
				if (AudioIndex <= BGMapsLoopStart.Num() - 1)
				{
					BGMComponent1->SetFloatParameter(FName{TEXTVIEW("LoopStart")}, BGMapsLoopStart[AudioIndex]);
				}
				//淡入组件1
				BGMComponent1->FadeIn(MusicLerpLen, 1, 0, EAudioFaderCurve::Sin);
			}
			if (!SyncSwitch)
			{
				PlayedTimeSingle = 0.f;
			}
		}
	}
}

void AMusicPlayerC::SwitchBGM(int Index, bool SyncSwitch)
{
	if (Index != AudioIndex) //要切换到别的音频
	{
		AudioIndex = Index; //记录现在的歌
		if (!Pauseing) //没有暂停
		{
			USoundWave* ReadyPlay = BGMaps[AudioIndex].LoadSynchronous(); //立即加载音频
			if (AudioTimerHandle.IsValid()) //音频计时器句柄有效
			{
				float MusicLen = ReadyPlay->GetDuration(); //获取音频长度
				if (PlayedTimeSingle >= MusicLen) //播放时长超过音频长度
				{
					PlayedTimeSingle -= MusicLen * PlayedTimeSingle / MusicLen; //减去音频长度（包含循环次数计算）
				}
			}
			else //创建一个音频计时器
			{
				AudioTimerHandle = UKismetSystemLibrary::K2_SetTimer(this, FString("RecordBGMTime"), MusicLerpAccuracy,
				                                                     true, false, 0, 0);
			}
			Is2 = !Is2; //切换组件
			BGMName = ReadyPlay->GetName();
			if (Is2) //切换组件2
			{
				//淡出组件1
				BGMComponent1->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
				//设置组件2
				BGMComponent2->SetWaveParameter(FName{TEXTVIEW("IN")}, ReadyPlay); //设置音频
				//设置播放时间
				BGMComponent2->SetFloatParameter(FName{TEXTVIEW("TimeIN")}, (SyncSwitch ? PlayedTimeSingle : 0.f));
				//设置当前音频的循环起始时间
				if (AudioIndex <= BGMapsLoopStart.Num() - 1)
				{
					BGMComponent2->SetFloatParameter(FName{TEXTVIEW("LoopStart")}, BGMapsLoopStart[AudioIndex]);
				}
				//淡入组件2
				BGMComponent2->FadeIn(MusicLerpLen, 1, 0, EAudioFaderCurve::Sin);
			}
			else
			{
				//淡出组件2
				BGMComponent2->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
				//设置组件1
				BGMComponent1->SetWaveParameter(FName{TEXTVIEW("IN")}, ReadyPlay); //设置音频
				//设置播放时间
				BGMComponent1->SetFloatParameter(FName{TEXTVIEW("TimeIN")}, (SyncSwitch ? PlayedTimeSingle : 0.f));
				//设置当前音频的循环起始时间
				if (AudioIndex <= BGMapsLoopStart.Num() - 1)
				{
					BGMComponent1->SetFloatParameter(FName{TEXTVIEW("LoopStart")}, BGMapsLoopStart[AudioIndex]);
				}
				//淡入组件1
				BGMComponent1->FadeIn(MusicLerpLen, 1, 0, EAudioFaderCurve::Sin);
			}
			if (!SyncSwitch)
			{
				PlayedTimeSingle = 0.f;
			}
		}
	}
}

void AMusicPlayerC::RecordBGMTime()
{
	//播放时长累加
	PlayedTimeSingle += MusicLerpAccuracy;
}

void AMusicPlayerC::PauseBGM(bool Pause)
{
	Pauseing = Pause; //记录暂停状态
	int AfterPauseIndex = AudioIndex; //记录暂停前的音频索引
	AudioIndex = -1; //重置音频索引
	if (Pause)
	{
		//淡出BGM组件
		BGMComponent1->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
		BGMComponent2->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
	}
	else
	{
		SwitchBGM(AfterPauseIndex, false);
	}
}

void AMusicPlayerC::BGMLoop(bool Loop)
{
	BGMComponent1->SetBoolParameter(FName{TEXTVIEW("IsLoop")}, Loop);
	BGMComponent2->SetBoolParameter(FName{TEXTVIEW("IsLoop")}, Loop);
}

void AMusicPlayerC::PlayScene(int Index, bool Pause)
{
	if (Pause)
	{
		//淡出环境音
		SceneComponent->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
	}
	else
	{
		if (Index == SceneIndex)
		{
			return;
		}
		SceneIndex = Index;
		if (SceneIndex == 0) //如果环境音是0，就停止
		{
			//淡出环境音
			SceneComponent->FadeOut(MusicLerpLen, -50, EAudioFaderCurve::Linear);
			return;
		}
		if (SceneMaps.Num() < SceneIndex)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("环境音索引超出"));
			return;
		}
		USoundWave* ReadyPlay = SceneMaps[SceneIndex].LoadSynchronous(); //立即加载音频
		if (!SceneMaps[SceneIndex])
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("当前索引环境音不存在"));
			return;
		}

		//设置环境音资源
		SceneComponent->SetWaveParameter(FName{TEXTVIEW("IN")}, SceneMaps[SceneIndex].LoadSynchronous());
		//淡入环境音
		SceneComponent->FadeIn(MusicLerpLen, 1, 0, EAudioFaderCurve::Sin);
	}
}

void AMusicPlayerC::SceneLoop(bool Loop)
{
	SceneComponent->SetBoolParameter(FName{TEXTVIEW("IsLoop")}, Loop);
}

void AMusicPlayerC::PlayVoice(UDataTable* Table, int Index)
{
	if (!VoiceMaps.Contains(Table)) //没有这个表格
	{
		return;
	}
	if (Index > VoiceMaps.Find(Table)->TableWaves.Num() - 1) //索引超出范围
	{
		return;
	}
	USoundWave* TartgetVoice = VoiceMaps.Find(Table)->TableWaves[Index].LoadSynchronous(); //获取语音
	if (IsValid(TartgetVoice))
	{
		VoiceComponent->SetWaveParameter(FName{TEXTVIEW("IN")}, TartgetVoice);
		VoiceComponent->Play(0);
	}
}

void AMusicPlayerC::StopVoice()
{
	VoiceComponent->Stop();
}

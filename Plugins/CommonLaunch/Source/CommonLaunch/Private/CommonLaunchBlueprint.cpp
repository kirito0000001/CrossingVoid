// Fill out your copyright notice in the Description page of Project Settings.


#include "CommonLaunchBlueprint.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformProcess.h"  // 仅限 Windows
#endif


bool UCommonLaunchBlueprint::LaunchApp(FString Path, FString Args)
{
#if PLATFORM_WINDOWS && UE_SERVER
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*Path, *Args, true, false, false, nullptr, 0, nullptr,
	                                                      nullptr, nullptr);
	return ProcHandle.IsValid();
#endif
	return false;
}

void UCommonLaunchBlueprint::LaunchAppAdvanced(FString Path, FString Args, bool& bIsLaunch, int32& ProcessId,
                                               bool bLaunchDetached, bool bLaunchHidden,
                                               bool bLaunchReallyHidden, int PriorityModifier)
{
#if PLATFORM_WINDOWS && UE_SERVER
	uint32 OutProcessId;
	
	FProcHandle ProcHandle =
		FPlatformProcess::CreateProc(*Path, *Args, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &OutProcessId,
		                             PriorityModifier, nullptr, nullptr, nullptr);
	ProcessId = OutProcessId;
	bIsLaunch = ProcHandle.IsValid();
#endif
}

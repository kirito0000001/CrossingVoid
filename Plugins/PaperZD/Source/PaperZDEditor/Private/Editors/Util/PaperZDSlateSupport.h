// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

namespace ZD::Slate
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
	using FVector2 = FVector2f;
	using FBox2 = FBox2f;
#else
	using FVector2 = FVector2D;
	using FBox2 = FBox2D;
#endif

};	

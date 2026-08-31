// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/PaperZDVersionCompatibility.h"

namespace ZD::Slate
{
#if PAPERZD_UE_5_6_OR_LATER
	using FVector2 = FVector2f;
	using FBox2 = FBox2f;
#else
	using FVector2 = FVector2D;
	using FBox2 = FBox2D;
#endif

};	

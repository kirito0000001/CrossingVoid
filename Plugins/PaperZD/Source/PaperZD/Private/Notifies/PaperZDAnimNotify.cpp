// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#include "Notifies/PaperZDAnimNotify.h"

#if ZD_VERSION_INLINED_CPP_SUPPORT
#include UE_INLINE_GENERATED_CPP_BY_NAME(PaperZDAnimNotify)
#endif

UPaperZDAnimNotify::UPaperZDAnimNotify(const FObjectInitializer& ObjectInitializer)
	: Super()
{
}

void UPaperZDAnimNotify::TickNotify(float DeltaTime, float Playtime, float LastPlaybackTime, UPrimitiveComponent* AnimRenderComponent, bool& bPersistentActiveState, UPaperZDAnimInstance* OwningInstance /* = nullptr */) const
{
	//Super takes care of setting world context object
	Super::TickNotify(DeltaTime, Playtime, LastPlaybackTime, AnimRenderComponent, bPersistentActiveState, OwningInstance);

	//These kinds of notifies cannot be active twice in a row.
	//Because our conditions for checking if a notify is triggered can have problems with edge cases where 'Playtime' and 'LastPlaybackTime' exactly match the notify time, it will cause them to trigger twice.
	//We cannot change the conditions for checking as this would make non-looping sequences and overrides not work when putting a notify on either the very start or very end of a notify.
	//We thus mark the notifies as active and ignore them if they triggered last frame.
	const bool bWasActiveLastFrame = bPersistentActiveState;
	bPersistentActiveState = false;

	//Check if the notify should indeed trigger this frame
	if (DeltaTime > 0.0f)
	{
		const bool bLooped = Playtime < LastPlaybackTime;
		if (bLooped && (Playtime >= Time || LastPlaybackTime <= Time))
		{
			bPersistentActiveState = true;
		}
		else if (Playtime >= Time && LastPlaybackTime <= Time)
		{
			bPersistentActiveState = true;
		}
	}
	else
	{
		const bool bLooped = Playtime > LastPlaybackTime;
		if (bLooped && (Playtime <= Time || LastPlaybackTime >= Time))
		{
			bPersistentActiveState = true;
		}
		else if (Playtime <= Time && LastPlaybackTime >= Time)
		{
			bPersistentActiveState = true;
		}
	}

	//If we haven't triggered this notify yet, do so. Make sure to not trigger twice if this notify is already active though.
	if (bPersistentActiveState && !bWasActiveLastFrame)
	{
		OnReceiveNotify(OwningInstance);
	}
}

void UPaperZDAnimNotify::OnReceiveNotify_Implementation(UPaperZDAnimInstance* OwningInstance /* = nullptr*/) const
{
	//Empty implementation
}

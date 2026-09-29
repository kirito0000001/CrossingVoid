// Copyright 2022 - 2025 Dream Moon Team. All Rights Reserved.


#include "Classes/DreamTaskData.h"

#include "Classes/DreamTaskType.h"

UTexture2D* UDreamTaskData::GetTaskIcon() const
{
	return TaskIcon.LoadSynchronous();
}

UTexture2D* UDreamTaskData::GetTaskImage() const
{
	return TaskImage.LoadSynchronous();
}

UDreamTaskType* UDreamTaskData::GetTaskType() const
{
	return TaskType;
}


UWorld* UDreamTaskData::GetWorld() const
{
	return GWorld;
}

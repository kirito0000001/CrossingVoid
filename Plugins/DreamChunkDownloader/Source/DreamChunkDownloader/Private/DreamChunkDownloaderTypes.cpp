// Copyright (C) 2025 Dream Moon, All Rights Reserved.


#include "DreamChunkDownloaderTypes.h"

FDreamMultiCallback::FDreamMultiCallback(const FDreamChunkDownloaderTypes::FDreamCallback& OnCallback)
	: OuterCallback(OnCallback)
{
	IndividualCb = [this](bool bSuccess)
	{
		// update stats
		--NumPending;
		if (bSuccess)
			++NumSucceeded;
		else
			++NumFailed;

		// if we're the last one, trigger the outer callback
		if (NumPending <= 0)
		{
			check(NumPending == 0);
			if (OuterCallback)
			{
				OuterCallback(NumFailed <= 0);
			}

			// done with this
			delete this;
		}
	};
}

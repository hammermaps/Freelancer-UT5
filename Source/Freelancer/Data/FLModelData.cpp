// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/FLModelData.h"

int32 UFLModelData::FindHardpoint(FName HardpointName, int32& OutPartIndex) const
{
	for (int32 P = 0; P < Parts.Num(); ++P)
	{
		for (int32 H = 0; H < Parts[P].Hardpoints.Num(); ++H)
		{
			if (Parts[P].Hardpoints[H].Name == HardpointName)
			{
				OutPartIndex = P;
				return H;
			}
		}
	}
	OutPartIndex = -1;
	return -1;
}

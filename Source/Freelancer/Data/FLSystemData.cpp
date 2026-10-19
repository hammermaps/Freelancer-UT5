// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/FLSystemData.h"

const FFLSystemObject& UFLSystemData::FindObjectOrDefault(FName ObjectNickname, bool& bFound) const
{
	static const FFLSystemObject Empty;
	for (const FFLSystemObject& O : Objects)
	{
		if (O.Nickname == ObjectNickname)
		{
			bFound = true;
			return O;
		}
	}
	bFound = false;
	return Empty;
}

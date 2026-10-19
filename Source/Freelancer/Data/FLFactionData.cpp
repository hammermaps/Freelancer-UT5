// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/FLFactionData.h"

float UFLFactionData::GetRelation(FName OtherGroup, float Default) const
{
	const float* V = Relations.Find(OtherGroup);
	return V ? *V : Default;
}

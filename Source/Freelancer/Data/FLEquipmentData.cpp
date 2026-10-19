// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/FLEquipmentData.h"

float UFLEquipmentData::GetFloatProperty(FName Key, float Default) const
{
	if (const FString* V = Properties.Find(Key))
	{
		return FCString::Atof(**V);
	}
	return Default;
}

FString UFLEquipmentData::GetStringProperty(FName Key) const
{
	const FString* V = Properties.Find(Key);
	return V ? *V : FString();
}

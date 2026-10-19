// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLFactionData.generated.h"

/** Fraktion/Gruppe mit Grundhaltungen und Ruf-Regeln aus initialworld.ini, empathy.ini und faction_prop.ini. */
UCLASS(BlueprintType)
class FREELANCER_API UFLFactionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 IdsName = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 IdsInfo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 IdsShortName = 0;

	/** Grundhaltung dieser Gruppe zu anderen Gruppen (-1 feindlich .. 1 freundlich) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> Relations;

	/** Aenderung des Rufs bei Ereignissen (object_destruction, random_mission_success, ...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> RepEvents;

	/** Anteil einer Rufaenderung, der auf verwandte Gruppen abfaerbt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> Empathy;

	/** Uebrige Eintraege (Praefix fp_ = aus faction_prop.ini; Wiederholungen als "key#2") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> Properties;

	/** Grundhaltung zu einer Gruppe, Default wenn nicht definiert */
	UFUNCTION(BlueprintPure)
	float GetRelation(FName OtherGroup, float Default = 0.f) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLFaction")), Nickname);
	}
};

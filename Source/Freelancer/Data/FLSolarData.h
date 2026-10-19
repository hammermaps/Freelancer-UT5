// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLSolarData.generated.h"

class UFLModelData;

/** Solar-Archetyp (Planet, Station, Sprungtor, Handelsroute, Wrack, ...) aus SOLAR/solararch.ini. Wird von [Object] der Systeme per Archetype-Nickname referenziert. */
UCLASS(BlueprintType)
class FREELANCER_API UFLSolarData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	FName Nickname;

	/** PLANET, STATION, JUMP_GATE, JUMP_HOLE, TRADELANE_RING, ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	FName SolarType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	int32 IdsName = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	int32 IdsInfo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	float Mass = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	int32 HitPoints = 0;

	/** Radius in cm (Planeten: solar_radius), 0 wenn unbekannt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	float Radius = 0.f;

	/** Planeten (.sph): Seitenzahl der Kugel und Materialnamen je Seite (M0..Mn), sonst leer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	int32 PlanetSides = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	TArray<FString> PlanetSideMaterials;

	/** Importiertes Modell; leer bei Planeten (.sph) oder fehlendem Modell */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UFLModelData> Model;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourceArchetype;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TArray<FString> SourceMaterialLibraries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar")
	TMap<FName, FString> Properties;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLSolar")), Nickname);
	}
};

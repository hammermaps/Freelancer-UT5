// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLAsteroidFieldData.generated.h"

/** Ein Asteroid im Feldwuerfel: Lage als Anteil der Wuerfelgroesse, in Unreal-Achsen. */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLFieldCube
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Asteroid;

	/** (-1..1) je Achse, Unreal-Achsen (x vorn, y rechts, z oben); mit cube_size/2 multiplizieren */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FQuat Rotation = FQuat::Identity;
};

USTRUCT(BlueprintType)
struct FREELANCER_API FFLFieldSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> Properties;
};

/** Asteroidenfeld/Nebel-Band-Definition aus den INI-Dateien im Ordner SOLAR/ASTEROIDS. Der Server erzeugt daraus Hindernisse und Rohstoffe; Clients Streuung und Billboards. */
UCLASS(BlueprintType)
class FREELANCER_API UFLAsteroidFieldData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Dateiname ohne Endung (wird in Zonen/Systemen referenziert) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName FieldName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLFieldCube> Cubes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLFieldSection> Sections;

	/** Zonen-Nicknames, in denen das Feld ausgespart wird */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> Exclusions;

	/** Wuerfelgroesse in cm (Feld: cube_size, Meter), 0 wenn nicht angegeben */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CubeSize = 0.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLAsteroidField")), FieldName);
	}
};

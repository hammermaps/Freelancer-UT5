// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLShipData.generated.h"

/** Schiffsdefinition, vom Importer (FLImport) aus shiparch.ini erzeugt. Werte sind daten-getrieben, nichts hardcoded. */
class UFLModelData;

UCLASS(BlueprintType)
class FREELANCER_API UFLShipData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName Nickname;

	/** FIGHTER, FREIGHTER, ... (Original-Typ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	FName ShipType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	int32 IdsName = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	int32 IdsInfo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	int32 ShipClass = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float Mass = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 HitPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 HoldSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	float LinearDrag = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	float NudgeForce = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	float StrafeForce = 0.f;

	/** Pitch, Yaw, Roll wie im Original */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	FVector SteeringTorque = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	FVector AngularDrag = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	FVector RotationInertia = FVector::ZeroVector;

	/** Importiertes Modell (Teile, Hardpoints, Meshes); leer, solange kein Modell-Import lief */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UFLModelData> Model;

	/** Pfad des Originalmodells (nur Herkunft, wird zur Laufzeit nicht gelesen) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourceArchetype;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TArray<FString> SourceMaterialLibraries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourceCockpit;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLShip")), Nickname);
	}
};

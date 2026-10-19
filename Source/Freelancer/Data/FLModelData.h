// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLModelData.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EFLJointType : uint8
{
	Fixed,
	Revolute,
	Prismatic
};

USTRUCT(BlueprintType)
struct FREELANCER_API FFLHardpoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bRevolute = false;

	/** Relativ zum Teil (Unreal-Koordinaten, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform Transform;
};

USTRUCT(BlueprintType)
struct FREELANCER_API FFLModelPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	/** Index des Elternteils in Parts, -1 = Wurzel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ParentIndex = -1;

	/** Relativ zum Elternteil (Unreal-Koordinaten, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform LocalTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLHardpoint> Hardpoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EFLJointType Joint = EFLJointType::Fixed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Axis = FVector::ZeroVector;

	/** Grenzen der Bewegung (Rev: Bogenmass, Pris: cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Max = 0.f;
};

/** Zusammengesetztes Modell (Schiff, Station, Ausruestung) aus dem Original-Import. Der Server braucht nur Struktur und Hardpoints, keine Meshes. */
UCLASS(BlueprintType)
class FREELANCER_API UFLModelData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLModelPart> Parts;

	/** Index des Teils mit Hardpoint dieses Namens oder -1 */
	UFUNCTION(BlueprintPure)
	int32 FindHardpoint(FName HardpointName, int32& OutPartIndex) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLModel")), GetFName());
	}
};

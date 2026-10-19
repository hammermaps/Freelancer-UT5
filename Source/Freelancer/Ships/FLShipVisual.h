// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FLShipVisual.generated.h"

class UFLModelData;
class UStaticMeshComponent;

/**
 * Baut aus einem importierten UFLModelData (Teile, Hierarchie) die sichtbare Huelle eines Schiffs/Objekts.
 * Reine Darstellung; Flugphysik und Replikation liegen in eigenen Klassen (M2).
 */
UCLASS(Blueprintable)
class FREELANCER_API AFLShipVisual : public AActor
{
	GENERATED_BODY()

public:
	AFLShipVisual();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	TSoftObjectPtr<UFLModelData> Model;

	/** (Neu) aufbauen: entfernt vorhandene Teilkomponenten und erzeugt sie aus Model. */
	UFUNCTION(BlueprintCallable, Category = "Model")
	void RebuildFromModel();

	/** Komponente des Teils (nach Name) oder nullptr */
	UFUNCTION(BlueprintPure, Category = "Model")
	UStaticMeshComponent* FindPart(FName PartName) const;

	/** Weltposition eines Hardpoints (Name wie im Original, z. B. HpWeapon01); false wenn unbekannt. */
	UFUNCTION(BlueprintPure, Category = "Model")
	bool GetHardpointTransform(FName HardpointName, FTransform& OutWorld) const;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;
};

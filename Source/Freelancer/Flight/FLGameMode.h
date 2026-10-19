// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FLGameMode.generated.h"

class UFLShipData;
class UFLEquipmentData;

/** Spielmodus der Testwelt: jeder Spieler bekommt ein AFLShip (Standard: Liberty-Fighter mit Standard-Triebwerk). */
UCLASS()
class FREELANCER_API AFLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFLGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TSoftObjectPtr<UFLShipData> DefaultShip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TSoftObjectPtr<UFLEquipmentData> DefaultEngine;

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};

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

	/** Abstand der Startplaetze im Raster (cm); mehr Spieler als Startpunkte werden nebeneinander gesetzt, damit sie nicht ueberlappen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	double SpawnGridSpacing = 4000.0;

	/** System, dessen feste Objekte (Planeten, Stationen) auf dem Server als Kollisionskugeln registriert werden; leer = keins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System")
	FName DefaultSystem;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
private:
	int32 SpawnCounter = 0;
	FVector SpawnOffset = FVector::ZeroVector; // -FLSpawnOffset=x,y,z (cm): Startregion verschieben (Tests ausserhalb von Sonne/Objekten)
};

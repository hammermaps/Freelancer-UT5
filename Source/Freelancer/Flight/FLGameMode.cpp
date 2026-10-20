// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLGameMode.h"

#include "Engine/World.h"
#include "Flight/FLShip.h"

AFLGameMode::AFLGameMode()
{
	DefaultPawnClass = AFLShip::StaticClass();
	DefaultShip = TSoftObjectPtr<UFLShipData>(FSoftObjectPath(TEXT("/Game/Imported/Ships/DA_Ship_li_fighter.DA_Ship_li_fighter")));
	DefaultEngine = TSoftObjectPtr<UFLEquipmentData>(FSoftObjectPath(TEXT("/Game/Imported/Equipment/DA_Equip_ge_gf2_engine_01.DA_Equip_ge_gf2_engine_01")));
}

APawn* AFLGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Raster um den Startpunkt: 10 Plaetze in Y, dann naechste Reihe in Z
	FTransform Placed = SpawnTransform;
	const int32 Index = SpawnCounter++;
	Placed.AddToTranslation(FVector(0.0, SpawnGridSpacing * (Index % 10), SpawnGridSpacing * (Index / 10)));
	AFLShip* Ship = GetWorld()->SpawnActorDeferred<AFLShip>(AFLShip::StaticClass(), Placed, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ship)
	{
		Ship->ShipData = DefaultShip;
		Ship->Engine = DefaultEngine;
		Ship->FinishSpawning(Placed);
	}
	return Ship;
}

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
	AFLShip* Ship = GetWorld()->SpawnActorDeferred<AFLShip>(AFLShip::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ship)
	{
		Ship->ShipData = DefaultShip;
		Ship->Engine = DefaultEngine;
		Ship->FinishSpawning(SpawnTransform);
	}
	return Ship;
}

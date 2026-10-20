// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLGameMode.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Flight/FLShip.h"
#include "Flight/FLStaticCollision.h"

AFLGameMode::AFLGameMode()
{
	DefaultPawnClass = AFLShip::StaticClass();
	DefaultShip = TSoftObjectPtr<UFLShipData>(FSoftObjectPath(TEXT("/Game/Imported/Ships/DA_Ship_li_fighter.DA_Ship_li_fighter")));
	DefaultEngine = TSoftObjectPtr<UFLEquipmentData>(FSoftObjectPath(TEXT("/Game/Imported/Equipment/DA_Equip_ge_gf2_engine_01.DA_Equip_ge_gf2_engine_01")));
}

void AFLGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// System per Option (?System=Li01) oder Kommandozeile (-FLSystem=Li01) waehlen
	FString Sys = UGameplayStatics::ParseOption(Options, TEXT("System"));
	if (Sys.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("-FLSystem="), Sys);
	}
	if (!Sys.IsEmpty())
	{
		DefaultSystem = FName(*Sys);
	}
	FString Offset;
	if (FParse::Value(FCommandLine::Get(), TEXT("-FLSpawnOffset="), Offset, false) /* false: nicht am Komma abbrechen */)
	{
		TArray<FString> P;
		Offset.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 3)
		{
			SpawnOffset = FVector(FCString::Atod(*P[0]), FCString::Atod(*P[1]), FCString::Atod(*P[2]));
		}
	}
}

void AFLGameMode::StartPlay()
{
	Super::StartPlay();
	if (GetNetMode() == NM_Client)
	{
		return;
	}
	UFLStaticCollisionSubsystem* Statics = GetWorld()->GetSubsystem<UFLStaticCollisionSubsystem>();
	if (!DefaultSystem.IsNone())
	{
		AFLSystemWorld* SysWorld = GetWorld()->SpawnActorDeferred<AFLSystemWorld>(AFLSystemWorld::StaticClass(), FTransform::Identity);
		if (SysWorld)
		{
			SysWorld->SystemName = DefaultSystem;
			SysWorld->FinishSpawning(FTransform::Identity);
		}
	}
	// Testkugel fuer Netzwerktests: -FLTestSphere=x,y,z,radius (cm)
	FString TestSphere;
	if (Statics && FParse::Value(FCommandLine::Get(), TEXT("-FLTestSphere="), TestSphere, false))
	{
		TArray<FString> P;
		TestSphere.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 4)
		{
			Statics->AddSphere(FFLStaticSphere{FName("TestSphere"), FVector(FCString::Atod(*P[0]), FCString::Atod(*P[1]), FCString::Atod(*P[2])), FCString::Atod(*P[3])});
		}
	}
}

APawn* AFLGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Raster um den Startpunkt: 10 Plaetze in Y, dann naechste Reihe in Z
	FTransform Placed = SpawnTransform;
	const int32 Index = SpawnCounter++;
	Placed.AddToTranslation(SpawnOffset + FVector(0.0, SpawnGridSpacing * (Index % 10), SpawnGridSpacing * (Index / 10)));
	AFLShip* Ship = GetWorld()->SpawnActorDeferred<AFLShip>(AFLShip::StaticClass(), Placed, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ship)
	{
		Ship->ShipData = DefaultShip;
		Ship->Engine = DefaultEngine;
		Ship->FinishSpawning(Placed);
	}
	return Ship;
}

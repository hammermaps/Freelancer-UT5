// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLStaticCollision.h"

#include "Data/FLSolarData.h"
#include "Data/FLSystemData.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLStatic, Log, All);

bool UFLStaticCollisionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Nur Spielwelten (nicht Editor-Vorschau), Server und Standalone brauchen es
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

int32 UFLStaticCollisionSubsystem::LoadSystem(const UFLSystemData& System)
{
	const TArray<FFLStaticSphere> Built = FLCollision::BuildStaticSpheres(System, [](FName Archetype) -> const UFLSolarData*
	{
		const FString Name = FString::Printf(TEXT("DA_Solar_%s"), *Archetype.ToString().Replace(TEXT("-"), TEXT("_")));
		return LoadObject<UFLSolarData>(nullptr, *FString::Printf(TEXT("/Game/Imported/Solars/%s.%s"), *Name, *Name));
	});
	Spheres.Append(Built);
	return Built.Num();
}

void UFLStaticCollisionSubsystem::Query(const FVector& Position, double Radius, TArray<const FFLStaticSphere*>& OutHits) const
{
	for (const FFLStaticSphere& S : Spheres)
	{
		const double Reach = Radius + S.Radius;
		if (FVector::DistSquared(Position, S.Position) < Reach * Reach)
		{
			OutHits.Add(&S);
		}
	}
}

AFLSystemWorld::AFLSystemWorld()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Kollisionsdaten nur auf dem Server
}

void AFLSystemWorld::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	UFLStaticCollisionSubsystem* Sub = World ? World->GetSubsystem<UFLStaticCollisionSubsystem>() : nullptr;
	if (!Sub || SystemName.IsNone())
	{
		return;
	}
	const FString Asset = FString::Printf(TEXT("DA_System_%s"), *SystemName.ToString());
	const UFLSystemData* System = LoadObject<UFLSystemData>(nullptr, *FString::Printf(TEXT("/Game/Imported/Systems/%s.%s"), *Asset, *Asset));
	if (!System)
	{
		UE_LOG(LogFLStatic, Warning, TEXT("System %s nicht gefunden"), *SystemName.ToString());
		return;
	}
	const int32 Count = Sub->LoadSystem(*System);
	UE_LOG(LogFLStatic, Display, TEXT("System %s: %d feste Kollisionskugeln"), *SystemName.ToString(), Count);
}

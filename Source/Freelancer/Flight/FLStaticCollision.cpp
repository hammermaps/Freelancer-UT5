// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLStaticCollision.h"

#include "Data/FLAsteroidFieldData.h"
#include "Data/FLModelData.h"
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

	// Asteroidenfelder: Feld-Asset + Zone + Ausschlusszonen -> Laufzeitfeld
	TMap<FName, double> RadiusCache;
	auto RadiusOf = [&RadiusCache](FName Asteroid) -> double
	{
		if (const double* Known = RadiusCache.Find(Asteroid))
		{
			return *Known;
		}
		const FString Name = FString::Printf(TEXT("DA_Solar_%s"), *Asteroid.ToString());
		const UFLSolarData* Solar = LoadObject<UFLSolarData>(nullptr, *FString::Printf(TEXT("/Game/Imported/Solars/%s.%s"), *Name, *Name));
		double Radius = 0.0;
		if (Solar)
		{
			Radius = Solar->Radius > 0.f ? Solar->Radius : FLCollision::ModelRadius(Solar->Model.LoadSynchronous(), 0.0);
		}
		RadiusCache.Add(Asteroid, Radius);
		return Radius;
	};
	auto FindZone = [&System](FName Name) -> const FFLSystemZone*
	{
		return System.Zones.FindByPredicate([Name](const FFLSystemZone& Z) { return Z.Nickname == Name; });
	};
	int32 FieldCount = 0;
	for (const FFLSystemField& Link : System.AsteroidFields)
	{
		const FString Name = FString::Printf(TEXT("DA_Field_%s"), *Link.FieldName.ToString());
		const UFLAsteroidFieldData* Data = LoadObject<UFLAsteroidFieldData>(nullptr, *FString::Printf(TEXT("/Game/Imported/AsteroidFields/%s.%s"), *Name, *Name));
		const FFLSystemZone* Zone = FindZone(Link.ZoneName);
		if (!Data || !Zone)
		{
			continue;
		}
		TArray<const FFLSystemZone*> Exclusions;
		for (FName Ex : Data->Exclusions)
		{
			Exclusions.Add(FindZone(Ex));
		}
		FFLFieldRuntime Field = FLAsteroids::BuildField(*Data, Zone, Exclusions, RadiusOf);
		if (!Field.Asteroids.IsEmpty())
		{
			AddField(MoveTemp(Field));
			++FieldCount;
		}
	}
	UE_LOG(LogFLStatic, Display, TEXT("%d Asteroidenfelder aktiv"), FieldCount);
	return Built.Num();
}

void UFLStaticCollisionSubsystem::QueryAsteroids(const FVector& Position, double Radius, TArray<FFLStaticSphere>& Out) const
{
	for (const FFLFieldRuntime& F : Fields)
	{
		// Grobtest: nur Felder, deren Zone in der Naehe liegt (Zonenmitte-Test im Query selbst, hier spart man das Wuerfelraster)
		FLAsteroids::Query(F, Position, Radius, Out);
	}
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

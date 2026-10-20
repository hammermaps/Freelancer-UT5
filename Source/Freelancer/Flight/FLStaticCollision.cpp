// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLStaticCollision.h"

#include "Data/FLAsteroidFieldData.h"
#include "Data/FLModelData.h"
#include "Data/FLSolarData.h"
#include "Data/FLSystemData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLStatic, Log, All);

bool UFLStaticCollisionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Nur Spielwelten (nicht Editor-Vorschau), Server und Standalone brauchen es
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

TArray<FFLFieldRuntime> UFLStaticCollisionSubsystem::BuildFields(const UFLSystemData& System)
{
	TArray<FFLFieldRuntime> Result;
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
			Result.Add(MoveTemp(Field));
		}
	}
	return Result;
}

int32 UFLStaticCollisionSubsystem::LoadSystem(const UFLSystemData& System)
{
	const TArray<FFLStaticSphere> Built = FLCollision::BuildStaticSpheres(System, [](FName Archetype) -> const UFLSolarData*
	{
		const FString Name = FString::Printf(TEXT("DA_Solar_%s"), *Archetype.ToString().Replace(TEXT("-"), TEXT("_")));
		return LoadObject<UFLSolarData>(nullptr, *FString::Printf(TEXT("/Game/Imported/Solars/%s.%s"), *Name, *Name));
	});
	Spheres.Append(Built);

	int32 FieldCount = 0;
	for (FFLFieldRuntime& Field : BuildFields(System))
	{
		AddField(MoveTemp(Field));
		++FieldCount;
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

UFLAsteroidVisualComponent::UFLAsteroidVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

UStaticMesh* UFLAsteroidVisualComponent::FindMesh(FName Asteroid)
{
	if (const TWeakObjectPtr<UStaticMesh>* Known = MeshCache.Find(Asteroid))
	{
		return Known->Get();
	}
	UStaticMesh* Mesh = nullptr;
	const FString Name = FString::Printf(TEXT("DA_Solar_%s"), *Asteroid.ToString());
	if (const UFLSolarData* Solar = LoadObject<UFLSolarData>(nullptr, *FString::Printf(TEXT("/Game/Imported/Solars/%s.%s"), *Name, *Name)))
	{
		if (const UFLModelData* Model = Solar->Model.LoadSynchronous())
		{
			for (const FFLModelPart& Part : Model->Parts)
			{
				if (UStaticMesh* M = Part.Mesh.LoadSynchronous()) { Mesh = M; break; } // erstes Teil mit Mesh (Asteroiden haben genau eines)
			}
		}
	}
	MeshCache.Add(Asteroid, Mesh);
	return Mesh;
}

void UFLAsteroidVisualComponent::RefreshAround(const FVector& Viewer)
{
	TMap<FName, TArray<FTransform>> ByType;
	for (const FFLFieldRuntime& Field : Fields)
	{
		TArray<FFLFieldInstance> Instances;
		FLAsteroids::QueryInstances(Field, Viewer, Field.FillDist, Instances, 3000);
		for (const FFLFieldInstance& I : Instances)
		{
			ByType.FindOrAdd(I.Nickname).Add(FTransform(I.Rotation, I.Position));
		}
	}
	VisibleCount = 0;
	// vorhandene Typen leeren, die nicht mehr vorkommen
	for (auto& Pair : Meshes)
	{
		if (!ByType.Contains(Pair.Key) && Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
	for (auto& Pair : ByType)
	{
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& Comp = Meshes.FindOrAdd(Pair.Key);
		if (!Comp)
		{
			UStaticMesh* Mesh = FindMesh(Pair.Key);
			if (!Mesh) { continue; }
			Comp = NewObject<UHierarchicalInstancedStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
			Comp->SetStaticMesh(Mesh);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Kollision entscheidet der Server (Kugeln)
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetupAttachment(GetOwner()->GetRootComponent());
			Comp->RegisterComponent();
		}
		Comp->ClearInstances();
		Comp->AddInstances(Pair.Value, false, true); // Welt-Transformationen
		VisibleCount += Pair.Value.Num();
	}
}

void UFLAsteroidVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Fields.IsEmpty() || !GetWorld())
	{
		return;
	}
	NextRefresh -= DeltaTime;
	if (NextRefresh > 0.f)
	{
		return;
	}
	NextRefresh = 0.4f;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FVector Loc; FRotator Rot;
		PC->GetPlayerViewPoint(Loc, Rot);
		RefreshAround(Loc);
	}
}

AFLSystemWorld::AFLSystemWorld()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // nur der Systemname; Clients berechnen die Felder selbst (deterministisch) und zeichnen sie
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	AsteroidVisuals = CreateDefaultSubobject<UFLAsteroidVisualComponent>(TEXT("AsteroidVisuals"));
}

void AFLSystemWorld::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFLSystemWorld, SystemName);
}

void AFLSystemWorld::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || SystemName.IsNone())
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
	if (HasAuthority())
	{
		// Server: feste Kollisionskugeln und Asteroidenfelder registrieren
		if (UFLStaticCollisionSubsystem* Sub = World->GetSubsystem<UFLStaticCollisionSubsystem>())
		{
			const int32 Count = Sub->LoadSystem(*System);
			UE_LOG(LogFLStatic, Display, TEXT("System %s: %d feste Kollisionskugeln"), *SystemName.ToString(), Count);
		}
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		// Clients/Standalone: Asteroiden zeichnen
		AsteroidVisuals->SetFields(UFLStaticCollisionSubsystem::BuildFields(*System));
	}
}

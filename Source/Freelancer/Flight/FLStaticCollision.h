// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flight/FLAsteroids.h"
#include "Flight/FLCollision.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "FLStaticCollision.generated.h"

class UFLSystemData;

/** Feste Kollisionskugeln der Welt (Server): Planeten, Stationen, ... Ships fragen sie pro Tick ab. */
UCLASS()
class FREELANCER_API UFLStaticCollisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void AddSphere(const FFLStaticSphere& Sphere) { Spheres.Add(Sphere); }
	void Clear() { Spheres.Reset(); }
	const TArray<FFLStaticSphere>& GetSpheres() const { return Spheres; }

	/** Laufzeitfelder eines Systems (Feld-Assets + Zonen + Radien); Server und Clients (Darstellung) nutzen dieselbe Berechnung. */
	static TArray<FFLFieldRuntime> BuildFields(const UFLSystemData& System);

	/** Registriert alle Objekte eines Systems; Solars werden unter /Game/Imported/Solars/DA_Solar_<Archetyp> gesucht. Gibt die Zahl der Kugeln zurueck. */
	int32 LoadSystem(const UFLSystemData& System);

	void AddField(FFLFieldRuntime&& Field) { Fields.Add(MoveTemp(Field)); }
	const TArray<FFLFieldRuntime>& GetFields() const { return Fields; }

	/** Asteroiden aller Felder in der Naehe einer Kugel (Server, pro Tick und Schiff) */
	void QueryAsteroids(const FVector& Position, double Radius, TArray<FFLStaticSphere>& Out) const;

	/** Kugeln, die eine Kugel (Position, Radius) beruehren; nur der Server nutzt das. */
	void Query(const FVector& Position, double Radius, TArray<const FFLStaticSphere*>& OutHits) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	TArray<FFLStaticSphere> Spheres;
	TArray<FFLFieldRuntime> Fields;
};

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/** Client: zeichnet die Asteroiden der Felder in der Naehe der Kamera (Instanzen je Asteroidentyp). */
UCLASS()
class FREELANCER_API UFLAsteroidVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFLAsteroidVisualComponent();

	void SetFields(TArray<FFLFieldRuntime>&& InFields) { Fields = MoveTemp(InFields); NextRefresh = 0.f; }
	int32 GetVisibleCount() const { return VisibleCount; }

	/** Instanzen fuer die Betrachterposition neu aufbauen (auch direkt aufrufbar, z. B. fuer Tests) */
	void RefreshAround(const FVector& ViewerPosition);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TArray<FFLFieldRuntime> Fields;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Meshes;
	TMap<FName, TWeakObjectPtr<UStaticMesh>> MeshCache;
	float NextRefresh = 0.f;
	int32 VisibleCount = 0;

	UStaticMesh* FindMesh(FName Asteroid);
};

/** Platzierbarer Actor (Server): laedt beim Start ein Sonnensystem und registriert dessen feste Kugeln. */
UCLASS()
class FREELANCER_API AFLSystemWorld : public AActor
{
	GENERATED_BODY()

public:
	AFLSystemWorld();

	/** Nickname des Systems (z. B. Li01); wird repliziert, damit Clients dieselben Felder berechnen und zeichnen. Asset /Game/Imported/Systems/DA_System_<Nickname> */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "System")
	FName SystemName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "System")
	TObjectPtr<UFLAsteroidVisualComponent> AsteroidVisuals;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

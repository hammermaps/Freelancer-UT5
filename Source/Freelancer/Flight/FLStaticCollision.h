// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flight/FLCollision.h"
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

	/** Registriert alle Objekte eines Systems; Solars werden unter /Game/Imported/Solars/DA_Solar_<Archetyp> gesucht. Gibt die Zahl der Kugeln zurueck. */
	int32 LoadSystem(const UFLSystemData& System);

	/** Kugeln, die eine Kugel (Position, Radius) beruehren; nur der Server nutzt das. */
	void Query(const FVector& Position, double Radius, TArray<const FFLStaticSphere*>& OutHits) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	TArray<FFLStaticSphere> Spheres;
};

/** Platzierbarer Actor (Server): laedt beim Start ein Sonnensystem und registriert dessen feste Kugeln. */
UCLASS()
class FREELANCER_API AFLSystemWorld : public AActor
{
	GENERATED_BODY()

public:
	AFLSystemWorld();

	/** Nickname des Systems (z. B. Li01); Asset /Game/Imported/Systems/DA_System_<Nickname> */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System")
	FName SystemName;

	virtual void BeginPlay() override;
};

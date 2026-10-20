// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/FLSystemData.h"
#include "Flight/FLCollision.h"

class UFLAsteroidFieldData;

/** Zone (Kugel, Ellipsoid, Box) mit Lage; Zylinder/Ring werden nicht unterstuetzt (Contains = false). */
struct FREELANCER_API FFLZoneShape
{
	EFLZoneShape Shape = EFLZoneShape::Unknown;
	FVector Center = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Size = FVector::ZeroVector; // Kugel: Radius = Size.X; Ellipsoid: Halbachsen; Box: volle Kantenlaengen (alles cm, Unreal-Achsen)

	bool Contains(const FVector& Point) const;
};

struct FREELANCER_API FFLFieldAsteroid
{
	FVector Offset = FVector::ZeroVector; // Anteil der Wuerfelgroesse, Unreal-Achsen
	double Radius = 0.0;                  // cm (Modellradius)
};

/** Laufzeit-Beschreibung eines Asteroidenfelds, das eine Zone ausfuellt. */
struct FREELANCER_API FFLFieldRuntime
{
	FName Name;
	double CubeSize = 40000.0;           // cm
	double EmptyFrequency = 0.5;         // Anteil leerer Wuerfel
	TArray<FFLFieldAsteroid> Asteroids;  // Inhalt jedes (nicht leeren) Wuerfels
	FFLZoneShape Zone;
	TArray<FFLZoneShape> Exclusions;
	double MaxAsteroidRadius = 0.0;
};

namespace FLAsteroids
{
	/** Stabiler Hash der Wuerfelindizes (Server und Clients muessen dasselbe Feld sehen). */
	FREELANCER_API uint32 HashCube(int64 X, int64 Y, int64 Z);

	/** Ist der Wuerfel (nach EmptyFrequency) nicht leer? */
	FREELANCER_API bool CubeExists(int64 X, int64 Y, int64 Z, double EmptyFrequency);

	/** Zonenform aus importierten Zonendaten. */
	FREELANCER_API FFLZoneShape MakeZone(const FFLSystemZone& Zone);

	/**
	 * Kugeln aller Feld-Asteroiden, die eine Kugel (Position, Radius) beruehren koennten: Wuerfel im Umkreis, nicht leer,
	 * Wuerfelmitte in der Zone und nicht in einer Ausschlusszone. MaxResults begrenzt die Ausgabe.
	 */
	FREELANCER_API void Query(const FFLFieldRuntime& Field, const FVector& Position, double Radius, TArray<FFLStaticSphere>& Out, int32 MaxResults = 256);

	/** Laufzeitfeld aus Feld-Asset, Zonenliste und Radius-Funktion (Asteroid-Nickname -> Radius in cm, 0 = ueberspringen). */
	FREELANCER_API FFLFieldRuntime BuildField(const UFLAsteroidFieldData& Data, const FFLSystemZone* Zone, const TArray<const FFLSystemZone*>& Exclusions,
		TFunctionRef<double(FName)> RadiusOf);
}

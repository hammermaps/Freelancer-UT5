// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UFLModelData;

/** Kugel-Koerper fuer die Kollision (Einheiten: cm, cm/s, kg). */
struct FREELANCER_API FFLCollisionBody
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	double Mass = 100.0;
	double Radius = 500.0;
};

struct FREELANCER_API FFLCollisionResult
{
	bool bHit = false;
	FVector Normal = FVector::ForwardVector;   // von A nach B
	double Penetration = 0.0;                  // cm
	double ImpactSpeed = 0.0;                  // Annaeherungsgeschwindigkeit entlang der Normalen, cm/s (Grundlage fuer Schaden)
	FVector DeltaVelocityA = FVector::ZeroVector;
	FVector DeltaVelocityB = FVector::ZeroVector;
	FVector PushA = FVector::ZeroVector;       // Verschiebung zum Aufloesen der Ueberlappung
	FVector PushB = FVector::ZeroVector;
};

namespace FLCollision
{
	/**
	 * Kugel gegen Kugel: bei Ueberlappung Impuls (Restitution 0 = plastisch, 1 = elastisch) und Aufloesung nach Masse.
	 * Rein und deterministisch; Server wendet das Ergebnis an (Schaden aus ImpactSpeed folgt im Kampfsystem).
	 */
	FREELANCER_API FFLCollisionResult ResolveSpheres(const FFLCollisionBody& A, const FFLCollisionBody& B, double Restitution = 0.4);

	/** Huellkugel eines Modells: umfasst alle Teile (Teilposition + Radius der Mesh-Grenzen); ohne Modell/Meshes DefaultRadius. */
	FREELANCER_API double ModelRadius(const UFLModelData* Model, double DefaultRadius = 500.0);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLCollision.h"

#include "Data/FLModelData.h"
#include "Engine/StaticMesh.h"

FFLCollisionResult FLCollision::ResolveSpheres(const FFLCollisionBody& A, const FFLCollisionBody& B, double Restitution)
{
	FFLCollisionResult R;
	const FVector Delta = B.Position - A.Position;
	const double Dist = Delta.Size();
	const double Penetration = A.Radius + B.Radius - Dist;
	if (Penetration <= 0.0)
	{
		return R;
	}
	R.bHit = true;
	R.Penetration = Penetration;
	R.Normal = Dist > KINDA_SMALL_NUMBER ? Delta / Dist : FVector::ForwardVector;

	const double MassA = FMath::Max(A.Mass, KINDA_SMALL_NUMBER);
	const double MassB = FMath::Max(B.Mass, KINDA_SMALL_NUMBER);
	const double TotalMass = MassA + MassB;
	// Ueberlappung aufloesen: der leichtere Koerper bewegt sich mehr
	R.PushA = -R.Normal * Penetration * (MassB / TotalMass);
	R.PushB = R.Normal * Penetration * (MassA / TotalMass);

	// Impuls nur wenn sich die Koerper annaehern
	const double Closing = FVector::DotProduct(A.Velocity - B.Velocity, R.Normal);
	if (Closing > 0.0)
	{
		R.ImpactSpeed = Closing;
		const double J = (1.0 + FMath::Clamp(Restitution, 0.0, 1.0)) * Closing / (1.0 / MassA + 1.0 / MassB);
		R.DeltaVelocityA = -R.Normal * (J / MassA);
		R.DeltaVelocityB = R.Normal * (J / MassB);
	}
	return R;
}

double FLCollision::ModelRadius(const UFLModelData* Model, double DefaultRadius)
{
	if (!Model || Model->Parts.IsEmpty())
	{
		return DefaultRadius;
	}
	// Weltposition der Teile entlang der Hierarchie
	TArray<FTransform> World;
	World.SetNum(Model->Parts.Num());
	double Radius = 0.0;
	bool bAny = false;
	for (int32 I = 0; I < Model->Parts.Num(); ++I)
	{
		const FFLModelPart& P = Model->Parts[I];
		World[I] = (P.ParentIndex >= 0 && P.ParentIndex < I) ? P.LocalTransform * World[P.ParentIndex] : P.LocalTransform;
		if (const UStaticMesh* Mesh = P.Mesh.LoadSynchronous())
		{
			const FBoxSphereBounds B = Mesh->GetBounds();
			const FVector Center = World[I].TransformPosition(B.Origin);
			Radius = FMath::Max(Radius, Center.Size() + B.SphereRadius * World[I].GetMaximumAxisScale());
			bAny = true;
		}
	}
	return bAny ? Radius : DefaultRadius;
}

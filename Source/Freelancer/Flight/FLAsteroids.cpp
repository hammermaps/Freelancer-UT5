// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLAsteroids.h"

#include "Data/FLAsteroidFieldData.h"

bool FFLZoneShape::Contains(const FVector& Point) const
{
	const FVector Local = Rotation.UnrotateVector(Point - Center);
	switch (Shape)
	{
	case EFLZoneShape::Sphere:
		return Local.SizeSquared() <= Size.X * Size.X;
	case EFLZoneShape::Ellipsoid:
	{
		if (Size.X <= 0.0 || Size.Y <= 0.0 || Size.Z <= 0.0)
		{
			return false;
		}
		const double X = Local.X / Size.X, Y = Local.Y / Size.Y, Z = Local.Z / Size.Z;
		return X * X + Y * Y + Z * Z <= 1.0;
	}
	case EFLZoneShape::Box:
		return FMath::Abs(Local.X) <= Size.X * 0.5 && FMath::Abs(Local.Y) <= Size.Y * 0.5 && FMath::Abs(Local.Z) <= Size.Z * 0.5;
	default:
		return false;
	}
}

uint32 FLAsteroids::HashCube(int64 X, int64 Y, int64 Z)
{
	// 32-Bit-Mischfunktion (Murmur-artig) ueber die Indizes
	auto Mix = [](uint64 H)
	{
		H ^= H >> 33; H *= 0xff51afd7ed558ccdULL;
		H ^= H >> 33; H *= 0xc4ceb9fe1a85ec53ULL;
		H ^= H >> 33;
		return H;
	};
	uint64 H = Mix((uint64)X + 0x9e3779b97f4a7c15ULL);
	H = Mix(H ^ ((uint64)Y * 0xbf58476d1ce4e5b9ULL));
	H = Mix(H ^ ((uint64)Z * 0x94d049bb133111ebULL));
	return (uint32)(H >> 32);
}

bool FLAsteroids::CubeExists(int64 X, int64 Y, int64 Z, double EmptyFrequency)
{
	if (EmptyFrequency >= 1.0)
	{
		return false;
	}
	const double Value = (double)HashCube(X, Y, Z) / 4294967296.0; // [0,1)
	return Value >= EmptyFrequency;
}

FFLZoneShape FLAsteroids::MakeZone(const FFLSystemZone& Zone)
{
	FFLZoneShape S;
	S.Shape = Zone.Shape;
	S.Center = Zone.Position;
	S.Rotation = Zone.Rotation;
	S.Size = Zone.Size;
	return S;
}

void FLAsteroids::Query(const FFLFieldRuntime& Field, const FVector& Position, double Radius, TArray<FFLStaticSphere>& Out, int32 MaxResults)
{
	if (Field.CubeSize <= 0.0 || Field.Asteroids.IsEmpty())
	{
		return;
	}
	// Asteroiden liegen bis zu Offset*CubeSize von der Mitte entfernt (Offsets bis 1): Suchbereich entsprechend erweitern
	const double Reach = Radius + Field.MaxAsteroidRadius + Field.CubeSize * 1.75;
	const int64 MinX = (int64)FMath::FloorToDouble((Position.X - Reach) / Field.CubeSize), MaxX = (int64)FMath::FloorToDouble((Position.X + Reach) / Field.CubeSize);
	const int64 MinY = (int64)FMath::FloorToDouble((Position.Y - Reach) / Field.CubeSize), MaxY = (int64)FMath::FloorToDouble((Position.Y + Reach) / Field.CubeSize);
	const int64 MinZ = (int64)FMath::FloorToDouble((Position.Z - Reach) / Field.CubeSize), MaxZ = (int64)FMath::FloorToDouble((Position.Z + Reach) / Field.CubeSize);
	for (int64 IX = MinX; IX <= MaxX; ++IX)
	{
		for (int64 IY = MinY; IY <= MaxY; ++IY)
		{
			for (int64 IZ = MinZ; IZ <= MaxZ; ++IZ)
			{
				if (!CubeExists(IX, IY, IZ, Field.EmptyFrequency))
				{
					continue;
				}
				const FVector Center((IX + 0.5) * Field.CubeSize, (IY + 0.5) * Field.CubeSize, (IZ + 0.5) * Field.CubeSize);
				if (!Field.Zone.Contains(Center))
				{
					continue;
				}
				bool bExcluded = false;
				for (const FFLZoneShape& E : Field.Exclusions)
				{
					if (E.Contains(Center)) { bExcluded = true; break; }
				}
				if (bExcluded)
				{
					continue;
				}
				for (int32 A = 0; A < Field.Asteroids.Num(); ++A)
				{
					const FFLFieldAsteroid& Ast = Field.Asteroids[A];
					const FVector P = Center + Ast.Offset * Field.CubeSize;
					const double Touch = Radius + Ast.Radius;
					if (FVector::DistSquared(P, Position) <= Touch * Touch)
					{
						FFLStaticSphere S;
						S.Id = FName(*FString::Printf(TEXT("%s_%lld_%lld_%lld_%d"), *Field.Name.ToString(), IX, IY, IZ, A));
						S.Position = P;
						S.Radius = Ast.Radius;
						Out.Add(S);
						if (Out.Num() >= MaxResults)
						{
							return;
						}
					}
				}
			}
		}
	}
}

FFLFieldRuntime FLAsteroids::BuildField(const UFLAsteroidFieldData& Data, const FFLSystemZone* Zone, const TArray<const FFLSystemZone*>& Exclusions,
	TFunctionRef<double(FName)> RadiusOf)
{
	FFLFieldRuntime F;
	F.Name = Data.FieldName;
	F.CubeSize = Data.CubeSize;
	for (const FFLFieldSection& S : Data.Sections)
	{
		if (S.Name == FName("Field"))
		{
			F.EmptyFrequency = FCString::Atod(*S.Properties.FindRef(FName("empty_cube_frequency")));
		}
	}
	if (Zone)
	{
		F.Zone = MakeZone(*Zone);
	}
	for (const FFLSystemZone* E : Exclusions)
	{
		if (E) { F.Exclusions.Add(MakeZone(*E)); }
	}
	for (const FFLFieldCube& C : Data.Cubes)
	{
		const double R = RadiusOf(C.Asteroid);
		if (R <= 0.0)
		{
			continue;
		}
		FFLFieldAsteroid A;
		A.Offset = C.Offset;
		A.Radius = R;
		F.Asteroids.Add(A);
		F.MaxAsteroidRadius = FMath::Max(F.MaxAsteroidRadius, R);
	}
	return F;
}

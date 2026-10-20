// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLSystemData.generated.h"

/** Objekt eines Sonnensystems (Planet, Station, Sprungtor, Asteroidenfeld-Marker, ...) aus [Object] der System-INI. */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLSystemObject
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Nickname;

	/** Archetyp-Nickname (Solar-Archetyp, Planet, Sprungtor, ...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Archetype;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 IdsName = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 IdsInfo = 0;

	/** Unreal-Koordinaten, cm (Large World Coordinates) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FQuat Rotation = FQuat::Identity;

	/** Zugehoerige Basis (Station/Planet mit Landung), sonst leer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Base;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Reputation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Loadout;

	/** "goto = Zielsystem, Zielobjekt, Effekt" bei Sprungtoren/-loechern */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GotoSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GotoObject;

	/** Alle INI-Eintraege (Schluessel klein, Wiederholungen als "key#2") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> Properties;
};

UENUM(BlueprintType)
enum class EFLZoneShape : uint8
{
	Sphere,
	Ellipsoid,
	Box,
	Cylinder,
	Ring,
	Unknown
};

/** Zone ([zone]): Gefahrenbereich, Nebel, Asteroidenfeld, Patrouille, Verbot, ... */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLSystemZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EFLZoneShape Shape = EFLZoneShape::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FQuat Rotation = FQuat::Identity;

	/** Halbachsen bzw. Radius in cm (Unreal-Achsen); bei Kugel alle gleich */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Size = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> Properties;
};

/** Verknuepfung "Asteroidenfeld fuellt Zone" aus [Asteroids] der System-INI. */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLSystemField
{
	GENERATED_BODY()

	/** Dateiname des Felds ohne Endung, klein (Asset DA_Field_<Name>) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName FieldName;

	/** Zone, die das Feld ausfuellt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ZoneName;
};

/** Ein Sonnensystem aus universe.ini + System-INI. Der Server leitet daraus Instanzen und Spawns ab. */
UCLASS(BlueprintType)
class FREELANCER_API UFLSystemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 IdsName = 0;

	/** Position auf der Universumskarte (x, y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D MapPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLSystemObject> Objects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLSystemZone> Zones;

	/** Asteroidenfelder dieses Systems (Feld + Zone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLSystemField> AsteroidFields;

	/** Basen dieses Systems aus universe.ini (Nickname) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> Bases;

	/** Eintraege der universe.ini-Sektion [system] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> Properties;

	/** Eintraege von [SystemInfo] der System-INI (Navigationskarte, Ruf, ...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> Info;

	UFUNCTION(BlueprintPure)
	const FFLSystemObject& FindObjectOrDefault(FName ObjectNickname, bool& bFound) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLSystem")), Nickname);
	}
};

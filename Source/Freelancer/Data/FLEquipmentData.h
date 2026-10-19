// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLEquipmentData.generated.h"

/**
 * Ausruestung (Waffe, Antrieb, Schild, Munition, ...), vom Importer aus *_equip.ini erzeugt.
 * Gemeinsame Felder sind typisiert, alle weiteren Eintraege stehen in Properties (Schluessel klein; Wiederholungen als "key#2", "key#3").
 */
class UFLModelData;

UCLASS(BlueprintType)
class FREELANCER_API UFLEquipmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName Nickname;

	/** Sektionstyp im Original: Gun, Engine, ShieldGenerator, Power, Munition, ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName EquipType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	int32 IdsName = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	int32 IdsInfo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	float Mass = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	float Volume = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	int32 HitPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TMap<FName, FString> Properties;

	/** Importiertes Modell (nur wenn das Original eine .cmp/.3db hat und der Modell-Import lief) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TSoftObjectPtr<UFLModelData> Model;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourceArchetype;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TArray<FString> SourceMaterialLibraries;

	/** Zahl aus Properties (erster Wert), Default wenn nicht vorhanden */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetFloatProperty(FName Key, float Default = 0.f) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FString GetStringProperty(FName Key) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLEquipment")), Nickname);
	}
};

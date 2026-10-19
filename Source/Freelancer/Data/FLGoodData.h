// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLGoodData.generated.h"

/** Handelsware aus goods.ini / *_good.ini (Ausruestung, Rohstoffe, Schiffe). Preis ist der Basispreis; Maerkte folgen. */
UCLASS(BlueprintType)
class FREELANCER_API UFLGoodData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good")
	FName Nickname;

	/** equipment, commodity, ship, shiphull */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good")
	int64 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good")
	bool bCombinable = false;

	/** Nickname der zugehoerigen Ausruestung bzw. des Schiffs (equipment= / ship=) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good")
	FName Reference;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good")
	TMap<FName, FString> Properties;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLGood")), Nickname);
	}
};

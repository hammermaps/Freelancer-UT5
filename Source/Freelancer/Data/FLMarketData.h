// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FLMarketData.generated.h"

UENUM(BlueprintType)
enum class EFLMarketKind : uint8
{
	Commodity,
	Equipment,
	Ship
};

/** Eine Ware im Angebot einer Basis. Preis = Basispreis der Ware (UFLGoodData::Price) * Multiplier. */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLMarketGood
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Good;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EFLMarketKind Kind = EFLMarketKind::Equipment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Rank = 0;

	/** Benoetigter Ruf bei der Basisfraktion, -1 = keiner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Reputation = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Min = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Max = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPreserve = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PriceMultiplier = 1.f;
};

/** Markt einer Basis. Server-autoritativ: Preise und Verfuegbarkeit kommen nur von hier. */
UCLASS(BlueprintType)
class FREELANCER_API UFLMarketData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Base;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFLMarketGood> Goods;

	/** Eintrag zur Ware oder nullptr */
	const FFLMarketGood* FindGood(FName GoodNickname) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("FLMarket")), Base);
	}
};

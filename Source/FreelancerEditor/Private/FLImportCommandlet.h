// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "FLImportCommandlet.generated.h"

class UFLShipData;
class UFLEquipmentData;
class UFLGoodData;
namespace fl { struct FlShipRecord; struct FlRecord; }

/**
 * Importiert die Freelancer-Originaldaten nach /Game/Imported/.
 *   UnrealEditor-Cmd Freelancer.uproject -run=FLImport -source=<Pfad zu FL/DATA>
 */
UCLASS()
class UFLImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UFLImportCommandlet();
	virtual int32 Main(const FString& Params) override;

	/** Fuellt ein Data Asset aus einem geparsten Schiffseintrag (ohne Dateizugriff, testbar). */
	static void FillShipData(UFLShipData* Data, const fl::FlShipRecord& Record);

	static void FillEquipmentData(UFLEquipmentData* Data, const fl::FlRecord& Record);
	static void FillGoodData(UFLGoodData* Data, const fl::FlRecord& Record);

	/** Properties -> TMap: Schluessel klein, Wiederholungen als "key#2", "key#3". */
	static void FillProperties(TMap<FName, FString>& Out, const fl::FlRecord& Record);

private:
	int32 ImportShips(const FString& DataDir);
	int32 ImportEquipment(const FString& DataDir);
	int32 ImportGoods(const FString& DataDir);
};

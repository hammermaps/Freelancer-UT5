// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "FLImportCommandlet.generated.h"

class UFLShipData;
namespace fl { struct FlShipRecord; }

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

private:
	int32 ImportShips(const FString& DataDir);
};

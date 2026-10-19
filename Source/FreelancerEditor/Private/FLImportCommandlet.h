// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <vector>
#include "Commandlets/Commandlet.h"
#include "FLImportCommandlet.generated.h"

class UFLShipData;
class UFLEquipmentData;
class UFLGoodData;
class UFLSystemData;
class UFLMarketData;
class UFLSolarData;
class UFLFactionData;
class UFLAsteroidFieldData;
namespace fl { struct FlAsteroidField; }
namespace fl { struct FlFaction; struct FlSphere; }
class FFLModelImporter;
namespace fl { struct FlBaseMarket; }
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
	/** Fuellt ein Solar-Archetyp-Asset aus einem [Solar]-Eintrag der solararch.ini. */
	static void FillSolarData(UFLSolarData* Data, const fl::FlRecord& Record);

	/** Uebernimmt Radius, Seiten und Seitenmaterialien einer Planeten-Kugel (.sph). */
	static void ApplySphere(UFLSolarData* Data, const fl::FlSphere& Sphere);

	static void FillFactionData(UFLFactionData* Data, const fl::FlFaction& Faction);

	static void FillAsteroidFieldData(UFLAsteroidFieldData* Data, const FString& FieldName, const fl::FlAsteroidField& Field);

	static void FillGoodData(UFLGoodData* Data, const fl::FlRecord& Record);

	/** Fuellt ein System aus dem [system]-Eintrag der universe.ini und den Records der System-INI (Objekte, Zonen, Info). */
	static void FillSystemData(UFLSystemData* Data, const fl::FlRecord& UniverseEntry, const std::vector<fl::FlRecord>& Objects,
		const std::vector<fl::FlRecord>& Zones, const fl::FlRecord* Info, const TArray<FName>& Bases);

	/** Fuegt die Waren eines Marktes (Kommodity/Ausruestung/Schiff) dem Marktasset hinzu. */
	static void AddMarketGoods(UFLMarketData* Data, const fl::FlBaseMarket& Market, uint8 Kind);

	/** Properties -> TMap: Schluessel klein, Wiederholungen als "key#2", "key#3". */
	static void FillProperties(TMap<FName, FString>& Out, const fl::FlRecord& Record);

private:
	int32 ImportShips(const FString& DataDir, const FString& ModelFilter, FFLModelImporter& Models);
	int32 ImportSolars(const FString& DataDir, bool bWithModels, FFLModelImporter& Models);
	int32 ImportEquipment(const FString& DataDir, bool bWithModels, FFLModelImporter& Models);
	int32 ImportGoods(const FString& DataDir);
	int32 ImportUniverse(const FString& DataDir);
	int32 ImportMarkets(const FString& DataDir);
	int32 ImportFactions(const FString& DataDir);
	int32 ImportAsteroidFields(const FString& DataDir);
};

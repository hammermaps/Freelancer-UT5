// Copyright Epic Games, Inc. All Rights Reserved.

#include "FLImportCommandlet.h"

#include "Data/FLShipData.h"
#include "Misc/AutomationTest.h"
#include "Data/FLEquipmentData.h"
#include "Data/FLGoodData.h"
#include "FlRecords.h"
#include "FlShipData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLShipImportTest, "Freelancer.Import.ShipData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLShipImportTest::RunTest(const FString& Parameters)
{
	const char* Text =
		"[Ship]\nnickname = li_fighter\ntype = FIGHTER\nmass = 100.0\nhit_pts = 1300\nhold_size = 25\n"
		"steering_torque = 24000, 24000, 58000\nDA_archetype = ships\\liberty\\li_fighter.cmp\n"
		"material_library = a.mat\nmaterial_library = fx\\b.mat\n[Ship]\ntype = X\n";
	fl::FlShipRecord Rec;
	const auto Ships = fl::ParseShipArch(fl::IniFile::Parse(Text));
	TestEqual(TEXT("Eintrag ohne nickname wird uebersprungen"), Ships.size(), (size_t)1);
	if (Ships.size() != 1)
	{
		return false;
	}

	UFLShipData* Data = NewObject<UFLShipData>(GetTransientPackage());
	UFLImportCommandlet::FillShipData(Data, Ships[0]);
	TestEqual(TEXT("Nickname"), Data->Nickname, FName("li_fighter"));
	TestEqual(TEXT("Typ"), Data->ShipType, FName("FIGHTER"));
	TestEqual(TEXT("Masse"), Data->Mass, 100.f);
	TestEqual(TEXT("Trefferpunkte"), Data->HitPoints, 1300);
	TestEqual(TEXT("Steuerkraft Roll"), Data->SteeringTorque.Z, 58000.0);
	TestEqual(TEXT("Archetyp mit Schraegstrichen"), Data->SourceArchetype, FString(TEXT("ships/liberty/li_fighter.cmp")));
	TestEqual(TEXT("Materialbibliotheken"), Data->SourceMaterialLibraries.Num(), 2);
	TestEqual(TEXT("Primary Asset Id"), Data->GetPrimaryAssetId().ToString(), FString(TEXT("FLShip:li_fighter")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLEquipmentImportTest, "Freelancer.Import.Equipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLEquipmentImportTest::RunTest(const FString& Parameters)
{
	const char* Text =
		"[Gun]\nnickname = laser01\nids_name = 5\nmass = 10\nhit_pts = 400\nDA_archetype = equipment\\w.cmp\n"
		"material_library = a.mat\nmaterial_library = b.mat\nrefire_delay = 0.5\nhp_gun_type = hp_gun_special_1\nhp_gun_type = hp_gun_special_2\n"
		"[LOD]\nnickname = ignoriert\n[Good]\nnickname = laser01\ncategory = equipment\nprice = 1234\ncombinable = false\nequipment = laser01\n";
	const fl::IniFile Ini = fl::IniFile::Parse(Text);
	const auto Eq = fl::ParseRecords(Ini, fl::NonEquipmentSections());
	TestEqual(TEXT("LOD und Good werden uebersprungen"), (int32)Eq.size(), 1);
	if (Eq.size() != 1)
	{
		return false;
	}
	UFLEquipmentData* E = NewObject<UFLEquipmentData>(GetTransientPackage());
	UFLImportCommandlet::FillEquipmentData(E, Eq[0]);
	TestEqual(TEXT("Typ"), E->EquipType, FName("Gun"));
	TestEqual(TEXT("Masse"), E->Mass, 10.f);
	TestEqual(TEXT("Trefferpunkte"), E->HitPoints, 400);
	TestEqual(TEXT("Archetyp"), E->SourceArchetype, FString(TEXT("equipment/w.cmp")));
	TestEqual(TEXT("Materialbibliotheken"), E->SourceMaterialLibraries.Num(), 2);
	TestEqual(TEXT("Zahl aus Properties"), E->GetFloatProperty("refire_delay"), 0.5f);
	TestEqual(TEXT("Fehlende Property -> Default"), E->GetFloatProperty("nope", 7.f), 7.f);
	TestEqual(TEXT("Wiederholter Schluessel 1"), E->GetStringProperty("hp_gun_type"), FString(TEXT("hp_gun_special_1")));
	TestEqual(TEXT("Wiederholter Schluessel 2"), E->GetStringProperty("hp_gun_type#2"), FString(TEXT("hp_gun_special_2")));

	const auto Goods = fl::ParseRecordsOfType(Ini, "good");
	TestEqual(TEXT("eine Ware"), (int32)Goods.size(), 1);
	if (Goods.size() != 1)
	{
		return false;
	}
	UFLGoodData* G = NewObject<UFLGoodData>(GetTransientPackage());
	UFLImportCommandlet::FillGoodData(G, Goods[0]);
	TestEqual(TEXT("Kategorie"), G->Category, FName("equipment"));
	TestEqual(TEXT("Preis"), G->Price, (int64)1234);
	TestEqual(TEXT("Verweis"), G->Reference, FName("laser01"));
	TestFalse(TEXT("combinable"), G->bCombinable);
	return true;
}

#include "Data/FLModelData.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

// Prueft das importierte Modell des Liberty-Fighters (nur wenn der Import gelaufen ist; Content/Imported ist nicht im Repo).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLImportedModelTest, "Freelancer.Import.ImportedModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLImportedModelTest::RunTest(const FString& Parameters)
{
	UFLModelData* Model = LoadObject<UFLModelData>(nullptr, TEXT("/Game/Imported/Models/DA_Model_li_fighter.DA_Model_li_fighter"));
	if (!Model)
	{
		AddInfo(TEXT("li_fighter nicht importiert - Test uebersprungen (FLImport -models=li_fighter ausfuehren)"));
		return true;
	}
	TestTrue(TEXT("mehrere Teile"), Model->Parts.Num() >= 5);

	FBox Bounds(ForceInit);
	int32 MeshParts = 0, Hardpoints = 0, MissingMaterials = 0;
	TArray<FTransform> World;
	World.SetNum(Model->Parts.Num());
	for (int32 I = 0; I < Model->Parts.Num(); ++I)
	{
		const FFLModelPart& P = Model->Parts[I];
		World[I] = (P.ParentIndex >= 0 && P.ParentIndex < I) ? P.LocalTransform * World[P.ParentIndex] : P.LocalTransform;
		Hardpoints += P.Hardpoints.Num();
		if (UStaticMesh* SM = P.Mesh.LoadSynchronous())
		{
			++MeshParts;
			Bounds += SM->GetBounds().GetBox().TransformBy(World[I]);
			for (const FStaticMaterial& M : SM->GetStaticMaterials())
			{
				MissingMaterials += M.MaterialInterface ? 0 : 1;
			}
		}
	}
	TestTrue(TEXT("Teile mit Mesh"), MeshParts >= 3);
	TestTrue(TEXT("Hardpoints vorhanden"), Hardpoints >= 10);
	TestEqual(TEXT("alle Materialien aufgeloest"), MissingMaterials, 0);
	const FVector Size = Bounds.GetSize();
	// Ein Jaeger ist rund 10-20 m lang (X = vorn) und deutlich laenger als hoch
	TestTrue(*FString::Printf(TEXT("Laenge 5-30 m (ist %.1f m)"), Size.X / 100.0), Size.X > 500.0 && Size.X < 3000.0);
	TestTrue(*FString::Printf(TEXT("Nase zeigt nach +X (Laenge %.0f > Hoehe %.0f)"), Size.X, Size.Z), Size.X > Size.Z);
	return true;
}

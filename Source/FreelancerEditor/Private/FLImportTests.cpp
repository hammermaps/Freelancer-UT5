// Copyright Epic Games, Inc. All Rights Reserved.

#include "FLImportCommandlet.h"

#include "Data/FLShipData.h"
#include "Misc/AutomationTest.h"
#include "ShipData.h"

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

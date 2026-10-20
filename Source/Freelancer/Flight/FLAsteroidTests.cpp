// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/FLAsteroidFieldData.h"
#include "Flight/FLAsteroids.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLAsteroidZoneTest, "Freelancer.Flight.AsteroidZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLAsteroidZoneTest::RunTest(const FString& Parameters)
{
	FFLZoneShape Sphere;
	Sphere.Shape = EFLZoneShape::Sphere; Sphere.Center = FVector(1000, 0, 0); Sphere.Size = FVector(500, 500, 500);
	TestTrue(TEXT("Kugel innen"), Sphere.Contains(FVector(1400, 0, 0)));
	TestFalse(TEXT("Kugel aussen"), Sphere.Contains(FVector(1600, 0, 0)));
	TestTrue(TEXT("Kugel Rand"), Sphere.Contains(FVector(1500, 0, 0)));

	FFLZoneShape Ellipsoid;
	Ellipsoid.Shape = EFLZoneShape::Ellipsoid; Ellipsoid.Size = FVector(1000, 200, 100);
	TestTrue(TEXT("Ellipsoid lange Achse"), Ellipsoid.Contains(FVector(950, 0, 0)));
	TestFalse(TEXT("Ellipsoid ausserhalb der kurzen Achse"), Ellipsoid.Contains(FVector(0, 250, 0)));
	TestFalse(TEXT("Ellipsoid Ecke ausserhalb"), Ellipsoid.Contains(FVector(800, 150, 0)));   // (0.8^2 + 0.75^2 > 1)
	TestTrue(TEXT("Ellipsoid Mitte"), Ellipsoid.Contains(FVector::ZeroVector));

	// gedrehte Ellipsoid-Zone: lange Achse zeigt nach +Y
	FFLZoneShape Rotated = Ellipsoid;
	Rotated.Rotation = FRotator(0, 90, 0).Quaternion();
	TestTrue(TEXT("gedreht: lange Achse zeigt nach Y"), Rotated.Contains(FVector(0, 950, 0)));
	TestFalse(TEXT("gedreht: X ist jetzt kurz"), Rotated.Contains(FVector(950, 0, 0)));

	FFLZoneShape Box;
	Box.Shape = EFLZoneShape::Box; Box.Size = FVector(1000, 400, 200);
	TestTrue(TEXT("Box innen (halbe Kantenlaenge)"), Box.Contains(FVector(499, 199, 99)));
	TestFalse(TEXT("Box aussen"), Box.Contains(FVector(501, 0, 0)));

	// Zylinder (Handelsrouten): Radius 750, Hoehe 18000, Achse = lokale Z-Achse
	FFLZoneShape Cyl;
	Cyl.Shape = EFLZoneShape::Cylinder; Cyl.Center = FVector(100, 0, 0); Cyl.Size = FVector(750, 750, 18000);
	TestTrue(TEXT("Zylinder: auf der Achse weit oben"), Cyl.Contains(FVector(100, 0, 8900)));
	TestFalse(TEXT("Zylinder: ueber dem Ende"), Cyl.Contains(FVector(100, 0, 9100)));
	TestTrue(TEXT("Zylinder: am Rand des Radius"), Cyl.Contains(FVector(100 + 740, 0, 0)));
	TestFalse(TEXT("Zylinder: ausserhalb des Radius"), Cyl.Contains(FVector(100 + 760, 0, 0)));
	TestFalse(TEXT("Zylinder: Ecke ausserhalb (Radius ueberschritten)"), Cyl.Contains(FVector(100 + 600, 600, 0)));
	FFLZoneShape CylTilt = Cyl;
	CylTilt.Rotation = FRotator(0, 0, 90).Quaternion();    // Achse kippt in die Y-Richtung
	TestTrue(TEXT("gekippter Zylinder: Achse zeigt nach Y"), CylTilt.Contains(FVector(100, 8900, 0)) || CylTilt.Contains(FVector(100, -8900, 0)));
	TestFalse(TEXT("gekippter Zylinder: Z ist jetzt kurz"), CylTilt.Contains(FVector(100, 0, 8900)));

	FFLZoneShape RingZone;
	RingZone.Shape = EFLZoneShape::Ring; RingZone.Size = FVector(1000, 1000, 1000);
	TestFalse(TEXT("Ring wird nicht unterstuetzt (kommt in den Daten nicht vor)"), RingZone.Contains(FVector::ZeroVector));

	// Aus importierten Zonendaten
	FFLSystemZone Z; Z.Shape = EFLZoneShape::Sphere; Z.Position = FVector(5, 6, 7); Z.Size = FVector(10, 10, 10);
	const FFLZoneShape FromData = FLAsteroids::MakeZone(Z);
	TestTrue(TEXT("MakeZone"), FromData.Center == Z.Position && FromData.Shape == EFLZoneShape::Sphere && FromData.Contains(FVector(5, 6, 16)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLAsteroidFieldTest, "Freelancer.Flight.AsteroidField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLAsteroidFieldTest::RunTest(const FString& Parameters)
{
	// Hash: stabil und gleichverteilt genug
	TestEqual(TEXT("Hash stabil"), FLAsteroids::HashCube(3, -7, 11), FLAsteroids::HashCube(3, -7, 11));
	TestTrue(TEXT("Hash unterscheidet Wuerfel"), FLAsteroids::HashCube(3, -7, 11) != FLAsteroids::HashCube(3, -7, 12) && FLAsteroids::HashCube(1, 2, 3) != FLAsteroids::HashCube(3, 2, 1));
	int32 Existing = 0;
	const int32 Total = 40 * 40 * 40;
	for (int32 X = -20; X < 20; ++X) for (int32 Y = -20; Y < 20; ++Y) for (int32 Z = -20; Z < 20; ++Z)
	{
		Existing += FLAsteroids::CubeExists(X, Y, Z, 0.4) ? 1 : 0;
	}
	TestTrue(*FString::Printf(TEXT("etwa 60 %% der Wuerfel sind nicht leer (ist %.1f %%)"), 100.0 * Existing / Total), FMath::Abs(100.0 * Existing / Total - 60.0) < 2.0);
	TestFalse(TEXT("Leerfrequenz 1: alles leer"), FLAsteroids::CubeExists(0, 0, 0, 1.0));
	TestTrue(TEXT("Leerfrequenz 0: alles gefuellt"), FLAsteroids::CubeExists(5, 5, 5, 0.0));

	// Ein Feld: Wuerfelgroesse 1000, nichts leer, ein Asteroid mit Radius 100 bei Offset (0.2, 0, 0) -> 200 cm neben der Wuerfelmitte
	FFLFieldRuntime Field;
	Field.Name = "test";
	Field.CubeSize = 1000.0;
	Field.EmptyFrequency = 0.0;
	Field.MaxAsteroidRadius = 100.0;
	Field.Asteroids.Add(FFLFieldAsteroid{FVector(0.2, 0, 0), 100.0});
	Field.Zone.Shape = EFLZoneShape::Sphere; Field.Zone.Center = FVector(500, 500, 500); Field.Zone.Size = FVector(5000, 5000, 5000);

	// Wuerfel (0,0,0) hat Mitte (500,500,500), Asteroid bei (700,500,500)
	TArray<FFLStaticSphere> Hits;
	FLAsteroids::Query(Field, FVector(700, 500, 500), 50.0, Hits);
	bool bFound = false;
	for (const FFLStaticSphere& S : Hits) { bFound |= S.Position.Equals(FVector(700, 500, 500), 1e-6) && S.Radius == 100.0; }
	TestTrue(TEXT("Asteroid im Wuerfel wird gefunden"), bFound);
	TestTrue(TEXT("jeder Treffer beruehrt die Abfragekugel"), [&]()
	{
		for (const FFLStaticSphere& S : Hits) { if (FVector::Dist(S.Position, FVector(700, 500, 500)) > S.Radius + 50.0 + 1e-6) { return false; } }
		return true;
	}());

	// Deterministisch: zweite Abfrage liefert dieselben Kugeln in derselben Reihenfolge
	TArray<FFLStaticSphere> Again;
	FLAsteroids::Query(Field, FVector(700, 500, 500), 50.0, Again);
	TestTrue(TEXT("Abfrage deterministisch"), Hits.Num() == Again.Num() && (Hits.IsEmpty() || (Hits[0].Position == Again[0].Position && Hits[0].Id == Again[0].Id)));

	// Ausserhalb der Zone: nichts
	Hits.Reset();
	FLAsteroids::Query(Field, FVector(1.0e6, 0, 0), 50.0, Hits);
	TestEqual(TEXT("ausserhalb der Zone keine Asteroiden"), Hits.Num(), 0);

	// Ausschlusszone um den Wuerfel: nichts
	FFLFieldRuntime Excluded = Field;
	FFLZoneShape Ex; Ex.Shape = EFLZoneShape::Sphere; Ex.Center = FVector(500, 500, 500); Ex.Size = FVector(300, 300, 300);
	Excluded.Exclusions.Add(Ex);
	Hits.Reset();
	FLAsteroids::Query(Excluded, FVector(700, 500, 500), 50.0, Hits);
	TestEqual(TEXT("Ausschlusszone entfernt den Wuerfel"), Hits.Num(), 0);

	// Instanzen fuer die Darstellung: Name, Rotation und Radius kommen mit
	{
		FFLFieldRuntime Vis = Field;
		Vis.Asteroids[0].Nickname = "rock_a";
		Vis.Asteroids[0].Rotation = FRotator(0, 90, 0).Quaternion();
		TArray<FFLFieldInstance> Instances;
		FLAsteroids::QueryInstances(Vis, FVector(700, 500, 500), 50.0, Instances);
		TestTrue(TEXT("Instanz mit Name, Lage, Rotation"), Instances.Num() >= 1 && Instances[0].Nickname == FName("rock_a")
			&& Instances[0].Position.Equals(FVector(700, 500, 500), 1e-6) && Instances[0].Rotation.Equals(Vis.Asteroids[0].Rotation, 1e-9) && Instances[0].Radius == 100.0);
		// Bereich: grosse Sichtweite liefert viele Instanzen, alle innerhalb von Sichtweite + Radius
		Instances.Reset();
		FLAsteroids::QueryInstances(Vis, FVector(500, 500, 500), 3000.0, Instances, 4096);
		TestTrue(TEXT("grosser Bereich liefert viele Instanzen"), Instances.Num() > 20);
		bool bAllClose = true;
		for (const FFLFieldInstance& I : Instances) { bAllClose &= FVector::Dist(I.Position, FVector(500, 500, 500)) <= 3000.0 + I.Radius + 1e-6; }
		TestTrue(TEXT("alle Instanzen im Sichtbereich"), bAllClose);
	}

	// Alles leer: nichts
	FFLFieldRuntime Empty = Field;
	Empty.EmptyFrequency = 1.0;
	Hits.Reset();
	FLAsteroids::Query(Empty, FVector(700, 500, 500), 50.0, Hits);
	TestEqual(TEXT("leeres Feld"), Hits.Num(), 0);

	// Ergebnisbegrenzung
	Hits.Reset();
	FLAsteroids::Query(Field, FVector(500, 500, 500), 5000.0, Hits, 5);
	TestEqual(TEXT("MaxResults begrenzt die Ausgabe"), Hits.Num(), 5);

	// Negative Koordinaten: Wuerfelraster mit Rundung nach unten
	FFLFieldRuntime Neg = Field;
	Neg.Zone.Center = FVector(-500, -500, -500);
	Hits.Reset();
	FLAsteroids::Query(Neg, FVector(-300, -500, -500), 50.0, Hits);   // Wuerfel (-1,-1,-1): Mitte (-500,-500,-500), Asteroid bei (-300,-500,-500)
	bool bNegFound = false;
	for (const FFLStaticSphere& S : Hits) { bNegFound |= S.Position.Equals(FVector(-300, -500, -500), 1e-6); }
	TestTrue(TEXT("negative Wuerfelindizes"), bNegFound);

	// Laufzeitfeld aus importierten Daten
	UFLAsteroidFieldData* Data = NewObject<UFLAsteroidFieldData>(GetTransientPackage());
	Data->FieldName = "feld";
	Data->CubeSize = 40000.f;
	FFLFieldSection Sec; Sec.Name = "Field"; Sec.Properties.Add("empty_cube_frequency", TEXT("0.4")); Sec.Properties.Add("fill_dist", TEXT("1500"));
	Data->Sections.Add(Sec);
	FFLFieldCube C1; C1.Asteroid = "rock"; C1.Offset = FVector(0.1, 0, 0); C1.Rotation = FRotator(10, 20, 30).Quaternion();
	FFLFieldCube C2; C2.Asteroid = "unbekannt";
	Data->Cubes = {C1, C2};
	FFLSystemZone Zone; Zone.Shape = EFLZoneShape::Sphere; Zone.Size = FVector(1000, 1000, 1000);
	const FFLFieldRuntime Built = FLAsteroids::BuildField(*Data, &Zone, {&Zone, nullptr}, [](FName N) { return N == FName("rock") ? 250.0 : 0.0; });
	TestEqual(TEXT("Asteroiden ohne Radius entfallen"), Built.Asteroids.Num(), 1);
	TestEqual(TEXT("Wuerfelgroesse"), Built.CubeSize, 40000.0);
	TestTrue(TEXT("Leerfrequenz aus dem Feld-Abschnitt"), FMath::IsNearlyEqual(Built.EmptyFrequency, 0.4));
	TestEqual(TEXT("Ausschlusszonen (null wird ignoriert)"), Built.Exclusions.Num(), 1);
	TestEqual(TEXT("groesster Radius"), Built.MaxAsteroidRadius, 250.0);
	TestTrue(TEXT("Sichtweite aus fill_dist (m -> cm)"), FMath::IsNearlyEqual(Built.FillDist, 150000.0));
	TestTrue(TEXT("Name und Rotation des Asteroiden"), Built.Asteroids[0].Nickname == FName("rock") && Built.Asteroids[0].Rotation.Equals(C1.Rotation, 1e-9));
	return true;
}
#endif

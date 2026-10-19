// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLFlightModel.h"

#include "Data/FLEquipmentData.h"
#include "Data/FLShipData.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
/** Liberty-Fighter mit Standard-Triebwerk: Masse 100, Widerstand 1 + 599, Schub 48000 */
FFLFlightParams FighterParams()
{
	FFLFlightParams P;
	P.Mass = 100.0;
	P.LinearDrag = 600.0;
	P.EngineForce = 48000.0;
	return P;
}

FFLFlightState Run(const FFLFlightState& Start, const FFLFlightInput& In, const FFLFlightParams& P, double Seconds, double Dt)
{
	FFLFlightState S = Start;
	const int32 Steps = FMath::RoundToInt(Seconds / Dt);
	for (int32 I = 0; I < Steps; ++I)
	{
		S = FLFlight::Step(S, In, P, Dt);
	}
	return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLFlightModelTest, "Freelancer.Flight.Model",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLFlightModelTest::RunTest(const FString& Parameters)
{
	const FFLFlightParams P = FighterParams();

	// Endgeschwindigkeit = Schub / Widerstand = 80 m/s
	TestEqual(TEXT("Endgeschwindigkeit (cm/s)"), FLFlight::TerminalSpeed(P), 8000.0);
	FFLFlightInput Full;
	Full.Throttle = 1.f;
	FFLFlightState S = Run(FFLFlightState(), Full, P, 5.0, 1.0 / 60.0);
	TestTrue(*FString::Printf(TEXT("Vorwaerts erreicht ~80 m/s (ist %.1f)"), S.Velocity.X / 100.0), FMath::IsNearlyEqual(S.Velocity.X, 8000.0, 8.0));
	TestTrue(TEXT("Nase zeigt nach +X: keine Seitenbewegung"), FMath::Abs(S.Velocity.Y) < 1e-6 && FMath::Abs(S.Velocity.Z) < 1e-6);
	TestTrue(TEXT("Position vorn"), S.Position.X > 0.0);

	// Rueckwaerts: reverse_fraction 0.2 -> 16 m/s
	FFLFlightInput Back;
	Back.Throttle = -1.f;
	S = Run(FFLFlightState(), Back, P, 5.0, 1.0 / 60.0);
	TestTrue(*FString::Printf(TEXT("Rueckwaerts ~16 m/s (ist %.1f)"), S.Velocity.X / 100.0), FMath::IsNearlyEqual(S.Velocity.X, -1600.0, 2.0));

	// Ohne Eingabe bremst das Schiff ab (Zeitkonstante Masse/Widerstand = 1/6 s)
	FFLFlightState Moving;
	Moving.Velocity = FVector(8000.0, 0.0, 0.0);
	S = Run(Moving, FFLFlightInput(), P, 2.0, 1.0 / 60.0);
	TestTrue(TEXT("Schub aus: kommt zum Stillstand"), S.Velocity.Size() < 1.0);

	// Eingaben ausserhalb -1..1 werden begrenzt (Server-Validierung)
	FFLFlightInput Cheat;
	Cheat.Throttle = 50.f;
	Cheat.Yaw = -9.f;
	const FFLFlightInput Clamped = Cheat.Clamped();
	TestEqual(TEXT("Schub begrenzt"), Clamped.Throttle, 1.f);
	TestEqual(TEXT("Gieren begrenzt"), Clamped.Yaw, -1.f);
	S = Run(FFLFlightState(), Cheat, P, 5.0, 1.0 / 60.0);
	TestTrue(TEXT("uebertriebene Eingabe ueberschreitet die Endgeschwindigkeit nicht"), S.Velocity.Size() <= 8000.0 + 8.0);

	// Schrittweite aendert das Ergebnis geradeaus nicht (exakte Loesung)
	const FFLFlightState Coarse = Run(FFLFlightState(), Full, P, 2.0, 1.0 / 20.0);
	const FFLFlightState Fine = Run(FFLFlightState(), Full, P, 2.0, 1.0 / 240.0);
	TestTrue(TEXT("Position unabhaengig von der Schrittweite"), FMath::IsNearlyEqual(Coarse.Position.X, Fine.Position.X, 1.0));
	TestTrue(TEXT("Geschwindigkeit unabhaengig von der Schrittweite"), FMath::IsNearlyEqual(Coarse.Velocity.X, Fine.Velocity.X, 0.5));

	// Determinismus: gleiche Eingabe -> bitgleicher Zustand (Voraussetzung fuer Client-Vorhersage)
	FFLFlightInput Mixed;
	Mixed.Throttle = 0.7f; Mixed.Pitch = 0.3f; Mixed.Yaw = -0.5f; Mixed.Roll = 0.2f; Mixed.StrafeRight = 0.4f;
	const FFLFlightState A = Run(FFLFlightState(), Mixed, P, 3.0, 1.0 / 60.0);
	const FFLFlightState B = Run(FFLFlightState(), Mixed, P, 3.0, 1.0 / 60.0);
	TestTrue(TEXT("deterministisch"), A.Position == B.Position && A.Velocity == B.Velocity && A.Rotation == B.Rotation && A.AngularVelocity == B.AngularVelocity);
	TestTrue(TEXT("Rotation bleibt normiert"), FMath::IsNearlyEqual(A.Rotation.Size(), 1.0, 1e-6));

	// Degenerierter Schritt
	const FFLFlightState Same = FLFlight::Step(A, Mixed, P, 0.0);
	TestTrue(TEXT("Dt = 0 aendert nichts"), Same.Position == A.Position && Same.Rotation == A.Rotation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLFlightRotationTest, "Freelancer.Flight.Rotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLFlightRotationTest::RunTest(const FString& Parameters)
{
	const FFLFlightParams P = FighterParams();

	// Endwinkelgeschwindigkeit = Drehmoment / Winkelwiderstand: 24000/15000 = 1.6 rad/s (Pitch/Yaw), 58000/35000 ~ 1.657 (Roll)
	const FVector Term = FLFlight::TerminalAngularSpeed(P);
	TestEqual(TEXT("Pitch"), Term.X, 1.6);
	TestTrue(TEXT("Roll"), FMath::IsNearlyEqual(Term.Z, 58000.0 / 35000.0, 1e-9));

	FFLFlightInput PitchUp;
	PitchUp.Pitch = 1.f;
	FFLFlightState S = Run(FFLFlightState(), PitchUp, P, 3.0, 1.0 / 60.0);
	TestTrue(*FString::Printf(TEXT("Pitch erreicht Endwert (ist %.3f)"), S.AngularVelocity.X), FMath::IsNearlyEqual(S.AngularVelocity.X, 1.6, 0.01));
	// Nase hoch: die Vorwaertsachse bekommt eine positive Z-Komponente
	FFLFlightState Short = Run(FFLFlightState(), PitchUp, P, 0.3, 1.0 / 60.0);
	TestTrue(TEXT("Pitch + hebt die Nase"), Short.Rotation.GetForwardVector().Z > 0.1);

	FFLFlightInput YawRight;
	YawRight.Yaw = 1.f;
	Short = Run(FFLFlightState(), YawRight, P, 0.3, 1.0 / 60.0);
	TestTrue(TEXT("Yaw + dreht die Nase nach rechts"), Short.Rotation.GetForwardVector().Y > 0.1);

	FFLFlightInput RollRight;
	RollRight.Roll = 1.f;
	Short = Run(FFLFlightState(), RollRight, P, 0.3, 1.0 / 60.0);
	TestTrue(TEXT("Roll + senkt die rechte Seite"), Short.Rotation.GetRightVector().Z < -0.1);

	// Schub folgt der Lage: nach 90 Grad Gieren nach rechts fliegt das Schiff nach +Y
	FFLFlightState Turned;
	Turned.Rotation = FRotator(0, 90, 0).Quaternion();
	FFLFlightInput Full;
	Full.Throttle = 1.f;
	S = Run(Turned, Full, P, 5.0, 1.0 / 60.0);
	TestTrue(TEXT("Schub in Blickrichtung (+Y)"), S.Velocity.Y > 7900.0 && FMath::Abs(S.Velocity.X) < 10.0);

	// Seitwaerts und hoch
	FFLFlightInput Strafe;
	Strafe.StrafeRight = 1.f;
	Strafe.StrafeUp = 1.f;
	S = Run(FFLFlightState(), Strafe, P, 5.0, 1.0 / 60.0);
	TestTrue(TEXT("Strafe rechts"), S.Velocity.Y > 1000.0);
	TestTrue(TEXT("Strafe hoch"), S.Velocity.Z > 1000.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLFlightParamsTest, "Freelancer.Flight.Params",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLFlightParamsTest::RunTest(const FString& Parameters)
{
	UFLShipData* Ship = NewObject<UFLShipData>(GetTransientPackage());
	Ship->Mass = 100.f;
	Ship->LinearDrag = 1.f;
	Ship->StrafeForce = 20000.f;
	Ship->SteeringTorque = FVector(24000, 24000, 58000);
	Ship->AngularDrag = FVector(15000, 15000, 35000);
	Ship->RotationInertia = FVector(2800, 2800, 1000);
	UFLEquipmentData* Engine = NewObject<UFLEquipmentData>(GetTransientPackage());
	Engine->Properties.Add("max_force", TEXT("48000"));
	Engine->Properties.Add("linear_drag", TEXT("599"));
	Engine->Properties.Add("reverse_fraction", TEXT("0.2"));

	const FFLFlightParams P = FLFlight::MakeParams(Ship, Engine);
	TestEqual(TEXT("Gesamtwiderstand = Schiff + Triebwerk"), P.LinearDrag, 600.0);
	TestEqual(TEXT("Schub"), P.EngineForce, 48000.0);
	TestEqual(TEXT("Rueckwaerts"), P.ReverseFraction, 0.2);
	TestEqual(TEXT("Drehmoment Roll"), P.SteeringTorque.Z, 58000.0);
	TestEqual(TEXT("Endgeschwindigkeit des Fighters 80 m/s"), FLFlight::TerminalSpeed(P) / 100.0, 80.0);

	const FFLFlightParams NoEngine = FLFlight::MakeParams(Ship, nullptr);
	TestEqual(TEXT("ohne Triebwerk: Standardschub"), NoEngine.EngineForce, FFLFlightParams().EngineForce);
	const FFLFlightParams Empty = FLFlight::MakeParams(nullptr, nullptr);
	TestEqual(TEXT("ohne alles: Standardwerte"), Empty.Mass, FFLFlightParams().Mass);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

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

#if WITH_DEV_AUTOMATION_TESTS
#include "Flight/FLPrediction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLPredictionTest, "Freelancer.Flight.Prediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLPredictionTest::RunTest(const FString& Parameters)
{
	const FFLFlightParams P = FighterParams();
	const double Dt = 1.0 / 60.0;
	FFLFlightInput In;
	In.Throttle = 1.f; In.Yaw = 0.3f;

	// Client sagt voraus und merkt sich die Bewegungen; der Server rechnet dasselbe, aber mit Verzoegerung
	FFLMoveHistory History;
	FFLFlightState Client, Server;
	TArray<FFLFlightState> ServerAfter; // Serverzustand nach Bewegung i
	for (uint32 Seq = 1; Seq <= 30; ++Seq)
	{
		FFLMove M; M.Sequence = Seq; M.Input = In; M.Dt = (float)Dt;
		TestTrue(TEXT("Sequenz aufsteigend"), History.Add(M));
		Client = FLFlight::Step(Client, M.Input, P, M.Dt);
		Server = FLFlight::Step(Server, M.Input, P, M.Dt);
		ServerAfter.Add(Server);
	}
	TestFalse(TEXT("alte Sequenz wird abgelehnt"), History.Add(FFLMove{20, In, 0.016f}));
	TestEqual(TEXT("30 offene Bewegungen"), History.Num(), 30);

	// Server bestaetigt Bewegung 20 (Zustand nach 20): Wiederholung der Bewegungen 21..30 ergibt den Client-Zustand
	History.Acknowledge(20);
	TestEqual(TEXT("10 Bewegungen bleiben offen"), History.Num(), 10);
	const FFLFlightState Replayed = History.Replay(ServerAfter[19], P);
	TestTrue(TEXT("Reconciliation ohne Abweichung reproduziert den Client-Zustand bitgleich"),
		Replayed.Position == Client.Position && Replayed.Rotation == Client.Rotation && Replayed.Velocity == Client.Velocity);
	TestFalse(TEXT("kein Korrekturbedarf"), FLPrediction::NeedsCorrection(Replayed, Client));

	// Der Server hat eine Bewegung anders berechnet (z. B. Kollision/Treffer): der Client wird korrigiert
	FFLFlightState Hit = ServerAfter[19];
	Hit.Position += FVector(0, 500.0, 0);
	const FFLFlightState Corrected = History.Replay(Hit, P);
	TestTrue(TEXT("Abweichung wird erkannt"), FLPrediction::NeedsCorrection(Client, Corrected));
	TestTrue(TEXT("Abweichung nach Wiederholung ~500 cm"), FMath::IsNearlyEqual(FLPrediction::PositionError(Client, Corrected), 500.0, 15.0));

	// Glaetten: sanft bei kleinem Fehler, Sprung bei grossem
	FFLFlightState Near = Client; Near.Position += FVector(0, 50, 0);
	const FFLFlightState Half = FLPrediction::Smooth(Client, Near, 0.5);
	TestTrue(TEXT("Glaetten halbiert den Fehler"), FMath::IsNearlyEqual(FLPrediction::PositionError(Half, Near), 25.0, 0.5));
	FFLFlightState Far = Client; Far.Position += FVector(0, 5000, 0);
	TestTrue(TEXT("grosser Fehler: sofort uebernehmen"), FLPrediction::Smooth(Client, Far, 0.1).Position == Far.Position);

	// Redundantes Senden: die letzten N offenen Bewegungen, aelteste zuerst
	{
		FFLMoveHistory H;
		for (uint32 Seq = 1; Seq <= 10; ++Seq) { H.Add(FFLMove{Seq, In, 0.016f}); }
		const TArray<FFLMove> Last = H.LastMoves(4);
		TestEqual(TEXT("letzte 4"), Last.Num(), 4);
		TestTrue(TEXT("aelteste zuerst, endet bei 10"), Last[0].Sequence == 7 && Last[3].Sequence == 10);
		TestEqual(TEXT("mehr verlangt als vorhanden"), H.LastMoves(100).Num(), 10);
		H.Acknowledge(8);
		TestEqual(TEXT("nur offene"), H.LastMoves(8).Num(), 2);
		TestEqual(TEXT("0 angefordert"), H.LastMoves(0).Num(), 0);
	}

	// Verlauf ist begrenzt
	FFLMoveHistory Long;
	for (uint32 Seq = 1; Seq <= 1000; ++Seq)
	{
		Long.Add(FFLMove{Seq, In, 0.016f});
	}
	TestEqual(TEXT("Verlauf auf MaxMoves begrenzt"), Long.Num(), FFLMoveHistory::MaxMoves);
	TestEqual(TEXT("neueste Sequenz bleibt"), (int32)Long.LastSequence(), 1000);
	Long.Acknowledge(5000);
	TestEqual(TEXT("alles bestaetigt"), Long.Num(), 0);

	// Zeitbudget: ganz oder gar nicht, mit begrenztem Guthaben und begrenzter Schuld
	FFLMoveBudget Budget(0.25, 0.25);
	TestTrue(TEXT("anfangs volles Guthaben"), FMath::IsNearlyEqual(Budget.Remaining(), 0.25));
	TestEqual(TEXT("zu lange Einzelbewegung wird abgelehnt"), Budget.Consume(5.0), 0.0);
	TestEqual(TEXT("ungueltige Dauer wird abgelehnt"), Budget.Consume(-1.0), 0.0);
	TestTrue(TEXT("Guthaben blieb unberuehrt"), FMath::IsNearlyEqual(Budget.Remaining(), 0.25));
	TestEqual(TEXT("1. Bewegung voll erlaubt"), Budget.Consume(0.1), 0.1);
	TestEqual(TEXT("2. Bewegung voll erlaubt"), Budget.Consume(0.1), 0.1);
	TestEqual(TEXT("3. Bewegung voll erlaubt (Schuld beginnt)"), Budget.Consume(0.1), 0.1);
	TestEqual(TEXT("4. Bewegung voll erlaubt (Schuld 0.15)"), Budget.Consume(0.1), 0.1);
	TestEqual(TEXT("5. Bewegung erlaubt (Schuld genau 0.25)"), Budget.Consume(0.1), 0.1);
	TestEqual(TEXT("6. Bewegung abgelehnt (mehr als 0.25 Schuld)"), Budget.Consume(0.1), 0.0);
	Budget.Accrue(0.1);
	TestEqual(TEXT("Echtzeit baut die Schuld ab, danach wieder erlaubt"), Budget.Consume(0.1), 0.1);
	Budget.Accrue(100.0);
	TestTrue(TEXT("Guthaben ist nach oben begrenzt"), FMath::IsNearlyEqual(Budget.Remaining(), 0.25));

	// Ehrlicher Client im Gleichtakt (60 Bewegungen/s) mit unregelmaessigen Server-Ticks (30 Hz): mal 2, mal 3 oder 1 Bewegung pro Tick
	FFLMoveBudget Honest(0.25, 0.25);
	int32 Rejected = 0;
	for (int32 Tick = 0; Tick < 3000; ++Tick)
	{
		Honest.Accrue(1.0 / 30.0);
		const int32 Moves = (Tick % 4 == 1) ? 3 : ((Tick % 4 == 2) ? 1 : 2);   // Mittel 2
		for (int32 K = 0; K < Moves; ++K)
		{
			Rejected += Honest.Consume(1.0 / 60.0) <= 0.0 ? 1 : 0;
		}
	}
	TestEqual(TEXT("ehrlicher Client: keine Ablehnung"), Rejected, 0);

	// Ein Client, der dauerhaft 20 % schneller laeuft, wird irgendwann abgelehnt
	FFLMoveBudget Cheater(0.25, 0.25);
	int32 CheatRejected = 0;
	for (int32 I = 0; I < 6000; ++I)
	{
		Cheater.Accrue(1.0 / 60.0);
		CheatRejected += Cheater.Consume(1.2 / 60.0) <= 0.0 ? 1 : 0;
	}
	TestTrue(TEXT("Speed-Hack wird abgewiesen"), CheatRejected > 500);
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLFlightCruiseTest, "Freelancer.Flight.Cruise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLFlightCruiseTest::RunTest(const FString& Parameters)
{
	const FFLFlightParams P = FighterParams();
	const double Dt = 1.0 / 60.0;
	FFLFlightInput Cruise;
	Cruise.bCruise = true;
	Cruise.Throttle = 1.f;

	// Aufladen: 4.9 s lang weiter normales Flugverhalten (80 m/s), Phase Charging
	FFLFlightState S = Run(FFLFlightState(), Cruise, P, 4.9, Dt);
	TestEqual(TEXT("Phase Aufladen"), S.CruisePhase, EFLCruisePhase::Charging);
	TestTrue(*FString::Printf(TEXT("beim Aufladen nur Normaltempo (ist %.1f m/s)"), S.Velocity.X / 100.0), S.Velocity.X < 8100.0);

	// Nach der Aufladezeit wird der Cruise aktiv und beschleunigt ueber CruiseAccelTime auf 300 m/s
	S = Run(S, Cruise, P, 0.3, Dt);
	TestEqual(TEXT("Phase aktiv"), S.CruisePhase, EFLCruisePhase::Active);
	S = Run(S, Cruise, P, P.CruiseAccelTime + 1.5, Dt);
	TestTrue(*FString::Printf(TEXT("Reisegeschwindigkeit ~300 m/s (ist %.1f)"), S.Velocity.X / 100.0), FMath::IsNearlyEqual(S.Velocity.X, 30000.0, 300.0));

	// Schubregler wirkt im Cruise nicht
	FFLFlightInput Reverse = Cruise;
	Reverse.Throttle = -1.f;
	FFLFlightState Held = Run(S, Reverse, P, 2.0, Dt);
	TestTrue(TEXT("Schubregler wird im Cruise ignoriert"), FMath::IsNearlyEqual(Held.Velocity.X, 30000.0, 300.0));

	// Lenkung im Cruise gedaempft: Endwinkelgeschwindigkeit = Faktor * Normalwert
	FFLFlightInput Pitch = Cruise;
	Pitch.Pitch = 1.f;
	const FFLFlightState Turning = Run(S, Pitch, P, 4.0, Dt);
	TestTrue(*FString::Printf(TEXT("gedaempfte Lenkung im Cruise (ist %.3f rad/s)"), Turning.AngularVelocity.X),
		FMath::IsNearlyEqual(Turning.AngularVelocity.X, 1.6 * P.CruiseTurnFactor, 0.02));

	// Abschalten: Geschwindigkeit faellt auf Normalwerte
	FFLFlightInput Off;
	Off.Throttle = 1.f;
	const FFLFlightState Slowed = Run(S, Off, P, 3.0, Dt);
	TestEqual(TEXT("Cruise aus"), Slowed.CruisePhase, EFLCruisePhase::Off);
	TestTrue(*FString::Printf(TEXT("zurueck auf Normaltempo (ist %.1f m/s)"), Slowed.Velocity.X / 100.0), Slowed.Velocity.X < 8100.0 && Slowed.Velocity.X > 7900.0);

	// Abbruch waehrend des Aufladens setzt zurueck: danach wieder volle Aufladezeit
	FFLFlightState Charging = Run(FFLFlightState(), Cruise, P, 3.0, Dt);
	Charging = Run(Charging, Off, P, 0.5, Dt);
	TestEqual(TEXT("Abbruch -> aus"), Charging.CruisePhase, EFLCruisePhase::Off);
	Charging = Run(Charging, Cruise, P, 3.0, Dt);
	TestEqual(TEXT("neu aufladen: nach 3 s noch nicht aktiv"), Charging.CruisePhase, EFLCruisePhase::Charging);

	// Stoerung (Treffer): Cruise endet und ist fuer CruiseDisruptTime gesperrt
	FFLFlightState Hit = S;
	FLFlight::DisruptCruise(Hit, P);
	TestEqual(TEXT("Stoerung beendet Cruise"), Hit.CruisePhase, EFLCruisePhase::Off);
	FFLFlightState Locked = Run(Hit, Cruise, P, 4.0, Dt);
	TestEqual(TEXT("gesperrt (4 s)"), Locked.CruisePhase, EFLCruisePhase::Off);
	Locked = Run(Locked, Cruise, P, 2.0, Dt);
	TestEqual(TEXT("nach der Sperre laedt er wieder auf"), Locked.CruisePhase, EFLCruisePhase::Charging);

	// Determinismus und Wiederholung (Vorhersage) mit Cruise
	FFLMoveHistory History;
	FFLFlightState Client, Server;
	for (uint32 Seq = 1; Seq <= 1200; ++Seq)
	{
		FFLFlightInput In = Cruise;
		In.Yaw = 0.2f;
		FFLMove M{Seq, In, (float)Dt};
		History.Add(M);
		Client = FLFlight::Step(Client, In, P, M.Dt);
		if (Seq == 1000) { Server = Client; History.Acknowledge(1000); }   // 200 offene Bewegungen < MaxMoves (256)
	}
	const FFLFlightState Replay = History.Replay(Server, P);
	TestTrue(TEXT("Wiederholung mit Cruise-Zustand ist bitgleich"), Replay.Position == Client.Position && Replay.CruisePhase == Client.CruisePhase && Replay.CruiseTimer == Client.CruiseTimer);

	// Parameter aus Triebwerksdaten
	UFLEquipmentData* Engine = NewObject<UFLEquipmentData>(GetTransientPackage());
	Engine->Properties.Add("cruise_charge_time", TEXT("7"));
	TestEqual(TEXT("cruise_charge_time aus den Daten"), FLFlight::MakeParams(nullptr, Engine).CruiseChargeTime, 7.0);
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "EnhancedInput/Public/InputAction.h"
#include "EnhancedInput/Public/InputMappingContext.h"
#include "Flight/FLFlightInput.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLFlightInputTest, "Freelancer.Flight.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLFlightInputTest::RunTest(const FString& Parameters)
{
	const FFLInputSettings S;

	// Totzone: kleine Stickwerte ergeben nichts, voller Ausschlag 1, dazwischen weich
	TestEqual(TEXT("in der Totzone"), FLInput::ApplyDeadZone(0.05f, 0.12f), 0.f);
	TestEqual(TEXT("voller Ausschlag"), FLInput::ApplyDeadZone(1.f, 0.12f), 1.f);
	TestEqual(TEXT("negativ"), FLInput::ApplyDeadZone(-1.f, 0.12f), -1.f);
	TestTrue(TEXT("knapp ueber der Totzone ist klein"), FLInput::ApplyDeadZone(0.13f, 0.12f) < 0.02f);

	// Tasten: Werte werden durchgereicht und begrenzt
	FFLRawInput Raw;
	Raw.Throttle = 1.f; Raw.StrafeRight = -1.f; Raw.StrafeUp = 5.f; Raw.Roll = -3.f;
	FFLFlightInput In = FLInput::Compose(Raw, S);
	TestEqual(TEXT("Schub"), In.Throttle, 1.f);
	TestEqual(TEXT("seitwaerts"), In.StrafeRight, -1.f);
	TestEqual(TEXT("begrenzt hoch"), In.StrafeUp, 1.f);
	TestEqual(TEXT("begrenzt Rollen"), In.Roll, -1.f);

	// Maus: Delta wird skaliert, + = Nase hoch / rechts
	FFLRawInput Mouse;
	Mouse.Look = FVector2D(5.0, -5.0);
	In = FLInput::Compose(Mouse, S);
	TestTrue(TEXT("Maus rechts -> Gieren +"), FMath::IsNearlyEqual(In.Yaw, 5.f * S.MouseSensitivity, 1e-6f));
	TestTrue(TEXT("Maus runter -> Nicken -"), FMath::IsNearlyEqual(In.Pitch, -5.f * S.MouseSensitivity, 1e-6f));
	Mouse.Look = FVector2D(500.0, 500.0);
	In = FLInput::Compose(Mouse, S);
	TestEqual(TEXT("grosse Maus-Bewegung begrenzt (Gieren)"), In.Yaw, 1.f);
	TestEqual(TEXT("grosse Maus-Bewegung begrenzt (Nicken)"), In.Pitch, 1.f);

	// Stick: Totzone statt Skalierung
	FFLRawInput Stick;
	Stick.bLookIsDelta = false;
	Stick.Look = FVector2D(0.05, 1.0);
	In = FLInput::Compose(Stick, S);
	TestEqual(TEXT("Stick in Totzone"), In.Yaw, 0.f);
	TestEqual(TEXT("Stick voll"), In.Pitch, 1.f);

	// Cruise-Umschalter wird durchgereicht
	FFLRawInput CruiseRaw;
	CruiseRaw.bCruise = true;
	TestTrue(TEXT("Cruise in der Eingabe"), FLInput::Compose(CruiseRaw, S).bCruise);

	// Nicken umkehren
	FFLInputSettings Inverted;
	Inverted.bInvertPitch = true;
	FFLRawInput Up;
	Up.bLookIsDelta = false;
	Up.Look = FVector2D(0.0, 1.0);
	TestEqual(TEXT("Nicken invertiert"), FLInput::Compose(Up, Inverted).Pitch, -1.f);

	// Standardbelegung: Aktionen und Tasten vorhanden
	UObject* Outer = GetTransientPackage();
	const FFLInputActions A = FLInput::CreateDefaultActions(Outer);
	TestNotNull(TEXT("Mapping"), A.Mapping.Get());
	TestNotNull(TEXT("Schub-Aktion"), A.Throttle.Get());
	if (A.Mapping)
	{
		auto HasKey = [&](const UInputAction* Action, const FKey& Key, bool bNegated)
		{
			for (const FEnhancedActionKeyMapping& M : A.Mapping->GetMappings())
			{
				if (M.Action == Action && M.Key == Key && (M.Modifiers.Num() > 0) == bNegated)
				{
					return true;
				}
			}
			return false;
		};
		TestTrue(TEXT("W = Schub"), HasKey(A.Throttle, EKeys::W, false));
		TestTrue(TEXT("S = Rueckwaerts (negiert)"), HasKey(A.Throttle, EKeys::S, true));
		TestTrue(TEXT("D = rechts"), HasKey(A.StrafeRight, EKeys::D, false));
		TestTrue(TEXT("A = links (negiert)"), HasKey(A.StrafeRight, EKeys::A, true));
		TestTrue(TEXT("Leertaste = hoch"), HasKey(A.StrafeUp, EKeys::SpaceBar, false));
		TestTrue(TEXT("Strg = runter (negiert)"), HasKey(A.StrafeUp, EKeys::LeftControl, true));
		TestTrue(TEXT("E = Rolle rechts"), HasKey(A.Roll, EKeys::E, false));
		TestTrue(TEXT("Q = Rolle links (negiert)"), HasKey(A.Roll, EKeys::Q, true));
		TestTrue(TEXT("Maus steuert"), HasKey(A.Look, EKeys::Mouse2D, false));
		TestTrue(TEXT("Gamepad steuert"), HasKey(A.Look, EKeys::Gamepad_Right2D, false));
		TestTrue(TEXT("Gamepad Trigger = Schub"), HasKey(A.Throttle, EKeys::Gamepad_RightTriggerAxis, false));
		TestTrue(TEXT("C = Cruise"), HasKey(A.Cruise, EKeys::C, false));
		TestTrue(TEXT("Gamepad linker Trigger = zurueck (negiert)"), HasKey(A.Throttle, EKeys::Gamepad_LeftTriggerAxis, true));
	}
	return true;
}
#endif

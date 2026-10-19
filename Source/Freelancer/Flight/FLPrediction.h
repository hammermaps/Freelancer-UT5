// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flight/FLFlightModel.h"
#include "FLPrediction.generated.h"

/** Eine vom Client gesendete Bewegung: Eingabe plus Dauer, nummeriert. */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLMove
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Sequence = 0;

	UPROPERTY()
	FFLFlightInput Input;

	/** Dauer in Sekunden, fuer die diese Eingabe galt */
	UPROPERTY()
	float Dt = 0.f;
};

/**
 * Verlauf noch nicht bestaetigter Bewegungen auf dem Client. Nach einer Server-Bestaetigung wird der Serverzustand
 * uebernommen und alle neueren Bewegungen erneut angewendet (Reconciliation).
 */
class FREELANCER_API FFLMoveHistory
{
public:
	/** Neue Bewegung anhaengen (Sequenz muss aufsteigen); gibt false zurueck, wenn sie nicht neuer als die letzte ist. */
	bool Add(const FFLMove& Move);

	/** Alle Bewegungen bis einschliesslich Sequence verwerfen. */
	void Acknowledge(uint32 Sequence);

	/** Zustand nach Wiederholung aller offenen Bewegungen ab dem uebergebenen (vom Server bestaetigten) Zustand. */
	FFLFlightState Replay(const FFLFlightState& ServerState, const FFLFlightParams& Params) const;

	int32 Num() const { return Moves.Num(); }
	uint32 LastSequence() const { return Moves.Num() ? Moves.Last().Sequence : 0; }

	/** Begrenzung, damit der Verlauf bei Verbindungsabbruch nicht unbegrenzt waechst (aelteste werden verworfen). */
	static constexpr int32 MaxMoves = 256;

private:
	TArray<FFLMove> Moves;
};

/**
 * Zeitbudget auf dem Server gegen Speed-Hacks: Ein Client darf nur so viel Bewegungszeit schicken, wie auf dem Server
 * tatsaechlich vergangen ist (plus kleiner Puffer fuer Jitter). Zu lange Bewegungen werden gekuerzt.
 */
class FREELANCER_API FFLMoveBudget
{
public:
	explicit FFLMoveBudget(double InMaxBurstSeconds = 0.25) : MaxBurst(InMaxBurstSeconds), Budget(InMaxBurstSeconds) {}

	/** Echtzeit auf dem Server vergangen. */
	void Accrue(double RealDt);

	/** Verbraucht Zeit fuer eine Bewegung und gibt die erlaubte Dauer zurueck (<= gewuenscht, <= MaxStep, <= Budget). */
	double Consume(double RequestedDt);

	double Remaining() const { return Budget; }

	/** Groesste Dauer einer einzelnen Bewegung (Schutz gegen riesige Einzelschritte). */
	static constexpr double MaxStep = 0.1;

private:
	double MaxBurst;
	double Budget;
};

namespace FLPrediction
{
	/** Fehler zwischen vorhergesagtem und bestaetigtem Zustand (Position in cm). */
	FREELANCER_API double PositionError(const FFLFlightState& Predicted, const FFLFlightState& Server);

	/** true, wenn der Fehler die Toleranz ueberschreitet und der Client korrigiert werden muss. */
	FREELANCER_API bool NeedsCorrection(const FFLFlightState& Predicted, const FFLFlightState& Server, double ToleranceCm = 5.0);

	/** Sanfte Korrektur: naehert Predicted dem Server an (Alpha 0..1); bei grossem Fehler (> SnapCm) sofort uebernehmen. */
	FREELANCER_API FFLFlightState Smooth(const FFLFlightState& Predicted, const FFLFlightState& Server, double Alpha, double SnapCm = 500.0);
}

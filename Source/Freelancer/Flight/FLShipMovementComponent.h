// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Flight/FLFlightModel.h"
#include "Flight/FLPrediction.h"
#include "FLShipMovementComponent.generated.h"

/**
 * Bewegung eines Schiffs. Rollen:
 *  - Server (Authority): wendet vom Client gesendete Bewegungen nach Validierung (Zeitbudget, begrenzte Eingaben) an und ist die Wahrheit.
 *  - Besitzender Client (AutonomousProxy): sagt mit FLFlight::Step voraus, merkt sich Bewegungen, gleicht mit Server-Bestaetigungen ab.
 *  - Andere Clients (SimulatedProxy): naehern sich dem replizierten Zustand an.
 * Die eigentliche Bewegung berechnet ausschliesslich FLFlight::Step.
 */
UCLASS(ClassGroup = (Freelancer), meta = (BlueprintSpawnableComponent))
class FREELANCER_API UFLShipMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFLShipMovementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
	FFLFlightParams Params;

	/** Aktuelle Eingabe (nur Besitzer setzt sie, z. B. aus Enhanced Input oder dem Test-Bot). */
	UFUNCTION(BlueprintCallable, Category = "Flight")
	void SetInput(const FFLFlightInput& NewInput) { Input = NewInput.Clamped(); }

	UFUNCTION(BlueprintPure, Category = "Flight")
	const FFLFlightState& GetState() const { return State; }

	/** Zustand setzen (Spawn, Teleport/Sprung); setzt Vorhersage-Verlauf zurueck. */
	UFUNCTION(BlueprintCallable, Category = "Flight")
	void ResetState(const FFLFlightState& NewState);

	/** Vom Server aufgerufen: Bewegung eines Clients anwenden. Gibt false zurueck, wenn nichts angewendet wurde. */
	bool ServerApplyMove(const FFLMove& Move);

	/** Vom Besitzer aufgerufen, wenn der Server einen Zustand bestaetigt. */
	void ClientReconcile(uint32 AckedSequence, const FFLFlightState& ServerState);

	/** Replizierter Zustand fuer fremde Clients. */
	void SetRemoteTarget(const FFLFlightState& Target);

	uint32 GetLastProcessedSequence() const { return LastProcessedSequence; }

	/** Statistik fuer Tests: Anzahl Korrekturen und groesster Positionsfehler (cm) seit Start */
	int32 GetCorrectionCount() const { return CorrectionCount; }
	double GetMaxPositionError() const { return MaxPositionError; }
	int32 GetTrimmedMoves() const { return TrimmedMoves; }
	int32 GetAppliedMoves() const { return AppliedMoves; }
	double GetBudgetAccrued() const { return Budget.TotalAccrued(); }
	double GetBudgetConsumed() const { return Budget.TotalConsumed(); }

	/** Wird auf dem Besitzer einmal pro Tick mit den letzten unbestaetigten Bewegungen aufgerufen (der Pawn leitet sie als RPC an den Server).
	 *  Redundanz: verlorene unzuverlaessige RPCs werden durch die naechsten ausgeglichen (der Server ignoriert bereits verarbeitete). */
	DECLARE_DELEGATE_OneParam(FOnSendMoves, const TArray<FFLMove>&);
	FOnSendMoves OnSendMoves;

	/** Anzahl redundant mitgesendeter Bewegungen */
	static constexpr int32 RedundantMoves = 8;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FFLFlightInput Input;
	FFLFlightState State;
	FFLFlightState RemoteTarget;
	bool bHasRemoteTarget = false;

	FFLMoveHistory History;
	uint32 NextSequence = 1;

	// Server
	uint32 LastProcessedSequence = 0;
	FFLMoveBudget Budget;

	/** Feste Simulationsschrittweite (60 Hz): jede Bewegung hat dieselbe Dauer, unabhaengig von der Bildrate. */
	static constexpr double FixedStep = 1.0 / 60.0;
	double StepAccumulator = 0.0;

	int32 TrimmedMoves = 0;
	int32 AppliedMoves = 0;

	/** Besitzender Client: erst nach dem ersten Serverzustand vorhersagen (der Startzustand kommt vom Server) */
	bool bInitializedFromServer = false;

	int32 CorrectionCount = 0;
	double MaxPositionError = 0.0;

	void ApplyToActor();
};

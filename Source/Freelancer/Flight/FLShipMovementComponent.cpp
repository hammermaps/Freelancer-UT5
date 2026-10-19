// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLShipMovementComponent.h"

#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogFLMove, Log, All);

UFLShipMovementComponent::UFLShipMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UFLShipMovementComponent::ResetState(const FFLFlightState& NewState)
{
	State = NewState;
	RemoteTarget = NewState;
	History = FFLMoveHistory();
	NextSequence = 1;
	LastProcessedSequence = 0;
	ApplyToActor();
}

void UFLShipMovementComponent::ApplyToActor()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocationAndRotation(State.Position, State.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool UFLShipMovementComponent::ServerApplyMove(const FFLMove& Move)
{
	if (Move.Sequence <= LastProcessedSequence)
	{
		return false; // veraltet oder doppelt
	}
	// Zeitbudget: nie mehr Bewegungszeit, als auf dem Server vergangen ist
	const double Dt = Budget.Consume(Move.Dt);
	LastProcessedSequence = Move.Sequence;
	++AppliedMoves;
	if (Dt <= 0.0)
	{
		++TrimmedMoves; // abgelehnt (Zeitbudget erschoepft oder Dauer unzulaessig): Server und Client rechnen dann verschieden
	}
	if (Dt > 0.0)
	{
		State = FLFlight::Step(State, Move.Input.Clamped(), Params, Dt);
		ApplyToActor();
	}
	return Dt > 0.0;
}

void UFLShipMovementComponent::ClientReconcile(uint32 AckedSequence, const FFLFlightState& ServerState)
{
	if (!bInitializedFromServer)
	{
		// Erster Serverzustand: Startzustand uebernehmen, noch nichts vorherzusagen gewesen
		bInitializedFromServer = true;
		State = ServerState;
		History = FFLMoveHistory();
		NextSequence = AckedSequence + 1;
		ApplyToActor();
		return;
	}
	History.Acknowledge(AckedSequence);
	const FFLFlightState Replayed = History.Replay(ServerState, Params);
	const double Error = FLPrediction::PositionError(State, Replayed);
	MaxPositionError = FMath::Max(MaxPositionError, Error);
	if (FLPrediction::NeedsCorrection(State, Replayed))
	{
		++CorrectionCount;
		if (CorrectionCount <= 15 || CorrectionCount % 200 == 0)
		{
			const FVector Diff = State.Position - Replayed.Position;
			UE_LOG(LogFLMove, Display, TEXT("Korrektur %d: Fehler=%.1f cm Diff=(%.1f, %.1f, %.1f) offen=%d Ack=%u Naechste=%u Winkel=%.2f Grad dV=%.1f cm/s"),
				CorrectionCount, Error, Diff.X, Diff.Y, Diff.Z, History.Num(), AckedSequence, NextSequence,
				FMath::RadiansToDegrees(State.Rotation.AngularDistance(Replayed.Rotation)), (State.Velocity - Replayed.Velocity).Size());
		}
		State = FLPrediction::Smooth(State, Replayed, 0.3);
		ApplyToActor();
	}
}

void UFLShipMovementComponent::SetRemoteTarget(const FFLFlightState& Target)
{
	RemoteTarget = Target;
	if (!bHasRemoteTarget)
	{
		State = Target; // erster Zustand: direkt uebernehmen
		bHasRemoteTarget = true;
		ApplyToActor();
	}
}

void UFLShipMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AActor* Owner = GetOwner();
	if (!Owner || DeltaTime <= 0.f)
	{
		return;
	}
	const ENetRole Role = Owner->GetLocalRole();

	// Eigene Simulation (lokal gesteuert oder besitzender Client) laeuft in festen Schritten: gleich lange Bewegungen
	// halten die Zahl der RPCs pro Sekunde begrenzt und machen Vorhersage und Server-Wiederholung vergleichbar.
	StepAccumulator = FMath::Min(StepAccumulator + DeltaTime, 0.25);

	if (Role == ROLE_Authority)
	{
		Budget.Accrue(DeltaTime);
		// Server ohne eigenen Besitzer-Client: Zustand wird nur durch ServerApplyMove veraendert.
		// Lokal gesteuert (Standalone/Listen-Server): direkt simulieren.
		if (const APawn* Pawn = Cast<APawn>(Owner); Pawn && Pawn->IsLocallyControlled())
		{
			bool bStepped = false;
			while (StepAccumulator >= FixedStep)
			{
				StepAccumulator -= FixedStep;
				State = FLFlight::Step(State, Input, Params, FixedStep);
				bStepped = true;
			}
			if (bStepped)
			{
				ApplyToActor();
			}
		}
		else
		{
			StepAccumulator = 0.0;
		}
	}
	else if (Role == ROLE_AutonomousProxy)
	{
		if (!bInitializedFromServer)
		{
			StepAccumulator = 0.0;
			return; // auf den Startzustand vom Server warten
		}
		bool bStepped = false;
		while (StepAccumulator >= FixedStep)
		{
			StepAccumulator -= FixedStep;
			FFLMove Move;
			Move.Sequence = NextSequence++;
			Move.Input = Input;
			Move.Dt = (float)FixedStep;
			State = FLFlight::Step(State, Move.Input, Params, FixedStep);
			History.Add(Move);
			bStepped = true;
		}
		if (bStepped)
		{
			ApplyToActor();
			OnSendMoves.ExecuteIfBound(History.LastMoves(RedundantMoves));
		}
	}
	else if (bHasRemoteTarget)
	{
		// Fremde Clients: mit letzter Geschwindigkeit weiterlaufen und zum bestaetigten Zustand hin angleichen
		FFLFlightState Dead = State;
		Dead.Position += State.Velocity * DeltaTime;
		State = FLPrediction::Smooth(Dead, RemoteTarget, FMath::Min(1.0, DeltaTime * 12.0));
		ApplyToActor();
	}
}

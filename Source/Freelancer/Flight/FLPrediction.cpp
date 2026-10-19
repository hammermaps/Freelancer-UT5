// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLPrediction.h"

bool FFLMoveHistory::Add(const FFLMove& Move)
{
	if (Moves.Num() && Move.Sequence <= Moves.Last().Sequence)
	{
		return false;
	}
	Moves.Add(Move);
	if (Moves.Num() > MaxMoves)
	{
		Moves.RemoveAt(0, Moves.Num() - MaxMoves, EAllowShrinking::No);
	}
	return true;
}

void FFLMoveHistory::Acknowledge(uint32 Sequence)
{
	int32 Count = 0;
	while (Count < Moves.Num() && Moves[Count].Sequence <= Sequence)
	{
		++Count;
	}
	if (Count > 0)
	{
		Moves.RemoveAt(0, Count, EAllowShrinking::No);
	}
}

TArray<FFLMove> FFLMoveHistory::LastMoves(int32 MaxCount) const
{
	const int32 Start = FMath::Max(0, Moves.Num() - FMath::Max(0, MaxCount));
	TArray<FFLMove> Out;
	Out.Append(Moves.GetData() + Start, Moves.Num() - Start);
	return Out;
}

FFLFlightState FFLMoveHistory::Replay(const FFLFlightState& ServerState, const FFLFlightParams& Params) const
{
	FFLFlightState S = ServerState;
	for (const FFLMove& M : Moves)
	{
		S = FLFlight::Step(S, M.Input, Params, M.Dt);
	}
	return S;
}

void FFLMoveBudget::Accrue(double RealDt)
{
	if (RealDt > 0.0)
	{
		Budget = FMath::Min(Budget + RealDt, MaxBurst);
		Accrued += RealDt;
	}
}

double FFLMoveBudget::Consume(double RequestedDt)
{
	if (RequestedDt <= 0.0 || RequestedDt > MaxStep || Budget - RequestedDt < -MaxDebt - 1e-12)
	{
		return 0.0;
	}
	Budget -= RequestedDt;
	Consumed += RequestedDt;
	return RequestedDt;
}

double FLPrediction::PositionError(const FFLFlightState& Predicted, const FFLFlightState& Server)
{
	return FVector::Dist(Predicted.Position, Server.Position);
}

bool FLPrediction::NeedsCorrection(const FFLFlightState& Predicted, const FFLFlightState& Server, double ToleranceCm)
{
	return PositionError(Predicted, Server) > ToleranceCm;
}

FFLFlightState FLPrediction::Smooth(const FFLFlightState& Predicted, const FFLFlightState& Server, double Alpha, double SnapCm)
{
	if (PositionError(Predicted, Server) > SnapCm)
	{
		return Server;
	}
	const double A = FMath::Clamp(Alpha, 0.0, 1.0);
	FFLFlightState Out = Server; // Geschwindigkeit/Winkelgeschwindigkeit stammen vom Server, nur sichtbare Lage wird geglaettet
	Out.Position = FMath::Lerp(Predicted.Position, Server.Position, A);
	Out.Rotation = FQuat::Slerp(Predicted.Rotation, Server.Rotation, A).GetNormalized();
	return Out;
}

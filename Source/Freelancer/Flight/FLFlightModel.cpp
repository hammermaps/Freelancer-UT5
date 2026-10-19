// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLFlightModel.h"

#include "Data/FLEquipmentData.h"
#include "Data/FLShipData.h"

FFLFlightInput FFLFlightInput::Clamped() const
{
	FFLFlightInput Out = *this;
	Out.Throttle = FMath::Clamp(Throttle, -1.f, 1.f);
	Out.StrafeRight = FMath::Clamp(StrafeRight, -1.f, 1.f);
	Out.StrafeUp = FMath::Clamp(StrafeUp, -1.f, 1.f);
	Out.Pitch = FMath::Clamp(Pitch, -1.f, 1.f);
	Out.Yaw = FMath::Clamp(Yaw, -1.f, 1.f);
	Out.Roll = FMath::Clamp(Roll, -1.f, 1.f);
	return Out;
}

namespace
{
/** Gedaempfte Bewegung mit konstanter Kraft: v' = (F - D v) / M. Liefert neue Geschwindigkeit und Weg ueber Dt (exakt). */
void Damped(double Velocity, double Force, double Drag, double Mass, double Dt, double& OutVelocity, double& OutDistance)
{
	if (Drag <= KINDA_SMALL_NUMBER)
	{
		// ohne Widerstand: gleichmaessige Beschleunigung
		const double A = Mass > 0.0 ? Force / Mass : 0.0;
		OutVelocity = Velocity + A * Dt;
		OutDistance = Velocity * Dt + 0.5 * A * Dt * Dt;
		return;
	}
	const double Terminal = Force / Drag;
	const double K = Drag / FMath::Max(Mass, KINDA_SMALL_NUMBER);
	const double E = FMath::Exp(-K * Dt);
	OutVelocity = Terminal + (Velocity - Terminal) * E;
	OutDistance = Terminal * Dt + (Velocity - Terminal) * (1.0 - E) / K;
}
}

FFLFlightState FLFlight::Step(const FFLFlightState& State, const FFLFlightInput& RawInput, const FFLFlightParams& P, double Dt)
{
	FFLFlightState Out = State;
	if (Dt <= 0.0)
	{
		return Out;
	}
	const FFLFlightInput In = RawInput.Clamped();

	// Drehung: je Achse gedaempft (lokal). Reihenfolge der Achsen: x = Pitch, y = Yaw, z = Roll.
	const double Input[3] = {In.Pitch, In.Yaw, In.Roll};
	FVector AngleStep; // ueberstrichener Winkel je Achse in rad
	for (int32 I = 0; I < 3; ++I)
	{
		double Omega = 0.0, Angle = 0.0;
		Damped(State.AngularVelocity[I], P.SteeringTorque[I] * Input[I], P.AngularDrag[I], P.RotationInertia[I], Dt, Omega, Angle);
		Out.AngularVelocity[I] = Omega;
		AngleStep[I] = Angle;
	}
	const FQuat Delta = FRotator(FMath::RadiansToDegrees(AngleStep.X), FMath::RadiansToDegrees(AngleStep.Y), FMath::RadiansToDegrees(AngleStep.Z)).Quaternion();
	Out.Rotation = (State.Rotation * Delta).GetNormalized();

	// Translation: lokaler Schub in Weltrichtung (Lage am Schrittanfang), dann exakt gedaempft je Weltachse
	const double Thrust = In.Throttle >= 0.f ? In.Throttle * P.EngineForce : In.Throttle * P.EngineForce * P.ReverseFraction;
	const FVector LocalForce(Thrust, In.StrafeRight * P.StrafeForce, In.StrafeUp * P.StrafeForce);
	const FVector WorldForce = State.Rotation.RotateVector(LocalForce);
	for (int32 I = 0; I < 3; ++I)
	{
		double V = 0.0, Dist = 0.0;
		// Geschwindigkeit in m/s rechnen (Kraefte in N), Ergebnis zurueck in cm
		Damped(State.Velocity[I] * 0.01, WorldForce[I], P.LinearDrag, P.Mass, Dt, V, Dist);
		Out.Velocity[I] = V * 100.0;
		Out.Position[I] = State.Position[I] + Dist * 100.0;
	}
	return Out;
}

double FLFlight::TerminalSpeed(const FFLFlightParams& P)
{
	return P.LinearDrag > KINDA_SMALL_NUMBER ? P.EngineForce / P.LinearDrag * 100.0 : 0.0;
}

FVector FLFlight::TerminalAngularSpeed(const FFLFlightParams& P)
{
	FVector R = FVector::ZeroVector;
	for (int32 I = 0; I < 3; ++I)
	{
		R[I] = P.AngularDrag[I] > KINDA_SMALL_NUMBER ? P.SteeringTorque[I] / P.AngularDrag[I] : 0.0;
	}
	return R;
}

FFLFlightParams FLFlight::MakeParams(const UFLShipData* Ship, const UFLEquipmentData* Engine)
{
	FFLFlightParams P;
	if (Ship)
	{
		P.Mass = Ship->Mass;
		P.StrafeForce = Ship->StrafeForce;
		P.SteeringTorque = Ship->SteeringTorque;
		P.AngularDrag = Ship->AngularDrag;
		P.RotationInertia = Ship->RotationInertia;
		P.LinearDrag = Ship->LinearDrag;
	}
	if (Engine)
	{
		P.EngineForce = Engine->GetFloatProperty("max_force", P.EngineForce);
		P.LinearDrag += Engine->GetFloatProperty("linear_drag", 0.f);
		P.ReverseFraction = Engine->GetFloatProperty("reverse_fraction", P.ReverseFraction);
	}
	return P;
}

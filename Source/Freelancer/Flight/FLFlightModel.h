// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FLFlightModel.generated.h"

class UFLShipData;
class UFLEquipmentData;

/**
 * Flugmodell wie im Original: Schub minus linearer Widerstand ergibt eine Endgeschwindigkeit = Schub / Widerstand
 * (Liberty-Fighter: 48000 N / 600 = 80 m/s), Drehung entsprechend Drehmoment / Winkelwiderstand.
 * Die Schrittfunktion ist rein und deterministisch (gleiche Eingabe -> gleicher Zustand) und damit fuer
 * Server-Autoritaet und Client-Vorhersage geeignet. Einheiten: Positionen/Geschwindigkeiten in cm bzw. cm/s (Unreal),
 * Kraefte in N, Massen in kg, Winkelgeschwindigkeit in rad/s.
 */
USTRUCT(BlueprintType)
struct FREELANCER_API FFLFlightParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Mass = 100.0;

	/** Gesamtwiderstand: linear_drag des Schiffs + linear_drag des Triebwerks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double LinearDrag = 600.0;

	/** max_force des Triebwerks (N) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double EngineForce = 48000.0;

	/** Anteil des Schubs beim Rueckwaertsflug (reverse_fraction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double ReverseFraction = 0.2;

	/** Seitwaerts-/Hoch-Runter-Schub (strafe_force) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double StrafeForce = 20000.0;

	/** steering_torque: x = Pitch, y = Yaw, z = Roll (Reihenfolge wie im Original) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SteeringTorque = FVector(24000.0, 24000.0, 58000.0);

	/** angular_drag: x = Pitch, y = Yaw, z = Roll */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector AngularDrag = FVector(15000.0, 15000.0, 35000.0);

	/** rotation_inertia: x = Pitch, y = Yaw, z = Roll */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector RotationInertia = FVector(2800.0, 2800.0, 1000.0);
};

USTRUCT(BlueprintType)
struct FREELANCER_API FFLFlightInput
{
	GENERATED_BODY()

	/** -1 (rueckwaerts) .. 1 (voller Schub) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Throttle = 0.f;

	/** Seitwaerts (+ = rechts) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrafeRight = 0.f;

	/** Hoch/Runter (+ = hoch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrafeUp = 0.f;

	/** -1..1: + = Nase hoch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Pitch = 0.f;

	/** -1..1: + = Nase rechts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Yaw = 0.f;

	/** -1..1: + = Rolle nach rechts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Roll = 0.f;

	/** Werte auf -1..1 begrenzen (Server-Validierung: Clients duerfen keine staerkeren Eingaben schicken) */
	FFLFlightInput Clamped() const;
};

USTRUCT(BlueprintType)
struct FREELANCER_API FFLFlightState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Position = FVector::ZeroVector;

	/** Weltgeschwindigkeit in cm/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FQuat Rotation = FQuat::Identity;

	/** Lokale Winkelgeschwindigkeit in rad/s: x = Pitch, y = Yaw, z = Roll */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector AngularVelocity = FVector::ZeroVector;
};

namespace FLFlight
{
	/** Ein Simulationsschritt (Dt in Sekunden, > 0). Exakte Loesung der gedaempften Bewegung: stabil fuer jedes Dt. */
	FREELANCER_API FFLFlightState Step(const FFLFlightState& State, const FFLFlightInput& Input, const FFLFlightParams& Params, double Dt);

	/** Endgeschwindigkeit bei vollem Schub vorwaerts in cm/s */
	FREELANCER_API double TerminalSpeed(const FFLFlightParams& Params);

	/** Endwinkelgeschwindigkeit (rad/s) je Achse bei voller Eingabe: x = Pitch, y = Yaw, z = Roll */
	FREELANCER_API FVector TerminalAngularSpeed(const FFLFlightParams& Params);

	/** Parameter aus importierten Daten: Schiff (Masse, Drehmoment, Widerstand) + Triebwerk (Schub, Widerstand, Rueckwaerts). Engine darf null sein. */
	FREELANCER_API FFLFlightParams MakeParams(const UFLShipData* Ship, const UFLEquipmentData* Engine);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flight/FLFlightModel.h"

class UInputAction;
class UInputMappingContext;

/** Rohwerte der Eingabegeraete eines Frames (vor Umrechnung in Flugeingaben). */
struct FREELANCER_API FFLRawInput
{
	float Throttle = 0.f;      // -1..1 (W/S, Trigger)
	float StrafeRight = 0.f;   // -1..1
	float StrafeUp = 0.f;      // -1..1
	float Roll = 0.f;          // -1..1
	FVector2D Look = FVector2D::ZeroVector; // Maus-Delta oder Stick: x = Gieren, y = Nicken (+ = Nase hoch)
	bool bLookIsDelta = true;  // true: Maus (Delta pro Frame, wird skaliert), false: Stick (-1..1)
};

struct FREELANCER_API FFLInputSettings
{
	float MouseSensitivity = 0.08f; // Maus-Delta -> -1..1 pro Frame
	float StickDeadZone = 0.12f;
	bool bInvertPitch = false;
};

namespace FLInput
{
	/** Rohwerte -> Flugeingabe: Totzone fuer Sticks, Maus skaliert, alles auf -1..1 begrenzt. */
	FREELANCER_API FFLFlightInput Compose(const FFLRawInput& Raw, const FFLInputSettings& Settings);

	/** Totzone mit Neuskalierung: |x| < Dead -> 0, sonst weiche Skalierung auf 0..1. */
	FREELANCER_API float ApplyDeadZone(float Value, float DeadZone);
}

/** Eingabeaktionen und Standardbelegung (ohne Editor-Assets, zur Laufzeit erzeugt). */
struct FREELANCER_API FFLInputActions
{
	TObjectPtr<UInputAction> Throttle;
	TObjectPtr<UInputAction> StrafeRight;
	TObjectPtr<UInputAction> StrafeUp;
	TObjectPtr<UInputAction> Roll;
	TObjectPtr<UInputAction> Look;
	TObjectPtr<UInputMappingContext> Mapping;
};

namespace FLInput
{
	/**
	 * Erzeugt Aktionen und Belegung. Tastatur/Maus: W/S Schub, A/D seitwaerts, Leertaste/Strg hoch/runter, Q/E Rollen, Maus Steuern.
	 * Gamepad: Trigger Schub, linker Stick seitwaerts/hoch, rechter Stick Steuern, Schultertasten Rollen.
	 */
	FREELANCER_API FFLInputActions CreateDefaultActions(UObject* Outer);
}

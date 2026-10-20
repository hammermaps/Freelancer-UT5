// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLFlightInput.h"

#include "EnhancedInput/Public/InputAction.h"
#include "EnhancedInput/Public/InputMappingContext.h"
#include "EnhancedInput/Public/InputModifiers.h"
#include "InputCoreTypes.h"

float FLInput::ApplyDeadZone(float Value, float DeadZone)
{
	const float Abs = FMath::Abs(Value);
	if (Abs <= DeadZone)
	{
		return 0.f;
	}
	const float Scaled = (Abs - DeadZone) / FMath::Max(1.f - DeadZone, KINDA_SMALL_NUMBER);
	return FMath::Clamp(Scaled, 0.f, 1.f) * FMath::Sign(Value);
}

FFLFlightInput FLInput::Compose(const FFLRawInput& Raw, const FFLInputSettings& S)
{
	FFLFlightInput Out;
	Out.Throttle = FMath::Clamp(Raw.Throttle, -1.f, 1.f);
	Out.StrafeRight = FMath::Clamp(Raw.StrafeRight, -1.f, 1.f);
	Out.StrafeUp = FMath::Clamp(Raw.StrafeUp, -1.f, 1.f);
	Out.Roll = FMath::Clamp(Raw.Roll, -1.f, 1.f);
	float Yaw, Pitch;
	if (Raw.bLookIsDelta)
	{
		Yaw = Raw.Look.X * S.MouseSensitivity;
		Pitch = Raw.Look.Y * S.MouseSensitivity;
	}
	else
	{
		Yaw = ApplyDeadZone(Raw.Look.X, S.StickDeadZone);
		Pitch = ApplyDeadZone(Raw.Look.Y, S.StickDeadZone);
	}
	Out.Yaw = FMath::Clamp(Yaw, -1.f, 1.f);
	Out.Pitch = FMath::Clamp(S.bInvertPitch ? -Pitch : Pitch, -1.f, 1.f);
	return Out;
}

namespace
{
UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* A = NewObject<UInputAction>(Outer, Name);
	A->ValueType = Type;
	return A;
}

void Negate(UObject* Outer, FEnhancedActionKeyMapping& M)
{
	M.Modifiers.Add(NewObject<UInputModifierNegate>(Outer));
}
}

FFLInputActions FLInput::CreateDefaultActions(UObject* Outer)
{
	FFLInputActions A;
	A.Throttle = MakeAction(Outer, TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	A.StrafeRight = MakeAction(Outer, TEXT("IA_StrafeRight"), EInputActionValueType::Axis1D);
	A.StrafeUp = MakeAction(Outer, TEXT("IA_StrafeUp"), EInputActionValueType::Axis1D);
	A.Roll = MakeAction(Outer, TEXT("IA_Roll"), EInputActionValueType::Axis1D);
	A.Look = MakeAction(Outer, TEXT("IA_Look"), EInputActionValueType::Axis2D);

	UInputMappingContext* Ctx = NewObject<UInputMappingContext>(Outer, TEXT("IMC_Flight"));
	A.Mapping = Ctx;

	// Schub: W/S, Gamepad-Trigger
	Ctx->MapKey(A.Throttle, EKeys::W);
	Negate(Outer, Ctx->MapKey(A.Throttle, EKeys::S));
	Ctx->MapKey(A.Throttle, EKeys::Gamepad_RightTriggerAxis);
	Negate(Outer, Ctx->MapKey(A.Throttle, EKeys::Gamepad_LeftTriggerAxis));
	// Seitwaerts: D/A, linker Stick X
	Ctx->MapKey(A.StrafeRight, EKeys::D);
	Negate(Outer, Ctx->MapKey(A.StrafeRight, EKeys::A));
	Ctx->MapKey(A.StrafeRight, EKeys::Gamepad_LeftX);
	// Hoch/Runter: Leertaste/Strg, linker Stick Y
	Ctx->MapKey(A.StrafeUp, EKeys::SpaceBar);
	Negate(Outer, Ctx->MapKey(A.StrafeUp, EKeys::LeftControl));
	Ctx->MapKey(A.StrafeUp, EKeys::Gamepad_LeftY);
	// Rollen: E rechts, Q links, Schultertasten
	Ctx->MapKey(A.Roll, EKeys::E);
	Negate(Outer, Ctx->MapKey(A.Roll, EKeys::Q));
	Ctx->MapKey(A.Roll, EKeys::Gamepad_RightShoulder);
	Negate(Outer, Ctx->MapKey(A.Roll, EKeys::Gamepad_LeftShoulder));
	// Steuern: Maus (Delta) und rechter Stick
	Ctx->MapKey(A.Look, EKeys::Mouse2D);
	Ctx->MapKey(A.Look, EKeys::Gamepad_Right2D);
	return A;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flight/FLDamage.h"

double FLDamage::Collision(double ImpactSpeedCmPerSec, double MassA, double MassB, double Restitution, const FFLDamageParams& Params)
{
	if (ImpactSpeedCmPerSec < Params.MinImpactSpeed || MassA <= 0.0)
	{
		return 0.0;
	}
	const double V = ImpactSpeedCmPerSec * 0.01; // m/s
	const double E = FMath::Clamp(Restitution, 0.0, 1.0);
	const double ReducedMass = MassB > 0.0 ? 1.0 / (1.0 / MassA + 1.0 / MassB) : MassA;
	return Params.CollisionDamageFactor * Params.EnergyScale * (1.0 - E * E) * 0.5 * ReducedMass * V * V;
}

double FLDamage::Apply(double& Hull, double Damage)
{
	if (Damage <= 0.0 || Hull <= 0.0)
	{
		return 0.0;
	}
	const double Taken = FMath::Min(Hull, Damage);
	Hull -= Taken;
	return Taken;
}

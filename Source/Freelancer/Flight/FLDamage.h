// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Schadensparameter. Die Formel ist vorlaeufig (Faktor aus constants.ini, Formel selbst nicht dokumentiert -> per Reverse Engineering pruefen). */
struct FREELANCER_API FFLDamageParams
{
	/** COLLISION_DAMAGE_FACTOR aus constants.ini */
	double CollisionDamageFactor = 0.5;

	/** Vorlaeufiger Massstab (aus dem Spielgefuehl kalibriert: Jaeger mit 100 kg und 1300 Trefferpunkten verliert bei 80 m/s gegen ein
	 *  festes Objekt (Restitution 0,3) etwa die Haelfte seiner Trefferpunkte). Per Reverse Engineering/Vergleich mit dem Original zu pruefen. */
	double EnergyScale = 0.004;

	/** Unterhalb dieser Annaeherungsgeschwindigkeit (cm/s, = 1 m/s) richtet ein Zusammenstoss keinen Schaden an */
	double MinImpactSpeed = 100.0;
};

namespace FLDamage
{
	/**
	 * Kollisionsschaden: Faktor * Massstab * verlorene Bewegungsenergie = Faktor * Massstab * (1 - e^2) * 0,5 * mu * v^2
	 * (mu = reduzierte Masse, v in m/s). MassB <= 0 bedeutet unbewegliches Objekt (mu = MassA). Schnelle Rammstoesse
	 * (Cruise!) sind dadurch deutlich gefaehrlicher als langsame.
	 */
	FREELANCER_API double Collision(double ImpactSpeedCmPerSec, double MassA, double MassB, double Restitution, const FFLDamageParams& Params = FFLDamageParams());

	/** Zieht Schaden von den Trefferpunkten ab (nicht unter 0); gibt den tatsaechlich abgezogenen Schaden zurueck. */
	FREELANCER_API double Apply(double& Hull, double Damage);
}

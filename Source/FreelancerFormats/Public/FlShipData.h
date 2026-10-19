// Schiffsdefinitionen aus shiparch.ini (Datenmodell des Imports, ohne UE-Abhaengigkeit).
#pragma once
#include "FlApi.h"
#include "FlIni.h"
#include <string>
#include <vector>

namespace fl
{
struct FlShipRecord
{
	std::string Nickname;
	std::string Type;          // FIGHTER, FREIGHTER, ...
	int IdsName = 0;
	int IdsInfo = 0;
	int ShipClass = 0;
	float Mass = 0;
	float LinearDrag = 0;
	int HitPts = 0;
	int HoldSize = 0;
	float NudgeForce = 0;
	float StrafeForce = 0;
	float SteeringTorque[3] = {0, 0, 0};
	float AngularDrag[3] = {0, 0, 0};
	float RotationInertia[3] = {0, 0, 0};
	std::string Archetype;     // DA_archetype, Pfad im Original (\ -> /)
	std::vector<std::string> MaterialLibraries;
	std::string Cockpit;
};

// Liest alle [Ship]-Sektionen. Sektionen ohne nickname werden uebersprungen; Zahlen mit Fehlern werden 0.
FLF_API std::vector<FlShipRecord> ParseShipArch(const IniFile& Ini);
}

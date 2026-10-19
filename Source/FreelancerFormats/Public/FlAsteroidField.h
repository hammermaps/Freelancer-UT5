// Asteroidenfelder (SOLAR/ASTEROIDS/*.ini): Feldparameter, Wuerfel-Anordnung, Baender, Ausschlusszonen.
#pragma once
#include "FlApi.h"
#include "FlIni.h"
#include <string>
#include <utility>
#include <vector>

namespace fl
{
struct FlFieldCube
{
	std::string Asteroid;     // Nickname aus asteroidarch.ini
	float Offset[3] = {0, 0, 0};   // Lage im Wuerfel als Anteil der Wuerfelgroesse (-1 .. 1)
	float RotationDeg[3] = {0, 0, 0}; // Euler-Winkel in Grad (wie "rotate")
};

struct FlFieldSection
{
	std::string Name; // z.B. "Field", "Band", "properties", "DynamicAsteroids"
	std::vector<std::pair<std::string, std::string>> Props; // Schluessel klein, Wiederholungen bleiben
};

struct FlAsteroidField
{
	std::vector<FlFieldSection> Sections;     // alle Sektionen ausser [Cube]
	std::vector<FlFieldCube> Cubes;           // Zeilen "asteroid = Name, x, y, z, rx, ry, rz" aus [Cube]
	std::vector<std::string> Exclusions;      // "exclusion = Zone" aus [Exclusion Zones]
};

FLF_API FlAsteroidField ParseAsteroidField(const IniFile& Ini);
}

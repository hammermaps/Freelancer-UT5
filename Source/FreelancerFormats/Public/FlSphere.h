// Planeten-Kugeldefinition (.sph, UTF): Radius, Seitenzahl und Materialnamen je Seite.
#pragma once
#include "FlApi.h"
#include "FlUtf.h"
#include <string>
#include <vector>

namespace fl
{
struct FlSphere
{
	float Radius = 0;
	int Sides = 0;
	// M0, M1, ... in Reihenfolge der Seiten (Namen der Materialien, nicht Texturen)
	std::vector<std::string> SideMaterials;
};

// "Sphere"/{Radius, Sides, M0..Mn}; false wenn der Knoten fehlt.
FLF_API bool ReadSphere(const UtfNode& Root, FlSphere& Out);
}

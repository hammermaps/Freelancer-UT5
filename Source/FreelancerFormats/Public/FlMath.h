// Kleine Hilfen fuer INI-Werte (Zahlenlisten) und Winkel.
#pragma once
#include "FlApi.h"
#include <string>
#include <vector>

namespace fl
{
// "1, 2.5, -3" -> {1, 2.5, -3}; nicht lesbare Teile werden 0.
FLF_API std::vector<float> ParseFloats(const std::string& Text);

// Euler-Winkel in Grad (INI "rotate = x, y, z") -> 3x3 zeilenweise, Spaltenvektor-Konvention.
// Reihenfolge wie im Original: erst um X, dann Y, dann Z (R = Rz * Ry * Rx).
FLF_API void FlEulerToRotation(const float Degrees[3], float Out[9]);
}

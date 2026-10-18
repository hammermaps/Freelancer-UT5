// Materialien, Hardpoints und CRC-Hashes aus Freelancer-Modelldateien.
#pragma once
#include "Utf.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fl
{
// CRC fuer Modell-/Materialnamen (nicht case-sensitiv). Referenz: Librelancer CrcTool.FLModelCrc.
uint32_t FlModelCrc(const std::string& Name);

struct FlMaterial
{
	std::string Name;
	std::string Type;      // z.B. "DcDt"
	std::string DiffuseTexture; // Dt_name
	float Dc[3] = {1, 1, 1};
};

// "Material library" -> Map CRC(Name) -> Material
std::map<uint32_t, FlMaterial> ReadMaterials(const UtfNode& Root);

struct FlHardpoint
{
	std::string Name;
	bool bRevolute = false;
	float Position[3] = {0, 0, 0};
	float Orientation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // 3x3 zeilenweise
};

// "Hardpoints"/{Fixed|Revolute}/<name>/{Position, Orientation}
std::vector<FlHardpoint> ReadHardpoints(const UtfNode& Root);
}

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

struct FlConstruct
{
	enum EKind { Fix, Rev, Pris } Kind = Fix;
	std::string Parent, Child;
	float Origin[3] = {0, 0, 0};
	float Offset[3] = {0, 0, 0};            // nur Rev/Pris
	float Rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // 3x3 zeilenweise (Spaltenvektor-Konvention)
	float Axis[3] = {0, 0, 0};              // nur Rev/Pris
	float Min = 0, Max = 0;                 // nur Rev/Pris
};

// "Cmpnd"/"Cons"/{Fix,Rev,Pris}. Unbekannte Typen (Loose, Sphere) werden uebersprungen.
std::vector<FlConstruct> ReadConstructs(const UtfNode& Root);

struct FlPart
{
	std::string Object; // "Object name", Kind-Name im Konstrukt
	std::string File;   // "File name", Knoten mit dem 3db-Inhalt im .cmp
};
std::vector<FlPart> ReadParts(const UtfNode& Root);

struct FlMeshRef
{
	uint32_t MeshCrc = 0;
	uint16_t StartMesh = 0, MeshCount = 0;
};
// Erste VMeshRef im Modell-Knoten ("MultiLevel"/"Level0"/"VMeshPart"/"VMeshRef"), sonst false.
bool ReadLevel0Ref(const UtfNode& Model, FlMeshRef& Out);

// 3x3 (zeilenweise) -> Quaternion x,y,z,w
void RotationToQuat(const float R[9], float Q[4]);
}

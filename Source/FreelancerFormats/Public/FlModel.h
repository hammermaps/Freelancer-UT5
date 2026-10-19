// Materialien, Hardpoints und CRC-Hashes aus Freelancer-Modelldateien.
#pragma once
#include "FlApi.h"
#include "FlUtf.h"
#include "FlVms.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fl
{
// CRC fuer Modell-/Materialnamen (nicht case-sensitiv). Referenz: Librelancer CrcTool.FLModelCrc.
FLF_API uint32_t FlModelCrc(const std::string& Name);

struct FlMaterial
{
	std::string Name;
	std::string Type;      // z.B. "DcDt"
	std::string DiffuseTexture; // Dt_name
	float Dc[3] = {1, 1, 1};
};

// "Material library" -> Map CRC(Name) -> Material
FLF_API std::map<uint32_t, FlMaterial> ReadMaterials(const UtfNode& Root);

struct FlHardpoint
{
	std::string Name;
	bool bRevolute = false;
	float Position[3] = {0, 0, 0};
	float Orientation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // 3x3 zeilenweise
};

// "Hardpoints"/{Fixed|Revolute}/<name>/{Position, Orientation}
FLF_API std::vector<FlHardpoint> ReadHardpoints(const UtfNode& Root);

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
FLF_API std::vector<FlConstruct> ReadConstructs(const UtfNode& Root);

struct FlPart
{
	std::string Object; // "Object name", Kind-Name im Konstrukt
	std::string File;   // "File name", Knoten mit dem 3db-Inhalt im .cmp
};
FLF_API std::vector<FlPart> ReadParts(const UtfNode& Root);

struct FlMeshRef
{
	uint32_t MeshCrc = 0;
	uint16_t StartMesh = 0, MeshCount = 0;
};
// Erste VMeshRef im Modell-Knoten ("MultiLevel"/"Level0"/"VMeshPart"/"VMeshRef"), sonst false.
FLF_API bool ReadLevel0Ref(const UtfNode& Model, FlMeshRef& Out);

// 3x3 (zeilenweise) -> Quaternion x,y,z,w
FLF_API void RotationToQuat(const float R[9], float Q[4]);

struct FlTexture
{
	std::string Name; // ohne Endung .tga/.dds, '/' und '\\' durch '_' ersetzt
	std::vector<uint8_t> Data; // groesste MIP-Stufe, DDS oder TGA
};
// "Texture library"/<name>/{MIPS | MIP0..n}: je Textur die groesste Stufe.
FLF_API std::vector<FlTexture> ReadTextures(const UtfNode& Root);

// ---- Gesamtmodell (Compound .cmp oder einzelnes .3db) ----

struct FlModelPart
{
	std::string Name;
	int Parent = -1;               // Index in FlModel::Parts, -1 = Wurzel
	float Position[3] = {0, 0, 0}; // lokal, Originalkoordinaten
	float Rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // 3x3 zeilenweise
	bool bHasMesh = false;
	VmsMesh Mesh;                  // LOD0, Gruppen nach Material
	std::vector<FlHardpoint> Hardpoints;
	FlConstruct::EKind Joint = FlConstruct::Fix;
	float Axis[3] = {0, 0, 0};     // Rev/Pris
	float Min = 0, Max = 0;
};

struct FlModel
{
	std::vector<FlModelPart> Parts;
};

// Baut aus einem .cmp (Teile + Konstrukte) oder .3db (ein Teil) das Gesamtmodell. Fehler -> false + Error.
FLF_API bool BuildModel(const UtfNode& Root, FlModel& Out, std::string& Error);

// Koordinaten Original (x rechts, y oben, z hinten; Meter) -> Unreal (x vorn, y rechts, z oben; Zentimeter).
// Die Abbildung (x,y,z) -> (-z, x, y) spiegelt die Handedness; Dreiecke muessen umgedreht werden.
FLF_API void FlToUePosition(const float In[3], float Out[3]);
FLF_API void FlToUeDirection(const float In[3], float Out[3]);   // ohne Skalierung (Normalen, Achsen)
FLF_API void FlToUeRotation(const float In[9], float Out[9]);    // M * R * M^T
}

// VMeshData (Vertex-Mesh) aus UTF-Containern lesen. Referenz: Librelancer LibreLancer/Utf/Vms/VMeshData.cs
#pragma once
#include "FlApi.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fl
{
struct VmsGroup
{
	uint32_t MaterialCrc = 0;
	uint16_t StartVertex = 0, EndVertex = 0;
	uint16_t NumRefIndices = 0; // Indexanzahl (Dreiecke * 3)
	uint32_t IndexStart = 0;
};

struct VmsMesh
{
	uint16_t Fvf = 0;
	uint32_t Stride = 0;
	bool bNormal = false, bDiffuse = false;
	int TexCoords = 0;
	std::vector<float> Positions; // xyz
	std::vector<float> Normals;   // xyz, leer wenn nicht vorhanden
	std::vector<float> Uv0;       // uv, leer wenn nicht vorhanden
	std::vector<uint16_t> Indices;
	std::vector<VmsGroup> Groups;
};

// Gibt false und setzt Error bei ungueltigen Daten.
FLF_API bool ParseVms(const std::vector<uint8_t>& Data, VmsMesh& Out, std::string& Error);

struct GlbMaterial
{
	std::string Name;
	std::string Texture; // Dateiname ohne Pfad; wird als "<basis>.png" referenziert
	float Dc[3] = {1, 1, 1};
};

struct SceneNode
{
	std::string Name;
	int Parent = -1;
	float T[3] = {0, 0, 0};
	float Q[4] = {0, 0, 0, 1}; // x y z w
	bool bHasMesh = false;
	VmsMesh Mesh;
};

// Kopiert Gruppen [FirstGroup, FirstGroup+GroupCount) mit neu nummerierten Vertices (Teilmesh eines Teils).
FLF_API VmsMesh ExtractGroups(const VmsMesh& Src, size_t FirstGroup, size_t GroupCount);

// Szene mit Knotenhierarchie (Teile eines .cmp, Hardpoints als leere Knoten).
FLF_API std::vector<uint8_t> BuildGlbScene(const std::vector<SceneNode>& Scene, const std::map<uint32_t, GlbMaterial>* Mats = nullptr);

// Schreibt Meshes als binaeres glTF (.glb). Pro VmsGroup eine Primitive; MaterialCrc steht in extras.
FLF_API std::vector<uint8_t> BuildGlb(const std::vector<std::pair<std::string, VmsMesh>>& Meshes, const std::map<uint32_t, GlbMaterial>* Mats = nullptr);
}

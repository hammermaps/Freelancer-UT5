// VMeshData (Vertex-Mesh) aus UTF-Containern lesen. Referenz: Librelancer LibreLancer/Utf/Vms/VMeshData.cs
#pragma once
#include <cstdint>
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
bool ParseVms(const std::vector<uint8_t>& Data, VmsMesh& Out, std::string& Error);

// Schreibt Meshes als binaeres glTF (.glb). Pro VmsGroup eine Primitive; MaterialCrc steht in extras.
std::vector<uint8_t> BuildGlb(const std::vector<std::pair<std::string, VmsMesh>>& Meshes);
}

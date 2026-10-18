#include "Vms.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace fl
{
bool ParseVms(const std::vector<uint8_t>& D, VmsMesh& M, std::string& Err)
{
	if (D.size() < 16) { Err = "VMeshData zu kurz"; return false; }
	auto U16 = [&](size_t O) { uint16_t V; std::memcpy(&V, D.data() + O, 2); return V; };
	auto U32 = [&](size_t O) { uint32_t V; std::memcpy(&V, D.data() + O, 4); return V; };
	uint16_t MeshCount = U16(8), IndexCount = U16(10);
	M.Fvf = U16(12);
	uint16_t VertCount = U16(14);
	if (!(M.Fvf & 0x2)) { Err = "FVF ohne XYZ"; return false; }
	M.bNormal = M.Fvf & 0x10;
	M.bDiffuse = M.Fvf & 0x40;
	M.TexCoords = (M.Fvf >> 8) & 0xF;
	if (M.TexCoords > 4) { Err = "zu viele Texturkoordinaten"; return false; }
	M.Stride = 12 + (M.bNormal ? 12 : 0) + (M.bDiffuse ? 4 : 0) + M.TexCoords * 8;

	size_t HeaderEnd = 16 + (size_t)MeshCount * 12;
	size_t IndexEnd = HeaderEnd + (size_t)IndexCount * 2;
	size_t VertEnd = IndexEnd + (size_t)VertCount * M.Stride;
	if (VertEnd > D.size()) { Err = "VMeshData abgeschnitten"; return false; }

	uint32_t Start = 0;
	for (uint16_t i = 0; i < MeshCount; ++i)
	{
		size_t O = 16 + (size_t)i * 12;
		VmsGroup G;
		G.MaterialCrc = U32(O);
		G.StartVertex = U16(O + 4);
		G.EndVertex = U16(O + 6);
		G.NumRefIndices = U16(O + 8);
		G.IndexStart = Start;
		Start += G.NumRefIndices;
		if (Start > IndexCount) { Err = "Gruppe ueberschreitet Indexpuffer"; return false; }
		M.Groups.push_back(G);
	}
	M.Indices.resize(IndexCount);
	std::memcpy(M.Indices.data(), D.data() + HeaderEnd, (size_t)IndexCount * 2);
	for (uint16_t Idx : M.Indices) if (Idx >= VertCount) { Err = "Index ausserhalb der Vertices"; return false; }

	M.Positions.resize((size_t)VertCount * 3);
	if (M.bNormal) M.Normals.resize((size_t)VertCount * 3);
	if (M.TexCoords > 0) M.Uv0.resize((size_t)VertCount * 2);
	size_t UvOff = 12 + (M.bNormal ? 12 : 0) + (M.bDiffuse ? 4 : 0);
	for (size_t v = 0; v < VertCount; ++v)
	{
		const uint8_t* P = D.data() + IndexEnd + v * M.Stride;
		std::memcpy(&M.Positions[v * 3], P, 12);
		if (M.bNormal) std::memcpy(&M.Normals[v * 3], P + 12, 12);
		if (M.TexCoords > 0) std::memcpy(&M.Uv0[v * 2], P + UvOff, 8);
	}
	return true;
}

namespace
{
void Pad4(std::vector<uint8_t>& B, uint8_t Fill) { while (B.size() % 4) B.push_back(Fill); }
template <typename T> void Append(std::vector<uint8_t>& B, const std::vector<T>& V)
{
	size_t O = B.size();
	B.resize(O + V.size() * sizeof(T));
	if (!V.empty()) std::memcpy(B.data() + O, V.data(), V.size() * sizeof(T));
}
std::string Esc(const std::string& S)
{
	std::string R;
	for (char C : S) { if (C == '"' || C == '\\') R += '\\'; if ((unsigned char)C >= 32) R += C; }
	return R;
}
}

std::vector<uint8_t> BuildGlb(const std::vector<std::pair<std::string, VmsMesh>>& Meshes, const std::map<uint32_t, GlbMaterial>* Mats)
{
	std::map<uint32_t, int> MatIndex;
	std::string MatJson, TexJson, ImgJson;
	int TexCount = 0;
	std::vector<uint8_t> Bin;
	std::string Views, Accessors, MeshJson, Nodes, SceneNodes;
	int View = 0, Acc = 0;
	auto AddView = [&](size_t Off, size_t Len, int Target) {
		if (View) Views += ",";
		Views += "{\"buffer\":0,\"byteOffset\":" + std::to_string(Off) + ",\"byteLength\":" + std::to_string(Len) + ",\"target\":" + std::to_string(Target) + "}";
		return View++;
	};
	auto AddAcc = [&](int V, int CompType, size_t Count, const char* Type, const std::string& MinMax = "") {
		if (Acc) Accessors += ",";
		Accessors += "{\"bufferView\":" + std::to_string(V) + ",\"componentType\":" + std::to_string(CompType) + ",\"count\":" + std::to_string(Count) + ",\"type\":\"" + Type + "\"" + MinMax + "}";
		return Acc++;
	};
	for (size_t mi = 0; mi < Meshes.size(); ++mi)
	{
		const VmsMesh& M = Meshes[mi].second;
		size_t VC = M.Positions.size() / 3;
		float Mn[3] = {1e30f, 1e30f, 1e30f}, Mx[3] = {-1e30f, -1e30f, -1e30f};
		for (size_t v = 0; v < VC; ++v)
			for (int a = 0; a < 3; ++a) { float X = M.Positions[v * 3 + a]; if (X < Mn[a]) Mn[a] = X; if (X > Mx[a]) Mx[a] = X; }
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), ",\"min\":[%.6g,%.6g,%.6g],\"max\":[%.6g,%.6g,%.6g]", Mn[0], Mn[1], Mn[2], Mx[0], Mx[1], Mx[2]);

		size_t Off = Bin.size(); Append(Bin, M.Positions);
		int PosAcc = AddAcc(AddView(Off, Bin.size() - Off, 34962), 5126, VC, "VEC3", VC ? Buf : "");
		int NrmAcc = -1, UvAcc = -1;
		if (!M.Normals.empty()) { Off = Bin.size(); Append(Bin, M.Normals); NrmAcc = AddAcc(AddView(Off, Bin.size() - Off, 34962), 5126, VC, "VEC3"); }
		if (!M.Uv0.empty()) { Off = Bin.size(); Append(Bin, M.Uv0); UvAcc = AddAcc(AddView(Off, Bin.size() - Off, 34962), 5126, VC, "VEC2"); }
		std::string Prims;
		for (auto& G : M.Groups)
		{
			size_t Count = G.NumRefIndices - G.NumRefIndices % 3;
			if (!Count) continue;
			std::vector<uint16_t> Idx(M.Indices.begin() + G.IndexStart, M.Indices.begin() + G.IndexStart + Count);
			Pad4(Bin, 0);
			Off = Bin.size(); Append(Bin, Idx);
			int IdxAcc = AddAcc(AddView(Off, Bin.size() - Off, 34963), 5123, Count, "SCALAR");
			if (!Prims.empty()) Prims += ",";
			Prims += "{\"attributes\":{\"POSITION\":" + std::to_string(PosAcc);
			if (NrmAcc >= 0) Prims += ",\"NORMAL\":" + std::to_string(NrmAcc);
			if (UvAcc >= 0) Prims += ",\"TEXCOORD_0\":" + std::to_string(UvAcc);
			Prims += "},\"indices\":" + std::to_string(IdxAcc);
			if (Mats)
			{
				auto Found = Mats->find(G.MaterialCrc);
				if (Found != Mats->end())
				{
					auto Ins = MatIndex.emplace(G.MaterialCrc, (int)MatIndex.size());
					if (Ins.second)
					{
						const GlbMaterial& GM = Found->second;
						char C[160];
						std::snprintf(C, sizeof(C), "\"baseColorFactor\":[%.4g,%.4g,%.4g,1]", GM.Dc[0], GM.Dc[1], GM.Dc[2]);
						std::string Pbr = std::string("{") + C;
						if (!GM.Texture.empty())
						{
							std::string Base = GM.Texture;
							size_t Dot = Base.rfind('.');
							if (Dot != std::string::npos) Base.resize(Dot);
							if (TexCount) { TexJson += ","; ImgJson += ","; }
							ImgJson += "{\"uri\":\"" + Esc(Base) + ".png\"}";
							TexJson += "{\"source\":" + std::to_string(TexCount) + "}";
							Pbr += ",\"baseColorTexture\":{\"index\":" + std::to_string(TexCount++) + "}";
						}
						Pbr += ",\"metallicFactor\":0,\"roughnessFactor\":1}";
						if (!MatJson.empty()) MatJson += ",";
						MatJson += "{\"name\":\"" + Esc(GM.Name) + "\",\"pbrMetallicRoughness\":" + Pbr + "}";
					}
					Prims += ",\"material\":" + std::to_string(Ins.first->second);
				}
			}
			Prims += ",\"extras\":{\"materialCrc\":" + std::to_string(G.MaterialCrc) + "}}";
		}
		Pad4(Bin, 0);
		if (mi) { MeshJson += ","; Nodes += ","; SceneNodes += ","; }
		MeshJson += "{\"name\":\"" + Esc(Meshes[mi].first) + "\",\"primitives\":[" + Prims + "]}";
		Nodes += "{\"name\":\"" + Esc(Meshes[mi].first) + "\",\"mesh\":" + std::to_string(mi) + "}";
		SceneNodes += std::to_string(mi);
	}
	std::string Json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"fltool\"},\"scene\":0,\"scenes\":[{\"nodes\":[" + SceneNodes + "]}],\"nodes\":[" + Nodes + "],\"meshes\":[" + MeshJson + "]" + (MatJson.empty() ? std::string() : ",\"materials\":[" + MatJson + "]") + (TexJson.empty() ? std::string() : ",\"textures\":[" + TexJson + "],\"images\":[" + ImgJson + "]") + ",\"accessors\":[" + Accessors + "],\"bufferViews\":[" + Views + "],\"buffers\":[{\"byteLength\":" + std::to_string(Bin.size()) + "}]}";
	while (Json.size() % 4) Json += ' ';
	std::vector<uint8_t> Out;
	auto W32 = [&](uint32_t V) { Out.insert(Out.end(), (uint8_t*)&V, (uint8_t*)&V + 4); };
	W32(0x46546C67); W32(2); W32((uint32_t)(12 + 8 + Json.size() + 8 + Bin.size()));
	W32((uint32_t)Json.size()); W32(0x4E4F534A); Out.insert(Out.end(), Json.begin(), Json.end());
	W32((uint32_t)Bin.size()); W32(0x004E4942); Out.insert(Out.end(), Bin.begin(), Bin.end());
	return Out;
}
}

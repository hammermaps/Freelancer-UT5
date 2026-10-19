#include "FlModel.h"
#include "FlIni.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <cstring>

namespace fl
{
static const uint32_t CrcTable[256] = {
#include "CrcTable.inc"
};

uint32_t FlModelCrc(const std::string& Name)
{
	uint32_t Crc = 0xFFFFFFFF;
	for (unsigned char C : Name) Crc = (Crc >> 8) ^ CrcTable[(uint8_t)Crc ^ (uint8_t)std::tolower(C)];
	return ~Crc;
}

static std::string AsString(const std::vector<uint8_t>& D)
{
	size_t N = 0;
	while (N < D.size() && D[N]) ++N;
	return std::string((const char*)D.data(), N);
}

std::map<uint32_t, FlMaterial> ReadMaterials(const UtfNode& Root)
{
	std::map<uint32_t, FlMaterial> R;
	const UtfNode* Lib = FindChild(Root, "Material library");
	if (!Lib) return R;
	for (auto& N : Lib->Children)
	{
		if (!N->bIntermediate) continue;
		FlMaterial M;
		M.Name = N->Name;
		if (auto T = FindChild(*N, "Type")) M.Type = AsString(T->Data);
		if (auto T = FindChild(*N, "Dt_name")) M.DiffuseTexture = AsString(T->Data);
		if (auto T = FindChild(*N, "Dc"); T && T->Data.size() >= 12) std::memcpy(M.Dc, T->Data.data(), 12);
		R[FlModelCrc(M.Name)] = M;
	}
	return R;
}

std::vector<FlHardpoint> ReadHardpoints(const UtfNode& Root)
{
	std::vector<FlHardpoint> R;
	const UtfNode* HP = FindChild(Root, "Hardpoints");
	if (!HP) return R;
	for (auto& Kind : HP->Children)
	{
		if (!Kind->bIntermediate) continue;
		bool bRev = IEquals(Kind->Name, "revolute");
		if (!bRev && !IEquals(Kind->Name, "fixed")) continue;
		for (auto& H : Kind->Children)
		{
			if (!H->bIntermediate) continue;
			FlHardpoint P;
			P.Name = H->Name;
			P.bRevolute = bRev;
			if (auto T = FindChild(*H, "Position"); T && T->Data.size() >= 12) std::memcpy(P.Position, T->Data.data(), 12);
			if (auto T = FindChild(*H, "Orientation"); T && T->Data.size() >= 36) std::memcpy(P.Orientation, T->Data.data(), 36);
			R.push_back(P);
		}
	}
	return R;
}

std::vector<FlConstruct> ReadConstructs(const UtfNode& Root)
{
	std::vector<FlConstruct> R;
	const UtfNode* Cmpnd = FindChild(Root, "Cmpnd");
	const UtfNode* Cons = Cmpnd ? FindChild(*Cmpnd, "Cons") : nullptr;
	if (!Cons) return R;
	for (auto& N : Cons->Children)
	{
		FlConstruct::EKind Kind;
		size_t Size;
		if (IEquals(N->Name, "fix")) { Kind = FlConstruct::Fix; Size = 176; }
		else if (IEquals(N->Name, "rev")) { Kind = FlConstruct::Rev; Size = 208; }
		else if (IEquals(N->Name, "pris")) { Kind = FlConstruct::Pris; Size = 208; }
		else continue;
		for (size_t O = 0; O + Size <= N->Data.size(); O += Size)
		{
			const uint8_t* P = N->Data.data() + O;
			FlConstruct C;
			C.Kind = Kind;
			C.Parent = std::string((const char*)P, strnlen((const char*)P, 64));
			C.Child = std::string((const char*)P + 64, strnlen((const char*)P + 64, 64));
			std::memcpy(C.Origin, P + 128, 12);
			if (Kind == FlConstruct::Fix) std::memcpy(C.Rotation, P + 140, 36);
			else
			{
				std::memcpy(C.Offset, P + 140, 12);
				std::memcpy(C.Rotation, P + 152, 36);
				std::memcpy(C.Axis, P + 188, 12);
				std::memcpy(&C.Min, P + 200, 4);
				std::memcpy(&C.Max, P + 204, 4);
			}
			R.push_back(C);
		}
	}
	return R;
}

std::vector<FlPart> ReadParts(const UtfNode& Root)
{
	std::vector<FlPart> R;
	const UtfNode* Cmpnd = FindChild(Root, "Cmpnd");
	if (!Cmpnd) return R;
	for (auto& N : Cmpnd->Children)
	{
		std::string L = N->Name;
		std::transform(L.begin(), L.end(), L.begin(), [](unsigned char C) { return std::tolower(C); });
		if (!N->bIntermediate || (L.rfind("part_", 0) != 0 && L != "root")) continue;
		FlPart P;
		if (auto T = FindChild(*N, "Object name")) P.Object = AsString(T->Data);
		if (auto T = FindChild(*N, "File name")) P.File = AsString(T->Data);
		R.push_back(P);
	}
	return R;
}

bool ReadLevel0Ref(const UtfNode& Model, FlMeshRef& Out)
{
	const UtfNode* ML = FindChild(Model, "MultiLevel");
	const UtfNode* L0 = ML ? FindChild(*ML, "Level0") : nullptr;
	const UtfNode* VP = L0 ? FindChild(*L0, "VMeshPart") : nullptr;
	const UtfNode* Ref = VP ? FindChild(*VP, "VMeshRef") : nullptr;
	if (!Ref || Ref->Data.size() < 20) return false;
	std::memcpy(&Out.MeshCrc, Ref->Data.data() + 4, 4);
	std::memcpy(&Out.StartVertex, Ref->Data.data() + 8, 2);
	std::memcpy(&Out.StartMesh, Ref->Data.data() + 16, 2);
	std::memcpy(&Out.MeshCount, Ref->Data.data() + 18, 2);
	return true;
}

void RotationToQuat(const float R[9], float Q[4])
{
	float Tr = R[0] + R[4] + R[8];
	if (Tr > 0)
	{
		float S = std::sqrt(Tr + 1.0f) * 2;
		Q[3] = 0.25f * S; Q[0] = (R[7] - R[5]) / S; Q[1] = (R[2] - R[6]) / S; Q[2] = (R[3] - R[1]) / S;
	}
	else if (R[0] > R[4] && R[0] > R[8])
	{
		float S = std::sqrt(1.0f + R[0] - R[4] - R[8]) * 2;
		Q[3] = (R[7] - R[5]) / S; Q[0] = 0.25f * S; Q[1] = (R[1] + R[3]) / S; Q[2] = (R[2] + R[6]) / S;
	}
	else if (R[4] > R[8])
	{
		float S = std::sqrt(1.0f + R[4] - R[0] - R[8]) * 2;
		Q[3] = (R[2] - R[6]) / S; Q[0] = (R[1] + R[3]) / S; Q[1] = 0.25f * S; Q[2] = (R[5] + R[7]) / S;
	}
	else
	{
		float S = std::sqrt(1.0f + R[8] - R[0] - R[4]) * 2;
		Q[3] = (R[3] - R[1]) / S; Q[0] = (R[2] + R[6]) / S; Q[1] = (R[5] + R[7]) / S; Q[2] = 0.25f * S;
	}
	float L = std::sqrt(Q[0] * Q[0] + Q[1] * Q[1] + Q[2] * Q[2] + Q[3] * Q[3]);
	if (L > 0) for (int i = 0; i < 4; ++i) Q[i] /= L; else { Q[0] = Q[1] = Q[2] = 0; Q[3] = 1; }
}

std::vector<FlTexture> ReadTextures(const UtfNode& Root)
{
	std::vector<FlTexture> R;
	const UtfNode* Lib = FindChild(Root, "Texture library");
	if (!Lib) return R;
	auto Lower = [](std::string S) { std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return (char)std::tolower(C); }); return S; };
	for (auto& T : Lib->Children)
	{
		const UtfNode* Best = nullptr;
		for (auto& M : T->Children)
		{
			std::string N = Lower(M->Name);
			if (N == "mips" || N == "mip0") { Best = M.get(); break; }
			if (N.rfind("mip", 0) == 0 && (!Best || M->Data.size() > Best->Data.size())) Best = M.get();
		}
		if (!Best || Best->Data.size() < 4) continue;
		FlTexture Tex;
		Tex.Name = T->Name;
		for (auto& Ch : Tex.Name) if (Ch == '/' || Ch == '\\') Ch = '_';
		std::string Low = Lower(Tex.Name);
		for (const char* E : {".tga", ".dds"})
			if (Low.size() > 4 && Low.compare(Low.size() - 4, 4, E) == 0) { Tex.Name.resize(Tex.Name.size() - 4); break; }
		Tex.Data = Best->Data;
		R.push_back(std::move(Tex));
	}
	return R;
}

void FlToUeDirection(const float In[3], float Out[3])
{
	Out[0] = -In[2]; Out[1] = In[0]; Out[2] = In[1];
}

void FlToUePosition(const float In[3], float Out[3])
{
	FlToUeDirection(In, Out);
	for (int i = 0; i < 3; ++i) Out[i] *= 100.0f;
}

void FlToUeRotation(const float R[9], float Out[9])
{
	// M: UE = M * FL mit M = [[0,0,-1],[1,0,0],[0,1,0]]
	static const float M[9] = {0, 0, -1, 1, 0, 0, 0, 1, 0};
	float T[9];
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
		{
			float S = 0;
			for (int k = 0; k < 3; ++k) S += M[r * 3 + k] * R[k * 3 + c];
			T[r * 3 + c] = S;
		}
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
		{
			float S = 0;
			for (int k = 0; k < 3; ++k) S += T[r * 3 + k] * M[c * 3 + k]; // M^T
			Out[r * 3 + c] = S;
		}
}

namespace
{
std::map<uint32_t, VmsMesh> ReadMeshLibrary(const UtfNode& Root, bool bLod0Only, std::vector<std::string>* Names = nullptr)
{
	std::map<uint32_t, VmsMesh> R;
	const UtfNode* Lib = FindChild(Root, "VMeshLibrary");
	if (!Lib) return R;
	for (auto& E : Lib->Children)
	{
		std::string L = E->Name;
		std::transform(L.begin(), L.end(), L.begin(), [](unsigned char C) { return (char)std::tolower(C); });
		if (bLod0Only && L.find("lod") != std::string::npos && L.find("lod0") == std::string::npos) continue;
		const UtfNode* D = FindChild(*E, "VMeshData");
		VmsMesh M; std::string Err;
		if (D && ParseVms(D->Data, M, Err))
		{
			R[FlModelCrc(E->Name)] = std::move(M);
			if (Names) Names->push_back(E->Name);
		}
	}
	return R;
}
}

bool BuildModel(const UtfNode& Root, FlModel& Out, std::string& Error)
{
	Out.Parts.clear();
	auto Meshes = ReadMeshLibrary(Root, false);
	auto Parts = ReadParts(Root);
	if (Parts.empty())
	{
		// Einzelnes .3db: alle LOD0-Meshes der Bibliothek zu einem Teil vereinen (ein Mesh ist die Regel)
		std::vector<std::string> Names;
		auto Lod0 = ReadMeshLibrary(Root, true, &Names);
		if (Lod0.empty()) { Error = "keine Meshes"; return false; }
		FlModelPart P;
		P.Name = "Root";
		P.bHasMesh = true;
		P.Mesh = Lod0.begin()->second;
		P.Hardpoints = ReadHardpoints(Root);
		Out.Parts.push_back(std::move(P));
		return true;
	}
	std::map<std::string, int> ByObject;
	auto Lower = [](std::string S) { std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return (char)std::tolower(C); }); return S; };
	for (auto& P : Parts)
	{
		FlModelPart MP;
		MP.Name = P.Object;
		if (const UtfNode* Model = FindChild(Root, P.File))
		{
			FlMeshRef Ref;
			if (ReadLevel0Ref(*Model, Ref))
			{
				auto It = Meshes.find(Ref.MeshCrc);
				if (It != Meshes.end())
				{
					MP.bHasMesh = true;
					MP.Mesh = ExtractGroups(It->second, Ref.StartMesh, Ref.MeshCount, Ref.StartVertex);
				}
			}
			MP.Hardpoints = ReadHardpoints(*Model);
		}
		ByObject[Lower(P.Object)] = (int)Out.Parts.size();
		Out.Parts.push_back(std::move(MP));
	}
	for (auto& C : ReadConstructs(Root))
	{
		auto Ch = ByObject.find(Lower(C.Child)), Pa = ByObject.find(Lower(C.Parent));
		if (Ch == ByObject.end()) continue;
		FlModelPart& N = Out.Parts[Ch->second];
		if (Pa != ByObject.end() && Pa->second != Ch->second) N.Parent = Pa->second;
		std::memcpy(N.Position, C.Origin, 12);
		std::memcpy(N.Rotation, C.Rotation, 36);
		N.Joint = C.Kind;
		std::memcpy(N.Axis, C.Axis, 12);
		N.Min = C.Min; N.Max = C.Max;
	}
	return true;
}
}

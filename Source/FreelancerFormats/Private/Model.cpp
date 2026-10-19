#include "Model.h"
#include "Ini.h"
#include <algorithm>
#include <cctype>
#include <cmath>
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
}

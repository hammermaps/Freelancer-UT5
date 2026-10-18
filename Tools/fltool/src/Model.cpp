#include "Model.h"
#include "Ini.h"
#include <algorithm>
#include <cctype>
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
}

#include "FlSphere.h"
#include <cstring>

namespace fl
{
bool ReadSphere(const UtfNode& Root, FlSphere& Out)
{
	const UtfNode* S = FindChild(Root, "Sphere");
	if (!S) return false;
	Out = FlSphere();
	if (auto R = FindChild(*S, "Radius"); R && R->Data.size() >= 4) std::memcpy(&Out.Radius, R->Data.data(), 4);
	if (auto N = FindChild(*S, "Sides"); N && N->Data.size() >= 4) { int32_t V; std::memcpy(&V, N->Data.data(), 4); Out.Sides = V; }
	for (int i = 0; i < 64; ++i)
	{
		const UtfNode* M = FindChild(*S, "M" + std::to_string(i));
		if (!M) break;
		size_t L = 0;
		while (L < M->Data.size() && M->Data[L]) ++L;
		Out.SideMaterials.emplace_back((const char*)M->Data.data(), L);
	}
	return true;
}
}

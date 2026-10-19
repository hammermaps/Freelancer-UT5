#include "ShipData.h"
#include <algorithm>
#include <cstdlib>

namespace fl
{
namespace
{
std::string Slashes(std::string S)
{
	std::replace(S.begin(), S.end(), '\\', '/');
	return S;
}

float F(const IniEntry* E, size_t I = 0)
{
	return (E && I < E->Values.size()) ? (float)std::atof(E->Values[I].c_str()) : 0.0f;
}

int N(const IniEntry* E)
{
	return (E && !E->Values.empty()) ? std::atoi(E->Values[0].c_str()) : 0;
}

std::string S(const IniEntry* E)
{
	return (E && !E->Values.empty()) ? E->Values[0] : std::string();
}

void Vec3(const IniEntry* E, float Out[3])
{
	for (size_t i = 0; i < 3; ++i) Out[i] = F(E, i);
}
}

std::vector<FlShipRecord> ParseShipArch(const IniFile& Ini)
{
	std::vector<FlShipRecord> R;
	for (const IniSection* Sec : Ini.FindAll("Ship"))
	{
		FlShipRecord Ship;
		Ship.Nickname = S(Sec->Find("nickname"));
		if (Ship.Nickname.empty()) continue;
		Ship.Type = S(Sec->Find("type"));
		Ship.IdsName = N(Sec->Find("ids_name"));
		Ship.IdsInfo = N(Sec->Find("ids_info"));
		Ship.ShipClass = N(Sec->Find("ship_class"));
		Ship.Mass = F(Sec->Find("mass"));
		Ship.LinearDrag = F(Sec->Find("linear_drag"));
		Ship.HitPts = N(Sec->Find("hit_pts"));
		Ship.HoldSize = N(Sec->Find("hold_size"));
		Ship.NudgeForce = F(Sec->Find("nudge_force"));
		Ship.StrafeForce = F(Sec->Find("strafe_force"));
		Vec3(Sec->Find("steering_torque"), Ship.SteeringTorque);
		Vec3(Sec->Find("angular_drag"), Ship.AngularDrag);
		Vec3(Sec->Find("rotation_inertia"), Ship.RotationInertia);
		Ship.Archetype = Slashes(S(Sec->Find("DA_archetype")));
		for (const IniEntry* M : Sec->FindAll("material_library")) Ship.MaterialLibraries.push_back(Slashes(S(M)));
		Ship.Cockpit = Slashes(S(Sec->Find("cockpit")));
		R.push_back(std::move(Ship));
	}
	return R;
}
}

#include "FlAsteroidField.h"
#include <algorithm>
#include <cstdlib>

namespace fl
{
FlAsteroidField ParseAsteroidField(const IniFile& Ini)
{
	FlAsteroidField R;
	for (const IniSection& Sec : Ini.Sections)
	{
		if (IEquals(Sec.Name, "Cube"))
		{
			for (const IniEntry* E : Sec.FindAll("asteroid"))
			{
				if (E->Values.size() < 7 || E->Values[0].empty()) continue;
				FlFieldCube C;
				C.Asteroid = E->Values[0];
				for (int i = 0; i < 3; ++i)
				{
					C.Offset[i] = (float)std::atof(E->Values[1 + i].c_str());
					C.RotationDeg[i] = (float)std::atof(E->Values[4 + i].c_str());
				}
				R.Cubes.push_back(std::move(C));
			}
			continue;
		}
		FlFieldSection S;
		S.Name = Sec.Name;
		for (const IniEntry& E : Sec.Entries)
		{
			std::string K = E.Key;
			std::transform(K.begin(), K.end(), K.begin(), [](unsigned char C) { return (char)std::tolower(C); });
			std::string V;
			for (size_t i = 0; i < E.Values.size(); ++i) { if (i) V += ", "; V += E.Values[i]; }
			if (IEquals(Sec.Name, "Exclusion Zones") && K == "exclusion" && !V.empty()) R.Exclusions.push_back(V);
			S.Props.emplace_back(std::move(K), std::move(V));
		}
		R.Sections.push_back(std::move(S));
	}
	return R;
}
}

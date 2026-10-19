#include "FlMarket.h"
#include <cstdlib>

namespace fl
{
std::vector<FlBaseMarket> ParseMarkets(const IniFile& Ini)
{
	std::vector<FlBaseMarket> R;
	for (const IniSection* Sec : Ini.FindAll("BaseGood"))
	{
		const IniEntry* Base = Sec->Find("base");
		if (!Base || Base->Values.empty() || Base->Values[0].empty()) continue;
		FlBaseMarket M;
		M.Base = Base->Values[0];
		for (const IniEntry* E : Sec->FindAll("marketgood"))
		{
			if (E->Values.size() < 7) continue;
			FlMarketGood G;
			G.Good = E->Values[0];
			G.Rank = std::atoi(E->Values[1].c_str());
			G.Rep = (float)std::atof(E->Values[2].c_str());
			G.Min = std::atoi(E->Values[3].c_str());
			G.Max = std::atoi(E->Values[4].c_str());
			G.bPreserve = std::atoi(E->Values[5].c_str()) > 0;
			G.Multiplier = (float)std::atof(E->Values[6].c_str());
			M.Goods.push_back(std::move(G));
		}
		R.push_back(std::move(M));
	}
	return R;
}
}

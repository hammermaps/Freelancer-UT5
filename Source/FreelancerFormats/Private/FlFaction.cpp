#include "FlFaction.h"
#include <algorithm>
#include <cstdlib>

namespace fl
{
namespace
{
std::string Lower(std::string S)
{
	std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return (char)std::tolower(C); });
	return S;
}

std::string Join(const IniEntry& E)
{
	std::string R;
	for (size_t i = 0; i < E.Values.size(); ++i) { if (i) R += ", "; R += E.Values[i]; }
	return R;
}

int ToInt(const IniEntry* E) { return (E && !E->Values.empty()) ? std::atoi(E->Values[0].c_str()) : 0; }

// "Wert, Name" -> (Name, Wert)
bool NamedValue(const IniEntry& E, size_t NameIdx, size_t ValueIdx, std::pair<std::string, float>& Out)
{
	if (E.Values.size() <= std::max(NameIdx, ValueIdx) || E.Values[NameIdx].empty()) return false;
	Out = {E.Values[NameIdx], (float)std::atof(E.Values[ValueIdx].c_str())};
	return true;
}
}

std::vector<FlFaction> ParseFactions(const IniFile& World, const IniFile& Empathy, const IniFile& Props)
{
	std::vector<FlFaction> R;
	for (const IniSection* G : World.FindAll("Group"))
	{
		const IniEntry* Nick = G->Find("nickname");
		if (!Nick || Nick->Values.empty() || Nick->Values[0].empty()) continue;
		FlFaction F;
		F.Nickname = Nick->Values[0];
		F.IdsName = ToInt(G->Find("ids_name"));
		F.IdsInfo = ToInt(G->Find("ids_info"));
		F.IdsShortName = ToInt(G->Find("ids_short_name"));
		for (const IniEntry& E : G->Entries)
		{
			const std::string K = Lower(E.Key);
			if (K == "rep") { std::pair<std::string, float> P; if (NamedValue(E, 1, 0, P)) F.Relations.push_back(P); }
			else if (K != "nickname" && K != "ids_name" && K != "ids_info" && K != "ids_short_name") F.Props.emplace_back(K, Join(E));
		}
		R.push_back(std::move(F));
	}
	for (const IniSection* Eff : Empathy.FindAll("RepChangeEffects"))
	{
		const IniEntry* Grp = Eff->Find("group");
		if (!Grp || Grp->Values.empty()) continue;
		for (FlFaction& F : R)
		{
			if (!IEquals(F.Nickname, Grp->Values[0])) continue;
			for (const IniEntry& E : Eff->Entries)
			{
				const std::string K = Lower(E.Key);
				std::pair<std::string, float> P;
				if (K == "event" && NamedValue(E, 0, 1, P)) F.RepEvents.push_back(P);
				else if (K == "empathy_rate" && NamedValue(E, 0, 1, P)) F.Empathy.push_back(P);
			}
		}
	}
	for (const IniSection* FP : Props.FindAll("FactionProps"))
	{
		const IniEntry* Aff = FP->Find("affiliation");
		if (!Aff || Aff->Values.empty()) continue;
		for (FlFaction& F : R)
		{
			if (!IEquals(F.Nickname, Aff->Values[0])) continue;
			for (const IniEntry& E : FP->Entries)
				if (Lower(E.Key) != "affiliation") F.Props.emplace_back("fp_" + Lower(E.Key), Join(E));
		}
	}
	return R;
}
}

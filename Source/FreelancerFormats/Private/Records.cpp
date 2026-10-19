#include "Records.h"
#include <algorithm>

namespace fl
{
namespace
{
std::string Lower(std::string S)
{
	std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return (char)std::tolower(C); });
	return S;
}

std::string Join(const std::vector<std::string>& V)
{
	std::string R;
	for (size_t i = 0; i < V.size(); ++i) { if (i) R += ", "; R += V[i]; }
	return R;
}

FlRecord Build(const IniSection& Sec)
{
	FlRecord R;
	R.Section = Sec.Name;
	for (const IniEntry& E : Sec.Entries)
	{
		std::string Key = Lower(E.Key);
		std::string Val = Join(E.Values);
		if (Key == "nickname" && R.Nickname.empty()) R.Nickname = Val;
		R.Props.emplace_back(std::move(Key), std::move(Val));
	}
	return R;
}
}

const std::string* FlRecord::Find(const std::string& Key) const
{
	for (auto& P : Props) if (P.first == Key) return &P.second;
	return nullptr;
}

std::vector<std::string> FlRecord::FindAll(const std::string& Key) const
{
	std::vector<std::string> R;
	for (auto& P : Props) if (P.first == Key) R.push_back(P.second);
	return R;
}

const std::set<std::string>& NonEquipmentSections()
{
	static const std::set<std::string> S = {"lod", "light", "attachedfx", "internalfx", "good", "basegood", "factiongood", "commodity"};
	return S;
}

std::vector<FlRecord> ParseRecords(const IniFile& Ini, const std::set<std::string>& Skip)
{
	std::vector<FlRecord> R;
	for (const IniSection& Sec : Ini.Sections)
	{
		if (Skip.count(Lower(Sec.Name))) continue;
		FlRecord Rec = Build(Sec);
		if (!Rec.Nickname.empty()) R.push_back(std::move(Rec));
	}
	return R;
}

std::vector<FlRecord> ParseRecordsOfType(const IniFile& Ini, const std::string& Section)
{
	std::vector<FlRecord> R;
	for (const IniSection& Sec : Ini.Sections)
	{
		if (!IEquals(Sec.Name, Section)) continue;
		FlRecord Rec = Build(Sec);
		if (!Rec.Nickname.empty()) R.push_back(std::move(Rec));
	}
	return R;
}

std::string NormalizePath(std::string Path)
{
	std::replace(Path.begin(), Path.end(), '\\', '/');
	return Path;
}
}

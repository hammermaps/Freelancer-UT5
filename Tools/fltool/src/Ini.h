// Parser fuer Freelancer-INI-Dateien (Textform; BINI wird erkannt und abgelehnt).
// Sektionen koennen mehrfach vorkommen, Keys wiederholt; Namen sind nicht case-sensitiv.
#pragma once
#include <string>
#include <vector>

namespace fl
{
struct IniEntry
{
	std::string Key;                 // Originalschreibweise
	std::vector<std::string> Values; // an Kommas getrennt, getrimmt
	int Line = 0;
};

struct IniSection
{
	std::string Name;
	std::vector<IniEntry> Entries;
	const IniEntry* Find(const std::string& Key) const;                // erster Treffer
	std::vector<const IniEntry*> FindAll(const std::string& Key) const;
};

struct IniFile
{
	std::vector<IniSection> Sections;
	std::string Error;
	bool bOk = false;
	static IniFile Load(const std::string& Path);
	static IniFile Parse(const std::string& Text);
	std::vector<const IniSection*> FindAll(const std::string& Name) const;
};

bool IEquals(const std::string& A, const std::string& B);
}

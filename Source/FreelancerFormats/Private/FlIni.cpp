#include "FlIni.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace fl
{
static std::string Trim(const std::string& S)
{
	size_t B = 0, E = S.size();
	while (B < E && std::isspace((unsigned char)S[B])) ++B;
	while (E > B && std::isspace((unsigned char)S[E - 1])) --E;
	return S.substr(B, E - B);
}

bool IEquals(const std::string& A, const std::string& B)
{
	return A.size() == B.size() && std::equal(A.begin(), A.end(), B.begin(),
		[](char X, char Y) { return std::tolower((unsigned char)X) == std::tolower((unsigned char)Y); });
}

const IniEntry* IniSection::Find(const std::string& Key) const
{
	for (auto& E : Entries) if (IEquals(E.Key, Key)) return &E;
	return nullptr;
}

std::vector<const IniEntry*> IniSection::FindAll(const std::string& Key) const
{
	std::vector<const IniEntry*> R;
	for (auto& E : Entries) if (IEquals(E.Key, Key)) R.push_back(&E);
	return R;
}

std::vector<const IniSection*> IniFile::FindAll(const std::string& Name) const
{
	std::vector<const IniSection*> R;
	for (auto& S : Sections) if (IEquals(S.Name, Name)) R.push_back(&S);
	return R;
}

IniFile IniFile::Parse(const std::string& Text)
{
	IniFile F;
	if (Text.compare(0, 4, "BINI") == 0) { F.Error = "BINI (binaere INI) wird nicht unterstuetzt"; return F; }
	size_t Pos = (Text.compare(0, 3, "\xEF\xBB\xBF") == 0) ? 3 : 0;
	int LineNo = 0;
	while (Pos < Text.size())
	{
		size_t End = Text.find('\n', Pos);
		if (End == std::string::npos) End = Text.size();
		std::string Line = Text.substr(Pos, End - Pos);
		Pos = End + 1;
		++LineNo;
		size_t C = Line.find(';');
		if (C != std::string::npos) Line.resize(C);
		Line = Trim(Line);
		if (Line.empty()) continue;
		if (Line[0] == '[')
		{
			size_t Close = Line.find(']');
			if (Close == std::string::npos) { F.Error = "Zeile " + std::to_string(LineNo) + ": ']' fehlt"; return F; }
			IniSection S;
			S.Name = Trim(Line.substr(1, Close - 1));
			F.Sections.push_back(std::move(S));
			continue;
		}
		if (F.Sections.empty()) continue; // Zeilen vor der ersten Sektion ignorieren
		IniEntry E;
		E.Line = LineNo;
		size_t Eq = Line.find('=');
		std::string Rest;
		if (Eq == std::string::npos) E.Key = Line;
		else { E.Key = Trim(Line.substr(0, Eq)); Rest = Line.substr(Eq + 1); }
		size_t P = 0;
		while (!Rest.empty() && P <= Rest.size())
		{
			size_t Comma = Rest.find(',', P);
			if (Comma == std::string::npos) Comma = Rest.size();
			E.Values.push_back(Trim(Rest.substr(P, Comma - P)));
			P = Comma + 1;
		}
		F.Sections.back().Entries.push_back(std::move(E));
	}
	F.bOk = true;
	return F;
}

IniFile IniFile::Load(const std::string& Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In) { IniFile F; F.Error = "kann Datei nicht oeffnen: " + Path; return F; }
	std::string T((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	return Parse(T);
}
}

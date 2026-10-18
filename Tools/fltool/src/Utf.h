// Reader fuer Freelancer-UTF-Container (.cmp .3db .mat .txm .utf ...).
// Format-Referenz: Librelancer (MIT), LibreLancer/Utf/UtfFile.cs
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fl
{
struct UtfNode
{
	std::string Name;
	bool bIntermediate = false;
	std::vector<std::unique_ptr<UtfNode>> Children;
	std::vector<uint8_t> Data; // nur Blaetter
};

// Liest eine UTF-Datei; bei Fehler Root == nullptr und Error gesetzt.
struct UtfFile
{
	std::unique_ptr<UtfNode> Root;
	std::string Error;
	static UtfFile Load(const std::string& Path);
	static UtfFile Parse(const std::vector<uint8_t>& Bytes);
};

const UtfNode* FindChild(const UtfNode& Parent, const std::string& Name); // ohne Gross/Klein
}

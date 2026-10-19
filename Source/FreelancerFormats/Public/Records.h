// Allgemeine Datensaetze aus Freelancer-INIs (Ausruestung, Waren): Sektionstyp, Nickname, alle Eintraege.
#pragma once
#include "FlApi.h"
#include "Ini.h"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fl
{
struct FLF_API FlRecord
{
	std::string Section;  // z.B. "Gun", "Engine", "Good"
	std::string Nickname;
	// Eintraege in Dateireihenfolge; Werte mit ", " verbunden. Schluessel kleingeschrieben, Wiederholungen bleiben erhalten.
	std::vector<std::pair<std::string, std::string>> Props;

	const std::string* Find(const std::string& Key) const; // erster Treffer, Schluessel klein
	std::vector<std::string> FindAll(const std::string& Key) const;
};

// Sektionen, die keine Spielobjekte beschreiben (Effekte, Detailstufen); werden bei ParseEquipment uebersprungen.
FLF_API const std::set<std::string>& NonEquipmentSections();

// Alle Sektionen mit nickname, ausser den uebergebenen Namen (Vergleich ohne Gross/Klein).
FLF_API std::vector<FlRecord> ParseRecords(const IniFile& Ini, const std::set<std::string>& SkipSections);

// Nur Sektionen eines Typs (z.B. "Good").
FLF_API std::vector<FlRecord> ParseRecordsOfType(const IniFile& Ini, const std::string& Section);

// Backslashes durch Schraegstriche ersetzen (Originalpfade).
FLF_API std::string NormalizePath(std::string Path);
}

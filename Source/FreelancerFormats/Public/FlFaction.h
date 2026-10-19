// Fraktionen (initialworld.ini [Group]), Ruf-Aenderungen (MISSIONS/empathy.ini) und Fraktionseigenschaften (faction_prop.ini).
#pragma once
#include "FlApi.h"
#include "FlIni.h"
#include <string>
#include <utility>
#include <vector>

namespace fl
{
struct FlFaction
{
	std::string Nickname;
	int IdsName = 0, IdsInfo = 0, IdsShortName = 0;
	// Grundhaltung zu anderen Gruppen: "rep = Wert, Gruppe" (-1 .. 1)
	std::vector<std::pair<std::string, float>> Relations;
	// Ruf-Ereignisse der Gruppe: "event = Name, Aenderung"
	std::vector<std::pair<std::string, float>> RepEvents;
	// Mitgefuehl: "empathy_rate = Gruppe, Rate" (Anteil der Aenderung, der auf andere Gruppen abfaerbt)
	std::vector<std::pair<std::string, float>> Empathy;
	// Alle uebrigen Eintraege der [Group]-Sektion und der [FactionProps]-Sektion (affiliation = Gruppe), Schluessel klein
	std::vector<std::pair<std::string, std::string>> Props;
};

// Gruppen aus initialworld.ini; Empathy und FactionProps werden per Gruppen-Nickname zugeordnet (beide duerfen leer sein).
FLF_API std::vector<FlFaction> ParseFactions(const IniFile& InitialWorld, const IniFile& Empathy, const IniFile& FactionProps);
}

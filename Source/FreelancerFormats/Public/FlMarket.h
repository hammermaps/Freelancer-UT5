// Marktdaten (EQUIPMENT/market_*.ini): welche Basis verkauft/kauft welche Ware zu welchem Preisfaktor.
// Feldbedeutung wie in Librelancer (LibreLancer.Data/Schema/Goods/MarketGood.cs).
#pragma once
#include "FlApi.h"
#include "FlIni.h"
#include <string>
#include <vector>

namespace fl
{
struct FlMarketGood
{
	std::string Good;      // Nickname der Ware (goods.ini)
	int Rank = 0;          // benoetigter Rang
	float Rep = 0;         // benoetigter Ruf (-1 = keiner)
	int Min = 0, Max = 0;  // Lagerwerte (im Original ungenutzt)
	bool bPreserve = false;// nur Ausruestung: Ware bleibt erhalten
	float Multiplier = 1;  // Preisfaktor auf den Basispreis
};

struct FlBaseMarket
{
	std::string Base;
	std::vector<FlMarketGood> Goods;
};

// Alle [BaseGood]-Sektionen mit "base" und ihren "MarketGood"-Zeilen (mindestens 7 Werte, sonst uebersprungen).
FLF_API std::vector<FlBaseMarket> ParseMarkets(const IniFile& Ini);
}

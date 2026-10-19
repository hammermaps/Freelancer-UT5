// Bild-Decoder fuer Freelancer-Texturen (TGA, DDS) -> RGBA8. Keine UE-Abhaengigkeit.
#pragma once
#include "FlApi.h"
#include <cstdint>
#include <string>
#include <vector>

namespace fl
{
struct FlImage
{
	int Width = 0, Height = 0;
	std::vector<uint8_t> Rgba; // Width*Height*4, Zeile 0 = oben
};

// TGA: Typ 2/3/10/11, 8/16/24/32 bpp, ohne Farbtabelle. 16 bpp = A1R5G5B5.
FLF_API bool DecodeTga(const std::vector<uint8_t>& Data, FlImage& Out, std::string& Error);

// DDS: DXT1/DXT3/DXT5 und unkomprimiert (24/32 bpp mit Masken); nur die erste MIP-Stufe.
FLF_API bool DecodeDds(const std::vector<uint8_t>& Data, FlImage& Out, std::string& Error);

// Erkennt DDS ("DDS " am Anfang) sonst TGA.
FLF_API bool DecodeImage(const std::vector<uint8_t>& Data, FlImage& Out, std::string& Error);
}

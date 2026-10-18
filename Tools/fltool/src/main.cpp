// fltool - Hilfswerkzeug fuer Freelancer-Dateiformate (Offline, nicht Teil des Spiels).
//   fltool utf ls <datei>                 Baum ausgeben
//   fltool utf extract <datei> <zielordner>  alle Blatt-Daten als Dateien schreiben
//   fltool ini sections <datei>              Sektionen mit Anzahl der Eintraege
//   fltool tex extract <datei> <zielordner>  je Textur die groesste MIP-Stufe als .dds/.tga schreiben
#include "Ini.h"
#include "Utf.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cctype>

namespace fs = std::filesystem;
using namespace fl;

static void Print(const UtfNode& N, int D)
{
	std::printf("%*s%s%s", D * 2, "", N.Name.c_str(), N.bIntermediate ? "/" : "");
	if (!N.bIntermediate) std::printf("  (%zu bytes)", N.Data.size());
	std::printf("\n");
	for (auto& C : N.Children) Print(*C, D + 1);
}

static void Extract(const UtfNode& N, const fs::path& Dir)
{
	std::string Safe = N.Name;
	for (auto& Ch : Safe) if (Ch == '/' || Ch == '\\') Ch = '_';
	fs::path P = Dir / (Safe.empty() ? "_" : Safe);
	if (N.bIntermediate)
	{
		fs::create_directories(P);
		for (auto& C : N.Children) Extract(*C, P);
	}
	else
	{
		std::ofstream Out(P, std::ios::binary);
		Out.write((const char*)N.Data.data(), N.Data.size());
	}
}


static std::string Lower(std::string S) { std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return std::tolower(C); }); return S; }

// Textur-Bibliothek: "Texture library"/<name>/{MIP0..n | MIPS}. Eintraege sind DDS oder TGA.
static int ExtractTextures(const UtfNode& Root, const fs::path& Dir)
{
	const UtfNode* Lib = FindChild(Root, "Texture library");
	if (!Lib) return 0;
	fs::create_directories(Dir);
	int Count = 0;
	for (auto& T : Lib->Children)
	{
		const UtfNode* Best = nullptr;
		for (auto& M : T->Children)
		{
			std::string N = Lower(M->Name);
			if (N == "mips" || N == "mip0") { Best = M.get(); break; }
			if (N.rfind("mip", 0) == 0 && (!Best || M->Data.size() > Best->Data.size())) Best = M.get();
		}
		if (!Best || Best->Data.size() < 4) continue;
		bool bDds = std::memcmp(Best->Data.data(), "DDS ", 4) == 0;
		std::string Base = T->Name;
		for (auto& Ch : Base) if (Ch == '/' || Ch == '\\') Ch = '_';
		std::string Low = Lower(Base);
		for (const char* E : {".tga", ".dds"})
			if (Low.size() > 4 && Low.compare(Low.size() - 4, 4, E) == 0) { Base.resize(Base.size() - 4); break; }
		std::ofstream Out(Dir / (Base + (bDds ? ".dds" : ".tga")), std::ios::binary);
		Out.write((const char*)Best->Data.data(), Best->Data.size());
		++Count;
	}
	return Count;
}

int main(int argc, char** argv)
{
	if (argc >= 4 && !std::strcmp(argv[1], "utf"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		if (!std::strcmp(argv[2], "ls")) { Print(*F.Root, 0); return 0; }
		if (!std::strcmp(argv[2], "extract") && argc >= 5) { Extract(*F.Root, argv[4]); return 0; }
	}
	if (argc >= 4 && !std::strcmp(argv[1], "ini") && !std::strcmp(argv[2], "sections"))
	{
		IniFile F = IniFile::Load(argv[3]);
		if (!F.bOk) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		for (auto& S : F.Sections) std::printf("[%s] %zu\n", S.Name.c_str(), S.Entries.size());
		return 0;
	}
	if (argc >= 5 && !std::strcmp(argv[1], "tex") && !std::strcmp(argv[2], "extract"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		std::printf("%d Texturen\n", ExtractTextures(*F.Root, argv[4]));
		return 0;
	}
	std::fprintf(stderr, "Aufruf:\n  fltool utf ls <datei>\n  fltool utf extract <datei> <zielordner>\n  fltool ini sections <datei>\n  fltool tex extract <datei> <zielordner>\n");
	return 2;
}

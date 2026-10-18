// fltool - Hilfswerkzeug fuer Freelancer-Dateiformate (Offline, nicht Teil des Spiels).
//   fltool utf ls <datei>                 Baum ausgeben
//   fltool utf extract <datei> <zielordner>  alle Blatt-Daten als Dateien schreiben
#include "Utf.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

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

int main(int argc, char** argv)
{
	if (argc >= 4 && !std::strcmp(argv[1], "utf"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		if (!std::strcmp(argv[2], "ls")) { Print(*F.Root, 0); return 0; }
		if (!std::strcmp(argv[2], "extract") && argc >= 5) { Extract(*F.Root, argv[4]); return 0; }
	}
	std::fprintf(stderr, "Aufruf:\n  fltool utf ls <datei>\n  fltool utf extract <datei> <zielordner>\n");
	return 2;
}

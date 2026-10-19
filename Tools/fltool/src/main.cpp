// fltool - Hilfswerkzeug fuer Freelancer-Dateiformate (Offline, nicht Teil des Spiels).
//   fltool utf ls <datei>                 Baum ausgeben
//   fltool utf extract <datei> <zielordner>  alle Blatt-Daten als Dateien schreiben
//   fltool ini sections <datei>              Sektionen mit Anzahl der Eintraege
//   fltool cmp export <datei.cmp> <out.glb> [--mat <datei.mat>]  Teile mit Hierarchie, Hardpoints als Knoten
//   fltool cmp export <datei.cmp> <out.glb> [--mat <datei.mat>]\n  fltool model info <datei>\n  fltool mesh export <datei> <out.glb> [--all-lods] [--mat <datei.mat>]  VMeshData nach glTF (LOD0, sonst alle)
//   fltool model info <datei>                Materialien (CRC, Typ, Textur) und Hardpoints
//   fltool tex decode <bild.dds|tga> <out.pam>  Bild decodieren (RGBA, PAM-Format)
//   fltool tex decode <bild> <out.pam>\n  fltool tex extract <datei> <zielordner>  je Textur die groesste MIP-Stufe als .dds/.tga schreiben
#include "FlImage.h"
#include "FlIni.h"
#include "FlModel.h"
#include "FlUtf.h"
#include "FlVms.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <map>
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

// Textur-Bibliothek: je Textur die groesste MIP-Stufe als .dds/.tga schreiben.
static int ExtractTextures(const UtfNode& Root, const fs::path& Dir)
{
	auto Texs = ReadTextures(Root);
	if (Texs.empty()) return 0;
	fs::create_directories(Dir);
	for (auto& T : Texs)
	{
		bool bDds = T.Data.size() >= 4 && std::memcmp(T.Data.data(), "DDS ", 4) == 0;
		std::ofstream Out(Dir / (T.Name + (bDds ? ".dds" : ".tga")), std::ios::binary);
		Out.write((const char*)T.Data.data(), T.Data.size());
	}
	return (int)Texs.size();
}

// Sammelt VMeshData-Knoten ("VMeshLibrary"/<name>/VMeshData).
static int ExportMeshes(const UtfNode& Root, const std::string& OutPath, bool bAllLods, const std::string& MatPath)
{
	const UtfNode* Lib = FindChild(Root, "VMeshLibrary");
	if (!Lib) { std::fprintf(stderr, "keine VMeshLibrary\n"); return 1; }
	std::vector<std::pair<std::string, VmsMesh>> Meshes;
	for (auto& E : Lib->Children)
	{
		std::string L = Lower(E->Name);
		if (!bAllLods && L.find("lod") != std::string::npos && L.find("lod0") == std::string::npos) continue;
		const UtfNode* D = FindChild(*E, "VMeshData");
		if (!D) continue;
		VmsMesh M; std::string Err;
		if (!ParseVms(D->Data, M, Err)) { std::fprintf(stderr, "%s: %s\n", E->Name.c_str(), Err.c_str()); return 1; }
		Meshes.emplace_back(E->Name, std::move(M));
	}
	if (Meshes.empty()) { std::fprintf(stderr, "keine Meshes\n"); return 1; }
	std::map<uint32_t, GlbMaterial> Mats;
	if (!MatPath.empty())
	{
		UtfFile MF = UtfFile::Load(MatPath);
		if (!MF.Root) { std::fprintf(stderr, "Material-Datei: %s\n", MF.Error.c_str()); return 1; }
		for (auto& [Crc, M] : ReadMaterials(*MF.Root))
		{
			GlbMaterial G; G.Name = M.Name; G.Texture = M.DiffuseTexture;
			std::memcpy(G.Dc, M.Dc, sizeof(G.Dc));
			Mats[Crc] = G;
		}
	}
	auto Glb = BuildGlb(Meshes, MatPath.empty() ? nullptr : &Mats);
	std::ofstream Out(OutPath, std::ios::binary);
	Out.write((const char*)Glb.data(), Glb.size());
	std::printf("%zu Meshes, %zu Bytes\n", Meshes.size(), Glb.size());
	return 0;
}

static int ModelInfo(const UtfNode& Root)
{
	for (auto& [Crc, M] : ReadMaterials(Root))
		std::printf("material %08x %s type=%s tex=%s\n", Crc, M.Name.c_str(), M.Type.c_str(), M.DiffuseTexture.c_str());
	for (auto& H : ReadHardpoints(Root))
		std::printf("hardpoint %s %s pos=%g,%g,%g\n", H.bRevolute ? "rev" : "fix", H.Name.c_str(), H.Position[0], H.Position[1], H.Position[2]);
	return 0;
}

// Exportiert ein .cmp/.3db als Szene: ein Knoten je Teil (Konstrukt-Transform), Hardpoints als leere Kindknoten.
static int ExportCmp(const UtfNode& Root, const std::string& OutPath, const std::string& MatPath)
{
	FlModel Model; std::string Err;
	if (!BuildModel(Root, Model, Err)) { std::fprintf(stderr, "%s\n", Err.c_str()); return 1; }
	std::vector<SceneNode> Scene;
	for (auto& P : Model.Parts)
	{
		SceneNode N; N.Name = P.Name; N.Parent = P.Parent;
		std::memcpy(N.T, P.Position, 12);
		RotationToQuat(P.Rotation, N.Q);
		N.bHasMesh = P.bHasMesh; N.Mesh = P.Mesh;
		Scene.push_back(std::move(N));
	}
	size_t PartCount = Scene.size();
	for (size_t i = 0; i < PartCount; ++i)
		for (auto& H : Model.Parts[i].Hardpoints)
		{
			SceneNode N; N.Name = "HP_" + H.Name; N.Parent = (int)i;
			std::memcpy(N.T, H.Position, 12);
			RotationToQuat(H.Orientation, N.Q);
			Scene.push_back(std::move(N));
		}
	std::map<uint32_t, GlbMaterial> Mats;
	if (!MatPath.empty())
	{
		UtfFile MF = UtfFile::Load(MatPath);
		if (!MF.Root) { std::fprintf(stderr, "Material-Datei: %s\n", MF.Error.c_str()); return 1; }
		for (auto& [Crc, M] : ReadMaterials(*MF.Root)) { GlbMaterial G; G.Name = M.Name; G.Texture = M.DiffuseTexture; std::memcpy(G.Dc, M.Dc, 12); Mats[Crc] = G; }
	}
	auto Glb = BuildGlbScene(Scene, MatPath.empty() ? nullptr : &Mats);
	std::ofstream Out(OutPath, std::ios::binary);
	Out.write((const char*)Glb.data(), Glb.size());
	std::printf("%zu Teile, %zu Knoten, %zu Bytes\n", PartCount, Scene.size(), Glb.size());
	return 0;
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
	if (argc >= 5 && !std::strcmp(argv[1], "cmp") && !std::strcmp(argv[2], "export"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		std::string Mat;
		for (int i = 5; i + 1 < argc; ++i) if (!std::strcmp(argv[i], "--mat")) Mat = argv[i + 1];
		return ExportCmp(*F.Root, argv[4], Mat);
	}
	if (argc >= 4 && !std::strcmp(argv[1], "model") && !std::strcmp(argv[2], "info"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		return ModelInfo(*F.Root);
	}
	if (argc >= 5 && !std::strcmp(argv[1], "mesh") && !std::strcmp(argv[2], "export"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		bool bAll = false; std::string Mat;
		for (int i = 5; i < argc; ++i)
		{
			if (!std::strcmp(argv[i], "--all-lods")) bAll = true;
			else if (!std::strcmp(argv[i], "--mat") && i + 1 < argc) Mat = argv[++i];
		}
		return ExportMeshes(*F.Root, argv[4], bAll, Mat);
	}
	if (argc >= 5 && !std::strcmp(argv[1], "tex") && !std::strcmp(argv[2], "decode"))
	{
		std::ifstream In(argv[3], std::ios::binary);
		std::vector<uint8_t> Bytes((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
		FlImage Img; std::string Err;
		if (!DecodeImage(Bytes, Img, Err)) { std::fprintf(stderr, "Fehler: %s\n", Err.c_str()); return 1; }
		std::ofstream Out(argv[4], std::ios::binary);
		Out << "P7\nWIDTH " << Img.Width << "\nHEIGHT " << Img.Height << "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
		Out.write((const char*)Img.Rgba.data(), Img.Rgba.size());
		return 0;
	}
	if (argc >= 5 && !std::strcmp(argv[1], "tex") && !std::strcmp(argv[2], "extract"))
	{
		UtfFile F = UtfFile::Load(argv[3]);
		if (!F.Root) { std::fprintf(stderr, "Fehler: %s\n", F.Error.c_str()); return 1; }
		std::printf("%d Texturen\n", ExtractTextures(*F.Root, argv[4]));
		return 0;
	}
	std::fprintf(stderr, "Aufruf:\n  fltool utf ls <datei>\n  fltool utf extract <datei> <zielordner>\n  fltool ini sections <datei>\n  fltool mesh export <datei> <out.glb> [--all-lods] [--mat <datei.mat>]\n  fltool tex extract <datei> <zielordner>\n");
	return 2;
}

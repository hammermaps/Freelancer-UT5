// Tests fuer flformats. Nur synthetische Daten, keine Originaldateien.
#include "Ini.h"
#include "Utf.h"
#include "Vms.h"
#include <cstdio>
#include <cstring>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); ++Failures; } } while (0)

using namespace fl;

static std::vector<uint8_t> MakeUtf(int32_t LeafPeer = 0, int32_t Version = 257)
{
	std::vector<uint8_t> B(140, 0);
	std::memcpy(B.data(), "UTF ", 4);
	int32_t H[9] = {Version, 40, 88, 0, 44, 128, 7, 7, 135};
	std::memcpy(B.data() + 4, H, sizeof(H));
	auto Node = [&](size_t At, int32_t Peer, int32_t Name, int32_t Flags, int32_t Child, int32_t Size) {
		int32_t N[7] = {Peer, Name, Flags, 0, Child, Size, Size};
		std::memcpy(B.data() + 40 + At, N, sizeof(N));
	};
	Node(0, 0, 0, 0x10, 44, 0);          // Wurzel "/"
	Node(44, LeafPeer, 2, 0x80, 0, 5);   // Blatt "Leaf" mit 5 Bytes
	std::memcpy(B.data() + 128, "/\0Leaf\0", 7);
	std::memcpy(B.data() + 135, "hello", 5);
	return B;
}

static void TestUtf()
{
	auto F = UtfFile::Parse(MakeUtf());
	CHECK(F.Root != nullptr);
	CHECK(F.Root->bIntermediate);
	CHECK(F.Root->Children.size() == 1);
	const UtfNode* Leaf = FindChild(*F.Root, "LEAF"); // case-insensitiv
	CHECK(Leaf != nullptr);
	CHECK(Leaf && Leaf->Data.size() == 5 && std::memcmp(Leaf->Data.data(), "hello", 5) == 0);
	CHECK(FindChild(*F.Root, "missing") == nullptr);

	auto Bad = MakeUtf();
	Bad[0] = 'X';
	CHECK(UtfFile::Parse(Bad).Root == nullptr);
	CHECK(UtfFile::Parse(MakeUtf(0, 999)).Root == nullptr);          // falsche Version
	auto Short = MakeUtf();
	Short.resize(100);
	CHECK(UtfFile::Parse(Short).Root == nullptr);                    // abgeschnitten
	CHECK(UtfFile::Parse({}).Root == nullptr);
	CHECK(UtfFile::Parse(MakeUtf(44)).Root == nullptr);              // Peer-Zyklus
	CHECK(UtfFile::Load("/nonexistent/file.utf").Root == nullptr);
}

static void TestIni()
{
	const char* Text = "\xEF\xBB\xBF; Kommentar\r\n[Ship]\r\nnickname = li_fighter ; Rest\r\nloadout=a, b ,c\r\n\r\n[Ship]\r\nnickname = second\r\n[Solar]\r\nvalue\r\n";
	auto F = IniFile::Parse(Text);
	CHECK(F.bOk);
	CHECK(F.Sections.size() == 3);
	auto Ships = F.FindAll("SHIP");
	CHECK(Ships.size() == 2);
	const IniEntry* Nick = Ships[0]->Find("NickName");
	CHECK(Nick && Nick->Values.size() == 1 && Nick->Values[0] == "li_fighter");
	const IniEntry* Load = Ships[0]->Find("loadout");
	CHECK(Load && Load->Values.size() == 3 && Load->Values[1] == "b" && Load->Values[2] == "c");
	CHECK(Ships[1]->Find("nickname")->Values[0] == "second");
	CHECK(F.Sections[2].Entries.size() == 1 && F.Sections[2].Entries[0].Values.empty()); // Key ohne Wert
	CHECK(Ships[0]->Find("nickname")->Line == 3);

	auto Rep = Ships[0]->FindAll("nickname");
	CHECK(Rep.size() == 1);
	CHECK(!IniFile::Parse("[Broken\nx=1").bOk);
	CHECK(!IniFile::Parse("BINI\x01\x00\x00\x00").bOk);
	CHECK(IniFile::Parse("").bOk && IniFile::Parse("").Sections.empty());
	CHECK(IniFile::Parse("x=1\n[A]\ny=2").Sections.size() == 1);      // Zeilen vor erster Sektion
	CHECK(!IniFile::Load("/nonexistent/x.ini").bOk);
}


// Ein Dreieck, FVF = XYZ|NORMAL|TEX1, ein Mesh-Header.
static std::vector<uint8_t> MakeVms(uint16_t IndexValue2 = 2)
{
	std::vector<uint8_t> B;
	auto P16 = [&](uint16_t V) { B.insert(B.end(), (uint8_t*)&V, (uint8_t*)&V + 2); };
	auto P32 = [&](uint32_t V) { B.insert(B.end(), (uint8_t*)&V, (uint8_t*)&V + 4); };
	auto PF = [&](float V) { B.insert(B.end(), (uint8_t*)&V, (uint8_t*)&V + 4); };
	P32(1); P32(4);
	P16(1); P16(3); P16(0x112); P16(3);          // Meshes, Indices, FVF, Vertices
	P32(0xDEADBEEF); P16(0); P16(2); P16(3); P16(0xCC); // Header
	P16(0); P16(1); P16(IndexValue2);
	for (int v = 0; v < 3; ++v) { PF((float)v); PF(0); PF(0); PF(0); PF(0); PF(1); PF(0.5f * v); PF(1); }
	return B;
}

static void TestVms()
{
	VmsMesh M; std::string Err;
	CHECK(ParseVms(MakeVms(), M, Err));
	CHECK(M.Stride == 32 && M.bNormal && !M.bDiffuse && M.TexCoords == 1);
	CHECK(M.Positions.size() == 9 && M.Positions[3] == 1.0f);
	CHECK(M.Normals.size() == 9 && M.Normals[5] == 1.0f);
	CHECK(M.Uv0.size() == 6 && M.Uv0[2] == 0.5f);
	CHECK(M.Groups.size() == 1 && M.Groups[0].MaterialCrc == 0xDEADBEEF && M.Groups[0].NumRefIndices == 3);

	VmsMesh Bad;
	CHECK(!ParseVms(MakeVms(7), Bad, Err));                 // Index ausserhalb
	auto Cut = MakeVms(); Cut.resize(Cut.size() - 4);
	CHECK(!ParseVms(Cut, Bad, Err));                         // abgeschnitten
	CHECK(!ParseVms({}, Bad, Err));
	auto NoXyz = MakeVms(); NoXyz[12] = 0x10;
	CHECK(!ParseVms(NoXyz, Bad, Err));

	auto Glb = BuildGlb({{"tri", M}});
	CHECK(Glb.size() > 20 && std::memcmp(Glb.data(), "glTF", 4) == 0);
	uint32_t Total; std::memcpy(&Total, Glb.data() + 8, 4);
	CHECK(Total == Glb.size() && Glb.size() % 4 == 0);
	std::string Text((const char*)Glb.data(), Glb.size());
	CHECK(Text.find("\"materialCrc\":3735928559") != std::string::npos);
	CHECK(Text.find("TEXCOORD_0") != std::string::npos);
}

int main()
{
	TestUtf();
	TestIni();
	TestVms();
	std::printf(Failures ? "%d Fehler\n" : "alle Tests bestanden\n", Failures);
	return Failures ? 1 : 0;
}

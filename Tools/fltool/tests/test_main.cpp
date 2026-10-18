// Tests fuer flformats. Nur synthetische Daten, keine Originaldateien.
#include "Ini.h"
#include "Utf.h"
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

int main()
{
	TestUtf();
	TestIni();
	std::printf(Failures ? "%d Fehler\n" : "alle Tests bestanden\n", Failures);
	return Failures ? 1 : 0;
}

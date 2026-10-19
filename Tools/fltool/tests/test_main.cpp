// Tests fuer flformats. Nur synthetische Daten, keine Originaldateien.
#include "FlIni.h"
#include "FlImage.h"
#include "FlMath.h"
#include "FlModel.h"
#include "FlRecords.h"
#include "FlShipData.h"
#include "FlUtf.h"
#include "FlVms.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>

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

static std::unique_ptr<UtfNode> Leaf(const std::string& Name, const void* Data, size_t Size)
{
	auto N = std::make_unique<UtfNode>();
	N->Name = Name;
	N->Data.assign((const uint8_t*)Data, (const uint8_t*)Data + Size);
	return N;
}
static std::unique_ptr<UtfNode> Dir(const std::string& Name)
{
	auto N = std::make_unique<UtfNode>();
	N->Name = Name;
	N->bIntermediate = true;
	return N;
}

static void TestModel()
{
	// Werte stammen aus dem Original (or_ships.mat): Name -> CRC
	CHECK(FlModelCrc("elite_256OR") == 0xf2e64555);
	CHECK(FlModelCrc("ELITE_256or") == 0xf2e64555);   // nicht case-sensitiv
	CHECK(FlModelCrc("O_glass") == 0x1519a611);
	CHECK(FlModelCrc("") == 0);

	auto Root = Dir("/");
	auto Lib = Dir("Material library");
	auto Mat = Dir("elite_256OR");
	Mat->Children.push_back(Leaf("Type", "DcDt\0", 5));
	Mat->Children.push_back(Leaf("Dt_name", "elite_256.TGA\0", 14));
	float Dc[3] = {0.5f, 0.25f, 1.0f};
	Mat->Children.push_back(Leaf("Dc", Dc, 12));
	Lib->Children.push_back(std::move(Mat));
	Lib->Children.push_back(Dir("leer"));
	Root->Children.push_back(std::move(Lib));
	auto Mats = ReadMaterials(*Root);
	CHECK(Mats.size() == 2);
	CHECK(Mats.count(0xf2e64555) == 1);
	CHECK(Mats[0xf2e64555].DiffuseTexture == "elite_256.TGA" && Mats[0xf2e64555].Type == "DcDt" && Mats[0xf2e64555].Dc[1] == 0.25f);
	CHECK(ReadMaterials(*Dir("/")).empty());

	auto Hp = Dir("Hardpoints");
	auto Fixed = Dir("Fixed");
	auto One = Dir("HpWeapon01");
	float Pos[3] = {1, 2, 3};
	One->Children.push_back(Leaf("Position", Pos, 12));
	Fixed->Children.push_back(std::move(One));
	Fixed->Children.push_back(Leaf("stray", Pos, 12));    // Blatt wird ignoriert
	Hp->Children.push_back(std::move(Fixed));
	auto Rev = Dir("Revolute");
	Rev->Children.push_back(Dir("HpTurret"));              // ohne Position -> Nullvektor
	Hp->Children.push_back(std::move(Rev));
	Root->Children.push_back(std::move(Hp));
	auto Hps = ReadHardpoints(*Root);
	CHECK(Hps.size() == 2);
	CHECK(Hps[0].Name == "HpWeapon01" && !Hps[0].bRevolute && Hps[0].Position[2] == 3.0f && Hps[0].Orientation[0] == 1.0f);
	CHECK(Hps[1].bRevolute && Hps[1].Position[0] == 0.0f);

	// glTF mit Material
	VmsMesh M; std::string Err;
	CHECK(ParseVms(MakeVms(), M, Err));
	std::map<uint32_t, GlbMaterial> GM;
	GlbMaterial G; G.Name = "m1"; G.Texture = "elite_256.TGA";
	GM[0xDEADBEEF] = G;
	auto Glb = BuildGlb({{"tri", M}}, &GM);
	std::string Text((const char*)Glb.data(), Glb.size());
	CHECK(Text.find("\"materials\":[{\"name\":\"m1\"") != std::string::npos);
	CHECK(Text.find("elite_256.png") != std::string::npos);
	CHECK(Text.find("\"material\":0") != std::string::npos);
	auto NoMat = BuildGlb({{"tri", M}}, nullptr);
	CHECK(std::string((const char*)NoMat.data(), NoMat.size()).find("\"materials\"") == std::string::npos);
}

static void TestCmp()
{
	// Konstrukte: ein Fix- und ein Rev-Datensatz
	auto Root = Dir("/");
	auto Cmpnd = Dir("Cmpnd");
	auto Cons = Dir("Cons");
	std::vector<uint8_t> Fix(176, 0), Rev(208, 0);
	std::strcpy((char*)Fix.data(), "Root");
	std::strcpy((char*)Fix.data() + 64, "Wing");
	float FixData[12] = {1, 2, 3, 0, 1, 0, -1, 0, 0, 0, 0, 1}; // Origin + 3x3: 90 Grad um Z
	std::memcpy(Fix.data() + 128, FixData, 48);
	std::strcpy((char*)Rev.data(), "Root");
	std::strcpy((char*)Rev.data() + 64, "Door");
	float Mm[2] = {0.0f, 1.9f};
	std::memcpy(Rev.data() + 200, Mm, 8);
	Cons->Children.push_back(Leaf("Fix", Fix.data(), Fix.size()));
	Cons->Children.push_back(Leaf("Rev", Rev.data(), Rev.size()));
	Cons->Children.push_back(Leaf("Loose", Fix.data(), 10));  // wird uebersprungen
	auto Trunc = std::vector<uint8_t>(100, 0);
	Cons->Children.push_back(Leaf("Pris", Trunc.data(), Trunc.size())); // zu kurz -> nichts
	Cmpnd->Children.push_back(std::move(Cons));
	auto Part = Dir("Part_wing");
	Part->Children.push_back(Leaf("Object name", "Wing\0", 5));
	Part->Children.push_back(Leaf("File name", "wing.3db\0", 9));
	Cmpnd->Children.push_back(std::move(Part));
	Cmpnd->Children.push_back(Dir("Cons2"));                   // kein Teil
	Root->Children.push_back(std::move(Cmpnd));
	auto Cs = ReadConstructs(*Root);
	CHECK(Cs.size() == 2);
	CHECK(Cs[0].Kind == FlConstruct::Fix && Cs[0].Parent == "Root" && Cs[0].Child == "Wing" && Cs[0].Origin[1] == 2.0f);
	CHECK(Cs[1].Kind == FlConstruct::Rev && Cs[1].Child == "Door" && Cs[1].Max == 1.9f);
	auto Ps = ReadParts(*Root);
	CHECK(Ps.size() == 1 && Ps[0].Object == "Wing" && Ps[0].File == "wing.3db");

	// Rotation: 90 Grad um Z => Quaternion (0,0,sin45,cos45)
	float Q[4];
	float Rz[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
	RotationToQuat(Rz, Q);
	CHECK(std::fabs(Q[2] - 0.70710678f) < 1e-4f && std::fabs(Q[3] - 0.70710678f) < 1e-4f && std::fabs(Q[0]) < 1e-4f);
	float Id[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
	RotationToQuat(Id, Q);
	CHECK(Q[3] == 1.0f && Q[0] == 0.0f);
	float Rx180[9] = {1, 0, 0, 0, -1, 0, 0, 0, -1};
	RotationToQuat(Rx180, Q);
	CHECK(std::fabs(Q[0] - 1.0f) < 1e-4f);

	// VMeshRef
	auto Model = Dir("wing.3db");
	auto ML = Dir("MultiLevel"); auto L0 = Dir("Level0"); auto VP = Dir("VMeshPart");
	std::vector<uint8_t> Ref(60, 0);
	uint32_t Crc = 0x1234; uint16_t Start = 2, Cnt = 3;
	std::memcpy(Ref.data() + 4, &Crc, 4); std::memcpy(Ref.data() + 16, &Start, 2); std::memcpy(Ref.data() + 18, &Cnt, 2);
	VP->Children.push_back(Leaf("VMeshRef", Ref.data(), Ref.size()));
	L0->Children.push_back(std::move(VP)); ML->Children.push_back(std::move(L0)); Model->Children.push_back(std::move(ML));
	FlMeshRef R;
	CHECK(ReadLevel0Ref(*Model, R) && R.MeshCrc == 0x1234 && R.StartMesh == 2 && R.MeshCount == 3);
	CHECK(!ReadLevel0Ref(*Dir("x"), R));

	// ExtractGroups: zwei Gruppen mit getrennten Vertices, zweite herausloesen
	VmsMesh Src;
	Src.Positions = {0,0,0, 1,0,0, 0,1,0, 5,5,5, 6,5,5, 5,6,5};
	Src.Indices = {0,1,2, 3,4,5};
	VmsGroup G1; G1.MaterialCrc = 1; G1.NumRefIndices = 3; G1.IndexStart = 0;
	VmsGroup G2; G2.MaterialCrc = 2; G2.NumRefIndices = 3; G2.IndexStart = 3;
	Src.Groups = {G1, G2};
	VmsMesh Sub = ExtractGroups(Src, 1, 1);
	CHECK(Sub.Groups.size() == 1 && Sub.Groups[0].MaterialCrc == 2 && Sub.Groups[0].IndexStart == 0);
	CHECK(Sub.Positions.size() == 9 && Sub.Positions[0] == 5.0f && Sub.Indices.size() == 3 && Sub.Indices[2] == 2);
	CHECK(ExtractGroups(Src, 5, 2).Groups.empty());          // ausserhalb
	CHECK(ExtractGroups(Src, 0, 99).Groups.size() == 2);       // Ende wird begrenzt

	// Szene mit Hierarchie
	std::vector<SceneNode> Scene(2);
	Scene[0].Name = "Root";
	Scene[1].Name = "Child"; Scene[1].Parent = 0; Scene[1].T[0] = 4; Scene[1].bHasMesh = true; Scene[1].Mesh = Sub;
	auto Glb = BuildGlbScene(Scene);
	std::string Text((const char*)Glb.data(), Glb.size());
	CHECK(Text.find("\"children\":[1]") != std::string::npos);
	CHECK(Text.find("\"scenes\":[{\"nodes\":[0]}]") != std::string::npos);
	CHECK(Text.find("\"translation\":[4,0,0]") != std::string::npos);
	auto Empty = BuildGlbScene({});
	CHECK(Empty.size() >= 20 && std::memcmp(Empty.data(), "glTF", 4) == 0);
}

static void TestShips()
{
	const char* Text =
		"[Simple]\nnickname = generic_pilot\n[Ship]\nnickname = li_fighter\ntype = FIGHTER\nids_name = 237034\nmass = 100.5\nhit_pts = 1300\n"
		"hold_size = 25\nsteering_torque = 24000.0, 24000, 58000\nDA_archetype = ships\\liberty\\li_fighter.cmp\n"
		"material_library = a.mat\nmaterial_library = fx\\b.mat\ncockpit = cockpits\\l.ini\n[Ship]\ntype = X\n[Ship]\nnickname = bad\nmass = abc\n";
	auto Ships = ParseShipArch(IniFile::Parse(Text));
	CHECK(Ships.size() == 2);                      // Simple und Eintrag ohne nickname fallen weg
	CHECK(Ships[0].Nickname == "li_fighter" && Ships[0].Type == "FIGHTER" && Ships[0].IdsName == 237034);
	CHECK(Ships[0].Mass == 100.5f && Ships[0].HitPts == 1300 && Ships[0].HoldSize == 25);
	CHECK(Ships[0].SteeringTorque[0] == 24000.0f && Ships[0].SteeringTorque[2] == 58000.0f);
	CHECK(Ships[0].Archetype == "ships/liberty/li_fighter.cmp" && Ships[0].Cockpit == "cockpits/l.ini");
	CHECK(Ships[0].MaterialLibraries.size() == 2 && Ships[0].MaterialLibraries[1] == "fx/b.mat");
	CHECK(Ships[1].Nickname == "bad" && Ships[1].Mass == 0.0f);   // Unsinn wird 0
	CHECK(ParseShipArch(IniFile::Parse("")).empty());
}

static void TestRecords()
{
	const char* Text =
		"[Gun]\nnickname = laser01\nHP_type = a, b\nhp_type = c\nDA_archetype = x\\y.cmp\n[LOD]\nnickname = skip\n[Gun]\nmass = 1\n[Good]\nnickname = laser01\nprice = 5\n";
	auto Ini = IniFile::Parse(Text);
	auto Eq = ParseRecords(Ini, NonEquipmentSections());
	CHECK(Eq.size() == 1);                                   // LOD, Good und Gun ohne nickname fallen weg
	CHECK(Eq[0].Section == "Gun" && Eq[0].Nickname == "laser01");
	CHECK(Eq[0].Find("hp_type") && *Eq[0].Find("hp_type") == "a, b");   // Schluessel klein, Werte verbunden
	CHECK(Eq[0].FindAll("hp_type").size() == 2 && Eq[0].FindAll("hp_type")[1] == "c");
	CHECK(Eq[0].Find("nope") == nullptr);
	auto Goods = ParseRecordsOfType(Ini, "GOOD");
	CHECK(Goods.size() == 1 && *Goods[0].Find("price") == "5");
	CHECK(NormalizePath("a\\b\\c.cmp") == "a/b/c.cmp");
	CHECK(ParseRecords(IniFile::Parse(""), {}).empty());
}

static std::vector<uint8_t> Tga(uint8_t Type, int W, int H, int Bpp, uint8_t Desc, const std::vector<uint8_t>& Pixels)
{
	std::vector<uint8_t> B(18, 0);
	B[2] = Type; B[12] = (uint8_t)W; B[14] = (uint8_t)H; B[16] = (uint8_t)Bpp; B[17] = Desc;
	B.insert(B.end(), Pixels.begin(), Pixels.end());
	return B;
}

static std::vector<uint8_t> Dds(uint32_t FourCC, int W, int H, const std::vector<uint8_t>& Blocks, uint32_t PfFlags = 4, uint32_t Bits = 0, uint32_t Mr = 0, uint32_t Mg = 0, uint32_t Mb = 0, uint32_t Ma = 0)
{
	std::vector<uint8_t> B(128, 0);
	std::memcpy(B.data(), "DDS ", 4);
	auto S = [&](size_t O, uint32_t V) { std::memcpy(&B[O], &V, 4); };
	S(4, 124); S(12, (uint32_t)H); S(16, (uint32_t)W); S(76, 32); S(80, PfFlags); S(84, FourCC); S(88, Bits); S(92, Mr); S(96, Mg); S(100, Mb); S(104, Ma);
	B.insert(B.end(), Blocks.begin(), Blocks.end());
	return B;
}

static void TestImage()
{
	FlImage I; std::string E;
	// TGA 24 bpp, unten-links Ursprung, 1x2: Zeile 0 (unten) blau, Zeile 1 (oben) rot (BGR im File)
	CHECK(DecodeTga(Tga(2, 1, 2, 24, 0, {255, 0, 0, 0, 0, 255}), I, E));
	CHECK(I.Width == 1 && I.Height == 2);
	CHECK(I.Rgba[0] == 255 && I.Rgba[1] == 0 && I.Rgba[2] == 0);       // oben rot
	CHECK(I.Rgba[4] == 0 && I.Rgba[6] == 255);                          // unten blau
	// Ursprung oben-links (Bit 5): keine Spiegelung
	CHECK(DecodeTga(Tga(2, 1, 2, 24, 0x20, {255, 0, 0, 0, 0, 255}), I, E) && I.Rgba[2] == 255);
	// 16 bpp A1R5G5B5: 0x7C00 = rot ohne Alpha-Bits im Deskriptor -> deckend
	CHECK(DecodeTga(Tga(2, 1, 1, 16, 0, {0x00, 0x7C}), I, E) && I.Rgba[0] == 255 && I.Rgba[1] == 0 && I.Rgba[3] == 255);
	// 32 bpp mit Alpha (Deskriptor 8)
	CHECK(DecodeTga(Tga(2, 1, 1, 32, 8, {10, 20, 30, 40}), I, E) && I.Rgba[0] == 30 && I.Rgba[2] == 10 && I.Rgba[3] == 40);
	// 32 bpp ohne Alpha-Bits im Deskriptor: Alpha-Kanal wird trotzdem gelesen
	CHECK(DecodeTga(Tga(2, 1, 1, 32, 0, {10, 20, 30, 77}), I, E) && I.Rgba[3] == 77);
	// Palette (Typ 1): 2 Eintraege 24 bit BGR, Indizes 1 und 0
	{
		std::vector<uint8_t> B = Tga(1, 2, 1, 8, 0, {});
		B[1] = 1; B[5] = 2; B[7] = 24;
		B.insert(B.end(), {1, 2, 3, 4, 5, 6});     // Farbtabelle
		B.insert(B.end(), {1, 0});                 // Indizes
		CHECK(DecodeTga(B, I, E) && I.Rgba[0] == 6 && I.Rgba[2] == 4 && I.Rgba[4] == 3);
		B.resize(B.size() - 4);                    // Tabelle abgeschnitten
		CHECK(!DecodeTga(B, I, E));
	}
	// Graustufen
	CHECK(DecodeTga(Tga(3, 2, 1, 8, 0, {7, 9}), I, E) && I.Rgba[0] == 7 && I.Rgba[4] == 9);
	// RLE: ein Lauf mit 3 Pixeln (Header 0x82), 8 bpp gray
	CHECK(DecodeTga(Tga(11, 3, 1, 8, 0, {0x82, 5}), I, E) && I.Rgba[8] == 5);
	// Fehler
	CHECK(!DecodeTga({}, I, E));
	CHECK(!DecodeTga(Tga(2, 2, 2, 24, 0, {1, 2, 3}), I, E));            // abgeschnitten
	CHECK(!DecodeTga(Tga(7, 1, 1, 8, 0, {0}), I, E));                    // Typ
	CHECK(!DecodeTga(Tga(11, 3, 1, 8, 0, {0x82}), I, E));               // RLE abgeschnitten
	CHECK(!DecodeTga(Tga(2, 0, 1, 24, 0, {}), I, E));                    // Breite 0

	// DXT1: c0 = rot (0xF800), c1 = 0, alle Indizes 0 -> alle Pixel rot
	std::vector<uint8_t> Blk = {0x00, 0xF8, 0x00, 0x00, 0, 0, 0, 0};
	CHECK(DecodeDds(Dds(0x31545844, 4, 4, Blk), I, E) && I.Rgba[0] == 255 && I.Rgba[1] == 0 && I.Rgba[3] == 255);
	// DXT1 mit c0 <= c1 und Index 3 -> transparent
	std::vector<uint8_t> Blk2 = {0x00, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF};
	CHECK(DecodeDds(Dds(0x31545844, 4, 4, Blk2), I, E) && I.Rgba[3] == 0);
	// DXT5: Alpha0 = 128, Alpha1 = 0, Indizes 0 -> alpha 128
	std::vector<uint8_t> Blk5 = {128, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x00, 0x00, 0, 0, 0, 0};
	CHECK(DecodeDds(Dds(0x35545844, 4, 4, Blk5), I, E) && I.Rgba[3] == 128 && I.Rgba[0] == 255);
	// DXT3: Alpha-Nibble 0xF -> 255, 0x0 -> 0
	std::vector<uint8_t> Blk3 = {0x0F, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x00, 0x00, 0, 0, 0, 0};
	CHECK(DecodeDds(Dds(0x33545844, 4, 4, Blk3), I, E) && I.Rgba[3] == 255 && I.Rgba[7] == 0);
	// unkomprimiert 32 bpp BGRA
	CHECK(DecodeDds(Dds(0, 1, 1, {1, 2, 3, 4}, 0x41, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000), I, E)
		&& I.Rgba[0] == 3 && I.Rgba[1] == 2 && I.Rgba[2] == 1 && I.Rgba[3] == 4);
	// unkomprimiert 16 bpp R5G6B5: 0xF800 = rot
	CHECK(DecodeDds(Dds(0, 1, 1, {0x00, 0xF8}, 0x40, 16, 0xF800, 0x07E0, 0x001F, 0), I, E) && I.Rgba[0] == 255 && I.Rgba[1] == 0 && I.Rgba[3] == 255);
	// unkomprimiert 16 bpp A1R5G5B5: 0xFC00 = rot, Alpha an
	CHECK(DecodeDds(Dds(0, 1, 1, {0x00, 0xFC}, 0x41, 16, 0x7C00, 0x03E0, 0x001F, 0x8000), I, E) && I.Rgba[0] == 255 && I.Rgba[3] == 255);
	// Fehler
	CHECK(!DecodeDds({}, I, E));
	CHECK(!DecodeDds(Dds(0x31545844, 4, 4, {0, 0}), I, E));              // abgeschnitten
	CHECK(!DecodeDds(Dds(0x12345678, 4, 4, Blk), I, E));                 // unbekanntes FourCC
	// Auto-Erkennung
	CHECK(DecodeImage(Dds(0x31545844, 4, 4, Blk), I, E) && I.Width == 4);
	CHECK(DecodeImage(Tga(3, 1, 1, 8, 0, {9}), I, E) && I.Rgba[0] == 9);
}

static void TestBuildModel()
{
	// Compound mit einem Teil "Wing", Mesh-Bibliothek "m.vms", Konstrukt Root -> Wing
	auto Root = Dir("/");
	auto Lib = Dir("VMeshLibrary");
	auto Entry = Dir("m.vms");
	auto Vms = MakeVms();
	Entry->Children.push_back(Leaf("VMeshData", Vms.data(), Vms.size()));
	Lib->Children.push_back(std::move(Entry));
	Root->Children.push_back(std::move(Lib));

	auto Model = Dir("wing.3db");
	auto ML = Dir("MultiLevel"); auto L0 = Dir("Level0"); auto VP = Dir("VMeshPart");
	std::vector<uint8_t> Ref(60, 0);
	uint32_t Crc = FlModelCrc("m.vms"); uint16_t Start = 0, Cnt = 1;
	std::memcpy(Ref.data() + 4, &Crc, 4); std::memcpy(Ref.data() + 16, &Start, 2); std::memcpy(Ref.data() + 18, &Cnt, 2);
	VP->Children.push_back(Leaf("VMeshRef", Ref.data(), Ref.size()));
	L0->Children.push_back(std::move(VP)); ML->Children.push_back(std::move(L0)); Model->Children.push_back(std::move(ML));
	auto Hp = Dir("Hardpoints"); auto Fx = Dir("Fixed"); auto One = Dir("HpA");
	float Pos[3] = {1, 2, 3};
	One->Children.push_back(Leaf("Position", Pos, 12));
	Fx->Children.push_back(std::move(One)); Hp->Children.push_back(std::move(Fx)); Model->Children.push_back(std::move(Hp));
	Root->Children.push_back(std::move(Model));

	auto Cmpnd = Dir("Cmpnd");
	auto Part = Dir("Part_wing");
	Part->Children.push_back(Leaf("Object name", "Wing\0", 5));
	Part->Children.push_back(Leaf("File name", "wing.3db\0", 9));
	auto RootPart = Dir("Root");
	RootPart->Children.push_back(Leaf("Object name", "Root\0", 5));
	RootPart->Children.push_back(Leaf("File name", "none.3db\0", 9));
	Cmpnd->Children.push_back(std::move(RootPart));
	Cmpnd->Children.push_back(std::move(Part));
	auto Cons = Dir("Cons");
	std::vector<uint8_t> Fix(176, 0);
	std::strcpy((char*)Fix.data(), "Root"); std::strcpy((char*)Fix.data() + 64, "Wing");
	float FixData[12] = {5, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
	std::memcpy(Fix.data() + 128, FixData, 48);
	Cons->Children.push_back(Leaf("Fix", Fix.data(), Fix.size()));
	Cmpnd->Children.push_back(std::move(Cons));
	Root->Children.push_back(std::move(Cmpnd));

	FlModel M; std::string Err;
	CHECK(BuildModel(*Root, M, Err));
	CHECK(M.Parts.size() == 2);
	CHECK(M.Parts[0].Name == "Root" && !M.Parts[0].bHasMesh && M.Parts[0].Parent == -1);
	CHECK(!M.Parts[0].bHasMesh && M.Parts[0].Mesh.Positions.empty());     // Teil ohne Datei: kein Mesh
	CHECK(M.Parts[1].Name == "Wing" && M.Parts[1].Parent == 0 && M.Parts[1].Position[0] == 5.0f);
	CHECK(M.Parts[1].bHasMesh && M.Parts[1].Mesh.Groups.size() == 1 && M.Parts[1].Mesh.Positions.size() == 9);
	CHECK(M.Parts[1].Hardpoints.size() == 1 && M.Parts[1].Hardpoints[0].Position[2] == 3.0f);

	// einzelnes .3db: Meshes direkt in der Bibliothek, keine Teile
	auto Single = Dir("/");
	auto L2 = Dir("VMeshLibrary"); auto E2 = Dir("x.lod0-112.vms");
	E2->Children.push_back(Leaf("VMeshData", Vms.data(), Vms.size()));
	L2->Children.push_back(std::move(E2));
	Single->Children.push_back(std::move(L2));
	CHECK(BuildModel(*Single, M, Err) && M.Parts.size() == 1 && M.Parts[0].bHasMesh);
	CHECK(!BuildModel(*Dir("/"), M, Err));                      // weder Teile noch Meshes

	// Koordinatenumrechnung: vorn (-z) wird +X, rechts (+x) wird +Y, oben (+y) wird +Z; Meter -> cm
	float P[3] = {1, 2, -3}, Q[3];
	FlToUePosition(P, Q);
	CHECK(Q[0] == 300.0f && Q[1] == 100.0f && Q[2] == 200.0f);
	FlToUeDirection(P, Q);
	CHECK(Q[0] == 3.0f && Q[1] == 1.0f && Q[2] == 2.0f);
	float Id[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, R[9];
	FlToUeRotation(Id, R);
	CHECK(R[0] == 1.0f && R[4] == 1.0f && R[8] == 1.0f && R[1] == 0.0f);
	// 90 Grad um die Hochachse (FL y) wird 90 Grad um UE-Z, mit gespiegelter Handedness (Vorzeichen bleibt als R[1] = -R[3] erhalten)
	float Ry[9] = {0, 0, 1, 0, 1, 0, -1, 0, 0};
	FlToUeRotation(Ry, R);
	CHECK(std::fabs(R[8] - 1.0f) < 1e-6f && std::fabs(R[1] + R[3]) < 1e-6f && std::fabs(std::fabs(R[1]) - 1.0f) < 1e-6f);
}

// 4 Vertices (x = 0..3), eine Gruppe mit StartVertex 1 (End 3), Indizes relativ {0,1,2}, FVF nur XYZ (0x2).
static std::vector<uint8_t> MakeVmsRelative(uint16_t GroupStart)
{
	std::vector<uint8_t> B;
	auto P16 = [&](uint16_t V) { B.insert(B.end(), (uint8_t*)&V, (uint8_t*)&V + 2); };
	auto P32 = [&](uint32_t V) { B.insert(B.end(), (uint8_t*)&V, (uint8_t*)&V + 4); };
	auto PF = [&](float V) { B.insert(B.end(), (uint8_t*)&V, (uint8_t*)&V + 4); };
	P32(1); P32(4);
	P16(1); P16(3); P16(0x2); P16(4);
	P32(7); P16(GroupStart); P16(GroupStart + 2); P16(3); P16(0xCC);
	P16(0); P16(1); P16(2);
	for (int v = 0; v < 4; ++v) { PF((float)v); PF(0); PF(0); }
	return B;
}

static void TestVmsRelativeIndices()
{
	VmsMesh M; std::string Err;
	CHECK(ParseVms(MakeVmsRelative(1), M, Err));
	CHECK(M.Indices.size() == 3 && M.Indices[0] == 1 && M.Indices[1] == 2 && M.Indices[2] == 3);   // relativ + StartVertex
	CHECK(M.Positions[M.Indices[0] * 3] == 1.0f);
	VmsMesh Bad;
	CHECK(!ParseVms(MakeVmsRelative(2), Bad, Err));      // 2 + 2 = 4 >= 4 Vertices: ungueltig

	// ExtractGroups mit Teile-Versatz: Quelle mit 6 Vertices, Gruppe indiziert 0..2, Versatz 3 -> Vertices 3..5
	VmsMesh Src;
	Src.Positions = {0,0,0, 1,0,0, 2,0,0, 3,0,0, 4,0,0, 5,0,0};
	Src.Indices = {0, 1, 2};
	VmsGroup G; G.NumRefIndices = 3; G.IndexStart = 0;
	Src.Groups = {G};
	VmsMesh Sub = ExtractGroups(Src, 0, 1, 3);
	CHECK(Sub.Groups.size() == 1 && Sub.Positions.size() == 9 && Sub.Positions[0] == 3.0f && Sub.Positions[6] == 5.0f);
	CHECK(ExtractGroups(Src, 0, 1, 4).Groups.empty());   // Versatz zu gross -> Gruppe ausgelassen
	// Gruppen mit weniger als einem Dreieck liefern kein Mesh (haetten in der Engine ein leeres LOD ergeben)
	VmsMesh Tiny;
	Tiny.Positions = {0,0,0, 1,0,0, 2,0,0};
	Tiny.Indices = {0, 1};
	VmsGroup TG; TG.NumRefIndices = 2; TG.IndexStart = 0;
	Tiny.Groups = {TG};
	CHECK(ExtractGroups(Tiny, 0, 1).Groups.empty() && ExtractGroups(Tiny, 0, 1).Positions.empty());
	// 4 Indizes: nur das ganze Dreieck bleibt
	Tiny.Indices = {0, 1, 2, 0};
	Tiny.Groups[0].NumRefIndices = 4;
	VmsMesh Cut = ExtractGroups(Tiny, 0, 1);
	CHECK(Cut.Indices.size() == 3 && Cut.Groups[0].NumRefIndices == 3);
}

static void TestMath()
{
	auto V = ParseFloats("1, 2.5, -3");
	CHECK(V.size() == 3 && V[0] == 1.0f && V[1] == 2.5f && V[2] == -3.0f);
	CHECK(ParseFloats("").empty());
	CHECK(ParseFloats("7").size() == 1 && ParseFloats("7")[0] == 7.0f);
	auto W = ParseFloats("1,,x");
	CHECK(W.size() == 3 && W[1] == 0.0f && W[2] == 0.0f);          // Unsinn wird 0

	float R[9];
	float Zero[3] = {0, 0, 0};
	FlEulerToRotation(Zero, R);
	CHECK(R[0] == 1.0f && R[4] == 1.0f && R[8] == 1.0f && R[1] == 0.0f);
	float Z90[3] = {0, 0, 90};                                     // 90 Grad um Z: x -> y
	FlEulerToRotation(Z90, R);
	CHECK(std::fabs(R[3] - 1.0f) < 1e-6f && std::fabs(R[1] + 1.0f) < 1e-6f && std::fabs(R[8] - 1.0f) < 1e-6f);
	float X90[3] = {90, 0, 0};                                     // 90 Grad um X: y -> z
	FlEulerToRotation(X90, R);
	CHECK(std::fabs(R[7] - 1.0f) < 1e-6f && std::fabs(R[5] + 1.0f) < 1e-6f);
	float XZ[3] = {90, 0, 90};                                     // erst X, dann Z: y -> z, danach x -> y
	FlEulerToRotation(XZ, R);
	// R = Rz * Rx: die z-Zeile von Rz bleibt (0,0,1), also ist die z-Zeile von R gleich der von Rx = (0,1,0)
	CHECK(std::fabs(R[7] - 1.0f) < 1e-5f && std::fabs(R[6]) < 1e-5f && std::fabs(R[8]) < 1e-5f);
	float Q[4];
	RotationToQuat(R, Q);
	CHECK(std::fabs(Q[0] * Q[0] + Q[1] * Q[1] + Q[2] * Q[2] + Q[3] * Q[3] - 1.0f) < 1e-4f);
}

// Teil, dessen VMeshRef keine Gruppen umfasst (MeshCount 0): darf kein Mesh bekommen (sonst NaN-Grenzen in der Engine)
static void TestEmptyPartHasNoMesh()
{
	auto Root = Dir("/");
	auto Lib = Dir("VMeshLibrary"); auto Entry = Dir("m.vms");
	auto Vms = MakeVms();
	Entry->Children.push_back(Leaf("VMeshData", Vms.data(), Vms.size()));
	Lib->Children.push_back(std::move(Entry));
	Root->Children.push_back(std::move(Lib));
	auto Model = Dir("empty.3db");
	auto ML = Dir("MultiLevel"); auto L0 = Dir("Level0"); auto VP = Dir("VMeshPart");
	std::vector<uint8_t> Ref(60, 0);
	uint32_t Crc = FlModelCrc("m.vms");
	std::memcpy(Ref.data() + 4, &Crc, 4);              // StartMesh 0, MeshCount 0
	VP->Children.push_back(Leaf("VMeshRef", Ref.data(), Ref.size()));
	L0->Children.push_back(std::move(VP)); ML->Children.push_back(std::move(L0)); Model->Children.push_back(std::move(ML));
	Root->Children.push_back(std::move(Model));
	auto Cmpnd = Dir("Cmpnd");
	auto Part = Dir("Root");
	Part->Children.push_back(Leaf("Object name", "Root\0", 5));
	Part->Children.push_back(Leaf("File name", "empty.3db\0", 10));
	Cmpnd->Children.push_back(std::move(Part));
	Root->Children.push_back(std::move(Cmpnd));
	FlModel M; std::string Err;
	CHECK(BuildModel(*Root, M, Err));
	CHECK(M.Parts.size() == 1 && !M.Parts[0].bHasMesh);
}

int main()
{
	TestUtf();
	TestIni();
	TestVms();
	TestVmsRelativeIndices();
	TestModel();
	TestCmp();
	TestShips();
	TestRecords();
	TestImage();
	TestBuildModel();
	TestMath();
	TestEmptyPartHasNoMesh();
	std::printf(Failures ? "%d Fehler\n" : "alle Tests bestanden\n", Failures);
	return Failures ? 1 : 0;
}

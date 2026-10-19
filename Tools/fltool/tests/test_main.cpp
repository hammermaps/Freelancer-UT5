// Tests fuer flformats. Nur synthetische Daten, keine Originaldateien.
#include "Ini.h"
#include "Model.h"
#include "ShipData.h"
#include "Utf.h"
#include "Vms.h"
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

int main()
{
	TestUtf();
	TestIni();
	TestVms();
	TestModel();
	TestCmp();
	TestShips();
	std::printf(Failures ? "%d Fehler\n" : "alle Tests bestanden\n", Failures);
	return Failures ? 1 : 0;
}

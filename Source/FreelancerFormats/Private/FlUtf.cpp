#include "FlUtf.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace fl
{
namespace
{
struct FNodeRaw { int32_t Peer, Name, Flags, Pad, Child, Allocated, Size; };
constexpr size_t NodeStride = 44;

template <typename T> bool ReadAt(const std::vector<uint8_t>& B, size_t Off, T& Out)
{
	if (Off + sizeof(T) > B.size()) return false;
	std::memcpy(&Out, B.data() + Off, sizeof(T));
	return true;
}

struct FCtx
{
	const std::vector<uint8_t>& File;
	size_t NodeOff, NodeSize, StrOff, StrSize, DataOff;
	std::string Err;
	int Depth = 0;
};

std::unique_ptr<UtfNode> ReadNode(FCtx& C, int32_t Rel)
{
	if (Rel < 0 || (size_t)Rel + sizeof(FNodeRaw) > C.NodeSize) { C.Err = "Knoten ausserhalb des Node-Blocks"; return nullptr; }
	FNodeRaw R;
	ReadAt(C.File, C.NodeOff + Rel, R);
	auto N = std::make_unique<UtfNode>();
	if (R.Name < 0 || (size_t)R.Name >= C.StrSize) { C.Err = "Namensoffset ungueltig"; return nullptr; }
	const char* S = (const char*)C.File.data() + C.StrOff + R.Name;
	size_t Max = C.StrSize - R.Name;
	N->Name.assign(S, strnlen(S, Max));
	if (R.Flags & 0x10)
	{
		N->bIntermediate = true;
		if (++C.Depth > 64) { C.Err = "Baum zu tief"; return nullptr; }
		size_t Guard = 0;
		for (int32_t Next = R.Child; Next > 0; )
		{
			if (++Guard > C.NodeSize / NodeStride + 1) { C.Err = "Zyklus im Baum"; return nullptr; }
			auto Ch = ReadNode(C, Next);
			if (!Ch) return nullptr;
			FNodeRaw CR; ReadAt(C.File, C.NodeOff + Next, CR);
			N->Children.push_back(std::move(Ch));
			Next = CR.Peer;
		}
		--C.Depth;
	}
	else if (R.Size > 0)
	{
		size_t Start = C.DataOff + (size_t)R.Child;
		if (R.Child < 0 || Start + R.Size > C.File.size()) { C.Err = "Daten ausserhalb der Datei: " + N->Name; return nullptr; }
		N->Data.assign(C.File.begin() + Start, C.File.begin() + Start + R.Size);
	}
	return N;
}
}

UtfFile UtfFile::Parse(const std::vector<uint8_t>& B)
{
	UtfFile F;
	if (B.size() < 40 || std::memcmp(B.data(), "UTF ", 4) != 0) { F.Error = "kein UTF-Container"; return F; }
	int32_t H[9];
	std::memcpy(H, B.data() + 4, sizeof(H));
	if (H[0] != 257) { F.Error = "unbekannte UTF-Version " + std::to_string(H[0]); return F; }
	FCtx C{B, (size_t)H[1], (size_t)H[2], (size_t)H[5], (size_t)H[7], (size_t)H[8], {}};
	if (H[1] < 0 || H[2] < 0 || C.NodeOff + C.NodeSize > B.size() || C.StrOff + C.StrSize > B.size() || C.DataOff > B.size())
	{ F.Error = "Bloecke ausserhalb der Datei"; return F; }
	F.Root = ReadNode(C, 0);
	if (!F.Root) F.Error = C.Err;
	return F;
}

UtfFile UtfFile::Load(const std::string& Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In) { UtfFile F; F.Error = "kann Datei nicht oeffnen: " + Path; return F; }
	std::vector<uint8_t> B((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	return Parse(B);
}

const UtfNode* FindChild(const UtfNode& P, const std::string& Name)
{
	auto Lower = [](std::string S) { std::transform(S.begin(), S.end(), S.begin(), ::tolower); return S; };
	std::string L = Lower(Name);
	for (auto& C : P.Children) if (Lower(C->Name) == L) return C.get();
	return nullptr;
}
}

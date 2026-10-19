#include "Image.h"
#include <cstring>

namespace fl
{
namespace
{
bool Fail(std::string& E, const char* M) { E = M; return false; }

uint16_t R16(const uint8_t* P) { return (uint16_t)(P[0] | (P[1] << 8)); }
uint32_t R32(const uint8_t* P) { return (uint32_t)P[0] | ((uint32_t)P[1] << 8) | ((uint32_t)P[2] << 16) | ((uint32_t)P[3] << 24); }

void Put(uint8_t* D, uint8_t R, uint8_t G, uint8_t B, uint8_t A) { D[0] = R; D[1] = G; D[2] = B; D[3] = A; }

// 5-Bit-Kanal auf 8 Bit
uint8_t Expand5(uint32_t V) { return (uint8_t)((V << 3) | (V >> 2)); }
}

bool DecodeTga(const std::vector<uint8_t>& D, FlImage& Out, std::string& Err)
{
	if (D.size() < 18) return Fail(Err, "TGA zu kurz");
	const uint8_t IdLen = D[0], CmapType = D[1], Type = D[2];
	const int W = R16(&D[12]), H = R16(&D[14]), Bpp = D[16];
	const uint8_t Desc = D[17];
	if (CmapType > 1) return Fail(Err, "TGA-Farbtabellentyp ungueltig");
	const bool bRle = (Type == 9 || Type == 10 || Type == 11);
	const bool bGray = (Type == 3 || Type == 11);
	const bool bPal = (Type == 1 || Type == 9);
	if (Type != 1 && Type != 2 && Type != 3 && Type != 9 && Type != 10 && Type != 11) return Fail(Err, "TGA-Typ nicht unterstuetzt");
	if (bPal && CmapType != 1) return Fail(Err, "TGA ohne Farbtabelle");
	const int CmapFirst = R16(&D[3]), CmapLen = R16(&D[5]), CmapBits = D[7];
	const int CmapBytes = (CmapBits + 7) / 8;
	const size_t CmapSize = CmapType ? (size_t)CmapLen * CmapBytes : 0;
	if (W <= 0 || H <= 0 || W > 16384 || H > 16384) return Fail(Err, "TGA-Groesse ungueltig");
	if (bPal ? Bpp != 8 : (!(Bpp == 8 && bGray) && !(Bpp == 16 || Bpp == 24 || Bpp == 32))) return Fail(Err, "TGA-Bittiefe nicht unterstuetzt");
	if (CmapType && (CmapBytes < 2 || CmapBytes > 4)) return Fail(Err, "TGA-Farbtabelleneintrag nicht unterstuetzt");
	const int BytesPer = Bpp / 8;
	size_t Pos = 18 + IdLen;
	const size_t CmapPos = Pos;
	if (Pos + CmapSize > D.size()) return Fail(Err, "TGA abgeschnitten");
	Pos += CmapSize;
	std::vector<uint8_t> Raw((size_t)W * H * BytesPer);
	if (!bRle)
	{
		if (Pos + Raw.size() > D.size()) return Fail(Err, "TGA abgeschnitten");
		std::memcpy(Raw.data(), D.data() + Pos, Raw.size());
	}
	else
	{
		size_t O = 0;
		while (O < Raw.size())
		{
			if (Pos >= D.size()) return Fail(Err, "TGA-RLE abgeschnitten");
			uint8_t Hdr = D[Pos++];
			size_t Count = (size_t)(Hdr & 0x7F) + 1;
			if (O + Count * BytesPer > Raw.size()) return Fail(Err, "TGA-RLE ueberlaeuft");
			if (Hdr & 0x80)
			{
				if (Pos + BytesPer > D.size()) return Fail(Err, "TGA-RLE abgeschnitten");
				for (size_t i = 0; i < Count; ++i) { std::memcpy(&Raw[O], &D[Pos], BytesPer); O += BytesPer; }
				Pos += BytesPer;
			}
			else
			{
				if (Pos + Count * BytesPer > D.size()) return Fail(Err, "TGA-RLE abgeschnitten");
				std::memcpy(&Raw[O], &D[Pos], Count * BytesPer);
				O += Count * BytesPer; Pos += Count * BytesPer;
			}
		}
	}
	Out.Width = W; Out.Height = H;
	Out.Rgba.assign((size_t)W * H * 4, 255);
	const bool bTopDown = (Desc & 0x20) != 0;
	const int AlphaBits = Desc & 0x0F; // nur fuer 16 bpp relevant; 32 bpp liefert immer Alpha
	for (int y = 0; y < H; ++y)
	{
		const int Dy = bTopDown ? y : (H - 1 - y);
		for (int x = 0; x < W; ++x)
		{
			const uint8_t* S = &Raw[((size_t)y * W + x) * BytesPer];
			uint8_t* P = &Out.Rgba[((size_t)Dy * W + x) * 4];
			if (bPal)
			{
				const int Idx = S[0] - CmapFirst;
				if (Idx < 0 || Idx >= CmapLen) { Put(P, 0, 0, 0, 255); continue; }
				const uint8_t* C = &D[CmapPos + (size_t)Idx * CmapBytes];
				if (CmapBytes == 2) { const uint16_t V = R16(C); Put(P, Expand5((V >> 10) & 31), Expand5((V >> 5) & 31), Expand5(V & 31), 255); }
				else if (CmapBytes == 3) Put(P, C[2], C[1], C[0], 255);
				else Put(P, C[2], C[1], C[0], CmapBits == 32 ? C[3] : 255);
			}
			else if (Bpp == 8) Put(P, S[0], S[0], S[0], 255);
			else if (Bpp == 16)
			{
				const uint16_t V = R16(S);
				Put(P, Expand5((V >> 10) & 31), Expand5((V >> 5) & 31), Expand5(V & 31), (AlphaBits ? ((V >> 15) ? 255 : 0) : 255));
			}
			else if (Bpp == 24) Put(P, S[2], S[1], S[0], 255);
			else Put(P, S[2], S[1], S[0], S[3]);
		}
	}
	return true;
}

namespace
{
void Rgb565(uint16_t V, uint8_t C[3])
{
	C[0] = (uint8_t)(((V >> 11) & 31) * 255 / 31);
	C[1] = (uint8_t)(((V >> 5) & 63) * 255 / 63);
	C[2] = (uint8_t)((V & 31) * 255 / 31);
}

// Farbpalette eines DXT-Blocks; bThreeColor = DXT1 mit c0 <= c1 (1 Bit Alpha)
void ColorBlock(const uint8_t* B, uint8_t Pal[4][4], bool bDxt1)
{
	const uint16_t C0 = R16(B), C1 = R16(B + 2);
	uint8_t A[3], Bc[3];
	Rgb565(C0, A); Rgb565(C1, Bc);
	for (int i = 0; i < 3; ++i) { Pal[0][i] = A[i]; Pal[1][i] = Bc[i]; }
	Pal[0][3] = Pal[1][3] = Pal[2][3] = 255; Pal[3][3] = 255;
	if (!bDxt1 || C0 > C1)
	{
		for (int i = 0; i < 3; ++i) { Pal[2][i] = (uint8_t)((2 * A[i] + Bc[i]) / 3); Pal[3][i] = (uint8_t)((A[i] + 2 * Bc[i]) / 3); }
	}
	else
	{
		for (int i = 0; i < 3; ++i) { Pal[2][i] = (uint8_t)((A[i] + Bc[i]) / 2); Pal[3][i] = 0; }
		Pal[3][3] = 0;
	}
}
}

bool DecodeDds(const std::vector<uint8_t>& D, FlImage& Out, std::string& Err)
{
	if (D.size() < 128 || std::memcmp(D.data(), "DDS ", 4) != 0) return Fail(Err, "kein DDS");
	const int H = (int)R32(&D[12]), W = (int)R32(&D[16]);
	if (W <= 0 || H <= 0 || W > 16384 || H > 16384) return Fail(Err, "DDS-Groesse ungueltig");
	const uint32_t PfFlags = R32(&D[80]);
	const uint32_t FourCC = R32(&D[84]);
	const uint32_t Bits = R32(&D[88]);
	const uint8_t* Data = D.data() + 128;
	const size_t Avail = D.size() - 128;
	Out.Width = W; Out.Height = H;
	Out.Rgba.assign((size_t)W * H * 4, 255);
	if (PfFlags & 0x4) // FourCC
	{
		const bool bDxt1 = FourCC == 0x31545844, bDxt3 = FourCC == 0x33545844, bDxt5 = FourCC == 0x35545844;
		if (!bDxt1 && !bDxt3 && !bDxt5) return Fail(Err, "DDS-FourCC nicht unterstuetzt");
		const size_t BlockSize = bDxt1 ? 8 : 16;
		const int BW = (W + 3) / 4, BH = (H + 3) / 4;
		if ((size_t)BW * BH * BlockSize > Avail) return Fail(Err, "DDS abgeschnitten");
		for (int by = 0; by < BH; ++by)
			for (int bx = 0; bx < BW; ++bx)
			{
				const uint8_t* B = Data + ((size_t)by * BW + bx) * BlockSize;
				const uint8_t* CB = bDxt1 ? B : B + 8;
				uint8_t Pal[4][4];
				ColorBlock(CB, Pal, bDxt1);
				uint8_t Alpha[16];
				for (int i = 0; i < 16; ++i) Alpha[i] = 255;
				if (bDxt3)
				{
					for (int i = 0; i < 16; ++i) { uint8_t N = (B[i / 2] >> ((i & 1) * 4)) & 15; Alpha[i] = (uint8_t)(N * 17); }
				}
				else if (bDxt5)
				{
					uint8_t A[8]; A[0] = B[0]; A[1] = B[1];
					if (A[0] > A[1]) for (int i = 1; i < 7; ++i) A[i + 1] = (uint8_t)(((7 - i) * A[0] + i * A[1]) / 7);
					else { for (int i = 1; i < 5; ++i) A[i + 1] = (uint8_t)(((5 - i) * A[0] + i * A[1]) / 5); A[6] = 0; A[7] = 255; }
					uint64_t Bitsv = 0;
					for (int i = 0; i < 6; ++i) Bitsv |= (uint64_t)B[2 + i] << (8 * i);
					for (int i = 0; i < 16; ++i) Alpha[i] = A[(Bitsv >> (3 * i)) & 7];
				}
				const uint32_t Idx = R32(CB + 4);
				for (int py = 0; py < 4; ++py)
					for (int px = 0; px < 4; ++px)
					{
						const int X = bx * 4 + px, Y = by * 4 + py;
						if (X >= W || Y >= H) continue;
						const int I = py * 4 + px;
						const uint8_t* C = Pal[(Idx >> (2 * I)) & 3];
						uint8_t A8 = bDxt1 ? C[3] : Alpha[I];
						Put(&Out.Rgba[((size_t)Y * W + X) * 4], C[0], C[1], C[2], A8);
					}
			}
		return true;
	}
	if ((PfFlags & 0x40) && (Bits == 32 || Bits == 24 || Bits == 16)) // RGB mit Masken
	{
		const uint32_t MR = R32(&D[92]), MG = R32(&D[96]), MB = R32(&D[100]), MA = (PfFlags & 0x1) ? R32(&D[104]) : 0;
		const int Bytes = (int)Bits / 8;
		if ((size_t)W * H * Bytes > Avail) return Fail(Err, "DDS abgeschnitten");
		auto Chan = [](uint32_t V, uint32_t M) -> uint8_t {
			if (!M) return 255;
			int Shift = 0; while (!((M >> Shift) & 1)) ++Shift;
			uint32_t Max = M >> Shift;
			return (uint8_t)(((V & M) >> Shift) * 255 / Max);
		};
		for (int i = 0; i < W * H; ++i)
		{
			uint32_t V = 0;
			for (int b = 0; b < Bytes; ++b) V |= (uint32_t)Data[(size_t)i * Bytes + b] << (8 * b);
			Put(&Out.Rgba[(size_t)i * 4], Chan(V, MR), Chan(V, MG), Chan(V, MB), MA ? Chan(V, MA) : 255);
		}
		return true;
	}
	return Fail(Err, "DDS-Pixelformat nicht unterstuetzt");
}

bool DecodeImage(const std::vector<uint8_t>& Data, FlImage& Out, std::string& Err)
{
	if (Data.size() >= 4 && std::memcmp(Data.data(), "DDS ", 4) == 0) return DecodeDds(Data, Out, Err);
	return DecodeTga(Data, Out, Err);
}
}

#include "FlMath.h"
#include <cmath>
#include <cstdlib>

namespace fl
{
std::vector<float> ParseFloats(const std::string& Text)
{
	std::vector<float> R;
	size_t P = 0;
	while (P <= Text.size())
	{
		size_t Comma = Text.find(',', P);
		if (Comma == std::string::npos) Comma = Text.size();
		const std::string Part = Text.substr(P, Comma - P);
		if (!Part.empty() || Comma < Text.size()) R.push_back((float)std::atof(Part.c_str()));
		P = Comma + 1;
	}
	return R;
}

void FlEulerToRotation(const float D[3], float Out[9])
{
	const float Pi = 3.14159265358979f;
	const float A = D[0] * Pi / 180.f, B = D[1] * Pi / 180.f, C = D[2] * Pi / 180.f;
	const float Ca = std::cos(A), Sa = std::sin(A), Cb = std::cos(B), Sb = std::sin(B), Cc = std::cos(C), Sc = std::sin(C);
	const float Rx[9] = {1, 0, 0, 0, Ca, -Sa, 0, Sa, Ca};
	const float Ry[9] = {Cb, 0, Sb, 0, 1, 0, -Sb, 0, Cb};
	const float Rz[9] = {Cc, -Sc, 0, Sc, Cc, 0, 0, 0, 1};
	auto Mul = [](const float* X, const float* Y, float* Z) {
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
			{
				float S = 0;
				for (int k = 0; k < 3; ++k) S += X[r * 3 + k] * Y[k * 3 + c];
				Z[r * 3 + c] = S;
			}
	};
	float T[9];
	Mul(Rz, Ry, T);
	Mul(T, Rx, Out);
}
}

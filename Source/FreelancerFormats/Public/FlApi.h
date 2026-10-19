// Export-Makro: im UE-Modul FREELANCERFORMATS_API, im CMake-Build (fltool) leer.
#pragma once
#if defined(FREELANCERFORMATS_API)
#include "HAL/Platform.h" // definiert DLLEXPORT/DLLIMPORT
#define FLF_API FREELANCERFORMATS_API
#else
#define FLF_API
#endif

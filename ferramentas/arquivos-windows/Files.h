#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef FILES_BUILD
#define FILES_API extern "C" __declspec(dllexport)
#else
#define FILES_API extern "C" __declspec(dllimport)
#endif

FILES_API void PerformFilesCopy();
FILES_API void CALLBACK AplicarArquivos(HWND, HINSTANCE, LPSTR, int);
FILES_API DWORD FilesGetLastStatus();

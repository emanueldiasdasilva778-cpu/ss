#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#ifndef FSC_MOCK_KIND
#define FSC_MOCK_KIND 0
#endif
[[maybe_unused]] static DWORD Mock() {
    wchar_t crash[4]{};
    if (GetEnvironmentVariableW(L"FSC_MOCK_CRASH", crash, 4) && crash[0] == L'1')
        RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    Sleep(120);
    wchar_t log[32768]{};
    DWORD n = GetEnvironmentVariableW(L"FSC_TEST_LOG", log, 32768);
    if (n && n < 32768) {
        HANDLE file = CreateFileW(log, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD written;
            const char marker = '0' + FSC_MOCK_KIND;
            WriteFile(file, &marker, 1, &written, nullptr); CloseHandle(file);
        }
    }
    wchar_t code[16]{};
    GetEnvironmentVariableW(L"FSC_MOCK_RESULT", code, 16);
    return static_cast<DWORD>(wcstoul(code, nullptr, 10));
}
[[maybe_unused]] static const wchar_t* Report(const wchar_t* text) {
    thread_local std::wstring report;
    wchar_t large[4]{};
    if (GetEnvironmentVariableW(L"FSC_MOCK_LONG", large, 4) && large[0] == L'1')
        report.assign(70000, L'x');
    else report = text;
    return report.c_str();
}
#if FSC_MOCK_KIND == 0
extern "C" __declspec(dllexport) int LimparPc() { return static_cast<int>(Mock()); }
extern "C" __declspec(dllexport) const wchar_t* CleanerGetLastReport() { return Report(L"Relatorio simulado: C:\\Teste\\ocupado.bin [32]"); }
#elif FSC_MOCK_KIND == 1
extern "C" __declspec(dllexport) int LimparRegistroSilencioso() { return static_cast<int>(Mock()); }
extern "C" __declspec(dllexport) const wchar_t* RegistryGetLastReport() { return Report(L"Relatorio simulado: HKLM\\Teste [5]"); }
#elif FSC_MOCK_KIND == 2
thread_local DWORD status = 0;
extern "C" __declspec(dllexport) void PerformFilesCopy() { status = Mock(); }
extern "C" __declspec(dllexport) DWORD FilesGetLastStatus() { return status; }
extern "C" __declspec(dllexport) const wchar_t* FilesGetLastReport() { return Report(L"Relatorio simulado: C:\\Teste\\destino.xml [32]"); }
#else
extern "C" __declspec(dllexport) int ExportacaoIncorreta() { return 0; }
#endif

#define UNICODE
#define _UNICODE
#include <windows.h>
#ifndef FSC_MOCK_KIND
#define FSC_MOCK_KIND 0
#endif
[[maybe_unused]] static DWORD Mock() {
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
#if FSC_MOCK_KIND == 0
extern "C" __declspec(dllexport) int LimparPc() { return static_cast<int>(Mock()); }
#elif FSC_MOCK_KIND == 1
extern "C" __declspec(dllexport) int LimparRegistroSilencioso() { return static_cast<int>(Mock()); }
#elif FSC_MOCK_KIND == 2
thread_local DWORD status = 0;
extern "C" __declspec(dllexport) void PerformFilesCopy() { status = Mock(); }
extern "C" __declspec(dllexport) DWORD FilesGetLastStatus() { return status; }
#else
extern "C" __declspec(dllexport) int ExportacaoIncorreta() { return 0; }
#endif

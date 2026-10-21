#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <string>
#include <iostream>
#include <stdexcept>

// Executar somente com as DLLs de mock.cpp na pasta recebida.
static unsigned passed = 0;
static void Check(bool ok, const char* name) {
    if (!ok) throw std::runtime_error(name);
    ++passed; std::cout << "PASS: " << name << '\n' << std::flush;
}
struct Search { DWORD pid; HWND window = nullptr; };
static BOOL CALLBACK Find(HWND window, LPARAM parameter) {
    auto& search = *reinterpret_cast<Search*>(parameter);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    wchar_t type[64]{}; GetClassNameW(window, type, 64);
    if (pid == search.pid && std::wstring(type) == L"FSC.Native.Window") search.window = window;
    return TRUE;
}
struct Instance {
    HANDLE process = nullptr;
    HWND window = nullptr;
    ~Instance() {
        if (window && IsWindow(window)) SendMessageW(window, WM_CLOSE, 0, 0);
        if (process) { WaitForSingleObject(process, 5000); CloseHandle(process); }
    }
    void Open(const std::wstring& executable) {
        std::wstring command = L"\"" + executable + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION created{};
        Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
            nullptr, nullptr, &startup, &created) != FALSE, "interface iniciada com mocks");
        process = created.hProcess; CloseHandle(created.hThread);
        Search search{created.dwProcessId};
        const ULONGLONG deadline = GetTickCount64() + 10000;
        while (!search.window && GetTickCount64() < deadline && WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
            EnumWindows(Find, reinterpret_cast<LPARAM>(&search)); Sleep(20);
        }
        window = search.window;
        Check(window != nullptr, "janela nativa criada");
    }
    void Select(int action) {
        // Cada instancia inicia com o foco no primeiro card.
        if (action) SendMessageW(window, WM_KEYDOWN, VK_RIGHT, 0);
    }
    std::wstring Act() {
        SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0);
        Sleep(2020);
        SendMessageW(window, WM_KEYUP, VK_SPACE, 0);
        const ULONGLONG deadline = GetTickCount64() + 8000;
        HWND report = nullptr;
        while (!report && GetTickCount64() < deadline && IsWindow(window)) {
            SendMessageW(window, WM_KEYDOWN, VK_F1, 0);
            report = FindWindowW(L"FSC.Report.Window", nullptr); Sleep(50);
        }
        Check(report != nullptr && WaitForSingleObject(process, 0) == WAIT_TIMEOUT,
            "card conclui e a interface permanece aberta");
        HWND edit = FindWindowExW(report, nullptr, L"EDIT", nullptr);
        Check(edit != nullptr, "F1 mostra o relatorio da operacao");
        const auto length = SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        SendMessageW(edit, WM_GETTEXT, text.size(), reinterpret_cast<LPARAM>(text.data()));
        SendMessageW(report, WM_CLOSE, 0, 0);
        return text;
    }
};
int wmain(int argc, wchar_t** argv) {
    try {
        Check(argc == 2, "pasta exclusiva dos mocks recebida");
        const std::wstring executable = std::wstring(argv[1]) + L"\\FSC.exe";
        SetEnvironmentVariableW(L"FSC_MOCK_CRASH", nullptr);
        SetEnvironmentVariableW(L"FSC_MOCK_RESULT", L"0");
        {
            Instance app; app.Open(executable);
            SendMessageW(app.window, WM_APP + 1, 0, 0x1234);
            Check(IsWindow(app.window), "notificacao sem resultado pendente e ignorada com seguranca");
            // Soltar antes de dois segundos nao deve produzir relatorio.
            SendMessageW(app.window, WM_KEYDOWN, VK_SPACE, 0); Sleep(80);
            SendMessageW(app.window, WM_KEYUP, VK_SPACE, 0); Sleep(2200);
            SendMessageW(app.window, WM_KEYDOWN, VK_F1, 0);
            Check(FindWindowW(L"FSC.Report.Window", nullptr) == nullptr,
                "hold curto nao executa operacao pela interface");
            for (int action = 0; action < 3; ++action) {
                app.Select(action);
                auto report = app.Act();
                Check(report.find(L"Relatorio simulado") != std::wstring::npos &&
                    report.find(L"0xC0000005") == std::wstring::npos,
                    "interface chama card e recebe relatorio com TLS nao trivial");
            }
            Check(SetWindowPos(app.window, nullptr, 0, 0, 420, 860,
                SWP_NOMOVE | SWP_NOZORDER) != FALSE, "layout estreito redimensionado sem fechar");
        }
        SetEnvironmentVariableW(L"FSC_MOCK_CRASH", L"1");
        {
            Instance app; app.Open(executable);
            for (int action = 0; action < 3; ++action) {
                app.Select(action);
                auto report = app.Act();
                Check(report.find(L"0xC0000005") != std::wstring::npos,
                    "crash nativo do card aparece no F1 sem fechar a interface");
            }
        }
        SetEnvironmentVariableW(L"FSC_MOCK_CRASH", nullptr);
        std::cout << "TOTAL GUI: " << passed << " verificacoes passaram\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL GUI: " << error.what() << '\n'; return 1;
    }
}

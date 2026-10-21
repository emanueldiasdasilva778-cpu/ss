#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <map>
#include <mutex>
#include <optional>
#include <cwchar>
#include <vector>
#include <string>
#include <thread>
#include <utility>
#include <shellapi.h>
#include "../common/Diagnostics.h"

#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace fsc {
constexpr double HoldDuration = 2000.0;
constexpr UINT CompleteMessage = WM_APP + 1;
constexpr UINT_PTR AnimationTimer = 1;
constexpr wchar_t WindowClass[] = L"FSC.Native.Window";
constexpr std::array<const wchar_t*, 3> Names{L"Limpeza", L"Registro", L"Arquivos"};
constexpr std::array<const wchar_t*, 3> Libraries{L"Limpeza.dll", L"Registro.dll", L"arquivos.dll"};

double Now() {
    static const double frequency = [] {
        LARGE_INTEGER value{}; QueryPerformanceFrequency(&value);
        return static_cast<double>(value.QuadPart);
    }();
    LARGE_INTEGER ticks{}; QueryPerformanceCounter(&ticks);
    return static_cast<double>(ticks.QuadPart) * 1000.0 / frequency;
}
enum class Input { None, Mouse, Keyboard };
struct Hold {
    int card = -1;
    double started = 0;
    Input input = Input::None;
    void Begin(int id, double time, Input source) { card = id; started = time; input = source; }
    void Cancel() { card = -1; started = 0; input = Input::None; }
    float Progress(double time) const {
        return card < 0 ? 0.0f : static_cast<float>(std::clamp((time - started) / HoldDuration, 0.0, 1.0));
    }
    int TakeReady(double time) {
        if (card < 0 || time - started < HoldDuration) return -1;
        int result = card; Cancel(); return result;
    }
};
struct Layout {
    std::array<D2D1_RECT_F, 3> cards{};
    float titleY = 0;
};
Layout Arrange(float width, float height) {
    Layout result;
    if (width >= 760.0f) {
        const float gap = 24.0f;
        const float size = std::max(96.0f, std::min({232.0f, (width - 112.0f - 2 * gap) / 3,
            height - 258.0f}));
        const float start = (width - 3 * size - 2 * gap) / 2;
        const float top = std::max(154.0f, (height - size) / 2 + 16);
        for (int i = 0; i < 3; ++i) {
            float left = start + static_cast<float>(i) * (size + gap);
            result.cards[i] = D2D1::RectF(left, top, left + size, top + size);
        }
        result.titleY = 68;
    } else {
        const float gap = 18;
        const float size = std::max(80.0f, std::min({186.0f, width - 64,
            (height - 204 - 2 * gap) / 3}));
        const float left = (width - size) / 2;
        for (int i = 0; i < 3; ++i) {
            float top = 124 + static_cast<float>(i) * (size + gap);
            result.cards[i] = D2D1::RectF(left, top, left + size, top + size);
        }
        result.titleY = 42;
    }
    return result;
}
bool Contains(const D2D1_RECT_F& r, float x, float y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}
struct ActionResult {
    bool success = false;
    DWORD code = ERROR_GEN_FAILURE;
    std::wstring message;
    std::wstring details;
    ActionResult() = default;
    ActionResult(bool ok, DWORD error, std::wstring text, std::wstring report = {})
        : success(ok), code(error), message(std::move(text)), details(std::move(report)) {}
};
// A DLL pode registrar destruidores thread_local no runtime. FreeLibrary antes
// da saida da thread deixa esses callbacks apontando para memoria descarregada.
// Os handles permanecem validos ate o encerramento do processo auxiliar.
HMODULE RetainedLibrary(const std::wstring& file) {
    static std::mutex lock;
    static std::map<std::wstring, HMODULE> modules;
    const std::lock_guard<std::mutex> guard(lock);
    auto found = modules.find(file);
    if (found != modules.end()) return found->second;
    HMODULE loaded = LoadLibraryExW(file.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (loaded) modules.emplace(file, loaded);
    return loaded;
}
template<class T> T Export(HMODULE module, const char* name) {
    FARPROC pointer = GetProcAddress(module, name);
    T result{};
    static_assert(sizeof(result) == sizeof(pointer));
    std::memcpy(&result, &pointer, sizeof(result));
    return result;
}
std::wstring ExecutablePath() {
    std::wstring path(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!n || n >= path.size()) return {};
    path.resize(n);
    return path;
}
std::wstring ExecutableDirectory() {
    std::wstring path = ExecutablePath();
    auto separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return {};
    path.resize(separator + 1);
    return path;
}
ActionResult Invoke(int action, const std::wstring& directory) {
    if (action < 0 || action >= 3 || directory.empty())
        return {false, ERROR_INVALID_PARAMETER, L"Ação ou pasta inválida."};
    const std::wstring file = directory +
        ((directory.back() == L'\\' || directory.back() == L'/') ? L"" : L"\\") + Libraries[action];
    const HMODULE module = RetainedLibrary(file);
    if (!module) {
        const DWORD code = GetLastError();
        return {false, code, L"Não foi possível carregar " + std::wstring(Libraries[action]) +
            L" (erro " + std::to_wstring(code) + L").", L"Biblioteca: " + file + L"\r\n" + diagnostics::Explain(code)};
    }
    const auto report = [&] {
        using GetReport = const wchar_t* (*)();
        constexpr std::array<const char*, 3> exports{
            "CleanerGetLastReport", "RegistryGetLastReport", "FilesGetLastReport"};
        auto function = Export<GetReport>(module, exports[action]);
        if (!function) return std::wstring(L"Esta DLL antiga nao oferece relatorio detalhado. Atualize as tres DLLs junto com FSC.exe.");
        const wchar_t* text = function();
        return std::wstring(text ? text : L"Relatorio indisponivel.");
    };
    if (action < 2) {
        using Run = int (*)();
        Run function = Export<Run>(module,
            action == 0 ? "LimparPc" : "LimparRegistroSilencioso");
        if (!function) return {false, ERROR_PROC_NOT_FOUND,
            L"DLL incompatível: " + std::wstring(Libraries[action])};
        const int status = function();
        const auto details = report();
        if (status != 0) return {false, ERROR_PARTIAL_COPY,
            action == 0 ? L"Limpeza com pendências — F1: detalhes" : L"Registro com pendências — F1: detalhes", details};
        return {true, ERROR_SUCCESS, action == 0 ? L"Limpeza concluída" : L"Registro limpo", details};
    }
    using Run = void (*)();
    using Status = DWORD (*)();
    Run copy = Export<Run>(module, "PerformFilesCopy");
    Status status = Export<Status>(module, "FilesGetLastStatus");
    if (!copy || !status) return {false, ERROR_PROC_NOT_FOUND, L"DLL incompatível: arquivos.dll"};
    copy();
    const DWORD code = status(); // A DLL usa status local à thread: consultar nesta mesma thread.
    const auto details = report();
    if (code) return {false, code, L"Cópia incompleta (erro " + std::to_wstring(code) + L") — F1: detalhes", details};
    return {true, ERROR_SUCCESS, L"Arquivos aplicados", details};
}

struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE h = nullptr) : value(h) {}
    ~Handle() { Reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    void Reset(HANDLE h = nullptr) {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = h;
    }
};
constexpr DWORD PacketMagic = 0x46534333;
constexpr DWORD MaxMessage = 512, MaxDetails = 65536;
struct Packet {
    DWORD magic = PacketMagic, version = 1, success = 0, code = ERROR_GEN_FAILURE;
    DWORD messageLength = 0, detailsLength = 0;
};
bool Transfer(HANDLE pipe, void* buffer, DWORD bytes, bool writing) {
    auto data = static_cast<BYTE*>(buffer);
    while (bytes) {
        DWORD transferred = 0;
        const BOOL ok = writing ? WriteFile(pipe, data, bytes, &transferred, nullptr)
            : ReadFile(pipe, data, bytes, &transferred, nullptr);
        if (!ok || !transferred) return false;
        data += transferred; bytes -= transferred;
    }
    return true;
}
bool WriteResult(HANDLE pipe, const ActionResult& result) {
    Packet packet;
    packet.success = result.success ? 1 : 0; packet.code = result.code;
    packet.messageLength = static_cast<DWORD>(std::min<size_t>(result.message.size(), MaxMessage));
    packet.detailsLength = static_cast<DWORD>(std::min<size_t>(result.details.size(), MaxDetails));
    return Transfer(pipe, &packet, sizeof(packet), true) &&
        Transfer(pipe, const_cast<wchar_t*>(result.message.data()), packet.messageLength * sizeof(wchar_t), true) &&
        Transfer(pipe, const_cast<wchar_t*>(result.details.data()), packet.detailsLength * sizeof(wchar_t), true);
}
bool ReadResult(HANDLE pipe, ActionResult& result) {
    Packet packet;
    if (!Transfer(pipe, &packet, sizeof(packet), false) || packet.magic != PacketMagic ||
        packet.version != 1 || packet.success > 1 || packet.messageLength > MaxMessage ||
        packet.detailsLength > MaxDetails || (packet.success && packet.code != ERROR_SUCCESS)) return false;
    ActionResult read;
    read.success = packet.success != 0; read.code = packet.code;
    read.message.resize(packet.messageLength); read.details.resize(packet.detailsLength);
    if (!Transfer(pipe, read.message.data(), packet.messageLength * sizeof(wchar_t), false) ||
        !Transfer(pipe, read.details.data(), packet.detailsLength * sizeof(wchar_t), false)) return false;
    result = std::move(read); return true;
}
LONG WINAPI WorkerException(EXCEPTION_POINTERS*) {
    // A falha encerra somente o auxiliar. Nunca continuar apos uma violacao de acesso.
    return EXCEPTION_EXECUTE_HANDLER;
}
// Retorna -1 para inicializacao normal, ou o codigo de saida do auxiliar.
int WorkerMode() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return ERROR_INVALID_PARAMETER;
    const bool worker = count > 1 && std::wcscmp(arguments[1], L"--fsc-worker") == 0;
    if (!worker) { LocalFree(arguments); return -1; }
    if (count != 4) { LocalFree(arguments); return ERROR_INVALID_PARAMETER; }
    wchar_t* end = nullptr;
    const unsigned long action = std::wcstoul(arguments[2], &end, 10);
    const bool validAction = *arguments[2] && end && !*end && action < 3;
    end = nullptr;
    const unsigned long long number = std::wcstoull(arguments[3], &end, 10);
    const bool validHandle = *arguments[3] && end && !*end && number != 0;
    LocalFree(arguments);
    if (!validAction || !validHandle) return ERROR_INVALID_PARAMETER;
    Handle pipe(reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(number)));
    if (GetFileType(pipe.value) != FILE_TYPE_PIPE) return ERROR_INVALID_HANDLE;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(WorkerException);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool sent = false;
    try {
        ActionResult result = diagnostics::IsElevated()
            ? Invoke(static_cast<int>(action), ExecutableDirectory())
            : ActionResult{false, ERROR_ELEVATION_REQUIRED, L"O auxiliar precisa de elevação administrativa."};
        sent = WriteResult(pipe.value, result);
    } catch (...) {
        ActionResult failed; failed.code = ERROR_UNHANDLED_EXCEPTION;
        sent = WriteResult(pipe.value, failed);
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return sent ? 0 : ERROR_WRITE_FAULT;
}
ActionResult RunIsolated(int action, const std::wstring& executable) {
    if (action < 0 || action >= 3 || executable.empty())
        return {false, ERROR_INVALID_PARAMETER, L"Ação ou executável inválido."};
    auto failure = [](DWORD code) -> ActionResult {
        return {false, code, L"Não foi possível iniciar a operação — F1: detalhes",
            L"Erro " + std::to_wstring(code) + L": " + diagnostics::Explain(code)};
    };
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE read = nullptr, writePipe = nullptr;
    if (!CreatePipe(&read, &writePipe, &security, 0)) return failure(GetLastError());
    Handle reader(read), writer(writePipe);
    if (!SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0)) return failure(GetLastError());
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    if (!bytes) return failure(GetLastError());
    std::vector<BYTE> storage(bytes);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes)) return failure(GetLastError());
    struct AttributeGuard {
        LPPROC_THREAD_ATTRIBUTE_LIST value;
        ~AttributeGuard() { DeleteProcThreadAttributeList(value); }
    } guard{attributes};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        &writer.value, sizeof(writer.value), nullptr, nullptr)) return failure(GetLastError());
    std::wstring command = L"\"" + executable + L"\" --fsc-worker " + std::to_wstring(action) +
        L" " + std::to_wstring(reinterpret_cast<ULONG_PTR>(writer.value));
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW; startup.StartupInfo.wShowWindow = SW_HIDE;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr,
        &startup.StartupInfo, &process)) return failure(GetLastError());
    Handle child(process.hProcess), thread(process.hThread);
    writer.Reset(); // Somente o auxiliar mantem o lado de escrita aberto.
    ActionResult result;
    bool received = false;
    try { received = ReadResult(reader.value, result); }
    catch (...) { received = false; }
    reader.Reset(); // Ler antes de esperar: relatorios grandes podem encher o pipe.
    if (WaitForSingleObject(child.value, INFINITE) != WAIT_OBJECT_0) return failure(GetLastError());
    DWORD code = ERROR_GEN_FAILURE;
    if (!GetExitCodeProcess(child.value, &code)) return failure(GetLastError());
    if (code != 0) {
        wchar_t hex[16]{}; swprintf_s(hex, L"0x%08lX", static_cast<unsigned long>(code));
        return {false, code, L"A DLL foi interrompida — F1: detalhes",
            std::wstring(L"Processo auxiliar encerrado com código ") + hex + L".\r\nBiblioteca: " +
            Libraries[action] + L"\r\nA operação pode ter sido executada parcialmente. Não houve nova tentativa automática."
            L"\r\nA interface permaneceu aberta. Atualize FSC.exe e as três DLLs juntos."};
    }
    if (!received) return {false, ERROR_INVALID_DATA, L"Resposta inválida da operação — F1: detalhes",
        L"O auxiliar não enviou um relatório completo. A operação pode ter sido executada parcialmente."};
    if (result.message.empty()) result.message = L"Falha na operação — F1: detalhes";
    return result;
}

struct ReportView { HWND edit = nullptr; HFONT font = nullptr; };
LRESULT CALLBACK ReportProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto state = reinterpret_cast<ReportView*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_CREATE) {
        auto creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto text = static_cast<const std::wstring*>(creation->lpCreateParams);
        state = new (std::nothrow) ReportView;
        if (!state) return -1;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->edit = CreateWindowExW(0, L"EDIT", text->c_str(),
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL, 12, 12, 100, 100, window, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        if (!state->edit) return -1;
        state->font = CreateFontW(-MulDiv(14, static_cast<int>(GetDpiForWindow(window)), 96),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        if (state->font) SendMessageW(state->edit, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
        RECT r{}; GetClientRect(window, &r);
        MoveWindow(state->edit, 12, 12, std::max(1L, r.right - 24), std::max(1L, r.bottom - 24), TRUE);
        SetFocus(state->edit); return 0;
    }
    if (message == WM_SIZE && state && state->edit) {
        MoveWindow(state->edit, 12, 12, std::max(1, static_cast<int>(LOWORD(lparam)) - 24),
            std::max(1, static_cast<int>(HIWORD(lparam)) - 24), TRUE); return 0;
    }
    if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC) {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, RGB(225, 225, 225)); SetBkColor(dc, RGB(0, 0, 0));
        return reinterpret_cast<LRESULT>(GetStockObject(BLACK_BRUSH));
    }
    if (message == WM_NCDESTROY && state) {
        if (state->font) DeleteObject(state->font);
        delete state; SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
void ShowReport(HWND owner, const std::wstring& text) {
    static const ATOM type = [] {
        WNDCLASSEXW c{}; c.cbSize = sizeof(c); c.lpfnWndProc = ReportProcedure;
        c.hInstance = GetModuleHandleW(nullptr); c.lpszClassName = L"FSC.Report.Window";
        c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        c.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        return RegisterClassExW(&c);
    }();
    if (!type) return;
    HWND report = CreateWindowExW(WS_EX_APPWINDOW, L"FSC.Report.Window", L"FSC — Detalhes da operação",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 860, 600, owner, nullptr,
        GetModuleHandleW(nullptr), const_cast<std::wstring*>(&text));
    if (report) ShowWindow(report, SW_SHOW);
}

template<class T> void Release(T*& object) { if (object) { object->Release(); object = nullptr; } }
struct Completion { int card = -1; ActionResult result; };

class App {
public:
    HWND window = nullptr;
    ~App() {
        if (worker.joinable()) worker.join();
        DiscardTarget();
        Release(title); Release(label); Release(percent); Release(footer); Release(write); Release(factory);
    }
    bool Initialize() {
        dpi = static_cast<float>(GetDpiForWindow(window));
        if (!dpi) dpi = 96;
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory))) return false;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&write)))) return false;
        std::wstring family = L"Segoe UI";
        IDWriteFontCollection* collection = nullptr;
        if (SUCCEEDED(write->GetSystemFontCollection(&collection))) {
            for (const wchar_t* candidate : {L"Inter", L"Space Grotesk", L"Segoe UI Variable Display",
                L"Segoe UI", L"Arial", L"Liberation Sans", L"DejaVu Sans"}) {
                UINT32 index = 0; BOOL exists = FALSE;
                if (SUCCEEDED(collection->FindFamilyName(candidate, &index, &exists)) && exists) {
                    family = candidate; break;
                }
            }
            collection->Release();
        }
        auto font = [this, &family](float size, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** out) {
            HRESULT hr = write->CreateTextFormat(family.c_str(), nullptr, weight,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"pt-BR", out);
            if (SUCCEEDED(hr)) {
                (*out)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                (*out)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                (*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
            return SUCCEEDED(hr);
        };
        return font(38, DWRITE_FONT_WEIGHT_SEMI_BOLD, &title) &&
            font(16, DWRITE_FONT_WEIGHT_MEDIUM, &label) &&
            font(27, DWRITE_FONT_WEIGHT_LIGHT, &percent) &&
            font(12, DWRITE_FONT_WEIGHT_NORMAL, &footer);
    }
    LRESULT Handle(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_CREATE:
            return Initialize() ? 0 : -1;
        case WM_ERASEBKGND: return 1;
        case WM_NCCALCSIZE: return 0;
        case WM_NCACTIVATE: return TRUE;
        case WM_NCPAINT: return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; BeginPaint(window, &ps); Paint(); EndPaint(window, &ps); return 0;
        }
        case WM_SIZE:
            CancelHold();
            if (target && wparam != SIZE_MINIMIZED) target->Resize(D2D1::SizeU(LOWORD(lparam), HIWORD(lparam)));
            Redraw(); return 0;
        case WM_DPICHANGED: {
            dpi = static_cast<float>(HIWORD(wparam));
            if (target) target->SetDpi(dpi, dpi);
            auto rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(window, nullptr, rect->left, rect->top,
                rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto info = reinterpret_cast<MINMAXINFO*>(lparam);
            UINT currentDpi = window ? GetDpiForWindow(window) : 96;
            info->ptMinTrackSize = {MulDiv(360, static_cast<int>(currentDpi), 96),
                MulDiv(560, static_cast<int>(currentDpi), 96)};
            MONITORINFO monitor{sizeof(MONITORINFO), {}, {}, 0};
            if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
                info->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left,
                    monitor.rcWork.top - monitor.rcMonitor.top};
                info->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left,
                    monitor.rcWork.bottom - monitor.rcWork.top};
            }
            return 0;
        }
        case WM_NCHITTEST: return HitFrame(lparam);
        case WM_MOUSEMOVE: {
            const float x = static_cast<float>(GET_X_LPARAM(lparam)) / Scale();
            const float y = static_cast<float>(GET_Y_LPARAM(lparam)) / Scale();
            if (!trackingMouse) {
                TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window, 0};
                TrackMouseEvent(&track); trackingMouse = true;
            }
            const int hit = HitCard(x, y);
            const int control = HitControl(x, y);
            if (hit != hover || control != hoverControl) {
                hover = hit; hoverControl = control; Redraw();
            }
            if (hold.input == Input::Mouse && hit != hold.card) CancelHold();
            return 0;
        }
        case WM_MOUSELEAVE:
            trackingMouse = false; hover = -1; hoverControl = -1;
            CancelHold(); Redraw(); return 0;
        case WM_LBUTTONDOWN: {
            const float x = static_cast<float>(GET_X_LPARAM(lparam)) / Scale();
            const float y = static_cast<float>(GET_Y_LPARAM(lparam)) / Scale();
            const int control = HitControl(x, y);
            if (control == 0) { ShowWindow(window, SW_MINIMIZE); return 0; }
            if (control == 1) { SendMessageW(window, WM_CLOSE, 0, 0); return 0; }
            const int hit = HitCard(x, y);
            if (hit >= 0 && running < 0) {
                SetFocus(window); focus = hit; keyboardFocus = false;
                hold.Begin(hit, Now(), Input::Mouse); SetCapture(window); Animate(); Redraw();
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (hold.input == Input::Mouse) {
                int ready = hold.TakeReady(Now());
                CancelHold(true);
                if (ready >= 0) Start(ready);
            }
            return 0;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        case WM_KILLFOCUS: CancelHold(); return 0;
        case WM_KEYDOWN:
            if (wparam == VK_F1) {
                CancelHold();
                if (!lastDetails.empty()) ShowReport(window, lastDetails);
                else SetFooter(L"Execute uma operação para consultar os detalhes.", 4000);
                return 0;
            }
            if (wparam == VK_ESCAPE) { SendMessageW(window, WM_CLOSE, 0, 0); return 0; }
            if (wparam == VK_TAB || wparam == VK_LEFT || wparam == VK_RIGHT ||
                wparam == VK_UP || wparam == VK_DOWN) {
                CancelHold();
                const bool backwards = wparam == VK_LEFT || wparam == VK_UP ||
                    (wparam == VK_TAB && (GetKeyState(VK_SHIFT) & 0x8000));
                focus = (focus + (backwards ? 2 : 1)) % 3;
                keyboardFocus = true; Redraw(); return 0;
            }
            if ((wparam == VK_SPACE || wparam == VK_RETURN) && !(lparam & (1LL << 30)) &&
                running < 0 && hold.card < 0) {
                keyboardFocus = true; heldKey = static_cast<UINT>(wparam);
                hold.Begin(focus, Now(), Input::Keyboard); Animate(); Redraw();
            }
            return 0;
        case WM_KEYUP:
            if (hold.input == Input::Keyboard && wparam == heldKey) {
                int ready = hold.TakeReady(Now()); CancelHold(); if (ready >= 0) Start(ready);
            }
            return 0;
        case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, (hover >= 0 && running < 0) || hoverControl >= 0
                    ? IDC_HAND : IDC_ARROW)); return TRUE;
            }
            break;
        case WM_TIMER:
            if (wparam == AnimationTimer) Tick();
            return 0;
        case CompleteMessage: AcceptCompletion(); return 0;
        case WM_CLOSE:
            CancelHold();
            if (running >= 0) { SetFooter(L"Aguarde a operação terminar.", 8000); return 0; }
            DestroyWindow(window); return 0;
        case WM_DESTROY:
            KillTimer(window, AnimationTimer); PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

private:
    ID2D1Factory* factory = nullptr;
    IDWriteFactory* write = nullptr;
    ID2D1HwndRenderTarget* target = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
    IDWriteTextFormat *title = nullptr, *label = nullptr, *percent = nullptr, *footer = nullptr;
    float dpi = 96;
    Hold hold;
    std::thread worker;
    std::mutex completionLock;
    std::optional<Completion> pending;
    int running = -1, hover = -1, focus = 0, feedbackCard = -1, hoverControl = -1;
    bool feedbackSuccess = false, keyboardFocus = false, trackingMouse = false;
    UINT heldKey = 0;
    double feedbackTime = 0, footerTime = 0, footerDuration = 0;
    std::wstring footerMessage;
    std::wstring lastDetails;

    float Scale() const { return dpi / 96; }
    D2D1_SIZE_F Size() const {
        RECT rect{}; GetClientRect(window, &rect);
        return D2D1::SizeF(static_cast<float>(rect.right) / Scale(), static_cast<float>(rect.bottom) / Scale());
    }
    int HitCard(float x, float y) const {
        const auto size = Size(); const auto layout = Arrange(size.width, size.height);
        for (int i = 0; i < 3; ++i) if (Contains(layout.cards[i], x, y)) return i;
        return -1;
    }
    int HitControl(float x, float y) const {
        float width = Size().width;
        if (y < 8 || y > 40) return -1;
        if (x >= width - 86 && x < width - 48) return 0;
        if (x >= width - 46 && x <= width - 8) return 1;
        return -1;
    }
    LRESULT HitFrame(LPARAM lparam) const {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(window, &point);
        RECT rect{}; GetClientRect(window, &rect);
        int edge = static_cast<int>(6 * Scale());
        if (!IsZoomed(window)) {
            bool left = point.x < edge, right = point.x >= rect.right - edge;
            bool top = point.y < edge, bottom = point.y >= rect.bottom - edge;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }
        if (point.y < static_cast<int>(42 * Scale()) &&
            HitControl(static_cast<float>(point.x) / Scale(), static_cast<float>(point.y) / Scale()) < 0)
            return HTCAPTION;
        return HTCLIENT;
    }
    void Redraw() { InvalidateRect(window, nullptr, FALSE); }
    void Animate() { SetTimer(window, AnimationTimer, 16, nullptr); }
    void CancelHold(bool forceRelease = false) {
        const bool active = hold.card >= 0;
        const bool mouse = hold.input == Input::Mouse;
        hold.Cancel();
        if ((mouse || forceRelease) && GetCapture() == window) ReleaseCapture();
        if (active) Redraw();
    }
    void SetFooter(const std::wstring& text, double duration) {
        footerMessage = text; footerTime = Now(); footerDuration = duration; Animate(); Redraw();
    }
    void Start(int card) {
        CancelHold(true);
        if (running >= 0) return;
        running = card; feedbackCard = -1;
        lastDetails.clear();
        SetFooter(L"Executando…", 0);
        const auto executable = ExecutablePath();
        try {
            worker = std::thread([this, card, executable] {
                Completion completion; completion.card = card;
                try { completion.result = RunIsolated(card, executable); }
                catch (...) { completion.result.code = ERROR_UNHANDLED_EXCEPTION; }
                {
                    const std::lock_guard<std::mutex> guard(completionLock);
                    pending.emplace(std::move(completion));
                }
                PostMessageW(window, CompleteMessage, 0, 0);
            });
        } catch (...) {
            running = -1; SetFooter(L"Não foi possível iniciar a operação.", 8000);
        }
        Animate(); Redraw();
    }
    void AcceptCompletion() {
        std::optional<Completion> completion;
        {
            const std::lock_guard<std::mutex> guard(completionLock);
            if (!pending) return;
            completion = std::move(pending); pending.reset();
        }
        if (worker.joinable()) worker.join();
        running = -1;
        feedbackCard = completion->card; feedbackSuccess = completion->result.success;
        feedbackTime = Now();
        if (completion->result.message.empty()) completion->result.message = L"Falha na operação — F1: detalhes";
        lastDetails = completion->result.message + L"\r\n\r\n" + completion->result.details;
        SetFooter(completion->result.message, feedbackSuccess ? 5000 : 8000);
        Animate(); Redraw();
    }
    void Tick() {
        AcceptCompletion(); // Tambem conclui se a fila rejeitar a notificacao.
        const double now = Now();
        if (hold.card >= 0) {
            int ready = hold.TakeReady(now);
            if (ready >= 0) Start(ready);
        }
        if (feedbackCard >= 0 && now - feedbackTime > 1150) feedbackCard = -1;
        if (running < 0 && !footerMessage.empty() && now - footerTime > footerDuration + 350)
            footerMessage.clear();
        Redraw();
        if (hold.card < 0 && running < 0 && feedbackCard < 0 && footerMessage.empty())
            KillTimer(window, AnimationTimer);
    }
    void DiscardTarget() { Release(brush); Release(target); }
    bool EnsureTarget() {
        if (target) return true;
        RECT rect{}; GetClientRect(window, &rect);
        if (!rect.right || !rect.bottom) return false;
        HRESULT result = factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN), dpi, dpi),
            D2D1::HwndRenderTargetProperties(window, D2D1::SizeU(
                static_cast<UINT>(rect.right), static_cast<UINT>(rect.bottom))), &target);
        if (FAILED(result)) return false;
        result = target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush);
        if (FAILED(result)) { DiscardTarget(); return false; }
        return true;
    }
    void Ink(float alpha, float gray = 1) { brush->SetColor(D2D1::ColorF(gray, gray, gray, alpha)); }
    void Line(float x1, float y1, float x2, float y2, float alpha = 1, float width = 1.5f) {
        Ink(alpha); target->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush, width);
    }
    void Text(const std::wstring& text, IDWriteTextFormat* font, const D2D1_RECT_F& area, float alpha = 1) {
        Ink(alpha); target->DrawText(text.c_str(), static_cast<UINT32>(text.size()), font,
            area, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void DrawTitle(float width, float top) {
        std::array<float, 3> widths{};
        float total = 24;
        for (int i = 0; i < 3; ++i) {
            IDWriteTextLayout* glyph = nullptr;
            if (SUCCEEDED(write->CreateTextLayout(&L"FSC"[i], 1, title, 100, 64, &glyph))) {
                DWRITE_TEXT_METRICS metrics{}; glyph->GetMetrics(&metrics);
                widths[i] = metrics.width; glyph->Release();
            }
            total += widths[i];
        }
        float x = (width - total) / 2;
        for (int i = 0; i < 3; ++i) {
            Text(std::wstring(1, L"FSC"[i]), title, D2D1::RectF(x - 1, top, x + widths[i] + 1, top + 54));
            x += widths[i] + 12;
        }
    }
    void Border(const D2D1_RECT_F& r, float progress, float alpha = 1, float thickness = 2) {
        const float w = r.right - r.left, h = r.bottom - r.top;
        float remaining = std::clamp(progress, 0.0f, 1.0f) * (2 * w + 2 * h);
        const std::array<D2D1_POINT_2F, 5> points{
            D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.right, r.top),
            D2D1::Point2F(r.right, r.bottom), D2D1::Point2F(r.left, r.bottom), D2D1::Point2F(r.left, r.top)};
        for (size_t i = 0; i < 4 && remaining > 0; ++i) {
            float length = (i % 2 == 0) ? w : h;
            float amount = std::min(remaining, length) / length;
            auto end = D2D1::Point2F(points[i].x + (points[i + 1].x - points[i].x) * amount,
                points[i].y + (points[i + 1].y - points[i].y) * amount);
            Line(points[i].x, points[i].y, end.x, end.y, alpha, thickness);
            remaining -= length;
        }
    }
    void Icon(int id, float x, float y, float alpha) {
        if (id == 0) {
            Line(x + 3, y - 14, x - 5, y + 1, alpha);
            Line(x - 10, y - 1, x + 4, y + 6, alpha);
            Line(x - 10, y - 1, x - 15, y + 10, alpha);
            Line(x - 15, y + 10, x, y + 16, alpha);
            Line(x, y + 16, x + 4, y + 6, alpha);
            Line(x - 7, y + 3, x - 11, y + 11, alpha, 1);
            Line(x - 2, y + 5, x - 6, y + 13, alpha, 1);
            Line(x + 10, y - 6, x + 10, y, alpha, 1);
            Line(x + 7, y - 3, x + 13, y - 3, alpha, 1);
        } else if (id == 1) {
            for (int i = 0; i < 3; ++i) {
                float yy = y - 10 + static_cast<float>(i) * 10;
                float knob = x + (i == 1 ? 6 : -5);
                Line(x - 14, yy, knob - 3, yy, alpha);
                Line(knob + 3, yy, x + 14, yy, alpha);
                Ink(alpha); target->DrawRectangle(D2D1::RectF(knob - 3, yy - 3, knob + 3, yy + 3), brush, 1.5f);
            }
        } else {
            Line(x - 15, y - 10, x - 5, y - 10, alpha);
            Line(x - 5, y - 10, x - 1, y - 6, alpha);
            Line(x - 1, y - 6, x + 15, y - 6, alpha);
            Line(x + 15, y - 6, x + 15, y + 13, alpha);
            Line(x + 15, y + 13, x - 15, y + 13, alpha);
            Line(x - 15, y + 13, x - 15, y - 10, alpha);
            Line(x - 10, y, x + 10, y, alpha, 1);
        }
    }
    void Paint() {
        if (!factory || !EnsureTarget()) return;
        const auto size = target->GetSize();
        const auto layout = Arrange(size.width, size.height);
        const double now = Now();
        target->BeginDraw(); target->Clear(D2D1::ColorF(0, 0, 0, 1));
        Ink(0.07f);
        for (float y = 14; y < size.height; y += 24)
            for (float x = 14; x < size.width; x += 24)
                target->FillRectangle(D2D1::RectF(x, y, x + 1, y + 1), brush);
        DrawTitle(size.width, layout.titleY);
        for (int i = 0; i < 3; ++i) {
            const auto r = layout.cards[i];
            const float x = (r.left + r.right) / 2, y = (r.top + r.bottom) / 2;
            const bool pressed = hold.card == i, active = running == i, feedback = feedbackCard == i;
            const float opacity = running >= 0 && !active ? 0.36f : 1;
            Ink(1, 0.012f); target->FillRectangle(r, brush);
            Ink((hover == i || (keyboardFocus && focus == i)) && running < 0 ? 0.45f : 0.19f);
            target->DrawRectangle(r, brush, 1);
            if (pressed) {
                const float progress = hold.Progress(now);
                Border(r, progress);
                const int number = std::min(100, static_cast<int>(progress * 100));
                Text(std::to_wstring(number) + L"%", percent, D2D1::RectF(r.left, y - 43, r.right, y + 9), 0.86f);
            } else if (active) {
                const float t = static_cast<float>(std::fmod(now, 1100.0) / 1100.0);
                for (int p = 0; p < 20; ++p) {
                    const float a = (t + static_cast<float>(p) / 80) * 6.283185307f;
                    Ink(static_cast<float>(p + 1) / 20 * 0.85f);
                    target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 11 * std::cos(a),
                        y - 17 + 11 * std::sin(a)), 1.1f, 1.1f), brush);
                }
                Border(r, 1, 0.55f, 1);
            } else if (feedback) {
                const float fade = std::clamp(1 - static_cast<float>((now - feedbackTime) / 1150), 0.0f, 1.0f);
                if (feedbackSuccess) {
                    Ink(0.15f * std::exp(-static_cast<float>(now - feedbackTime) / 160));
                    target->FillRectangle(r, brush);
                    Line(x - 11, y - 17, x - 3, y - 9, fade, 2);
                    Line(x - 3, y - 9, x + 13, y - 25, fade, 2);
                } else {
                    Line(x - 8, y - 25, x + 8, y - 9, fade);
                    Line(x + 8, y - 25, x - 8, y - 9, fade);
                }
                Border(r, 1, fade, 1.5f);
                Icon(i, x, y - 17, 0.85f * (1 - fade));
            } else Icon(i, x, y - 17, 0.85f * opacity);
            Text(Names[i], label, D2D1::RectF(r.left + 8, y + 18, r.right - 8, y + 45), 0.92f * opacity);
        }
        if (hoverControl >= 0) {
            const float right = size.width - (hoverControl == 0 ? 48 : 8);
            Ink(0.075f); target->FillRectangle(D2D1::RectF(right - 38, 8, right, 40), brush);
        }
        Line(size.width - 73, 26, size.width - 61, 26, 0.65f, 1);
        Line(size.width - 32, 19, size.width - 22, 29, 0.65f, 1);
        Line(size.width - 22, 19, size.width - 32, 29, 0.65f, 1);
        if (!footerMessage.empty()) {
            const float alpha = running >= 0 ? 0.55f : 0.7f *
                std::clamp(1 - static_cast<float>((now - footerTime - footerDuration) / 350), 0.0f, 1.0f);
            Text(footerMessage, footer, D2D1::RectF(22, size.height - 58, size.width - 22, size.height - 20), alpha);
        }
        if (target->EndDraw() == static_cast<HRESULT>(D2DERR_RECREATE_TARGET)) { DiscardTarget(); Redraw(); }
    }
};

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->Handle(message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
}
} // namespace fsc

#ifndef FSC_TESTING
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    const int workerResult = fsc::WorkerMode();
    if (workerResult >= 0) return workerResult;
    if (!diagnostics::IsElevated()) {
        std::wstring path(32768, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!length || length >= path.size()) return 1;
        path.resize(length);
        SHELLEXECUTEINFOW elevation{}; elevation.cbSize = sizeof(elevation);
        elevation.lpVerb = L"runas"; elevation.lpFile = path.c_str(); elevation.nShow = show;
        if (!ShellExecuteExW(&elevation)) MessageBoxW(nullptr,
            L"FSC precisa da elevacao aprovada no UAC para iniciar. Nenhuma operacao foi executada.",
            L"FSC", MB_OK | MB_ICONWARNING);
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 1;
    {
        fsc::App app;
        WNDCLASSEXW type{}; type.cbSize = sizeof(type);
        type.hInstance = instance; type.lpfnWndProc = fsc::WindowProcedure;
        type.lpszClassName = fsc::WindowClass; type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        type.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        type.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        if (RegisterClassExW(&type)) {
            const int dpi = static_cast<int>(GetDpiForSystem());
            HWND window = CreateWindowExW(WS_EX_APPWINDOW, fsc::WindowClass, L"FSC",
                WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(1080, dpi, 96), MulDiv(680, dpi, 96),
                nullptr, nullptr, instance, &app);
            if (window) {
                ShowWindow(window, show); UpdateWindow(window);
                MSG message{};
                while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                    TranslateMessage(&message); DispatchMessageW(&message);
                }
                result = static_cast<int>(message.wParam);
            } else MessageBoxW(nullptr, L"Não foi possível inicializar a interface gráfica.", L"FSC", MB_OK | MB_ICONERROR);
        }
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
#endif

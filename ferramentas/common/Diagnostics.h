#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

namespace diagnostics {
inline DWORD CheckCloudReparse(const std::wstring& extendedPath) {
    HANDLE handle = CreateFileW(extendedPath.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return GetLastError();
    FILE_ATTRIBUTE_TAG_INFO info{};
    BOOL ok = GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info));
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (error) return error;
    return (info.ReparseTag & 0xFFFF0FFF) == 0x9000001A ? ERROR_SUCCESS : ERROR_NOT_SUPPORTED;
}
inline bool IsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}
inline std::wstring Explain(DWORD code) {
    wchar_t* message = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
        reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    if (!n) return L"Consulte o codigo retornado pelo Windows.";
    std::wstring text(message, n);
    LocalFree(message);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) text.pop_back();
    return text;
}
struct Report {
    unsigned total = 0, occupied = 0, denied = 0;
    std::wstring lines;
    void Reset() { total = occupied = denied = 0; lines.clear(); }
    void Add(DWORD code, const std::wstring& path) {
        ++total;
        if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION) ++occupied;
        if (code == ERROR_ACCESS_DENIED || code == ERROR_PRIVILEGE_NOT_HELD ||
            code == ERROR_ELEVATION_REQUIRED) ++denied;
        if (total <= 64 && lines.size() < 28000) {
            lines += L"\r\n[" + std::to_wstring(code) + L"] " + path + L"\r\n" + Explain(code) + L"\r\n";
        }
    }
    std::wstring Finish(const std::wstring& summary) const {
        std::wstring text = summary + L"\r\nProcesso elevado: " +
            (IsElevated() ? L"sim" : L"nao") + L"\r\nPendencias: " + std::to_wstring(total) +
            L"\r\nArquivos em uso: " + std::to_wstring(occupied) +
            L"\r\nAcessos negados: " + std::to_wstring(denied) + L"\r\n" + lines;
        if (total > 64) text += L"\r\nDemais ocorrencias omitidas para limitar o tamanho do relatorio.\r\n";
        if (occupied) text += L"\r\nFeche os programas que usam os caminhos indicados e tente novamente.\r\n";
        if (denied) text += L"\r\nA elevacao de administrador nao substitui as ACLs e protecoes do Windows.\r\n";
        return text;
    }
};
}

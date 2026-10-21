#define FILES_BUILD
#include "Files.h"
#include <shlobj.h>
#include <string>
#include <vector>

namespace {
thread_local DWORD ultimoStatus = ERROR_SUCCESS;
volatile LONG sequencia = 0;

struct Relatorio {
    unsigned long long arquivos = 0, pastas = 0, falhas = 0;
    DWORD primeiroErro = ERROR_SUCCESS;
    std::wstring primeiroCaminho;
    void erro(DWORD codigo, const std::wstring& caminho) {
        ++falhas;
        if (!primeiroErro) {
            primeiroErro = codigo ? codigo : ERROR_GEN_FAILURE;
            primeiroCaminho = caminho;
        }
    }
};
struct Handle {
    HANDLE valor;
    explicit Handle(HANDLE h) : valor(h) {}
    ~Handle() { if (valor != INVALID_HANDLE_VALUE) CloseHandle(valor); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
std::wstring unir(const std::wstring& base, const std::wstring& nome) {
    if (base.empty()) return {};
    return base + (base.back() == L'\\' ? L"" : L"\\") + nome;
}
std::wstring absoluta(const std::wstring& caminho) {
    if (caminho.empty()) return {};
    DWORD n = GetFullPathNameW(caminho.c_str(), 0, nullptr, nullptr);
    if (!n) return {};
    std::vector<wchar_t> texto(n + 1);
    DWORD usado = GetFullPathNameW(caminho.c_str(), static_cast<DWORD>(texto.size()),
        texto.data(), nullptr);
    if (!usado || usado >= texto.size()) return {};
    std::wstring resultado(texto.data(), usado);
    while (resultado.size() > 3 && resultado.back() == L'\\') resultado.pop_back();
    if (resultado.size() < 3 || resultado[1] != L':' || resultado[2] != L'\\') return {};
    return resultado;
}
std::wstring longo(const std::wstring& caminho) { return L"\\\\?\\" + caminho; }
std::wstring conhecida(REFKNOWNFOLDERID id) {
    PWSTR texto = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &texto))) return {};
    std::wstring resultado(texto);
    CoTaskMemFree(texto);
    return absoluta(resultado);
}
bool igual(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
bool dentro(const std::wstring& filho, const std::wstring& pai) {
    return !pai.empty() && filho.size() >= pai.size() &&
        igual(filho.substr(0, pai.size()), pai) &&
        (filho.size() == pai.size() || filho[pai.size()] == L'\\');
}
// OneDrive usa reparse points para arquivos sob demanda. Permite tags CLOUD;
// recusa links simbolicos, junctions e tags desconhecidas.
DWORD verificarItem(const std::wstring& caminho, DWORD& atributos) {
    atributos = GetFileAttributesW(longo(caminho).c_str());
    if (atributos == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if (!(atributos & FILE_ATTRIBUTE_REPARSE_POINT)) return ERROR_SUCCESS;
    Handle arquivo(CreateFileW(longo(caminho).c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (arquivo.valor == INVALID_HANDLE_VALUE) return GetLastError();
    FILE_ATTRIBUTE_TAG_INFO info{};
    if (!GetFileInformationByHandleEx(arquivo.valor, FileAttributeTagInfo, &info, sizeof(info)))
        return GetLastError();
    constexpr DWORD cloudMask = 0xFFFF0FFF;
    constexpr DWORD cloudTag = 0x9000001A;
    return (info.ReparseTag & cloudMask) == cloudTag
        ? ERROR_SUCCESS : ERROR_NOT_SUPPORTED;
}
DWORD verificarCaminho(const std::wstring& caminho) {
    if (caminho.size() <= 3) return ERROR_BAD_PATHNAME;
    for (size_t pos = 3;;) {
        size_t fim = caminho.find(L'\\', pos);
        DWORD atributos = 0;
        DWORD erro = verificarItem(caminho.substr(0, fim), atributos);
        if (erro) return erro;
        if (fim == std::wstring::npos) return ERROR_SUCCESS;
        if (!(atributos & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
        pos = fim + 1;
    }
}
DWORD criarPastas(const std::wstring& caminho, Relatorio& r) {
    if (caminho.size() <= 3) return ERROR_BAD_PATHNAME;
    for (size_t pos = 3;;) {
        size_t fim = caminho.find(L'\\', pos);
        auto parte = caminho.substr(0, fim);
        DWORD atributos = 0;
        DWORD erro = verificarItem(parte, atributos);
        if (erro == ERROR_FILE_NOT_FOUND || erro == ERROR_PATH_NOT_FOUND) {
            if (CreateDirectoryW(longo(parte).c_str(), nullptr)) ++r.pastas;
            else if (GetLastError() != ERROR_ALREADY_EXISTS) return GetLastError();
            erro = verificarItem(parte, atributos);
        }
        if (erro) return erro;
        if (!(atributos & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
        if (fim == std::wstring::npos) return ERROR_SUCCESS;
        pos = fim + 1;
    }
}
void descartarTemporario(const std::wstring& temporario) {
    auto caminho = longo(temporario);
    DWORD atributos = GetFileAttributesW(caminho.c_str());
    if (atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_READONLY))
        SetFileAttributesW(caminho.c_str(), atributos & ~FILE_ATTRIBUTE_READONLY);
    DeleteFileW(caminho.c_str());
}
void copiarArquivo(const std::wstring& origem, const std::wstring& destino, Relatorio& r) {
    DWORD erro = verificarCaminho(origem);
    if (erro) { r.erro(erro, origem); return; }
    DWORD atributos = 0;
    erro = verificarItem(origem, atributos);
    if (erro || (atributos & FILE_ATTRIBUTE_DIRECTORY)) {
        r.erro(erro ? erro : ERROR_DIRECTORY, origem); return;
    }
    auto pos = destino.find_last_of(L'\\');
    if (pos == std::wstring::npos || pos < 3) { r.erro(ERROR_BAD_PATHNAME, destino); return; }
    auto pasta = destino.substr(0, pos);
    erro = criarPastas(pasta, r);
    if (erro) { r.erro(erro, pasta); return; }
    erro = verificarItem(destino, atributos);
    if (erro != ERROR_FILE_NOT_FOUND && erro != ERROR_PATH_NOT_FOUND && erro != ERROR_SUCCESS) {
        r.erro(erro, destino); return;
    }
    if (!erro && (atributos & FILE_ATTRIBUTE_DIRECTORY)) { r.erro(ERROR_DIRECTORY, destino); return; }
    std::wstring temporario;
    bool copiado = false;
    for (unsigned tentativa = 0; tentativa < 32; ++tentativa) {
        auto numero = static_cast<unsigned long>(InterlockedIncrement(&sequencia));
        temporario = unir(pasta, L".files-copy-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(numero) + L".tmp");
        if (CopyFileExW(longo(origem).c_str(), longo(temporario).c_str(), nullptr,
            nullptr, nullptr, COPY_FILE_FAIL_IF_EXISTS)) { copiado = true; break; }
        erro = GetLastError();
        if (erro == ERROR_FILE_EXISTS || erro == ERROR_ALREADY_EXISTS) continue;
        // Um arquivo preexistente nunca e sobrescrito durante a preparacao.
        r.erro(erro, origem); return;
    }
    if (!copiado) { r.erro(ERROR_FILE_EXISTS, destino); return; }
    if (!MoveFileExW(longo(temporario).c_str(), longo(destino).c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        erro = GetLastError();
        descartarTemporario(temporario);
        r.erro(erro, destino); return;
    }
    ++r.arquivos;
}
void copiarArvore(const std::wstring& origem, const std::wstring& destino,
                 Relatorio& r, unsigned nivel = 0) {
    if (nivel > 128 || dentro(destino, origem) || dentro(origem, destino)) {
        r.erro(ERROR_BAD_PATHNAME, destino); return;
    }
    DWORD erro = verificarCaminho(origem), atributos = 0;
    if (!erro) erro = verificarItem(origem, atributos);
    if (erro || !(atributos & FILE_ATTRIBUTE_DIRECTORY)) {
        r.erro(erro ? erro : ERROR_DIRECTORY, origem); return;
    }
    erro = criarPastas(destino, r);
    if (erro) { r.erro(erro, destino); return; }
    WIN32_FIND_DATAW dados{};
    HANDLE busca = FindFirstFileW(unir(longo(origem), L"*").c_str(), &dados);
    if (busca == INVALID_HANDLE_VALUE) {
        erro = GetLastError();
        if (erro != ERROR_FILE_NOT_FOUND) r.erro(erro, origem);
        return;
    }
    std::vector<std::wstring> nomes;
    do {
        if (igual(dados.cFileName, L".") || igual(dados.cFileName, L"..")) continue;
        nomes.emplace_back(dados.cFileName);
    } while (FindNextFileW(busca, &dados));
    erro = GetLastError();
    FindClose(busca);
    if (erro != ERROR_NO_MORE_FILES) { r.erro(erro, origem); return; }
    for (const auto& nome : nomes) {
        auto de = unir(origem, nome), para = unir(destino, nome);
        erro = verificarItem(de, atributos);
        if (erro) { r.erro(erro, de); continue; }
        if (atributos & FILE_ATTRIBUTE_DIRECTORY) copiarArvore(de, para, r, nivel + 1);
        else copiarArquivo(de, para, r);
    }
}
Relatorio executar() {
    Relatorio r;
    auto perfil = conhecida(FOLDERID_Profile);
    auto local = conhecida(FOLDERID_LocalAppData);
    auto roaming = conhecida(FOLDERID_RoamingAppData);
    if (perfil.empty() || local.empty() || roaming.empty()) {
        r.erro(ERROR_BAD_PATHNAME, L"Pastas do usuario indisponiveis"); return r;
    }
    copiarArvore(unir(perfil, L"OneDrive\\Documentos\\fudi o luckyy"),
        unir(local, L"FiveM\\FiveM.app\\mods"), r);
    copiarArquivo(unir(perfil, L"OneDrive\\Documentos\\f\\gta5_settings.xml"),
        unir(roaming, L"CitizenFX\\gta5_settings.xml"), r);
    return r;
}
void iniciar(bool mostrar) noexcept {
    try {
        Relatorio r = executar();
        ultimoStatus = r.primeiroErro;
        if (mostrar) {
            std::wstring texto = r.falhas ? L"Copia incompleta.\n" : L"Arquivos aplicados.\n";
            texto += L"Arquivos copiados: " + std::to_wstring(r.arquivos);
            texto += L"\nPastas criadas: " + std::to_wstring(r.pastas);
            texto += L"\nFalhas: " + std::to_wstring(r.falhas);
            if (r.falhas) texto += L"\nPrimeiro erro Windows: " + std::to_wstring(r.primeiroErro) +
                L"\n" + r.primeiroCaminho;
            MessageBoxW(nullptr, texto.c_str(), L"Files.dll",
                MB_OK | (r.falhas ? MB_ICONWARNING : MB_ICONINFORMATION));
        }
    } catch (...) {
        ultimoStatus = ERROR_UNHANDLED_EXCEPTION;
        if (mostrar) MessageBoxW(nullptr, L"Erro inesperado. A copia pode ter sido parcial.",
            L"Files.dll", MB_OK | MB_ICONERROR);
    }
}
}

void PerformFilesCopy() { iniciar(false); }
void CALLBACK AplicarArquivos(HWND, HINSTANCE, LPSTR, int) { iniciar(true); }
DWORD FilesGetLastStatus() { return ultimoStatus; }

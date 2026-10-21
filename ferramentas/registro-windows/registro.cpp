#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <string>
#include <vector>

namespace {
struct Chave {
    HKEY valor = nullptr;
    Chave() = default;
    Chave(const Chave&) = delete;
    Chave& operator=(const Chave&) = delete;
    ~Chave() { if (valor) RegCloseKey(valor); }
};
struct Resultado {
    unsigned removidos = 0, preservados = 0, chaves = 0, falhas = 0, semAcesso = 0;
    void erro(LSTATUS codigo) {
        ++falhas;
        if (codigo == ERROR_ACCESS_DENIED) ++semAcesso;
    }
};
bool ausente(LSTATUS codigo) {
    return codigo == ERROR_FILE_NOT_FOUND || codigo == ERROR_PATH_NOT_FOUND;
}
bool igual(const std::wstring& a, const wchar_t* b) {
    return CompareStringOrdinal(a.c_str(), -1, b, -1, TRUE) == CSTR_EQUAL;
}
// Registros BAM usuais: REG_BINARY de 24 bytes, inclusive identidades de apps.
// Campos de controle e formatos desconhecidos nao sao tratados como historico.
bool historico(const std::wstring& nome, DWORD tipo, DWORD bytes) {
    return !nome.empty() && !igual(nome, L"Version") && !igual(nome, L"SequenceNumber") &&
        tipo == REG_BINARY && bytes == 24;
}

void apagarLastKey(HKEY chave, Resultado& resultado) {
    LSTATUS status = RegDeleteValueW(chave, L"LastKey");
    if (status == ERROR_SUCCESS) ++resultado.removidos;
    else if (!ausente(status)) resultado.erro(status);
}

void limparValores(HKEY leitura, Resultado& resultado) {
    std::vector<std::wstring> candidatos;
    std::vector<wchar_t> nome(32768);
    for (DWORD indice = 0;; ++indice) {
        DWORD tamanho = static_cast<DWORD>(nome.size()), tipo = 0, bytes = 0;
        LSTATUS status = RegEnumValueW(leitura, indice, nome.data(), &tamanho,
            nullptr, &tipo, nullptr, &bytes);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) { resultado.erro(status); return; }
        std::wstring valor(nome.data(), tamanho);
        if (historico(valor, tipo, bytes)) candidatos.push_back(valor);
        else ++resultado.preservados;
    }
    if (candidatos.empty()) return;
    Chave escrita;
    LSTATUS status = RegOpenKeyExW(leitura, L"", REG_OPTION_OPEN_LINK,
        KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &escrita.valor);
    if (status != ERROR_SUCCESS) { resultado.erro(status); return; }
    for (const auto& valor : candidatos) {
        DWORD tipo = 0, bytes = 0;
        status = RegQueryValueExW(escrita.valor, valor.c_str(), nullptr, &tipo, nullptr, &bytes);
        if (ausente(status)) continue;
        if (status != ERROR_SUCCESS) { resultado.erro(status); continue; }
        if (!historico(valor, tipo, bytes)) { ++resultado.preservados; continue; }
        status = RegDeleteValueW(escrita.valor, valor.c_str());
        if (status == ERROR_SUCCESS) ++resultado.removidos;
        else if (!ausente(status)) resultado.erro(status);
    }
}

void percorrerBam(HKEY leitura, Resultado& resultado, unsigned profundidade = 0) {
    if (profundidade > 128) { resultado.erro(ERROR_STACK_OVERFLOW); return; }
    ++resultado.chaves;
    // Enumera as subchaves antes de qualquer exclusao de valores.
    std::vector<std::wstring> filhos;
    wchar_t nome[512]{};
    for (DWORD indice = 0;; ++indice) {
        DWORD tamanho = 512;
        LSTATUS status = RegEnumKeyExW(leitura, indice, nome, &tamanho,
            nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) { resultado.erro(status); return; }
        filhos.emplace_back(nome, tamanho);
    }
    limparValores(leitura, resultado);
    for (const auto& nomeFilho : filhos) {
        Chave filho;
        LSTATUS status = RegOpenKeyExW(leitura, nomeFilho.c_str(), REG_OPTION_OPEN_LINK,
            KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &filho.valor);
        if (status == ERROR_SUCCESS) percorrerBam(filho.valor, resultado, profundidade + 1);
        else if (!ausente(status)) resultado.erro(status);
    }
}

Resultado executar() {
    Resultado resultado;
    {
        Chave regedit;
        LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0,
            KEY_SET_VALUE | KEY_WOW64_64KEY, &regedit.valor);
        if (status == ERROR_SUCCESS) apagarLastKey(regedit.valor, resultado);
        else if (!ausente(status)) resultado.erro(status);
    }
    {
        Chave bam;
        LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings", REG_OPTION_OPEN_LINK,
            KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &bam.valor);
        if (status == ERROR_SUCCESS) percorrerBam(bam.valor, resultado);
        else if (!ausente(status)) resultado.erro(status);
    }
    return resultado;
}
int iniciar(bool mostrar) noexcept {
    try {
        Resultado r = executar();
        if (mostrar) {
            std::wstring texto = r.falhas ? L"Limpeza parcial.\n" : L"Operacao concluida.\n";
            texto += L"Valores removidos: " + std::to_wstring(r.removidos);
            texto += L"\nValores preservados: " + std::to_wstring(r.preservados);
            texto += L"\nChaves BAM examinadas: " + std::to_wstring(r.chaves);
            texto += L"\nFalhas: " + std::to_wstring(r.falhas);
            texto += L"\nAcessos negados: " + std::to_wstring(r.semAcesso);
            texto += L"\n\nAs chaves foram preservadas. Campos de controle e formatos desconhecidos tambem.";
            texto += L"\nA DLL nao altera permissoes nem toma posse de chaves.";
            MessageBoxW(nullptr, texto.c_str(), L"Limpeza do Registro",
                MB_OK | (r.falhas ? MB_ICONWARNING : MB_ICONINFORMATION));
        }
        return r.falhas ? 1 : 0;
    } catch (...) {
        if (mostrar) MessageBoxW(nullptr,
            L"Erro inesperado. A operacao pode ter sido concluida parcialmente.",
            L"Limpeza do Registro", MB_OK | MB_ICONERROR);
        return 1;
    }
}
}
extern "C" int LimparRegistroSilencioso() { return iniciar(false); }
extern "C" void CALLBACK LimparRegistro(HWND, HINSTANCE, LPSTR, int) { iniciar(true); }

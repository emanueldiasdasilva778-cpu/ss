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
#include <shlobj.h>
#include <string>
#include <vector>
#include "../common/Diagnostics.h"

namespace {
thread_local diagnostics::Report detalhes;
thread_local std::wstring ultimoRelatorio;
struct Resultado {
    unsigned long long arquivos = 0, pastas = 0, falhas = 0, links = 0;
    void erro(DWORD codigo, const std::wstring& caminho) {
        ++falhas; detalhes.Add(codigo, caminho);
    }
};
bool ausente(DWORD erro) {
    return erro == ERROR_FILE_NOT_FOUND || erro == ERROR_PATH_NOT_FOUND;
}
bool igual(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
std::wstring unir(const std::wstring& base, const std::wstring& nome) {
    if (base.empty()) return {};
    return base + (base.back() == L'\\' ? L"" : L"\\") + nome;
}
std::wstring absoluta(const std::wstring& caminho) {
    if (caminho.empty()) return {};
    DWORD tamanho = GetFullPathNameW(caminho.c_str(), 0, nullptr, nullptr);
    if (!tamanho) return {};
    std::vector<wchar_t> buffer(tamanho + 1);
    DWORD usado = GetFullPathNameW(caminho.c_str(),
        static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (!usado || usado >= buffer.size()) return {};
    std::wstring valor(buffer.data(), usado);
    while (valor.size() > 3 && valor.back() == L'\\') valor.pop_back();
    if (valor.size() < 3 || valor[1] != L':' || valor[2] != L'\\') return {};
    return valor;
}
std::wstring longo(const std::wstring& caminho) {
    return L"\\\\?\\" + caminho;
}
std::wstring conhecida(REFKNOWNFOLDERID id) {
    PWSTR texto = nullptr;
    HRESULT hr = SHGetKnownFolderPath(id, 0, nullptr, &texto);
    if (FAILED(hr)) return {};
    std::wstring valor(texto);
    CoTaskMemFree(texto);
    return absoluta(valor);
}
std::wstring windows() {
    std::vector<wchar_t> buffer(32768);
    UINT tamanho = GetWindowsDirectoryW(buffer.data(),
        static_cast<UINT>(buffer.size()));
    if (!tamanho || tamanho >= buffer.size()) return {};
    return absoluta(std::wstring(buffer.data(), tamanho));
}
// Nao atravessa junctions nem links simbolicos. 0=seguro, 1=ausente, 2=bloqueado.
int verificar(const std::wstring& caminho) {
    if (caminho.size() <= 3) { SetLastError(ERROR_BAD_PATHNAME); return 2; }
    size_t posicao = 3;
    for (;;) {
        size_t fim = caminho.find(L'\\', posicao);
        auto parcial = caminho.substr(0, fim);
        DWORD atributos = GetFileAttributesW(longo(parcial).c_str());
        if (atributos == INVALID_FILE_ATTRIBUTES)
            return ausente(GetLastError()) ? 1 : 2;
        if (atributos & FILE_ATTRIBUTE_REPARSE_POINT) {
            DWORD erro = diagnostics::CheckCloudReparse(longo(parcial));
            if (erro) { SetLastError(erro); return 2; }
        }
        if (fim == std::wstring::npos) return 0;
        if (!(atributos & FILE_ATTRIBUTE_DIRECTORY)) { SetLastError(ERROR_DIRECTORY); return 2; }
        posicao = fim + 1;
    }
}
void remover(const std::wstring& caminho, bool manterPasta,
             bool preservarGameStorage, Resultado& resultado, unsigned profundidade = 0) {
    if (profundidade > 128) { resultado.erro(ERROR_STACK_OVERFLOW, caminho); return; }
    auto destino = longo(caminho);
    DWORD atributos = GetFileAttributesW(destino.c_str());
    if (atributos == INVALID_FILE_ATTRIBUTES) {
        DWORD erro = GetLastError();
        if (!ausente(erro)) resultado.erro(erro, caminho);
        return;
    }
    if (atributos & FILE_ATTRIBUTE_REPARSE_POINT) {
        DWORD erro = diagnostics::CheckCloudReparse(destino);
        if (erro) { ++resultado.links; detalhes.Add(erro, caminho); return; }
    }
    bool diretorio = (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (manterPasta && !diretorio) { resultado.erro(ERROR_DIRECTORY, caminho); return; }
    if (diretorio) {
        WIN32_FIND_DATAW dados{};
        HANDLE busca = FindFirstFileW(unir(destino, L"*").c_str(), &dados);
        if (busca == INVALID_HANDLE_VALUE) {
            DWORD erro = GetLastError();
            if (erro != ERROR_FILE_NOT_FOUND) { resultado.erro(erro, caminho); return; }
        } else {
            std::vector<std::wstring> filhos;
            do {
                std::wstring nome(dados.cFileName);
                if (nome == L"." || nome == L"..") continue;
                if (preservarGameStorage && igual(nome, L"game-storage")) continue;
                filhos.push_back(nome);
            } while (FindNextFileW(busca, &dados));
            DWORD erroBusca = GetLastError();
            FindClose(busca);
            if (erroBusca != ERROR_NO_MORE_FILES) { resultado.erro(erroBusca, caminho); return; }
            for (const auto& filho : filhos) {
                auto alvo = unir(caminho, filho);
                int estado = verificar(alvo);
                if (estado == 0) remover(alvo, false, false, resultado, profundidade + 1);
                else if (estado == 2) {
                    DWORD erro = GetLastError();
                    if (erro == ERROR_NOT_SUPPORTED) { ++resultado.links; detalhes.Add(erro, alvo); }
                    else resultado.erro(erro, alvo);
                }
            }
        }
        if (manterPasta) return;
    }
    // Remove apenas o atributo somente leitura no alvo ja autorizado; nao altera ACLs.
    bool mudouAtributo = false;
    if (atributos & FILE_ATTRIBUTE_READONLY) {
        if (!SetFileAttributesW(destino.c_str(), atributos & ~FILE_ATTRIBUTE_READONLY)) {
            resultado.erro(GetLastError(), caminho); return;
        }
        mudouAtributo = true;
    }
    BOOL apagou = diretorio ? RemoveDirectoryW(destino.c_str())
                           : DeleteFileW(destino.c_str());
    if (!apagou) {
        DWORD erro = GetLastError();
        if (mudouAtributo) {
            DWORD atual = GetFileAttributesW(destino.c_str());
            if (atual != INVALID_FILE_ATTRIBUTES) SetFileAttributesW(destino.c_str(), atual | FILE_ATTRIBUTE_READONLY);
        }
        if (!ausente(erro)) resultado.erro(erro, caminho);
    } else if (diretorio) ++resultado.pastas;
    else ++resultado.arquivos;
}
void limpar(const std::wstring& base, const wchar_t* relativo,
            bool manter, bool preservar, Resultado& resultado) {
    if (base.empty()) { resultado.erro(ERROR_BAD_PATHNAME, relativo); return; }
    auto alvo = absoluta(unir(base, relativo));
    if (alvo.empty()) { resultado.erro(ERROR_BAD_PATHNAME, relativo); return; }
    int estado = verificar(alvo);
    if (estado == 0) remover(alvo, manter, preservar, resultado);
    else if (estado == 2) resultado.erro(GetLastError(), alvo);
}
bool dentro(const std::wstring& caminho, const std::wstring& pasta) {
    if (pasta.empty() || caminho.size() < pasta.size()) return false;
    return igual(caminho.substr(0, pasta.size()), pasta) &&
        (caminho.size() == pasta.size() || caminho[pasta.size()] == L'\\');
}
Resultado executar() {
    Resultado resultado;
    auto local = conhecida(FOLDERID_LocalAppData);
    auto documentos = conhecida(FOLDERID_Documents);
    auto programData = conhecida(FOLDERID_ProgramData);
    auto sistema = windows();
    limpar(local, L"DigitalEntitlements", false, false, resultado);
    limpar(documentos, L"Rockstar Games", false, false, resultado);
    limpar(documentos, L"Rockstar", false, false, resultado);
    limpar(programData, L"Microsoft\\Windows\\WER", true, false, resultado);
    limpar(local, L"FiveM\\FiveM.app\\logs", true, false, resultado);
    limpar(local, L"FiveM\\FiveM.app\\crashes", true, false, resultado);
    limpar(local, L"FiveM\\FiveM.app\\mods", true, false, resultado);
    limpar(local, L"FiveM\\FiveM.app\\data", true, true, resultado);
    limpar(sistema, L"Temp", true, false, resultado);
    limpar(local, L"Temp", true, false, resultado);
    limpar(sistema, L"Prefetch", true, false, resultado);
    // Impede que TEMP adulterado aponte para a raiz do perfil ou para game-storage.
    std::vector<wchar_t> tempBuffer(32768);
    DWORD n = GetEnvironmentVariableW(L"TEMP", tempBuffer.data(),
        static_cast<DWORD>(tempBuffer.size()));
    if (n && n < tempBuffer.size()) {
        auto temp = absoluta(std::wstring(tempBuffer.data(), n));
        auto tempUsuario = unir(local, L"Temp");
        auto tempWindows = unir(sistema, L"Temp");
        auto systemTemp = unir(sistema, L"SystemTemp");
        if (!temp.empty() && !igual(temp, tempUsuario) && !igual(temp, tempWindows)) {
            if (dentro(temp, tempUsuario) || dentro(temp, tempWindows) || dentro(temp, systemTemp)) {
                int estado = verificar(temp);
                if (!estado) remover(temp, true, false, resultado);
                else if (estado == 2) resultado.erro(GetLastError(), temp);
            } else resultado.erro(ERROR_BAD_PATHNAME, temp);
        }
    } else if (n >= tempBuffer.size()) resultado.erro(ERROR_FILENAME_EXCED_RANGE, L"%TEMP%");
    // Remove itens e seus metadados pela API oficial da Lixeira.
    SHQUERYRBINFO info{}; info.cbSize = sizeof(info);
    HRESULT consulta = SHQueryRecycleBinW(nullptr, &info);
    if (SUCCEEDED(consulta) && info.i64NumItems == 0) return resultado;
    HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr,
        SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (FAILED(hr)) resultado.erro(HRESULT_FACILITY(hr) == FACILITY_WIN32 ?
        HRESULT_CODE(hr) : static_cast<DWORD>(hr), L"Lixeira");
    return resultado;
}
int executarComResultado(bool mostrar) noexcept {
    detalhes.Reset(); ultimoRelatorio.clear();
    if (!diagnostics::IsElevated()) {
        detalhes.Add(ERROR_ELEVATION_REQUIRED, L"Execute FSC.exe ou rundll32 como administrador.");
        ultimoRelatorio = detalhes.Finish(L"Limpeza nao iniciada.");
        if (mostrar) MessageBoxW(nullptr, ultimoRelatorio.c_str(), L"Limpeza", MB_OK | MB_ICONWARNING);
        return 1;
    }
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool liberarCom = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        detalhes.Add(static_cast<DWORD>(com), L"Inicializacao COM");
        ultimoRelatorio = detalhes.Finish(L"Limpeza nao iniciada.");
        if (mostrar) MessageBoxW(nullptr, L"Falha ao inicializar componentes do Windows.",
            L"Limpeza", MB_OK | MB_ICONERROR);
        return 1;
    }
    int retorno = 1;
    try {
        Resultado r = executar();
        retorno = (r.falhas || r.links) ? 1 : 0;
        ultimoRelatorio = detalhes.Finish(L"Arquivos removidos: " + std::to_wstring(r.arquivos) +
            L"\r\nPastas removidas: " + std::to_wstring(r.pastas) +
            L"\r\ngame-storage preservado. Contagens nao incluem a Lixeira.");
        if (mostrar) {
            std::wstring mensagem = retorno ? L"Limpeza parcial.\n" : L"Limpeza concluida.\n";
            mensagem += L"Arquivos excluidos: " + std::to_wstring(r.arquivos);
            mensagem += L"\nPastas excluidas: " + std::to_wstring(r.pastas);
            mensagem += L"\nFalhas/bloqueios: " + std::to_wstring(r.falhas);
            mensagem += L"\nLinks ou caminhos ignorados: " + std::to_wstring(r.links);
            mensagem += L"\n\nFiveM\\FiveM.app\\data\\game-storage foi preservado.";
            mensagem += L"\nArquivos em uso ou sem permissao podem permanecer.";
            mensagem += L"\nContagens nao incluem itens removidos da Lixeira.";
            MessageBoxW(nullptr, mensagem.c_str(), L"Limpeza",
                MB_OK | (retorno ? MB_ICONWARNING : MB_ICONINFORMATION));
        }
    } catch (...) {
        detalhes.Add(ERROR_UNHANDLED_EXCEPTION, L"Limpeza interrompida");
        ultimoRelatorio = detalhes.Finish(L"Resultado parcial.");
        if (mostrar) MessageBoxW(nullptr,
            L"Limpeza interrompida por um erro. Parte dos arquivos pode ter sido removida.",
            L"Limpeza", MB_OK | MB_ICONERROR);
    }
    if (liberarCom) CoUninitialize();
    return retorno;
}
}
extern "C" int LimparPc() { return executarComResultado(false); }
extern "C" const wchar_t* CleanerGetLastReport() { return ultimoRelatorio.c_str(); }
// Assinatura correta para rundll32; nao executa limpeza dentro de DllMain.
extern "C" void CALLBACK Limpar(HWND, HINSTANCE, LPSTR, int) {
    executarComResultado(true);
}

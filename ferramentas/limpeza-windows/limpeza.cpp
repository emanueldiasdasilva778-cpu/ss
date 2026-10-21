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

namespace {
struct Resultado {
    unsigned long long arquivos = 0, pastas = 0, falhas = 0, links = 0;
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
    if (caminho.size() <= 3) return 2;
    size_t posicao = 3;
    for (;;) {
        size_t fim = caminho.find(L'\\', posicao);
        auto parcial = caminho.substr(0, fim);
        DWORD atributos = GetFileAttributesW(longo(parcial).c_str());
        if (atributos == INVALID_FILE_ATTRIBUTES)
            return ausente(GetLastError()) ? 1 : 2;
        if (atributos & FILE_ATTRIBUTE_REPARSE_POINT) return 2;
        if (fim == std::wstring::npos) return 0;
        if (!(atributos & FILE_ATTRIBUTE_DIRECTORY)) return 2;
        posicao = fim + 1;
    }
}
void remover(const std::wstring& caminho, bool manterPasta,
             bool preservarGameStorage, Resultado& resultado, unsigned profundidade = 0) {
    if (profundidade > 128) { ++resultado.falhas; return; }
    auto destino = longo(caminho);
    DWORD atributos = GetFileAttributesW(destino.c_str());
    if (atributos == INVALID_FILE_ATTRIBUTES) {
        if (!ausente(GetLastError())) ++resultado.falhas;
        return;
    }
    if (atributos & FILE_ATTRIBUTE_REPARSE_POINT) { ++resultado.links; return; }
    bool diretorio = (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (manterPasta && !diretorio) { ++resultado.falhas; return; }
    if (diretorio) {
        WIN32_FIND_DATAW dados{};
        HANDLE busca = FindFirstFileW(unir(destino, L"*").c_str(), &dados);
        if (busca == INVALID_HANDLE_VALUE) {
            if (GetLastError() != ERROR_FILE_NOT_FOUND) { ++resultado.falhas; return; }
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
            if (erroBusca != ERROR_NO_MORE_FILES) { ++resultado.falhas; return; }
            for (const auto& filho : filhos) {
                auto alvo = unir(caminho, filho);
                int estado = verificar(alvo);
                if (estado == 0) remover(alvo, false, false, resultado, profundidade + 1);
                else if (estado == 2) ++resultado.links;
            }
        }
        if (manterPasta) return;
    }
    // Nao toma posse, nao muda ACLs e nao encerra processos.
    BOOL apagou = diretorio ? RemoveDirectoryW(destino.c_str())
                           : DeleteFileW(destino.c_str());
    if (!apagou) {
        if (!ausente(GetLastError())) ++resultado.falhas;
    } else if (diretorio) ++resultado.pastas;
    else ++resultado.arquivos;
}
void limpar(const std::wstring& base, const wchar_t* relativo,
            bool manter, bool preservar, Resultado& resultado) {
    if (base.empty()) { ++resultado.falhas; return; }
    auto alvo = absoluta(unir(base, relativo));
    if (alvo.empty()) { ++resultado.falhas; return; }
    int estado = verificar(alvo);
    if (estado == 0) remover(alvo, manter, preservar, resultado);
    else if (estado == 2) ++resultado.falhas;
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
                else if (estado == 2) ++resultado.falhas;
            } else ++resultado.falhas;
        }
    } else if (n >= tempBuffer.size()) ++resultado.falhas;
    // Remove itens e seus metadados pela API oficial da Lixeira.
    HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr,
        SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (FAILED(hr)) ++resultado.falhas;
    return resultado;
}
int executarComResultado(bool mostrar) noexcept {
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool liberarCom = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        if (mostrar) MessageBoxW(nullptr, L"Falha ao inicializar componentes do Windows.",
            L"Limpeza", MB_OK | MB_ICONERROR);
        return 1;
    }
    int retorno = 1;
    try {
        Resultado r = executar();
        retorno = (r.falhas || r.links) ? 1 : 0;
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
        if (mostrar) MessageBoxW(nullptr,
            L"Limpeza interrompida por um erro. Parte dos arquivos pode ter sido removida.",
            L"Limpeza", MB_OK | MB_ICONERROR);
    }
    if (liberarCom) CoUninitialize();
    return retorno;
}
}
extern "C" int LimparPc() { return executarComResultado(false); }
// Assinatura correta para rundll32; nao executa limpeza dentro de DllMain.
extern "C" void CALLBACK Limpar(HWND, HINSTANCE, LPSTR, int) {
    executarComResultado(true);
}

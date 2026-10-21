#include "../Files.cpp"
#include <iostream>
#include <stdexcept>

namespace {
unsigned passaram = 0;
void exigir(bool ok, const char* nome) {
    if (!ok) throw std::runtime_error(nome);
    ++passaram;
    std::cout << "PASS: " << nome << '\n';
}
void pasta(const std::wstring& caminho) {
    Relatorio r;
    if (criarPastas(caminho, r)) throw std::runtime_error("Criar pasta artificial falhou");
}
void gravar(const std::wstring& caminho, const std::string& conteudo) {
    Handle h(CreateFileW(longo(caminho).c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (h.valor == INVALID_HANDLE_VALUE) throw std::runtime_error("Gravar arquivo artificial falhou");
    DWORD n;
    if (!WriteFile(h.valor, conteudo.data(), static_cast<DWORD>(conteudo.size()), &n, nullptr) ||
        n != conteudo.size()) throw std::runtime_error("WriteFile do teste falhou");
}
std::string ler(const std::wstring& caminho) {
    Handle h(CreateFileW(longo(caminho).c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (h.valor == INVALID_HANDLE_VALUE) throw std::runtime_error("Ler arquivo artificial falhou");
    LARGE_INTEGER tamanho{};
    if (!GetFileSizeEx(h.valor, &tamanho) || tamanho.QuadPart > 4 * 1024 * 1024)
        throw std::runtime_error("Tamanho de arquivo de teste invalido");
    std::string dados(static_cast<size_t>(tamanho.QuadPart), '\0');
    DWORD n;
    if (!ReadFile(h.valor, dados.data(), static_cast<DWORD>(dados.size()), &n, nullptr) ||
        n != dados.size()) throw std::runtime_error("ReadFile do teste falhou");
    return dados;
}
bool existe(const std::wstring& caminho) {
    return GetFileAttributesW(longo(caminho).c_str()) != INVALID_FILE_ATTRIBUTES;
}
// Limpeza limitada exclusivamente a pasta artificial criada por este teste.
void removerTeste(const std::wstring& caminho) {
    WIN32_FIND_DATAW dado{};
    HANDLE busca = FindFirstFileW(unir(longo(caminho), L"*").c_str(), &dado);
    if (busca != INVALID_HANDLE_VALUE) {
        do {
            if (igual(dado.cFileName, L".") || igual(dado.cFileName, L"..")) continue;
            auto filho = unir(caminho, dado.cFileName);
            if (dado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) removerTeste(filho);
            else {
                SetFileAttributesW(longo(filho).c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(longo(filho).c_str());
            }
        } while (FindNextFileW(busca, &dado));
        FindClose(busca);
    }
    RemoveDirectoryW(longo(caminho).c_str());
}
}
int wmain(int argc, wchar_t** argv) {
    std::wstring raiz;
    try {
        wchar_t temporaria[32768]{};
        if (!GetTempPathW(32768, temporaria)) return 1;
        raiz = absoluta(unir(temporaria, L"FilesDllTeste-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(GetTickCount64())));
        pasta(raiz);
        auto origem = unir(raiz, L"fudi o luckyy"), destino = unir(raiz, L"mods");
        pasta(unir(origem, L"sub\\vazia"));
        pasta(destino);
        std::string binario(1024 * 1024, '\0');
        for (size_t i = 0; i < binario.size(); ++i) binario[i] = static_cast<char>(i % 251);
        gravar(unir(origem, L"mod.bin"), binario);
        gravar(unir(origem, L"sub\\config.ini"), "configuracao nova");
        gravar(unir(origem, L"oculto.txt"), "oculto");
        SetFileAttributesW(longo(unir(origem, L"oculto.txt")).c_str(), FILE_ATTRIBUTE_HIDDEN);
        gravar(unir(destino, L"mod.bin"), "antigo");
        gravar(unir(destino, L"extra.txt"), "manter");
        Relatorio r; copiarArvore(origem, destino, r);
        exigir(!r.falhas && r.arquivos == 3, "copia recursiva de tres arquivos");
        exigir(ler(unir(destino, L"mod.bin")) == binario, "binario de 1 MiB copiado sem alteracoes");
        exigir(ler(unir(destino, L"sub\\config.ini")) == "configuracao nova" &&
            existe(unir(destino, L"sub\\vazia")), "subpastas e pastas vazias copiadas");
        exigir(ler(unir(destino, L"oculto.txt")) == "oculto", "arquivo oculto copiado");
        exigir(ler(unir(destino, L"extra.txt")) == "manter", "arquivo extra do destino preservado");
        exigir(ler(unir(origem, L"mod.bin")) == binario, "origem preservada");
        Relatorio repetido; copiarArvore(origem, destino, repetido);
        exigir(!repetido.falhas && repetido.arquivos == 3, "operacao repetivel");
        pasta(unir(raiz, L"f"));
        auto xml = unir(raiz, L"f\\gta5_settings.xml");
        auto xmlDestino = unir(raiz, L"CitizenFX\\gta5_settings.xml");
        gravar(xml, "<Settings>novo</Settings>");
        Relatorio copiarXml; copiarArquivo(xml, xmlDestino, copiarXml);
        exigir(!copiarXml.falhas && ler(xmlDestino) == "<Settings>novo</Settings>",
            "XML copiado e CitizenFX criada");
        gravar(xml, "<Settings>atualizado</Settings>");
        Relatorio atualizarXml; copiarArquivo(xml, xmlDestino, atualizarXml);
        exigir(!atualizarXml.falhas && ler(xmlDestino) == "<Settings>atualizado</Settings>",
            "XML existente substituido");
        auto protegido = unir(destino, L"protegido.bin");
        gravar(protegido, "original");
        {
            Handle bloqueio(CreateFileW(longo(protegido).c_str(), GENERIC_READ, 0,
                nullptr, OPEN_EXISTING, 0, nullptr));
            if (bloqueio.valor == INVALID_HANDLE_VALUE) throw std::runtime_error("Bloqueio do teste falhou");
            Relatorio bloqueado; copiarArquivo(unir(origem, L"mod.bin"), protegido, bloqueado);
            exigir(bloqueado.falhas == 1 && !bloqueado.arquivos,
                "destino bloqueado produz falha real");
        }
        exigir(ler(protegido) == "original", "falha de substituicao preserva arquivo anterior");
        WIN32_FIND_DATAW dados{};
        HANDLE resto = FindFirstFileW(unir(longo(destino), L".files-copy-*").c_str(), &dados);
        bool semTemporarios = resto == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND;
        if (resto != INVALID_HANDLE_VALUE) FindClose(resto);
        exigir(semTemporarios, "temporario removido depois de substituicao bloqueada");
        Relatorio falta; copiarArvore(unir(raiz, L"ausente"), unir(raiz, L"nao-criar"), falta);
        exigir(falta.falhas == 1 && !existe(unir(raiz, L"nao-criar")),
            "origem ausente nao cria destino vazio");
        Relatorio ciclo; copiarArvore(origem, unir(origem, L"loop"), ciclo);
        exigir(ciclo.falhas == 1 && !existe(unir(origem, L"loop")), "copia para dentro da origem recusada");
        exigir(argc == 2, "caminho da DLL recebido");
        HMODULE dll = LoadLibraryExW(argv[1], nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        exigir(dll != nullptr, "DLL real carregada");
        exigir(GetProcAddress(dll, "PerformFilesCopy") && GetProcAddress(dll, "AplicarArquivos") &&
            GetProcAddress(dll, "FilesGetLastStatus"), "tres exportacoes encontradas");
        FreeLibrary(dll);
        removerTeste(raiz);
        exigir(!existe(raiz), "arquivos artificiais do teste removidos");
        std::cout << "TOTAL: " << passaram << " verificacoes passaram\n";
        return 0;
    } catch (const std::exception& erro) {
        std::cerr << "FAIL: " << erro.what() << '\n';
        return 1;
    }
}

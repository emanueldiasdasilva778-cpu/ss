#include "../limpeza.cpp"
#include <iostream>
#include <stdexcept>

static unsigned passou = 0;
static void exigir(bool condicao, const char* nome) {
    if (!condicao) throw std::runtime_error(nome);
    ++passou;
    std::cout << "PASS: " << nome << '\n';
}
static void pasta(const std::wstring& p) {
    if (!CreateDirectoryW(p.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw std::runtime_error("Criacao de pasta falhou");
}
static void arquivo(const std::wstring& p) {
    HANDLE f = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) throw std::runtime_error("Criacao de arquivo falhou");
    DWORD n;
    const char conteudo[] = "preservar";
    BOOL ok = WriteFile(f, conteudo, sizeof(conteudo), &n, nullptr);
    CloseHandle(f);
    if (!ok) throw std::runtime_error("Escrita falhou");
}
static bool existe(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}
int wmain(int argc, wchar_t** argv) {
    std::wstring raiz;
    try {
        wchar_t temp[32768]{};
        if (!GetTempPathW(32768, temp)) return 1;
        raiz = absoluta(unir(temp, L"TesteLimpeza-" + std::to_wstring(GetCurrentProcessId())
            + L"-" + std::to_wstring(GetTickCount64())));
        pasta(raiz);
        auto data = unir(raiz, L"data"); pasta(data);
        auto armazenamento = unir(data, L"GaMe-StOrAgE"); pasta(armazenamento);
        auto salvo = unir(armazenamento, L"salvo.bin"); arquivo(salvo);
        auto cache = unir(data, L"cache"); pasta(cache); arquivo(unir(cache, L"cache.bin"));
        arquivo(unir(data, L"direto.bin"));
        Resultado r;
        limpar(raiz, L"data", true, true, r);
        exigir(existe(salvo), "game-storage preservado com comparacao sem distinguir maiusculas");
        exigir(!existe(cache) && !existe(unir(data, L"direto.bin")), "demais conteudos de data excluidos");
        exigir(existe(data) && r.arquivos == 2 && r.pastas == 1 && !r.falhas,
            "raiz de data preservada e contagens corretas");
        Resultado repetido; limpar(raiz, L"data", true, true, repetido);
        exigir(!repetido.falhas && !repetido.arquivos && existe(salvo), "limpeza repetida preserva dados");
        auto inteira = unir(raiz, L"DigitalEntitlements"); pasta(inteira);
        arquivo(unir(inteira, L"exemplo.bin"));
        Resultado total; limpar(raiz, L"DigitalEntitlements", false, false, total);
        exigir(!existe(inteira) && total.arquivos == 1 && total.pastas == 1,
            "exclusao de pasta inteira");
        Resultado ausentes; limpar(raiz, L"nao-existe", false, false, ausentes);
        exigir(!ausentes.falhas, "pasta ausente nao causa falha");
        exigir(verificar(L"C:\\") == 2, "raiz do volume recusada");
        exigir(!dentro(L"C:\\TempOutro", L"C:\\Temp") && dentro(L"C:\\Temp\\sub", L"C:\\Temp"),
            "validacao dos limites da pasta temporaria");
        auto bloqueado = unir(raiz, L"bloqueado.bin"); arquivo(bloqueado);
        HANDLE aberto = CreateFileW(bloqueado.c_str(), GENERIC_READ, 0, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (aberto == INVALID_HANDLE_VALUE) throw std::runtime_error("Abrir bloqueio falhou");
        Resultado travado; limpar(raiz, L"bloqueado.bin", false, false, travado);
        exigir(existe(bloqueado) && travado.falhas == 1, "arquivo em uso permanece e falha e informada");
        CloseHandle(aberto);
        auto protegido = unir(raiz, L"protegido.bin"); arquivo(protegido);
        SetFileAttributesW(protegido.c_str(), FILE_ATTRIBUTE_READONLY);
        Resultado leitura; limpar(raiz, L"protegido.bin", false, false, leitura);
        exigir(!existe(protegido) && !leitura.falhas && leitura.arquivos == 1, "atributo somente leitura removido no alvo autorizado");
        SetFileAttributesW(protegido.c_str(), FILE_ATTRIBUTE_NORMAL);
        auto fora = unir(raiz, L"fora"); pasta(fora); arquivo(unir(fora, L"importante.bin"));
        auto links = unir(raiz, L"links"); pasta(links);
        auto link = unir(links, L"atalho");
        if (!CreateSymbolicLinkW(link.c_str(), fora.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY |
                SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
            throw std::runtime_error("Nao foi possivel criar link para testar protecao");
        DWORD attrLink = GetFileAttributesW(link.c_str());
        if (attrLink == INVALID_FILE_ATTRIBUTES || !(attrLink & FILE_ATTRIBUTE_REPARSE_POINT)) {
            std::cout << "SKIP: 2 verificacoes de reparse points; Wine nao representa o link como reparse point\n";
        } else {
            Resultado atalhos; limpar(raiz, L"links", true, false, atalhos);
            exigir(existe(unir(fora, L"importante.bin")) && atalhos.links == 1,
                "link de diretorio nao e seguido");
            Resultado ancestral; limpar(raiz, L"links\\atalho\\importante.bin", false, false, ancestral);
            exigir(existe(unir(fora, L"importante.bin")) && ancestral.falhas == 1,
                "ancestral que e link bloqueia exclusao");
        }
        RemoveDirectoryW(link.c_str());
        // Carrega o binario real sem chamar as exportacoes destrutivas.
        exigir(argc == 2, "caminho da DLL recebido");
        HMODULE dll = LoadLibraryW(argv[1]);
        exigir(dll != nullptr, "DLL real carregada pelo loader Windows");
        exigir(GetProcAddress(dll, "Limpar") && GetProcAddress(dll, "LimparPc"),
            "exportacoes Limpar e LimparPc encontradas");
        FreeLibrary(dll);
        Resultado limparTeste; remover(raiz, false, false, limparTeste);
        exigir(!existe(raiz), "arquivos artificiais de teste removidos");
        std::cout << "TOTAL: " << passou << " verificacoes passaram\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << " apos " << passou << " verificacoes\n";
        return 1;
    }
}

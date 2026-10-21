#include "../registro.cpp"
#include <iostream>
#include <stdexcept>

namespace {
unsigned passaram = 0;
void exigir(bool condicao, const char* descricao) {
    if (!condicao) throw std::runtime_error(descricao);
    ++passaram;
    std::cout << "PASS: " << descricao << '\n';
}
void criar(HKEY pai, const wchar_t* nome, Chave& chave) {
    if (RegCreateKeyExW(pai, nome, 0, nullptr, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY,
        nullptr, &chave.valor, nullptr) != ERROR_SUCCESS)
        throw std::runtime_error("Nao foi possivel criar a chave artificial");
}
void valor(HKEY chave, const wchar_t* nome, DWORD tipo, DWORD tamanho) {
    std::vector<BYTE> bytes(tamanho, 0);
    if (RegSetValueExW(chave, nome, 0, tipo, bytes.data(), tamanho) != ERROR_SUCCESS)
        throw std::runtime_error("Nao foi possivel criar o valor artificial");
}
bool existe(HKEY chave, const wchar_t* nome) {
    return RegQueryValueExW(chave, nome, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}
bool subchave(HKEY chave, const wchar_t* nome) {
    Chave filho;
    return RegOpenKeyExW(chave, nome, 0, KEY_READ | KEY_WOW64_64KEY,
        &filho.valor) == ERROR_SUCCESS;
}
}

int wmain(int argc, wchar_t** argv) {
    const std::wstring caminho = L"Software\\RegistroDllTestes\\Teste-" +
        std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    try {
        {
            Chave raiz, regedit, bam, sid, filho, vazia;
            criar(HKEY_CURRENT_USER, caminho.c_str(), raiz);
            criar(raiz.valor, L"Regedit", regedit);
            valor(regedit.valor, L"LastKey", REG_SZ, 20);
            valor(regedit.valor, L"View", REG_DWORD, 4);
            Resultado r;
            apagarLastKey(regedit.valor, r);
            exigir(!existe(regedit.valor, L"LastKey") && r.removidos == 1 && !r.falhas,
                "LastKey excluido");
            exigir(existe(regedit.valor, L"View") && subchave(raiz.valor, L"Regedit"),
                "outros valores e chave Regedit preservados");
            Resultado repetirLastKey;
            apagarLastKey(regedit.valor, repetirLastKey);
            exigir(!repetirLastKey.falhas && !repetirLastKey.removidos,
                "LastKey ausente e operacao repetivel");

            criar(raiz.valor, L"BAM", bam);
            criar(bam.valor, L"S-1-5-21-123", sid);
            criar(sid.valor, L"Subchave", filho);
            criar(bam.valor, L"Vazia", vazia);
            valor(bam.valor, L"\\Device\\HarddiskVolume3\\root.exe", REG_BINARY, 24);
            valor(sid.valor, L"\\Device\\HarddiskVolume3\\teste.exe", REG_BINARY, 24);
            valor(sid.valor, L"PacoteDeTeste!App", REG_BINARY, 24);
            valor(filho.valor, L"C:\\Teste\\filho.exe", REG_BINARY, 24);
            valor(sid.valor, L"Version", REG_BINARY, 24);
            valor(sid.valor, L"SequenceNumber", REG_DWORD, 4);
            valor(sid.valor, L"Configuracao", REG_SZ, 8);
            valor(sid.valor, L"FormatoDesconhecido", REG_BINARY, 16);
            valor(sid.valor, L"", REG_BINARY, 24);
            Resultado historicos;
            percorrerBam(bam.valor, historicos);
            exigir(historicos.removidos == 4 && !historicos.falhas,
                "quatro registros BAM reconhecidos excluidos");
            exigir(!existe(sid.valor, L"\\Device\\HarddiskVolume3\\teste.exe") &&
                !existe(sid.valor, L"PacoteDeTeste!App") &&
                !existe(filho.valor, L"C:\\Teste\\filho.exe"),
                "historicos de caminhos e identidade de aplicativo removidos");
            exigir(existe(sid.valor, L"Version") && existe(sid.valor, L"SequenceNumber"),
                "Version e SequenceNumber preservados");
            exigir(existe(sid.valor, L"Configuracao") && existe(sid.valor, L"FormatoDesconhecido") &&
                existe(sid.valor, L""), "valores desconhecidos e padrao preservados");
            exigir(historicos.chaves == 4 && historicos.preservados == 5 &&
                subchave(bam.valor, L"S-1-5-21-123\\Subchave") && subchave(bam.valor, L"Vazia"),
                "todas as subchaves preservadas, inclusive vazias");
            Resultado repetido;
            percorrerBam(bam.valor, repetido);
            exigir(!repetido.removidos && !repetido.falhas && repetido.preservados == 5,
                "limpeza BAM repetivel");
            exigir(!historico(L"vErSiOn", REG_BINARY, 24) &&
                !historico(L"sEqUeNcEnUmBeR", REG_BINARY, 24),
                "campos de controle reconhecidos sem distinguir maiusculas");
            Resultado invalido;
            limparValores(nullptr, invalido);
            exigir(invalido.falhas == 1 && !invalido.removidos,
                "falha de leitura reportada sem exclusoes");
            exigir(argc == 2, "caminho da DLL recebido");
            HMODULE dll = LoadLibraryExW(argv[1], nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            exigir(dll != nullptr, "binario DLL carregado");
            exigir(GetProcAddress(dll, "LimparRegistro") &&
                GetProcAddress(dll, "LimparRegistroSilencioso"), "exportacoes encontradas");
            FreeLibrary(dll);
        }
        LSTATUS status = RegDeleteTreeW(HKEY_CURRENT_USER, caminho.c_str());
        exigir(status == ERROR_SUCCESS, "somente chave artificial do teste removida");
        std::cout << "TOTAL: " << passaram << " verificacoes passaram\n";
        return 0;
    } catch (const std::exception& erro) {
        std::cerr << "FAIL: " << erro.what() << '\n';
        RegDeleteTreeW(HKEY_CURRENT_USER, caminho.c_str());
        return 1;
    }
}

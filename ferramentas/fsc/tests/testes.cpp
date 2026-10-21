#define FSC_TESTING
#include "../main.cpp"
#include <iostream>
#include <stdexcept>

static unsigned passed = 0;
static void Check(bool ok, const char* name) {
    if (!ok) throw std::runtime_error(name);
    ++passed; std::cout << "PASS: " << name << '\n';
}
int wmain(int argc, wchar_t** argv) {
    try {
        fsc::Hold hold;
        hold.Begin(0, 100, fsc::Input::Mouse);
        Check(hold.Progress(100) == 0, "progresso inicial zero");
        Check(hold.TakeReady(2099) == -1 && hold.Progress(2099) < 1,
            "1999 ms nao executam acao");
        Check(hold.TakeReady(2100) == 0, "2000 ms executam acao");
        Check(hold.TakeReady(2200) == -1, "mesmo hold nao executa duas vezes");
        hold.Begin(1, 0, fsc::Input::Mouse); hold.Cancel();
        Check(hold.Progress(1999) == 0 && hold.TakeReady(3000) == -1,
            "soltar antes do limite cancela e zera");
        hold.Begin(2, 0, fsc::Input::Keyboard);
        Check(hold.TakeReady(2000) == 2, "teclado usa o mesmo limite de 2000 ms");
        auto wide = fsc::Arrange(1080, 680);
        bool square = true;
        for (const auto& r : wide.cards) square = square &&
            std::abs((r.right - r.left) - (r.bottom - r.top)) < 0.01f;
        Check(square && wide.cards[0].top == wide.cards[2].top &&
            wide.cards[0].right < wide.cards[1].left, "tres quadrados em colunas no desktop");
        auto narrow = fsc::Arrange(420, 860);
        Check(narrow.cards[0].left == narrow.cards[2].left &&
            narrow.cards[0].bottom < narrow.cards[1].top &&
            narrow.cards[1].bottom < narrow.cards[2].top, "tres quadrados empilhados em janela estreita");
        auto minimum = fsc::Arrange(360, 560);
        Check(minimum.cards[0].left > 0 && minimum.cards[2].bottom < 560 - 58,
            "layout minimo nao sobrepoe rodape");
        Check(argc == 2, "pasta das DLLs simuladas recebida");
        const std::wstring base = argv[1];
        SetEnvironmentVariableW(L"FSC_MOCK_RESULT", L"0");
        Check(fsc::Invoke(0, base).success, "carrega Limpeza.dll e chama LimparPc");
        Check(fsc::Invoke(1, base).success, "carrega Registro.dll e chama exportacao silenciosa");
        auto files = fsc::Invoke(2, base);
        Check(files.success && files.message == L"Arquivos aplicados",
            "carrega arquivos.dll e consulta status na mesma thread");
        SetEnvironmentVariableW(L"FSC_MOCK_RESULT", L"1");
        Check(!fsc::Invoke(0, base).success, "limpeza parcial nao aparece como sucesso");
        Check(!fsc::Invoke(1, base).success, "registro parcial nao aparece como sucesso");
        SetEnvironmentVariableW(L"FSC_MOCK_RESULT", L"32");
        auto failed = fsc::Invoke(2, base);
        Check(!failed.success && failed.code == 32, "erro de copia propagado da DLL");
        SetEnvironmentVariableW(L"FSC_MOCK_RESULT", L"0");
        auto missing = fsc::Invoke(0, base + L"\\ausente");
        Check(missing.code != 0 && missing.details.find(L"Limpeza.dll") != std::wstring::npos,
            "DLL ausente gera erro com caminho no relatorio");
        Check(fsc::Invoke(0, base + L"\\incompativel").code == ERROR_PROC_NOT_FOUND,
            "DLL com exportacao errada rejeitada");
        Check(fsc::Invoke(3, base).code == ERROR_INVALID_PARAMETER,
            "acao fora dos tres cards rejeitada");
        auto report = fsc::Invoke(0, base);
        Check(report.details.find(L"ocupado.bin [32]") != std::wstring::npos,
            "relatorio e copiado da DLL antes do descarregamento");
        Check(fsc::Invoke(1, base).details.find(L"HKLM\\Teste [5]") != std::wstring::npos,
            "caminho do Registro e preservado no relatorio");
        Check(fsc::Invoke(2, base).details.find(L"destino.xml [32]") != std::wstring::npos,
            "relatorio de copia e consultado na mesma thread");
        diagnostics::Report diagnostic;
        diagnostic.Add(ERROR_SHARING_VIOLATION, L"C:\\Teste\\ocupado.bin");
        diagnostic.Add(ERROR_ACCESS_DENIED, L"HKLM\\Teste");
        auto text = diagnostic.Finish(L"Resumo de teste");
        Check(diagnostic.total == 2 && diagnostic.occupied == 1 && diagnostic.denied == 1 &&
            text.find(L"[32]") != std::wstring::npos && text.find(L"[5]") != std::wstring::npos,
            "diagnostico distingue arquivo em uso e acesso negado");
        diagnostic.Reset();
        Check(diagnostic.total == 0 && diagnostic.lines.empty(), "diagnostico nao acumula operacoes anteriores");
        std::cout << "TOTAL: " << passed << " verificacoes passaram\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}

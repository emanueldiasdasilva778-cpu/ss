# Files.dll — cópia de arquivos, Windows x64

DLL independente para copiar os dois conjuntos solicitados. Usa as pastas conhecidas do Windows para localizar o perfil e AppData do usuário que executa a DLL; não há nome de usuário fixo no código.

## O que copia

| Origem | Destino |
| --- | --- |
| `%USERPROFILE%\OneDrive\Documentos\fudi o luckyy\*` | `%LOCALAPPDATA%\FiveM\FiveM.app\mods\` |
| `%USERPROFILE%\OneDrive\Documentos\f\gta5_settings.xml` | `%APPDATA%\CitizenFX\gta5_settings.xml` |

A primeira cópia é recursiva: inclui arquivos ocultos, subpastas e pastas vazias. Cria os destinos quando necessário e substitui arquivos de mesmo nome, quando as permissões permitem. **Arquivos extras do destino e todos os arquivos de origem são preservados.** Não limpa caches nem altera o Registro.

Cada arquivo é copiado para um temporário no destino e só depois substitui o arquivo final. Se a substituição falhar, o anterior permanece. A operação completa não é uma transação: outros arquivos podem ter sido copiados quando uma falha ocorrer. Uma interrupção forçada ou falha de disco pode deixar temporários `.files-copy-*.tmp`.

## Executar

Feche FiveM/GTA para evitar bloqueios. Baixe a DLL e abra um Prompt de Comando de 64 bits **com o seu usuário normal**. Ajuste o caminho:

```bat
rundll32.exe "C:\caminho\Files.dll",AplicarArquivos
```

Não há confirmação inicial nem backup dos arquivos substituídos. Ao terminar, uma janela mostra quantos arquivos foram copiados, pastas criadas e falhas. Em caso de falha, apresenta o primeiro código de erro do Windows e o caminho correspondente. Não informe “sucesso” se o resultado foi parcial.

Não é necessário injetar a DLL em outro processo. Apenas carregá-la não inicia a cópia. Não execute simultaneamente nem altere os arquivos durante a operação. Usar outra conta faz as pastas apontarem para essa outra conta.

## OneDrive e permissões

A pasta de origem deve existir exatamente como `OneDrive\Documentos\fudi o luckyy` no perfil. Uma instalação com outro nome ou localização do OneDrive produzirá erro de origem ausente, sem copiar de uma pasta diferente.

O código permite reparse points da família CLOUD, usados pelo OneDrive para arquivos sob demanda, e recusa links simbólicos, junctions e tags desconhecidas. O Windows/OneDrive pode precisar baixar um arquivo antes de copiá-lo; erros de hidratação, permissões e arquivos bloqueados são reportados. Não toma posse nem altera permissões.

## Integração por LoadLibrary/GetProcAddress

`Files.h` declara três exportações C:

```cpp
void PerformFilesCopy();                       // Sem janela de resultado.
DWORD FilesGetLastStatus();                    // 0 = sucesso; outro = erro Win32.
void CALLBACK AplicarArquivos(HWND, HINSTANCE, LPSTR, int); // rundll32, com resultado.
```

Consulte `FilesGetLastStatus` na mesma thread após `PerformFilesCopy`. Não chame `PerformFilesCopy` de `DllMain`: a operação deve acontecer fora do loader lock.

## Compilar

No Visual Studio, instale “Desenvolvimento para desktop com C++”. Abra o **x64 Native Tools Command Prompt**, entre nesta pasta e execute `compilar.bat`. O script usa C++20 e compila somente a DLL, sem executar cópias.

O binário incluído foi compilado com MinGW-w64 GCC 14.2, C++20, runtime C++ estático e avisos tratados como erros. Depende apenas de bibliotecas do Windows.

## Validação

**18 verificações passaram no Wine**, usando somente pastas artificiais: cópia recursiva; integridade de um binário de 1 MiB; subpastas vazias; arquivos ocultos; preservação de arquivos extras e da origem; repetibilidade; criação de CitizenFX; cópia e substituição do XML; destino bloqueado; preservação do arquivo anterior; descarte do temporário após substituição bloqueada; origem ausente; recusa de cópia recursiva sobre a própria origem; carregamento da DLL e exportações; remoção dos arquivos artificiais.

Os testes não chamam as exportações de cópia real e não acessam as suas pastas OneDrive/FiveM/CitizenFX. Para testar no Windows:

```bat
cl /nologo /std:c++20 /EHsc /W4 /MT tests\testes.cpp /Fe:TestesFiles.exe /link shell32.lib ole32.lib uuid.lib user32.lib
TestesFiles.exe "%CD%\Files.dll"
```

**Não foi validada no Windows 11 Pro real ou com placeholders reais do OneDrive.** Os testes no Wine não comprovam permissões, disponibilidade ou integridade dos arquivos da instalação do usuário.

SHA-256 de `Files.dll`:

```text
f72be3144a3046af594de8933f0fcb7b69562d7e12d6273afd0537d9b2b0473e
```

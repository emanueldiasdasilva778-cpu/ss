# Registro.dll — Windows x64

DLL separada para remover o valor `LastKey` do Regedit e valores de histórico reconhecidos do BAM. A operação é permanente, sem backup nem confirmação inicial. **Nenhuma chave é excluída.** Não altera configurações de serviços nem permissões do Registro.

## Executar

Feche o Regedit para impedir que ele grave novamente `LastKey` ao encerrar. Abra um **Prompt de Comando de 64 bits como administrador do mesmo usuário**, ajuste o caminho e execute:

```bat
rundll32.exe "C:\caminho\Registro.dll",LimparRegistro
```

A janela final informa valores removidos/preservados, chaves BAM examinadas, falhas e acessos negados. Carregar a DLL não executa a limpeza automaticamente; a chamada à exportação é necessária.

Elevar com outra conta faz `HKEY_CURRENT_USER` apontar para essa outra conta. Mesmo como administrador, o Windows pode impedir a gravação no BAM. A DLL informa acesso negado; não toma posse, altera ACLs, eleva privilégios ou contorna a proteção. O histórico pode ser criado novamente pela atividade normal do Windows.

## Escopo exato

- Em `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Applets\Regedit`, apaga **somente o valor `LastKey`**. Preserva a chave e os demais valores.
- Percorre `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\bam\State\UserSettings` e suas subchaves, incluindo as de SID.
- No BAM, reconhece como registros de histórico os valores não padrão do tipo **`REG_BINARY` com 24 bytes**. Abrange nomes de caminhos e identidades de aplicativos desse formato. Preserva `Version` e `SequenceNumber`, sem distinguir maiúsculas/minúsculas, e preserva outros tipos e formatos desconhecidos. Não promete remover registros de formatos diferentes.
- Preserva todas as chaves, inclusive subchaves vazias. A DLL usa `RegDeleteValueW`, nunca exclusão de chaves. Não modifica outros locais do Registro.
- Não solicita dados de histórico para mostrar na tela e não cria arquivos de log. Não deve executar simultaneamente com edições manuais nessas chaves.

## Compilar

No Visual Studio, instale “Desenvolvimento para desktop com C++”. Abra o **x64 Native Tools Command Prompt**, entre nesta pasta e execute:

```bat
compilar.bat
```

A compilação não executa nenhuma limpeza. O binário incluído foi compilado com MinGW-w64 GCC 14.2, C++17, runtime C++ estático e avisos tratados como erros. Depende somente de DLLs do Windows: ADVAPI32, KERNEL32, msvcrt e USER32. Exporta `LimparRegistro` (assinatura do rundll32) e `LimparRegistroSilencioso` (retorna 0 sem falhas, 1 em falha parcial).

## Testes e limites

**15 verificações passaram no Wine**, usando exclusivamente uma árvore artificial em `HKEY_CURRENT_USER\Software\RegistroDllTestes`. Foram verificados: exclusão de `LastKey`; preservação de outros valores/chaves; repetição sem valor existente; exclusão de registros BAM em múltiplos níveis; preservação de metadados, valores padrão e formatos desconhecidos; manutenção de subchaves vazias; repetibilidade; comparação sem distinguir maiúsculas; erro de leitura sem exclusão; carregamento do binário e exportações; remoção da árvore artificial do teste.

Os testes **não chamam as exportações de limpeza real** e não tocam os caminhos reais de Regedit/BAM. Para executar os testes no Windows, compile e informe o caminho absoluto da DLL:

```bat
cl /nologo /std:c++17 /EHsc /W4 /MT tests\testes.cpp /Fe:TestesRegistro.exe /link advapi32.lib user32.lib
TestesRegistro.exe "%CD%\Registro.dll"
```

**Não foi validada em Windows 11 Pro real.** O teste no Wine não comprova permissões de escrita no BAM do Windows, nem representa todos os comportamentos de links do Registro. Não há garantia de limpeza integral diante de bloqueios, alterações concorrentes ou formatos desconhecidos.

SHA-256 do binário:

```text
0d869982cd1f06c69d32a3eeff1bee02245f3c665e941d45b72c49f5a1ee5c4a
```

## Versão 2: administrador e relatório

O início verifica o token elevado e recusa alterações sem elevação. Não há mudança de ACLs ou tentativa de obter SYSTEM. A exportação `const wchar_t* RegistryGetLastReport()` fornece contagens e os caminhos exatos das falhas, com código e mensagem do Windows. Consulte na mesma thread, antes de descarregar a DLL. O FSC 2 mostra esse relatório com F1.

Inclua o arquivo compartilhado `../common/Diagnostics.h` ao compilar. O critério de histórico e a preservação das chaves/metadados continuam os mesmos.

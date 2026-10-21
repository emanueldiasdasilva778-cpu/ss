# Limpeza.dll — Windows x64

Biblioteca C++ para executar a limpeza solicitada usando as pastas do usuário conectado. O binário compilado está em [`Limpeza.dll`](Limpeza.dll); o código está em [`limpeza.cpp`](limpeza.cpp).

**A execução exclui dados permanentemente, sem backup nem confirmação inicial.** Saves, mods e configurações podem ser perdidos; excluir DigitalEntitlements pode exigir nova autenticação. Feche FiveM, GTA e Rockstar Launcher antes de executar.

## Executar

Baixe a DLL. Em um Prompt de Comando de 64 bits como administrador **do mesmo usuário**, ajuste o caminho e execute:

```bat
rundll32.exe "C:\caminho\Limpeza.dll",Limpar
```

A janela final informa quantidades excluídas, falhas e caminhos ignorados. Se usar outra conta na elevação, as pastas pessoais e a Lixeira serão dessa outra conta. Carregar a DLL por si só não dispara a limpeza; ela ocorre na chamada à função exportada.

## Alvos

- `Documentos\Rockstar Games` e `Documentos\Rockstar`, se existirem: remove a pasta inteira.
- `%LOCALAPPDATA%\DigitalEntitlements`: remove a pasta inteira.
- `%PROGRAMDATA%\Microsoft\Windows\WER`: remove o conteúdo.
- `%LOCALAPPDATA%\FiveM\FiveM.app\logs`, `crashes` e `mods`: remove o conteúdo.
- `%LOCALAPPDATA%\FiveM\FiveM.app\data`: remove o conteúdo **exceto `game-storage` e tudo dentro dela**.
- `%WINDIR%\Temp`, `%LOCALAPPDATA%\Temp` e `%WINDIR%\Prefetch`: remove o conteúdo.
- `%TEMP%`: limpa quando aponta para essas pastas temporárias, suas subpastas, ou `%WINDIR%\SystemTemp`. Caminhos personalizados fora dessas árvores são recusados.
- Lixeira do usuário em todas as unidades: esvaziamento pela API oficial do Windows, incluindo os metadados dos itens esvaziados.

Downloads é preservado. Não há apagamento de eventos do Windows, USN Journal ou outros registros do sistema. Não existe uma operação separada para “logs da Lixeira”. Limpar Prefetch não é necessário para melhorar desempenho.

Arquivos bloqueados, somente leitura ou sem permissão podem permanecer. O código não toma posse, altera ACLs ou encerra processos; recusa links e junctions. Não execute simultaneamente nem altere os alvos durante a limpeza.

## Compilar

Instale “Desenvolvimento para desktop com C++” no Visual Studio. Abra o **x64 Native Tools Command Prompt**, entre nesta pasta e execute:

```bat
compilar.bat
```

O script compila somente a DLL; não executa a limpeza. O binário incluído foi produzido com MinGW-w64 GCC 14.2 e runtime C++ estático; depende apenas de bibliotecas do Windows.

## Validação e limites

A compilação com avisos tratados como erros passou. Em pastas artificiais no Wine, passaram 14 verificações: preservação de `game-storage` sem distinguir maiúsculas, exclusão dos demais conteúdos, manutenção da pasta `data`, repetibilidade, exclusão de pasta inteira, pasta ausente, recusa de raiz de volume, limites da pasta temporária, arquivo em uso, arquivo somente leitura, carregamento do binário, funções exportadas e remoção dos arquivos de teste. Duas verificações de links/reparse points ficaram sem validação porque o Wine não representou os links como reparse points.

**Não foi testada em Windows 11 Pro.** Os testes não comprovam que todos os alvos reais podem ser apagados nem substituem validação nesse sistema.

SHA-256 do binário incluído:

```text
01af3522fe281f9002981965a455fb0bc92bca093eb2ea8c8f87cb8d28004244
```

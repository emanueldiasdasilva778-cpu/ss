# FSC

Interface nativa Windows x64 em C++20 para as três DLLs existentes. O código da interface está inteiro em [`main.cpp`](main.cpp). Usa Win32, Direct2D e DirectWrite, sem navegador, ImGui ou dependências externas de UI.

Prévia capturada no Wine, com fonte substituta:

![FSC em janela larga](preview-desktop.png)

[Ver layout em janela estreita](preview-mobile.png)

## Usar

Baixe e extraia **FSC-Windows-x64.zip**, mantendo os quatro binários na mesma pasta:

```text
FSC.exe
Limpeza.dll
Registro.dll
arquivos.dll
```

Abra `FSC.exe`. O manifesto solicita elevação pelo UAC padrão do Windows para as operações que precisam de acesso administrativo. Use a mesma conta do seu perfil: elevar com outra conta muda as pastas pessoais e `HKEY_CURRENT_USER` usados pelas DLLs.

Pressione e segure um card por **2000 ms**. Soltar antes, sair do card ou perder o foco cancela o hold e zera a borda. O início da operação ocorre no primeiro evento/frame com tempo acumulado de pelo menos 2000 ms; o Windows não oferece agendamento de tempo real.

**O hold dispara a operação real, sem outra confirmação.** A Limpeza e o Registro podem excluir dados permanentemente; Arquivos substitui arquivos de mesmo nome. O aplicativo mantém o comportamento das DLLs publicadas anteriormente, sem ampliar seu escopo.

## Design e interação

- Fundo preto puro com pequenos pontos discretos e somente tons de cinza/branco.
- Título centralizado com espaçamento entre letras. Seleciona Inter, Space Grotesk, Segoe UI Variable Display ou uma fonte sans-serif alternativa instalada.
- Exatamente três cards quadrados com ícone e nome. Três colunas em janelas largas, empilhados abaixo de 760 DIPs.
- Contorno vetorial progressivo no perímetro, equivalente visual à borda SVG solicitada; é desenhado nativamente com Direct2D, sem carregar SVG no navegador.
- Percentual durante o hold. Ao iniciar a DLL, uma animação discreta indica trabalho em andamento.
- Check/flash e aviso de conclusão somente quando a DLL retorna sucesso. Falhas ou resultados parciais recebem mensagem própria.
- Estado visual retorna suavemente ao normal. Não há métricas, barra de status ou subtextos decorativos.
- `Tab`/`Shift+Tab` ou setas mudam o foco; segure `Espaço` ou `Enter` para executar. `Esc`/`Alt+F4` fecham a janela quando não há tarefa em andamento.
- Controles discretos de minimizar/fechar, arraste pela faixa superior e redimensionamento pelas bordas. Suporte a DPI por monitor.

## Integração exata

| Card | Arquivo esperado | Funções usadas |
| --- | --- | --- |
| Limpeza | `Limpeza.dll` | `int LimparPc()` |
| Registro | `Registro.dll` | `int LimparRegistroSilencioso()` |
| Arquivos | `arquivos.dll` | `void PerformFilesCopy()` e `DWORD FilesGetLastStatus()` |

`arquivos.dll` é a `Files.dll` criada anteriormente, renomeada no pacote. Esses nomes correspondem à imagem enviada.

As bibliotecas são carregadas pelo caminho absoluto da pasta do executável, usando `LoadLibraryExW`/`GetProcAddress`. A pasta de trabalho atual não é usada para localizar as DLLs. Exportações incompatíveis são rejeitadas sem chamada; não se chama uma função de `rundll32` com uma assinatura diferente.

As operações rodam em thread separada, uma por vez. O status de Arquivos é consultado nessa mesma thread. A janela continua responsiva; o fechamento espera a operação retornar. Não há interrupção forçada de cópias ou exclusões. Uma DLL defeituosa ainda pode encerrar o processo: não há isolamento de execução de plugins.

`LimparPc` e `LimparRegistroSilencioso` fornecem apenas sucesso/resultado parcial. Por isso o rodapé não apresenta detalhes de cada falha interna. Os avisos de acesso negado e bloqueios continuam sendo responsabilidade dessas DLLs.

## Compilar no Visual Studio 2022

Instale **Desenvolvimento para desktop com C++** e o Windows SDK. Abra `FSC.sln`, selecione **Release / x64** e compile. O projeto copia as DLLs das pastas vizinhas e renomeia `Files.dll` para `arquivos.dll` no diretório de saída.

Ou abra o **x64 Native Tools Command Prompt** nesta pasta e execute:

```bat
compilar.bat
```

Saída: `bin\x64\Release\FSC.exe`, acompanhado das três DLLs. O script não executa nenhuma ação de manutenção. `main.cpp` também pode ser usado em um projeto Windows C++20, vinculando `d2d1.lib`, `dwrite.lib`, `ole32.lib`, `user32.lib`, `gdi32.lib` e incluindo o recurso/manifesto fornecido. Salve como UTF-8 e compile com `/utf-8`.

## Validação

O executável distribuído foi compilado para Windows x64 com MinGW-w64 GCC 14.2, runtime C++ estático e avisos tratados como erros.

**19 verificações automatizadas passaram no Wine**: limite 1999/2000 ms, disparo único, cancelamento, teclado, disposição dos três quadrados, janela estreita, tamanho mínimo, carregamento e chamadas das três DLLs simuladas, resultados parciais, propagação de erro de cópia, DLL ausente, exportação errada e ação inválida.

Também foram executadas **6 verificações interativas com DLLs simuladas**: hold curto sem chamada; hold completo com uma chamada; DLL ausente; card Arquivos; cancelamento ao sair de um card; navegação e hold pelo teclado. Foram inspecionados os layouts largo e estreito. Os testes gráficos usam fontes substitutas do Wine.

Os testes **não executaram exclusões, alterações de Registro ou cópias reais nas pastas do usuário**. Para repetir os testes de lógica/integração com DLLs simuladas no Visual Studio:

```bat
tests\testar.bat
```

Não substitua os mocks pelas DLLs reais durante esses testes.

**Ainda não foi validado em Windows 11 Pro real, nem compilado com o MSVC neste ambiente Linux.** A compilação MinGW e os testes no Wine não comprovam permissões administrativas, UAC, todas as fontes, renderização em todos os drivers ou o funcionamento das três DLLs com os dados reais do usuário.

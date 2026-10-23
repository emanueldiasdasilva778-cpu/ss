# Valores do primeiro par aplicados ao segundo par

**Regra solicitada:** os dois primeiros arquivos enviados fornecem os valores. Os dois segundos arquivos são a base que recebe esses valores.

## Arquivos completos

- `visualsettings.dat`
- `tcinstbox_types.meta`

## O que foi feito

### visualsettings.dat

A ordem e os agrupamentos da segunda versão foram mantidos. Cada parâmetro comum recebeu o valor da primeira versão. A única chave exclusiva da segunda, `pedincar.lod.distance.high.x64`, foi preservada com seu valor original.

Resultado: **1.087 nomes de chave e 1.092 linhas de parâmetros**. Foram substituídos **478 valores diferentes**. As cinco repetições que já existiam nos arquivos foram preservadas por ordem de ocorrência, inclusive `misc.coronas.flareScreenCenterOffsetVerticalSizeMult` com `0` e `0.95`, exatamente como no primeiro envio. Nenhuma regra nova de precedência do motor foi presumida e nenhum ajuste extra de tuning foi acrescentado.

### tcinstbox_types.meta

Foram mantidos **28 registros com IDs únicos** da base. Todos receberam os valores correspondentes da primeira versão, incluindo limites `aabb`, `percentage`, `range` e horários. Assim, `percentage` e `range` ficam zerados em todos os registros, inclusive no ID `1609496462`, conforme o primeiro envio.

Os quatro originais continuam intactos. Os comentários de crédito da primeira origem eram `neiv visual` e `neiv PACKv4`; a formatação e os comentários da segunda base foram usados nos arquivos finais.

## Download e aplicação

O ZIP contém os dois arquivos completos, este README e o relatório detalhado de comparação. Não contém um novo RPF ou instalador.

Feche o FiveM e faça backup. Substitua os arquivos nos mesmos locais do pack que você já usa, respeitando o `assembly.xml`. Se forem carregados diretamente pela pasta citizen, os caminhos habituais são:

```text
%LOCALAPPDATA%\FiveM\FiveM.app\citizen\common\data\visualsettings.dat
%LOCALAPPDATA%\FiveM\FiveM.app\citizen\common\data\tcinstbox_types.meta
```

Se o seu pack os carrega dentro de um RPF, atualize os arquivos dentro dele nos caminhos definidos por seu manifesto. DAT e META soltos em `mods` não substituem um RPF carregável.

## Conferência

Cada ocorrência de parâmetro foi conferida contra os valores do primeiro envio; as exclusivas do segundo foram verificadas separadamente. O XML foi reaberto, e todos os campos foram comparados por ID. A integridade do ZIP foi conferida. `RELATORIO_MESCLAGEM.json` mostra valores anteriores, valores aplicados e hashes dos originais; `SHA256SUMS.txt` identifica os arquivos publicados.

Não houve teste dentro do FiveM ou medição de FPS. A mesclagem aplica os valores enviados; não certifica efeitos visuais ou funções que dependam de outros arquivos do jogo.

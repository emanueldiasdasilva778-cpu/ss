# Ajuste de renderização — versão de teste 1

Pacote preparado a partir do `timecycle.zip` e do `gta5_settings.xml` enviados para investigar o cenário distante/fundo preto. **Não foi testado dentro do FiveM.** Ele corrige defeitos verificáveis e propõe ajustes; não comprova uma causa única para a captura.

## O que foi encontrado e alterado

- `timecycle_mods_2.xml` tinha um `</modifier>` sem abertura correspondente: XML corrigido.
- Centenas de modificadores declaravam parâmetros que estavam ausentes. `numMods` foi alinhado aos campos existentes, mantendo os efeitos que já estavam vazios como vazios.
- Duas definições vazias repetidas no mods_1 foram consolidadas; os nomes e campos dos demais modificadores foram preservados.
- `TASVY.xml` tinha 15 curvas de cor do céu por região com 12 amostras, enquanto o restante usava 13, e `sky_hdr=0.2` em todos os horários. Essas 16 curvas foram substituídas pelas correspondentes de EXTRASUNNY da referência citada no relatório, com 13 amostras e variação por horário. Isso não adiciona texturas ou nuvens HD.
- A densidade/opacidade de neblina e haze permaneciam ativas, com brilho zerado; no timecycle fornecido foram zeradas as densidades, opacidades e intensidades de fogray/volumes para reduzir essa possível fonte de fundo preto. Isso vale para este clima; não é promessa de desligamento em todos os climas/recursos do servidor.
- Os campos repetidos da mesma região foram consolidados pela última declaração. Três segundas ocorrências `slod2` foram corrigidas para `slod3`, conforme a estrutura da referência, preservando suas curvas distintas.
- Bloom foi zerado no clima fornecido. As demais opções de aparência, inclusive iluminação dos personagens, foram preservadas.
- O `far_clip` do clima **já era 15.000 e foi mantido**. Foram removidos do mods_4 overrides de far_clip de até 500 e multiplicadores LOD abaixo de 1; se esses modificadores forem ativados, deixam de aplicar tais reduções. Outros efeitos de cada modificador foram preservados.
- Settings: `LodScale 0 -> 0.5`, `PedLodBias -0.15 -> 1.0`, `VehicleLodBias 0 -> 1.0`. `GrassQuality=0`, `MaxLodScale=0`, resolução, Hz, DX, buffers, áudio e demais opções foram preservados.
- `visualsettings.dat` e `tcinstbox_types.meta` são cópias **idênticas** da mesclagem anterior, que aplica os valores do primeiro par no segundo. Essa instrução não foi revertida.

## Instalação

Feche o FiveM e faça backup do que está em uso.

1. `gta5_settings.xml` vai para `%APPDATA%\CitizenFX\gta5_settings.xml`.
2. Substitua os arquivos de `timecycle/`, `visualsettings.dat` e `tcinstbox_types.meta` nos caminhos virtuais do pack que já os carrega. Se ele usa um RPF, substitua **dentro desse RPF**, respeitando seu `assembly.xml`. DAT/META/XML soltos em `mods` não formam um mod carregável.
3. Se usa edição direta em citizen, o diretório habitual é `%LOCALAPPDATA%\FiveM\FiveM.app\citizen\common\data\`, com a subpasta `timecycle`.

### Atenção aos nomes

O ZIP recebido continha `TASVY.xml` (clima EXTRASUNNY) e `timecycle_mods_4 (1).xml`. Esses **não são os nomes padrão** `w_extrasunny.xml` e `timecycle_mods_4.xml`. Um manifesto pode mapear nomes personalizados, por isso os originais foram preservados.

A pasta **`aliases_padrao` é opcional e não deve ser copiada inteira como uma instalação automática**. Ela oferece os mesmos dois arquivos corrigidos com os nomes padrão. Use-os somente se os destinos do seu pack/citizen forem os nomes padrão. Substitua o arquivo correspondente; não carregue duas cópias simultaneamente. O manifesto não foi enviado, então não foi possível confirmar qual nome o cliente realmente carrega.

O ZIP também não continha `w_clear.xml`, os demais arquivos de clima ou o manifesto. O ajuste de céu/neblina cobre o clima EXTRASUNNY fornecido, não recria climas ausentes.

## Teste no cliente

Abra o mesmo servidor e confira o mesmo lugar/clima após cada etapa: primeiro o timecycle corrigido com os settings antigos; depois os settings ajustados. Isso ajuda a separar o efeito visual da mudança de distância. Se os arquivos forem sobrescritos ou ignorados por outro pack, o resultado não muda até corrigir a precedência/caminho de carregamento.

Maior LOD pode custar FPS e aumentar também o detalhe de objetos/árvores: `GrassQuality=0` não cria um controle exclusivo de todas as árvores. Arquivos locais não ampliam o alcance de jogadores transmitidos pelo servidor. Não há garantia de manter os 210 FPS da captura.

## Conferência e referência

Todos os XMLs foram reabertos. Curvas finais têm 13 amostras, `numMods` foi conferido e os arquivos preservados foram comparados por hash. `RELATORIO.json` discrimina alterações e `SHA256SUMS.txt` identifica os arquivos publicados.

Referência das 16 curvas de céu, sem substituir o restante do arquivo:
https://github.com/robi29/VisualV/blob/5807891caadc0b3054d276918b9621580c5902a9/original%20files/update.rpf/common/data/timecycle/w_extrasunny.xml

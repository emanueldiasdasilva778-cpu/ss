# Renderização — versão de teste 2: fundo do céu restaurado

Esta versão corrige a alteração indevida do fundo do céu na versão 1. As 15 curvas RGB e o sky_hdr voltaram aos valores do timecycle.zip original do usuário, mantendo o fundo escuro pretendido para as imagens do céu personalizado. Não foram usadas as cores azuis da referência EXTRASUNNY.

As curvas RGB originais tinham 12 amostras: foi repetida a última para completar 13, sem alterar nenhuma cor original. O sky_hdr original foi preservado com suas 13 amostras. Isso preserva os parâmetros do fundo; não modifica as texturas das imagens.

## Ajustes mantidos

- Correção do XML inválido no mods_2, contagens numMods e definições vazias duplicadas.
- Correção dos nomes slod3 e consolidação dos campos repetidos pela última declaração.
- Neblina e bloom zerados no clima fornecido; far_clip de 15000 preservado.
- Remoção das 18 restrições de distância selecionadas no mods_4.
- Settings: LodScale=0.5, PedLodBias=1.0 e VehicleLodBias=1.0. Demais opções preservadas.
- visualsettings.dat e tcinstbox_types.meta continuam idênticos à mesclagem anterior.

## Instalação

Feche o FiveM e faça backup dos arquivos em uso. Substitua os arquivos da versão 1 pelos da versão 2 nos mesmos destinos.

1. Coloque gta5_settings.xml em `%APPDATA%\CitizenFX\gta5_settings.xml`.
2. Substitua timecycle/, visualsettings.dat e tcinstbox_types.meta nos caminhos do pack que já os carrega. Caso use RPF, substitua dentro do RPF nos destinos definidos pelo manifesto. Arquivos soltos em mods não formam um pack carregável.
3. Em instalações que usam citizen diretamente, o caminho habitual é `%LOCALAPPDATA%\FiveM\FiveM.app\citizen\common\data`, com a subpasta timecycle.

Os nomes recebidos TASVY.xml e timecycle_mods_4 (1).xml foram mantidos. A pasta aliases_padrao contém cópias equivalentes como w_extrasunny.xml e timecycle_mods_4.xml: use somente se esses forem os nomes de destino do seu pack. Não carregue as duas cópias simultaneamente. O manifesto não foi fornecido, portanto o mapeamento ativo não foi confirmado.

## Verificação e limites

XMLs reabertos, curvas conferidas com 13 amostras, contagens e integridade ZIP verificadas. A restauração das cores/HDR foi comparada ao ZIP original. Os demais parâmetros de renderização da versão 1 foram preservados.

Não foi possível executar o FiveM neste ambiente. Confira o resultado no mesmo clima e servidor. O pacote cobre o EXTRASUNNY fornecido; outros climas e recursos do servidor podem sobrescrevê-lo. Maior LOD pode custar FPS e não aumenta o alcance de entidades transmitidas pelo servidor.

RELATORIO.json detalha as alterações. backup/ no ZIP contém os arquivos de entrada para reversão. A versão 1 permanece disponível apenas como histórico; use FiveM_Renderizacao_Teste_v2.zip.

# ZeroM$ — bot de vendas para Discord

Bot modular em **Node.js 24 LTS** e **discord.js 14.27.0**, versão estável consultada no registro npm durante a implementação. A única dependência direta de execução é `discord.js`. O `package-lock.json` fixa a árvore de dependências.

O token e as opções da aplicação ficam em **`config.json`**. O bot não lê variáveis de ambiente para obter configuração ou credenciais. Este projeto fica separado do bot Python já existente no repositório.

## Catálogo completo do TXT

O `config.example.json` desta versão contém as oito descrições enviadas pelo operador, com ajustes de acentuação e formatação. A apresentação dos produtos e as afirmações comerciais são conteúdo do operador; não representam testes de compatibilidade, segurança ou garantia feitos pelo bot.

O TXT repetiu `1561129676124983306` para Rockstar e Guest. Conforme a escolha do operador, Guest fica em `1561129676124983306` e Rockstar em `1559215025615274004`. Steam WL fica em `1559214972549202091` e GTA5 Instalável em `1559215094989197444`. Preços e estoques iniciais foram preservados; os quatro produtos únicos continuam com estoque zero até reposição pelo operador.

Os nomes `:Zennify_box:`, `:9504_purple_flame:`, `:44250purplecheck:`, `:shoppingcart:`, `:ant5:`, `:fogue:`, `:144:` e `:star~1:` são resolvidos pelos emojis disponíveis no servidor. O alias numérico `:1298339916803215483:` é procurado como ID. Emojis animados também são suportados. Se não houver um emoji correspondente ou não for possível consultar a lista, são usados equivalentes Unicode; o bot não inventa IDs. O campo `emojis` fica vazio por padrão para permitir a descoberta automática. Códigos reais podem ser definidos nele para substituir a descoberta.

As descrições dos catálogos aceitam até 4000 caracteres de origem. Se a renderização passar de 4096 caracteres, o painel usa um embed de continuação na mesma mensagem, preservando o conteúdo e a imagem. O limite total de 6000 caracteres do Discord continua sendo respeitado; o bot informa quando um texto excede esse limite, sem truncá-lo silenciosamente.

**Entrega:** o TXT menciona entrega automática de contas por DM. Essas frases foram mantidas como conteúdo fornecido pelo operador. Este projeto não possui estoque de credenciais nem rotina de entrega automática. A confirmação do Pix é manual; a entrega precisa ser feita pela equipe ou por uma integração separada antes de anunciar esse serviço como automatizado. Não confunda o estoque numérico com contas/licenças cadastradas para entrega.

Para uma pasta nova no Windows:

```powershell
npm.cmd ci
npm.cmd run importar-catalogo
notepad config.json
```

O importador cria `config.json` a partir do exemplo quando ele ainda não existe. Preencha token e confira guildId, salve e feche o editor. Então execute:

```powershell
npm.cmd run deploy
npm.cmd start
```

Para atualizar uma instalação existente, pare o bot, preserve `config.json` e a pasta `data`, copie o código novo e execute `npm.cmd run importar-catalogo`. A importação atualiza a apresentação, preservando credenciais, IDs e os valores financeiros da configuração ativa; o banco de estoque e pedidos permanece intacto. Na primeira inicialização, configure o estoque real antes de vender.

## Atualizar imagens, descrições e emojis

O bot lê **config.json**. Alterar apenas `config.example.json` não altera a configuração ativa.

Se você editou o exemplo, pare o bot com Ctrl+C e execute na pasta do projeto:

```powershell
npm.cmd run importar-catalogo
npm.cmd start
```

O comando importa nomes, descrições, imagens e o mapa de emojis para `config.json`, preservando token, servidor, canais, preços e estoque. Também aceita outro JSON: `npm.cmd run importar-catalogo -- "C:\caminho\catalogo.json"`. Confira o arquivo de origem antes de importar. O banco não é apagado. Não execute a importação enquanto estiver editando o mesmo arquivo em outro programa.

Você pode editar esses campos diretamente no **config.json**. Com o bot ligado, ele detecta o arquivo a cada 2 segundos e atualiza imagens, nomes, descrições e emojis automaticamente. Token, cargos, servidor e mudanças na lista de produtos exigem reinício. O bot atualiza os painéis existentes, incluindo a imagem; não é necessário apagar mensagens nem o banco. A descrição das opções é sincronizada quando o texto do config muda; se o config continua igual, uma descrição salva por `/edit` é preservada. As imagens devem ser links HTTPS diretos e acessíveis ao Discord. Os quatro banners informados foram preservados. Os canais sem imagem própria recebem o banner PNG incluído em `assets/loja.png`, enviado como anexo pelo bot; não é preciso hospedar a imagem nem tornar o repositório público. Use `defaultImage` para definir outra imagem padrão por URL HTTPS, ou `/edit` para escolher uma imagem específica para cada canal.

Descrições aceitam quebras de linha `\n`, **Markdown**, emojis Unicode e códigos reais do Discord, como `<:nome:123456789012345678>` ou `<a:nome:123456789012345678>`. Esse ID é ilustrativo: substitua pelo ID real. O bot busca emojis do próprio servidor pelo nome. Para escolher um emoji específico, no mapa `emojis` associe `nome` ao código real; então use `:nome:` na descrição. Nomes comuns como `:shopping_cart:` têm alternativa Unicode. Se o nome não existir no servidor e não houver mapeamento, será usado um equivalente Unicode conhecido ou será mantido o alias para correção pelo operador.

No Windows, abra **iniciar.bat** dentro da pasta do projeto. Ele usa a pasta correta, instala dependências ausentes e cria `config.json` somente se ainda não existir. Preserve seu `config.json` e `data/` ao atualizar o código. Nenhum token real acompanha o download.

## Instalação

1. Instale Node.js 24 LTS ou posterior compatível.
2. Entre nesta pasta e instale a árvore fixada:

```bash
cd discord-sales
npm ci
```

3. Se `config.json` ainda não existir, copie o exemplo. **Não sobrescreva uma configuração existente**:

```bash
cp config.example.json config.json
```

No PowerShell, use `Copy-Item config.example.json config.json`, somente se o destino não existir.

4. Edite `config.json` em um editor local. Informe o token da aplicação e o ID do servidor. Nunca coloque o token em chat, issues, commits ou capturas de tela. O arquivo real está no `.gitignore`; o exemplo contém apenas um marcador. No Linux, restrinja a leitura:

```bash
chmod 600 config.json
```

5. No Discord Developer Portal, crie uma aplicação/bot. Convide com os escopos **`bot`** e **`applications.commands`**. Não é necessário ativar intents privilegiadas: o cliente usa apenas `Guilds`.
6. Configure os canais e as permissões descritas abaixo.
7. Valide os arquivos, publique o comando no servidor e inicie:

```bash
npm run check
npm test
npm run deploy
npm start
```

`deploy` registra/atualiza apenas `/edit`; preserva outros comandos da aplicação. Execute novamente ao alterar a definição do comando. O início publica ou atualiza os oito painéis de catálogo nos canais configurados. Mantenha **uma única instância** do processo, com armazenamento local persistente. Encerre com `Ctrl+C` ou `SIGTERM` e aguarde a saída antes de reiniciar.

Em ambientes de nuvem com a pasta pessoal sem escrita, pode-se passar `--cache /tmp/zeroms-npm-cache` ao `npm ci`. Não é preciso desativar TLS ou verificações de instalação.

## Configuração

O arquivo `config.example.json` contém o catálogo completo, as quatro imagens e todos os IDs informados. A cópia `config.json` incluída no pacote tem os mesmos valores e um token fictício.

| Campo | Uso |
| --- | --- |
| `token` | Token secreto do bot. Obrigatório para iniciar ou registrar comandos. |
| `guildId` | ID do servidor. Precisa ser preenchido. Todos os canais/cargos devem pertencer a ele. |
| `staffRoleIds` | Cargos que acessam carrinhos e podem confirmar/cancelar compras. Inicialmente contém `1561137818342920333`. |
| `editRoleId` | Cargo exclusivo para `/edit`: `1561137818342920333`. Deve também constar em `staffRoleIds`. |
| `logChannelId` | Canal de vendas: `1559063532321120386`. |
| `cartCategoryId` | Categoria opcional dos carrinhos; `null` cria fora de categoria. |
| `defaultImage` | Imagem HTTPS padrão para os canais sem imagem própria. `null` usa o banner PNG incluído, enviado como anexo. |
| `color`, `footer` | Aparência dos embeds. Rodapé padrão: `ZeroM$ © Todos os direitos reservados`. |
| `timezone` | Horário dos registros, inicialmente `America/Sao_Paulo`. O banco conserva ISO/UTC. |
| `cartIdleMinutes` | Expiração de carrinhos abertos sem alteração, inicialmente 60 minutos. |
| `reconcileIntervalSeconds` | Intervalo de recuperação/republicação de painéis, inicialmente 30 segundos. |
| `pix.key` | Chave Pix a exibir ao comprador. Pode ficar vazia para atendimento pela equipe. |
| `pix.beneficiary` | Favorecido a exibir. Confira cuidadosamente a conta antes de operar. |
| `pix.instructions` | Instruções de pagamento. Há um texto de atendimento manual como padrão. |
| `products` | Catálogos, canais, imagens, opções e valores iniciais. |

IDs Discord são **strings**, para preservar todos os dígitos. Valores `priceCents` e `startingPriceCents` estão em **centavos inteiros**: `875` representa R$ 8,75. `startingPriceCents` registra o preço inicial solicitado; o painel calcula o menor preço corrente das opções para se manter atualizado.

Os quatro canais de produto único têm os preços exatos pedidos: R$ 0,60; R$ 4,99; R$ 1,50; R$ 1,50. Os nomes e descrições foram preenchidos conforme o TXT: Steam WL, GTA5 Instalável, Contas Rockstar 2FA + Garantia e Conta Guest LV 20–29. Como não foram fornecidas quantidades, começam com **estoque zero**. Defina os nomes no `config.json` e os estoques iniciais antes da primeira execução, ou reponha pelo `/edit`. Eles usam exclusivamente o botão **Adicionar ao Carrinho**, sem menu de seleção.

### Configuração inicial e dados de operação

Na primeira execução o bot cria `data/store.json`, usando os preços, estoques e descrições das opções do `config.json`. Em execuções posteriores, preços e estoques vêm do banco persistido, incluindo alterações feitas por `/edit` e baixas de vendas. Descrições alteradas explicitamente no config são importadas no reinício, sem repor estoque; quando o config não mudou, preserva-se a descrição salva pelo painel. Mudar os números iniciais no JSON de configuração não repõe nem zera o estoque existente. Use `/edit` para operar a loja.

Nomes, imagens, canais, textos gerais, cargos e opções de Pix vêm de `config.json` a cada início. Os nomes das opções também são atualizados a partir da configuração; pedidos já pagos mantêm seu recibo histórico. Não altere IDs de opções já vendidas. Não remova nem mova produtos com pedidos ativos sem antes resolver esses pedidos.

`data/store.json` guarda dados de negócio — pedidos, estoque atual, recibos e IDs das mensagens — e não contém o token. Não apague `data/` para resolver problemas: isso perderia a contabilidade e poderia restaurar estoques iniciais indevidamente.

## Permissões e privacidade dos canais

O bot precisa de **Ver canais**, **Enviar mensagens**, **Inserir links**, **Ler histórico de mensagens**, **Anexar arquivos**, **Gerenciar canais** e **Gerenciar permissões/cargos** onde criar carrinhos. Não conceda Administrador. O código confere os cargos e permissões essenciais durante a inicialização. A categoria escolhida precisa permitir essas ações ao bot.

Os carrinhos são criados com sobrescritas explícitas:

- `@everyone`: não pode ver o canal;
- comprador: pode ver, escrever e ler histórico;
- cargos de `staffRoleIds`: podem ver e atender;
- bot: pode operar o carrinho.

As permissões não são sincronizadas com a categoria. Administradores e o dono do servidor podem acessar canais privados conforme as regras do próprio Discord; o bot não consegue impedir esse acesso. Mantenha o canal de logs restrito à equipe. A publicação dos painéis não altera permissões dos canais de produto ou do canal de logs.

O bot não lê mensagens comuns e não armazena conteúdo de conversas dos carrinhos. Guarda identificadores, nomes, itens, valores, cupom e marcos do pedido. Não peça senhas ou dados bancários. Dados de pagamento e comprovantes devem ser tratados pela equipe com acesso restrito.

## Como comprar

1. Nos quatro catálogos com opções, selecione uma opção disponível. Isso a adiciona ao carrinho. Nos produtos únicos, clique em **Adicionar ao Carrinho**.
2. O bot cria um canal privado, ou reutiliza o carrinho aberto do comprador. Adicionar novamente o mesmo produto aumenta a quantidade. Há um limite de 20 opções diferentes por carrinho.
3. O embed exibe comprador, produtos, quantidade, valor de cada linha, subtotal, cupom e total, com o rodapé solicitado. O valor de cada linha corresponde a quantidade × preço unitário.
4. **📦 Quantidade** abre uma seleção dos itens e um modal para a quantidade. Zero remove uma linha; para remover o último produto, use **⬛ Cancelar**.
5. **🎟️ Cupom** abre o modal. `ZEROMS25` concede 25% somente para subtotal **maior que R$ 3,00**. R$ 3,00 exatos não qualificam. O desconto é arredondado ao centavo mais próximo. Código vazio remove o cupom. Reduzir o subtotal para R$ 3,00 ou menos remove o desconto automaticamente.
6. **🟢 Pagar com Pix** exibe as instruções de pagamento apenas ao comprador. O preço é revisado antes de fechar a cotação. Se mudou, o comprador precisa conferir o carrinho e clicar novamente.
7. Depois de gerar as instruções, os valores ficam congelados e os botões de quantidade/cupom são desativados. Ainda é possível consultar o Pix ou cancelar. Para alterar itens, cancele e crie outra compra. Se já pagou, converse com a equipe antes de cancelar.

Adicionar itens e clicar em Pix **não reserva nem reduz estoque**. Outra compra pode consumir a última unidade antes da confirmação. A equipe deve verificar disponibilidade e pagamento e resolver reposição/reembolso se necessário. O sistema nunca confirma uma venda com estoque negativo. Carrinhos aguardando pagamento não expiram automaticamente, preservando o atendimento de quem pode ter pago.

## Confirmar pagamento e registrar a venda

O botão **✅ Confirmar Pagamento** fica no canal do carrinho. Ele é habilitado somente após o comprador solicitar Pix. Qualquer usuário pode ver o botão; **apenas membros com um cargo de `staffRoleIds` podem executá-lo**, e a associação ao cargo é consultada novamente no momento da ação. Não há liberação especial para administradores sem o cargo configurado.

A equipe deve conferir o **crédito real na conta** e o valor exato da cotação. O clique é uma declaração manual do staff; o bot não consulta banco nem valida comprovantes.

Depois de autorizado, o bot:

1. Grava o pedido pago, o responsável e o recibo, e reduz o estoque em uma transação local única.
2. Atualiza os catálogos com o novo estoque.
3. Envia ao canal de logs usuário/ID, responsável, produtos, quantidades, preços unitários, valor original, cupom, desconto, valor final e data/hora.
4. Salva o ID da mensagem de log.
5. Exclui o canal do carrinho e registra o fechamento.

Cliques duplicados ou simultâneos não reduzem estoque novamente. As operações de pagamento e edição são serializadas e persistidas. Se o Discord falhar, o pedido permanece pago e pendente de finalização; o bot tenta novamente a cada intervalo configurado e após reiniciar. O canal permanece até o log estar salvo e os catálogos atualizados. **Não cobre novamente um pedido já pago por causa de falha no log.**

O envio de mensagens ao Discord e a gravação local não compartilham uma transação. Para recuperar um envio interrompido, o bot procura o marcador do pedido nas 100 mensagens mais recentes do canal de logs. Numa falha muito específica após o envio e antes de salvar o ID, se esse histórico já tiver sido ultrapassado, um log pode se repetir. O ID de pedido permite reconciliar esses casos; a baixa de estoque continua idempotente.

O projeto controla vendas e estoque numérico. **Não envia licenças, arquivos ou credenciais de produtos automaticamente.** Combine a entrega com o comprador antes da confirmação que fecha o canal, ou implemente um processo de entrega separado.

## Painel `/edit` — descrição completa do embed, imagem, valor e estoque

Somente o cargo **`1561137818342920333`** pode usar o comando, seus botões e seus modals. O cargo é consultado novamente em cada ação. Para ocultar o comando dos demais membros na interface, configure a permissão também em **Configurações do servidor → Integrações**.

Execute **`/edit`** e selecione o canal no menu, ou use **`/edit canal:#canal-do-produto`** para abrir diretamente o painel correspondente. O painel mostra uma prévia do embed publicado e dois caminhos:

1. **Descrição completa e imagem do embed:** edita o texto principal inteiro do anúncio e a imagem do canal. Não exige selecionar uma modalidade. O campo de descrição é uma caixa de texto grande, com até **4000 caracteres**, quebras de linha, Markdown e emojis do Discord.
2. **Selecione uma opção para editar valor e quantidade:** abre um painel para a modalidade escolhida. Clique em **Editar embed, valor e quantidade**. Um único modal contém **descrição COMPLETA do embed do canal**, **URL da imagem**, **valor em reais** e **quantidade em estoque**. O texto editado é o anúncio principal compartilhado pelo canal; não é a descrição curta da opção.

A imagem deve ser uma URL HTTPS direta e acessível ao Discord. Deixe o campo vazio para usar a imagem padrão. Todos os oito canais continuam como embeds e recebem imagem. O valor deve ser escrito como `8,75`; o estoque é absoluto (`49`, não um incremento).

Ao salvar, o bot atualiza a mensagem existente no canal. O bloco automático de catálogo, preços e estoque continua sendo gerado a partir dos valores atuais. Se o Discord estiver indisponível ou negar permissão, as alterações ficam salvas e a atualização será tentada novamente. Descrições que excedam o limite total de 6000 caracteres do Discord após resolver os emojis são recusadas antes da gravação; não há truncamento silencioso.

A descrição e a imagem editadas ficam persistidas em `data/store.json`, em `embeds`, e sobrevivem a reinícios. O `config.json` mantém os valores de origem. Sincronizar a mesma configuração não apaga alterações feitas pelo painel; uma mudança explícita na descrição ou imagem do config passa a valer para esse campo. Uma instalação antiga migra esses registros automaticamente sem repor estoque nem apagar pedidos.

Cada embed e cada opção têm revisões. Se outra pessoa mudar o anúncio ou uma venda alterar o estoque enquanto um formulário estiver aberto, o formulário antigo é recusado. Reabra `/edit` para carregar os dados atuais. A alteração combinada é atômica: uma imagem ou descrição inválida não salva parcialmente um novo valor ou estoque.

Depois de atualizar esta versão, execute **`npm.cmd run deploy`** para registrar a opção `canal` do comando. Feche os painéis antigos e abra `/edit` novamente.

## Persistência, backup e recuperação

- A escrita usa arquivo temporário, `fsync` do arquivo e substituição atômica. Em sistemas POSIX, também executa `fsync` do diretório; essa operação não é suportada pelo Node.js no Windows e não é executada nessa plataforma. A resistência a perda de energia depende das garantias do sistema de arquivos. Transações são processadas uma por vez.
- `data/instance.lock` impede duas instâncias sobre o mesmo banco. **Não use múltiplas réplicas, shards com o mesmo diretório ou armazenamento de rede**. O desenho é para um processo em um host, com volume local persistente.
- Falhas de escrita no banco bloqueiam novas transações até revisão e reinício. Corrija espaço, permissões ou falha do volume antes de retomar. O log identifica a classe e o código de erro, sem imprimir tokens ou objetos de interação.
- Um arquivo de banco inválido interrompe a inicialização; o bot não o substitui por estoque novo.
- Pare o processo antes de fazer backup. Copie `data/store.json` e proteja uma cópia de `config.json` separadamente. Não distribua backups de produção com o pacote do código.
- Para restaurar, mantenha o bot parado, restaure os arquivos, confirme servidor/canais/IDs e inicie uma única instância. As finalizações pendentes são retomadas.
- Se houve desligamento forçado, pode restar `data/instance.lock`. Verifique o PID registrado e confirme que **nenhum processo usa esse diretório**. Só então remova esse arquivo e reinicie. Nunca remova um bloqueio de uma instância viva.
- Se um canal de carrinho em criação já existir, seu marcador no tópico permite recuperá-lo. Não altere o tópico `zeroms-cart:...`.
- Painéis excluídos são recriados no próximo ciclo. O bot preserva mensagens comuns do canal.
- Carrinhos abertos expiram após o tempo de inatividade configurado. Cancelamento/expiração não reduzem estoque. Carrinhos pagos e aguardando pagamento têm tratamento separado.

Para operação contínua, use um supervisor de processos com **uma instância**, usuário de sistema sem privilégios, reinício monitorado e tempo suficiente para encerramento gracioso. Acompanhe falhas de permissão, disco e conexão. Os pedidos e recibos são mantidos no banco; defina sua política de retenção e seus backups antes de operar com clientes reais.

## Estrutura

```text
discord-sales/
├── assets/loja.png             # Banner padrão anexado aos embeds sem imagem própria
├── config.json                 # Configuração local e token; ignorado pelo Git
├── config.example.json         # Exemplo completo sem credenciais reais
├── config/load.js              # Leitura e validação da configuração
├── commands/edit.js            # Comando de administração
├── events/
│   ├── clientReady.js          # Inicialização e recuperação periódica
│   └── interactionCreate.js    # Botões, seleções e modals com autorização
├── utils/
│   ├── discord-shop.js         # Canais, painéis e finalização no Discord
│   ├── shop.js                 # Regras de carrinho, estoque e pedidos
│   ├── store.js                # Persistência transacional local
│   ├── ui.js                   # Embeds, componentes e modals em português
│   ├── money.js                # Centavos, desconto e validação de entrada
│   └── errors.js               # Erros de usuário e logs sem segredos
├── tests/                     # Regras financeiras, persistência, UI e Discord simulado
├── data/                      # Criado na primeira execução; ignorado pelo Git
├── deploy-commands.js
├── index.js
├── package.json
└── package-lock.json
```

## Validação e homologação

```bash
npm run check
npm test
npm audit --omit=dev
```

Os testes locais exercitam cupom na fronteira de R$ 3,00, centavos, permissões de cargo/comprador, estoque concorrente, pagamento duplicado, revisões de modal, atualização de embeds, reinício, falhas de envio/exclusão e recuperação dos pedidos. Usam persistência real em diretórios temporários e um adaptador simulado para a API Discord.

**O login real e as interações num servidor Discord não foram homologados nesta entrega**, pois não foram fornecidos token nem ID do servidor. Os resultados dos testes não demonstram permissões reais ou recebimento bancário. Antes de atender clientes, faça uma compra de teste num servidor controlado: confira isolamento do canal com uma conta comum, edição com/sem cargo, cotação e cupom, pagamento manual, baixa única, log e exclusão. Simule perda de permissão de logs, restaure-a e confira a recuperação. Verifique também imagens e nomes dos produtos únicos. Somente após essa homologação promova a operação para produção.

**Aviso sobre Pix:** a automação real de pagamentos exige integração futura com uma API de Pix/provedor de pagamentos, incluindo cobrança vinculada ao pedido, webhooks autenticados, validação de valor/status, idempotência e conciliação. Este bot usa exclusivamente confirmação manual do staff e não gera QR Code bancário nem comprova recebimentos.

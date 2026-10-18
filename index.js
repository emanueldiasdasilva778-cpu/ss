import { Client, Events, GatewayIntentBits } from 'discord.js';
import { join } from 'node:path';
import { loadConfig, root } from './config/load.js';
import { Store } from './utils/store.js';
import { Shop } from './utils/shop.js';
import { DiscordShop } from './utils/discord-shop.js';
import { UserError, report } from './utils/errors.js';
import { handleInteraction } from './events/interactionCreate.js';
import { onReady } from './events/clientReady.js';

let context;
let closing = false;
async function shutdown(code = 0) {
  if (closing) return;
  closing = true;
  process.exitCode = code;
  if (!context) return;
  context.stopping = true;
  context.ready = false;
  context.stopWatching?.();
  clearInterval(context.timer);
  // Não libere o bloqueio enquanto ainda houver efeitos Discord em andamento.
  await context.startup?.catch(() => {});
  await Promise.allSettled([...context.interactions]);
  await context.discord.serial;
  await context.client.destroy();
  await context.store.close();
}

async function main() {
  const config = await loadConfig();
  if (closing) return;
  const store = await new Store(join(root, 'data')).init(config.products);
  if (closing) { await store.close(); return; }
  // Só Guilds: sem leitura de mensagens, sem Members intent privilegiado, sem conteúdo privado.
  const client = new Client({ intents: [GatewayIntentBits.Guilds], allowedMentions: { parse: [] } });
  const shop = new Shop(store, config);
  const discord = new DiscordShop(client, config, shop, store);
  context = { config, store, shop, client, discord, ready: false, stopping: false,
    timer: null, startup: null, interactions: new Set() };
  client.on(Events.Error, error => report('Cliente Discord', error));
  client.on(Events.Warn, () => console.warn('Aviso do Discord: confira conectividade e limites de requisição.'));
  client.on(Events.InteractionCreate, interaction => {
    if (context.stopping) return;
    const task = handleInteraction(interaction, context);
    context.interactions.add(task);
    void task.finally(() => context.interactions.delete(task));
  });
  client.once(Events.ClientReady, () => {
    if (context.stopping) return;
    context.startup = onReady(context);
    context.startup.catch(async error => {
      if (error instanceof UserError) console.error(error.message);
      else report('Falha ao iniciar a loja', error);
      await shutdown(1);
    });
  });
  await client.login(config.token);
}

process.once('SIGINT', () => { void shutdown(); });
process.once('SIGTERM', () => { void shutdown(); });
process.on('unhandledRejection', error => {
  report('Erro assíncrono não tratado; encerrando com segurança', error);
  void shutdown(1);
});
process.on('uncaughtException', error => {
  report('Erro fatal; encerrando com segurança', error);
  void shutdown(1);
});

main().catch(async error => {
  if (error instanceof UserError) console.error(error.message);
  else if (error.message === 'INSTANCE_LOCKED') console.error('Outra instância está usando data/instance.lock. Leia a recuperação segura no README antes de remover o bloqueio.');
  else report('Falha na inicialização; confira token, permissões e conectividade', error);
  await shutdown(1);
});

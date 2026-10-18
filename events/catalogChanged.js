import { watchFile, unwatchFile } from 'node:fs';
import { join } from 'node:path';
import { loadConfig, root } from '../config/load.js';
import { UserError, report } from '../utils/errors.js';
import { refreshGuildEmojis } from '../utils/emojis.js';

const membership = products => products.map(p => `${p.id}:${p.variants.map(v => v.id).sort().join(',')}`).sort().join('|');

export async function reloadCatalog(context, next) {
  if (next.guildId !== context.config.guildId || membership(next.products) !== membership(context.config.products)) {
    throw new UserError('Para mudar servidor ou adicionar/remover produtos, reinicie o bot.');
  }
  await context.discord.exclusive(async () => {
    if (context.stopping) return;
    await context.store.syncCatalog(next.products);
    context.config.products = next.products;
    context.config.emojis = next.emojis;
    await refreshGuildEmojis(context.discord.guild, context.config);
    await context.discord.catalogs();
  });
}

export function watchCatalog(context) {
  const file = join(root, 'config.json');
  const listener = () => {
    if (!context.ready || context.stopping) return;
    loadConfig().then(next => reloadCatalog(context, next)).catch(error => {
      if (error instanceof UserError) console.error(error.message);
      else report('Falha ao recarregar catálogo; confira o config.json', error);
    });
  };
  watchFile(file, { interval: 2000, persistent: false }, listener);
  return () => unwatchFile(file, listener);
}

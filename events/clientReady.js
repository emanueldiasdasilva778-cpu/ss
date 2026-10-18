import { report } from '../utils/errors.js';
import { watchCatalog } from './catalogChanged.js';

export async function onReady(context) {
  await context.discord.preflight();
  if (context.stopping) return;
  await context.discord.reconcile();
  if (context.stopping) return;
  context.ready = true;
  context.stopWatching = watchCatalog(context);
  console.info(`Loja iniciada como ${context.client.user.username}. Confirmação de Pix: manual.`);
  context.timer = setInterval(() => {
    context.discord.reconcile().catch(error => report('Reconciliação periódica', error));
  }, context.config.reconcileIntervalSeconds * 1000);
  context.timer.unref();
}

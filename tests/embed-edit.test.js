import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Store } from '../utils/store.js';
import { Shop } from '../utils/shop.js';
import { catalogPayload } from '../utils/ui.js';

const sample = JSON.parse(await readFile(new URL('../config.example.json', import.meta.url)));
async function setup(t) {
  const config = structuredClone(sample);
  config.products = [config.products[0]];
  const directory = await mkdtemp(join(tmpdir(), 'zeroms-embed-'));
  let store = await new Store(directory).init(config.products);
  t.after(async () => { await store.close(); await rm(directory, { recursive: true, force: true }); });
  return { config, get store() { return store; }, get shop() { return new Shop(store, config); },
    async restart() { await store.close(); store = await new Store(directory).init(config.products); } };
}

test('descrição e imagem do embed editadas persistem após reinício e sincronização do config inalterado', async t => {
  const ctx = await setup(t);
  await ctx.shop.editEmbed('external', { description: '📦 **Meu anúncio completo**\nTexto novo.', image: 'https://example.com/minha-imagem.png' }, 0);
  await ctx.restart();
  assert.equal(ctx.store.read().embeds.external.description, '📦 **Meu anúncio completo**\nTexto novo.');
  assert.equal(ctx.store.read().embeds.external.image, 'https://example.com/minha-imagem.png');
  await ctx.store.syncCatalog(ctx.config.products);
  assert.equal(ctx.store.read().embeds.external.revision, 1);
  ctx.config.products[0].description = 'Nova descrição explícita no config';
  await ctx.store.syncCatalog(ctx.config.products);
  assert.equal(ctx.store.read().embeds.external.description, 'Nova descrição explícita no config');
  assert.equal(ctx.store.read().embeds.external.image, 'https://example.com/minha-imagem.png');
});

test('alteração simultânea do embed recusa formulário antigo sem alterar preço nem estoque', async t => {
  const ctx = await setup(t);
  await ctx.shop.editEmbed('external', { description: 'Novo anúncio', image: '' }, 0);
  const before = ctx.store.read();
  await assert.rejects(ctx.shop.editPublicPanel('external-3dias', { description: 'Modal antigo', image: '', priceCents: 1, stock: 999 }, 0, 0), /embed mudou/);
  assert.deepEqual(ctx.store.read(), before);
});

test('venda durante edição impede restaurar estoque e não altera a descrição do embed', async t => {
  const ctx = await setup(t);
  const cart = await ctx.shop.add({ id: 'cliente', username: 'Cliente' }, 'external-3dias');
  await ctx.shop.attach(cart.id, 'canal', 'mensagem');
  await ctx.shop.checkout(cart.id, 'cliente', 'canal');
  await ctx.shop.confirm(cart.id, 'staff', 'canal');
  const before = ctx.store.read();
  await assert.rejects(ctx.shop.editPublicPanel('external-3dias', { description: 'Modal antigo', image: '', priceCents: 875, stock: 49 }, 0, 0), /estoque mudou/);
  assert.deepEqual(ctx.store.read(), before);
});

test('URL inválida e descrição acima dos limites não gravam alterações parciais', async t => {
  const ctx = await setup(t);
  const before = ctx.store.read();
  for (const changes of [
    { description: 'Texto', image: 'javascript:alert(1)' },
    { description: 'Texto', image: 'http://example.com/arquivo.png' },
    { description: 'x'.repeat(4001), image: '' },
  ]) await assert.rejects(ctx.shop.editEmbed('external', changes, 0));
  ctx.config.resolvedEmojis = { longo: '<:nome_de_emoji_longo:123456789012345678>' };
  await assert.rejects(ctx.shop.editPublicPanel('external-3dias', { description: ':longo:'.repeat(500), image: '', priceCents: 1, stock: 999 }, 0, 0), /6000 caracteres/);
  assert.deepEqual(ctx.store.read(), before);
});

test('banco de versão anterior migra embeds sem restaurar estoque vendido', async t => {
  const ctx = await setup(t);
  await ctx.shop.edit('external-3dias', { priceCents: 975, stock: 1, description: 'Descrição curta existente' });
  await ctx.store.transact(data => { delete data.embeds; });
  await ctx.restart();
  assert.equal(ctx.store.read().items['external-3dias'].stock, 1);
  assert.equal(ctx.store.read().items['external-3dias'].priceCents, 975);
  assert.equal(ctx.store.read().embeds.external.description, ctx.config.products[0].description);
});

test('descrição de 4000 caracteres é salva inteira quando o anúncio cabe nos limites do Discord', async t => {
  const ctx = await setup(t);
  const text = ('Linha completa do anúncio.\n'.repeat(160)).slice(0, 4000);
  await ctx.shop.editEmbed('external', { description: text, image: '' }, 0);
  assert.equal(ctx.store.read().embeds.external.description, text);
  const payload = catalogPayload(ctx.config, ctx.config.products[0], ctx.store.read());
  assert.ok(payload.embeds.map(e => e.data.description).join('').startsWith(text));
});

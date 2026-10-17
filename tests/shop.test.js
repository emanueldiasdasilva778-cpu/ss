import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Store } from '../utils/store.js';
import { Shop } from '../utils/shop.js';
import { parsePrice, parseQuantity, totals } from '../utils/money.js';

const products = [{ id: 'produto', variants: [
  { id: 'sku-a', name: 'Produto A', priceCents: 875, stock: 2, description: '' },
  { id: 'sku-b', name: 'Produto B', priceCents: 150, stock: 10, description: '' },
] }];

async function setup(t) {
  const directory = await mkdtemp(join(tmpdir(), 'zeroms-test-'));
  const store = await new Store(directory).init(products);
  const shop = new Shop(store, {});
  t.after(async () => { await store.close(); await rm(directory, { recursive: true, force: true }); });
  return { directory, store, shop };
}

async function cartFor(shop, id = 'comprador', item = 'sku-a') {
  const cart = await shop.add({ id, username: `Usuário ${id}` }, item);
  return shop.attach(cart.id, `canal-${id}`, `mensagem-${id}`);
}

test('valores monetários são exatos e cupom exige subtotal estritamente maior que R$ 3,00', () => {
  assert.equal(parsePrice('8,75'), 875);
  assert.equal(parsePrice('0.60'), 60);
  assert.equal(parsePrice('100'), 10000);
  for (const price of ['NaN', '1e3', '-1', '0', '1.000,00', '0,001']) assert.throws(() => parsePrice(price));
  assert.deepEqual(totals([{ priceCents: 150, quantity: 2 }], 'ZEROMS25'), { subtotal: 300, discount: 0, total: 300 });
  assert.deepEqual(totals([{ priceCents: 301, quantity: 1 }], 'ZEROMS25'), { subtotal: 301, discount: 75, total: 226 });
  assert.deepEqual(totals([{ priceCents: 875, quantity: 1 }], 'ZEROMS25'), { subtotal: 875, discount: 219, total: 656 });
  assert.equal(parseQuantity('0', { allowZero: true }), 0);
  for (const quantity of ['0', '-1', '1.5', 'abc']) assert.throws(() => parseQuantity(quantity));
});

test('adicionar, pagar e cancelar não reduzem estoque', async t => {
  const { shop, store } = await setup(t);
  const cart = await cartFor(shop);
  await shop.checkout(cart.id, cart.buyerId, cart.channelId);
  assert.equal(store.read().items['sku-a'].stock, 2);
  await shop.cancel(cart.id, cart.buyerId, cart.channelId);
  assert.equal(store.read().items['sku-a'].stock, 2);
  assert.equal(shop.cart(cart.id).status, 'cancelled');
});

test('vários produtos usam o mesmo carrinho e respeitam estoque', async t => {
  const { shop } = await setup(t);
  const first = await cartFor(shop);
  const second = await shop.add({ id: first.buyerId, username: 'Comprador' }, 'sku-b');
  assert.equal(second.id, first.id);
  assert.equal(second.lines.length, 2);
  await assert.rejects(shop.quantity(first.id, first.buyerId, first.channelId, 'sku-a', 3), /Estoque insuficiente/);
  await shop.quantity(first.id, first.buyerId, first.channelId, 'sku-b', 0);
  assert.equal(shop.cart(first.id).lines.length, 1);
  await assert.rejects(shop.quantity(first.id, first.buyerId, first.channelId, 'sku-a', 0), /Cancelar/);
});

test('dono e canal são verificados em alterações de carrinho', async t => {
  const { shop } = await setup(t);
  const c = await cartFor(shop);
  await assert.rejects(shop.coupon(c.id, 'intruso', c.channelId, 'ZEROMS25'), /não pertence/);
  await assert.rejects(shop.quantity(c.id, c.buyerId, 'outro-canal', 'sku-a', 1), /não pertence/);
  await assert.rejects(shop.checkout(c.id, 'intruso', c.channelId), /não pertence/);
  await assert.rejects(shop.cancel(c.id, 'intruso', c.channelId), /não pertence/);
  await assert.rejects(shop.confirm(c.id, 'staff', 'outro-canal'), /inválido/);
});

test('cupom pode ser removido e deixa de valer ao reduzir subtotal para R$ 3,00', async t => {
  const { shop } = await setup(t);
  const c = await cartFor(shop, 'comprador', 'sku-b');
  await assert.rejects(shop.coupon(c.id, c.buyerId, c.channelId, 'ZEROMS25'), /maior que/);
  await shop.quantity(c.id, c.buyerId, c.channelId, 'sku-b', 3);
  await assert.rejects(shop.coupon(c.id, c.buyerId, c.channelId, 'FALSO'), /inválido/);
  await shop.coupon(c.id, c.buyerId, c.channelId, ' zeroms25 ');
  assert.equal(shop.cart(c.id).coupon, 'ZEROMS25');
  await shop.quantity(c.id, c.buyerId, c.channelId, 'sku-b', 2);
  assert.equal(shop.cart(c.id).coupon, null);
});

test('dois cliques simultâneos de confirmação debitam estoque uma única vez', async t => {
  const { shop, store } = await setup(t);
  const c = await cartFor(shop);
  await assert.rejects(shop.confirm(c.id, 'staff', c.channelId), /Pagar com Pix/);
  await shop.coupon(c.id, c.buyerId, c.channelId, 'ZEROMS25');
  await shop.checkout(c.id, c.buyerId, c.channelId);
  const results = await Promise.all([shop.confirm(c.id, 'staff', c.channelId), shop.confirm(c.id, 'staff', c.channelId)]);
  assert.equal(results[0].receipt.total, 656);
  assert.equal(results[1].receipt.total, 656);
  assert.equal(store.read().items['sku-a'].stock, 1);
  assert.equal(shop.cart(c.id).status, 'paid');
  await assert.rejects(shop.cancel(c.id, c.buyerId, c.channelId), /não pode/);
});

test('dois compradores disputando a última unidade nunca geram estoque negativo', async t => {
  const { shop, store } = await setup(t);
  await shop.edit('sku-a', { priceCents: 875, stock: 1, description: 'Última unidade' });
  const a = await cartFor(shop, 'a');
  const b = await cartFor(shop, 'b');
  await shop.checkout(a.id, a.buyerId, a.channelId);
  await shop.checkout(b.id, b.buyerId, b.channelId);
  const outcomes = await Promise.allSettled([shop.confirm(a.id, 'staff', a.channelId), shop.confirm(b.id, 'staff', b.channelId)]);
  assert.equal(outcomes.filter(o => o.status === 'fulfilled').length, 1);
  assert.equal(outcomes.filter(o => o.status === 'rejected').length, 1);
  assert.equal(store.read().items['sku-a'].stock, 0);
  assert.equal(Object.values(store.read().carts).filter(c => c.status === 'awaiting_payment').length, 1);
});

test('alteração de preço exige nova revisão antes do Pix e não muda recibos já cotados', async t => {
  const { shop } = await setup(t);
  const c = await cartFor(shop);
  await shop.edit('sku-a', { priceCents: 1000, stock: 2, description: 'Nova descrição' });
  assert.equal((await shop.checkout(c.id, c.buyerId, c.channelId)).priceChanged, true);
  assert.equal(shop.cart(c.id).status, 'open');
  await shop.checkout(c.id, c.buyerId, c.channelId);
  await shop.edit('sku-a', { priceCents: 2000, stock: 2, description: '' });
  const paid = await shop.confirm(c.id, 'staff', c.channelId);
  assert.equal(paid.receipt.total, 1000);
  await assert.rejects(shop.quantity(c.id, c.buyerId, c.channelId, 'sku-a', 2), /fechado/);
});

test('quantidade e edição inválidas nunca chegam ao arquivo persistido', async t => {
  const { shop, store } = await setup(t);
  const c = await cartFor(shop);
  const before = store.read();
  await assert.rejects(shop.quantity(c.id, c.buyerId, c.channelId, 'sku-a', -1));
  await assert.rejects(shop.edit('sku-a', { priceCents: -100, stock: 1, description: '' }));
  await assert.rejects(shop.edit('sku-a', { priceCents: 100, stock: -1, description: '' }));
  assert.deepEqual(store.read(), before);
});

test('modal aberto antes de uma venda não pode restaurar estoque antigo', async t => {
  const { shop, store } = await setup(t);
  const revision = store.read().items['sku-a'].revision;
  const c = await cartFor(shop);
  await shop.checkout(c.id, c.buyerId, c.channelId);
  await shop.confirm(c.id, 'staff', c.channelId);
  await assert.rejects(shop.edit('sku-a', { priceCents: 900, stock: 2, description: 'Modal antigo' }, revision), /mudou desde/);
  assert.equal(store.read().items['sku-a'].stock, 1);
});

test('pedido, cupom, estoque e pendências sobrevivem ao reinício sem recontar a venda', async t => {
  const directory = await mkdtemp(join(tmpdir(), 'zeroms-restart-'));
  let store = await new Store(directory).init(products);
  t.after(async () => { await store.close(); await rm(directory, { recursive: true, force: true }); });
  const shop = new Shop(store, {});
  const c = await cartFor(shop);
  await shop.checkout(c.id, c.buyerId, c.channelId);
  await shop.confirm(c.id, 'staff', c.channelId);
  await store.close();
  const reopened = store = await new Store(directory).init(products);
  const recovered = new Shop(reopened, {});
  assert.equal(recovered.cart(c.id).status, 'paid');
  assert.equal(recovered.cart(c.id).logMessageId, null);
  await recovered.confirm(c.id, 'staff', c.channelId);
  assert.equal(reopened.read().items['sku-a'].stock, 1);
  assert.equal(JSON.parse(await readFile(join(directory, 'store.json'), 'utf8')).items['sku-a'].stock, 1);
});

test('segunda instância é bloqueada e arquivo corrompido nunca reinicia estoque', async t => {
  const { directory, store } = await setup(t);
  await assert.rejects(new Store(directory).init(products), /INSTANCE_LOCKED/);
  await store.close();
  await writeFile(join(directory, 'store.json'), '{arquivo inválido');
  await assert.rejects(new Store(directory).init(products), SyntaxError);
  assert.equal(await readFile(join(directory, 'store.json'), 'utf8'), '{arquivo inválido');
});

test('falha de escrita bloqueia novas transações e preserva o último estoque persistido', async t => {
  const { directory, store } = await setup(t);
  const actualFile = store.file;
  store.file = join(directory, 'diretorio-inexistente', 'store.json');
  await assert.rejects(store.transact(data => { data.items['sku-a'].stock = 0; }), { code: 'ENOENT' });
  await assert.rejects(store.transact(data => { data.items['sku-a'].stock = 1; }), /STORE_WRITE_DISABLED/);
  assert.equal(JSON.parse(await readFile(actualFile, 'utf8')).items['sku-a'].stock, 2);
  assert.equal(store.read().items['sku-a'].stock, 2);
});

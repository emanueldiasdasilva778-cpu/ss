import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { validateConfig } from '../config/load.js';
import { formatText, safeText } from '../utils/format.js';
import { mergeCatalog } from '../utils/catalog-config.js';
import { refreshGuildEmojis } from '../utils/emojis.js';
import { catalogPayload, cartPayload, receiptPayload, modal, productSelect, variantSelect, editPanel, quantitySelect } from '../utils/ui.js';

const config = JSON.parse(await readFile(new URL('../config.example.json', import.meta.url)));
const items = Object.fromEntries(config.products.flatMap(p => p.variants.map(v => [v.id, { ...v, productId: p.id }])));
const cart = { id: 'pedido-1', buyerId: 'comprador', username: 'Cliente', status: 'open', coupon: null,
  lines: [{ itemId: 'external-3dias', name: 'ZeroM$ External 3 Dias', priceCents: 875, quantity: 2 }] };

function serialize(payload) {
  return { ...payload, embeds: payload.embeds?.map(e => e.toJSON()), components: payload.components?.map(c => c.toJSON()) };
}

test('configuração contém exatamente os preços, canais e estoques solicitados', () => {
  validateConfig(config, { requireCredentials: false });
  assert.equal(config.editRoleId, '1561137818342920333');
  assert.equal(config.logChannelId, '1559063532321120386');
  assert.deepEqual(config.products.map(p => p.channelId), [
    '1559212215431856179', '1559213117718798336', '1559214412446044252', '1559214459824898099',
    '1559214972549202091', '1559215094989197444', '1559215025615274004', '1561129676124983306',
  ]);
  assert.deepEqual(config.products.map(p => p.variants.map(v => [v.priceCents, v.stock])), [
    [[12490, 39], [2990, 18], [1990, 68], [875, 49]],
    [[9990, 9], [3890, 8], [1990, 14], [790, 25], [575, 11]],
    [[749, 15], [1499, 3], [2999, 7], [14999, 92]],
    [[36990, 10], [17499, 9]], [[60, 0]], [[499, 0]], [[150, 0]], [[150, 0]],
  ]);
  assert.throws(() => validateConfig(config), /token/);
});

test('catálogos usam select; produtos únicos usam somente botão e estoque zero desabilita compra', () => {
  for (const product of config.products) {
    const payload = serialize(catalogPayload(config, product, { items }));
    assert.equal(payload.components[0].components[0].type, product.variants.length === 1 ? 2 : 3);
    assert.ok(payload.embeds[0].description.length <= 4096);
    if (product.image) assert.equal(payload.embeds[0].image.url, product.image);
    if (product.variants.length === 1) {
      assert.equal(payload.components[0].components[0].label, 'Adicionar ao Carrinho');
      assert.equal(payload.components[0].components[0].disabled, true);
    }
  }
});

test('preço, descrição e estoque atuais aparecem no embed e produto esgotado sai do select', () => {
  const changed = structuredClone(items);
  changed['external-3dias'].priceCents = 1000;
  changed['external-3dias'].description = 'Descrição nova';
  changed['external-3dias'].stock = 0;
  const payload = serialize(catalogPayload(config, config.products[0], { items: changed }));
  assert.match(payload.embeds[0].description, /Descrição nova/);
  assert.match(payload.embeds[0].description, /0 un/);
  assert.equal(payload.components[0].components[0].options.some(o => o.value === 'external-3dias'), false);
});

test('carrinho tem texto, rodapé e botões obrigatórios; confirmação só habilita após Pix', () => {
  const payload = serialize(cartPayload(config, cart));
  assert.equal(payload.embeds[0].title, '🛒 Carrinho de compras de Cliente');
  assert.match(payload.embeds[0].description, /Olá Cliente, seja bem-vindo ao seu carrinho de compras!/);
  assert.match(payload.embeds[0].description, /Sem cupom aplicado/);
  assert.equal(payload.embeds[0].footer.text, 'ZeroM$ © Todos os direitos reservados');
  assert.deepEqual(payload.components[0].components.map(c => c.label), ['🟢 Pagar com Pix', '🎟️ Cupom', '📦 Quantidade', '⬛ Cancelar']);
  assert.equal(payload.components[1].components[0].disabled, true);
  const waiting = serialize(cartPayload(config, { ...cart, status: 'awaiting_payment' }));
  assert.equal(waiting.components[1].components[0].disabled, false);
  assert.equal(waiting.components[0].components[1].disabled, true);
});

test('log contém os dados financeiros, equipe, usuário e horário brasileiro', () => {
  const paid = { ...cart, paidAt: '2026-10-17T18:00:00Z', staffId: 'equipe',
    receipt: { lines: cart.lines, subtotal: 1750, total: 1312, discount: 438, coupon: 'ZEROMS25' } };
  const payload = serialize(receiptPayload(config, paid));
  const description = payload.embeds[0].description;
  for (const text of ['comprador', 'equipe', 'ZeroM', 'ZEROMS25', '17/10/2026', '15:00:00', 'Valor original', 'Valor final pago', 'America/Sao_Paulo']) assert.ok(description.includes(text), text);
  assert.ok(payload.embeds[0].footer.text.endsWith('Pedido pedido-1'));
});

test('painel e modals serializam dentro dos limites de componentes do Discord', () => {
  serialize(productSelect(config));
  serialize(variantSelect(config.products[0], { items }));
  serialize(editPanel(config, items['external-3dias']));
  serialize(quantitySelect(cart));
  const data = modal('teste', 'Editar', [{ id: 'price', label: 'Preço', value: '8,75' }]).toJSON();
  assert.equal(data.components[0].components[0].value, '8,75');
});

test('descrição mantém Markdown, quebras de linha e emojis personalizados sem escapar IDs', () => {
  const edited = structuredClone(config);
  edited.emojis = { logo_loja: '<:logo_loja:123456789012345678>' };
  const product = { ...edited.products[0], description: ':logo_loja: **Minha descrição**\n:shopping_cart: Escolha abaixo.' };
  const changed = structuredClone(items);
  changed['external-3dias'].description = '<a:oferta_loja:123456789012345679> **Detalhes**';
  const payload = serialize(catalogPayload(edited, product, { items: changed }));
  assert.ok(payload.embeds[0].description.includes('<:logo_loja:123456789012345678> **Minha descrição**\n🛒'));
  assert.ok(payload.embeds[0].description.includes('<a:oferta_loja:123456789012345679> **Detalhes**'));
  assert.equal(safeText('<:logo_loja:123456789012345678> *nome*'), '<:logo_loja:123456789012345678> \\*nome\\*');
  assert.equal(formatText(':emoji_sem_id:'), ':emoji_sem_id:');
});

test('alteração da imagem do catálogo chega ao embed sem reaproveitar a imagem anterior', () => {
  const product = { ...config.products[0], image: 'https://example.com/nova-imagem.webp' };
  assert.equal(serialize(catalogPayload(config, product, { items })).embeds[0].image.url, product.image);
});

test('importar apresentação do exemplo preserva token, IDs, preços e estoque do arquivo ativo', () => {
  const current = structuredClone(config);
  current.token = 'credencial-de-teste-sem-validade';
  current.products[0].variants[0].stock = 2;
  const incoming = structuredClone(config);
  incoming.token = 'não-importar';
  incoming.guildId = 'não-importar';
  incoming.products[0].image = 'https://example.com/novo.webp';
  incoming.products[0].description = '**Nova descrição**';
  incoming.products[0].variants[0].description = '💎 Minha opção';
  const merged = mergeCatalog(current, incoming);
  assert.equal(merged.token, current.token);
  assert.equal(merged.guildId, current.guildId);
  assert.equal(merged.products[0].variants[0].stock, 2);
  assert.equal(merged.products[0].image, incoming.products[0].image);
  assert.equal(merged.products[0].variants[0].description, '💎 Minha opção');
  assert.notEqual(current.products[0].description, merged.products[0].description);
});

test('aliases do TXT usam emojis reais do servidor, inclusive animados e IDs numéricos', async () => {
  const current = { emojis: {} };
  const guild = { emojis: { async fetch() { return new Map([
    ['1', { id: '123456789012345678', name: 'Zennify_box', animated: false, available: true }],
    ['2', { id: '1298339916803215483', name: 'pagamento', animated: true, available: true }],
    ['3', { id: '123456789012345679', name: 'star', animated: false, available: true }],
  ]); } } };
  await refreshGuildEmojis(guild, current);
  assert.equal(formatText(':Zennify_box:', current), '<:Zennify_box:123456789012345678>');
  assert.equal(formatText(':1298339916803215483:', current), '<a:pagamento:1298339916803215483>');
  assert.equal(formatText(':star~1:', current), '<:star:123456789012345679>');
  assert.equal(formatText(':9504_purple_flame: :44250purplecheck: :shoppingcart: :ant5: :fogue: :144:', current), '🔥 ✅ 🛒 ⚡ 🔥 ℹ️');
});

test('descrições longas usam continuação sem truncar texto, imagem ou opções', () => {
  const product = { ...config.products[0], description: 'Descrição completa. '.repeat(180).slice(0, 3400) };
  const changed = structuredClone(items);
  for (const item of product.variants) changed[item.id].description = 'Detalhes completos. '.repeat(9).slice(0, 180);
  const payload = serialize(catalogPayload(config, product, { items: changed }));
  assert.ok(payload.embeds.length > 1);
  assert.ok(payload.embeds.every(e => e.description.length <= 4096));
  const all = payload.embeds.map(e => e.description).join('');
  assert.ok(all.startsWith(product.description));
  for (const item of product.variants) assert.ok(all.includes(changed[item.id].description));
  assert.equal(payload.embeds[0].image.url, product.image);
  assert.ok(payload.components.length > 0);
});

test('catálogo completo inclui as oito descrições do TXT nos canais correspondentes', () => {
  validateConfig(config, { requireCredentials: false });
  const singles = config.products.filter(p => p.id.startsWith('unico-'));
  assert.equal(singles.find(p => p.name.startsWith('Contas Rockstar')).channelId, '1559215025615274004');
  assert.equal(singles.find(p => p.name.startsWith('Conta Guest')).channelId, '1561129676124983306');
  assert.ok(config.products.find(p => p.id === 'external').description.includes('TriggerBot'));
  assert.ok(config.products.find(p => p.id === 'internal').description.includes('Silent Aim'));
  assert.ok(config.products.find(p => p.id === 'snow').description.includes('FiveGuard'));
  for (const product of config.products) {
    const payload = serialize(catalogPayload(config, product, { items }));
    assert.ok(payload.embeds.reduce((sum, e) => sum + e.description.length + e.title.length + e.footer.text.length, 0) <= 6000);
  }
});

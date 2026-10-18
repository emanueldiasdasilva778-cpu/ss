import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { ChannelType, Collection, PermissionFlagsBits } from 'discord.js';
import { Store } from '../utils/store.js';
import { Shop } from '../utils/shop.js';
import { DiscordShop } from '../utils/discord-shop.js';
import { handleInteraction } from '../events/interactionCreate.js';
import { reloadCatalog } from '../events/catalogChanged.js';

const sample = JSON.parse(await readFile(new URL('../config.example.json', import.meta.url)));

async function setup(t) {
  const config = structuredClone(sample);
  config.guildId = '1111111111111111111';
  config.products = [config.products[0]];
  const directory = await mkdtemp(join(tmpdir(), 'zeroms-discord-'));
  const store = await new Store(directory).init(config.products);
  t.after(async () => { await store.close(); await rm(directory, { recursive: true, force: true }); });
  const shop = new Shop(store, config);
  const channels = new Collection();
  const sequence = [];
  const staffUsers = new Set(['staff']);
  let nextId = 0;

  function channel(id, options = {}) {
    const messages = new Collection();
    const c = { id, guildId: config.guildId, type: ChannelType.GuildText, topic: options.topic ?? null,
      options, messages: {
        async fetch(arg) {
          if (arg.limit) return messages;
          const m = messages.get(typeof arg === 'string' ? arg : arg.message);
          if (!m) throw Object.assign(new Error('Mensagem desconhecida'), { code: 10008 });
          return m;
        },
      },
      async send(payload) {
        if (c.failSend) throw Object.assign(new Error('Sem permissão'), { code: 50013 });
        sequence.push(`send:${id}`);
        const m = { id: `m-${++nextId}`, author: { id: 'bot' },
          async edit(next) { sequence.push(`edit:${id}`); apply(next); return m; },
        };
        function apply(value) {
          m.payload = value;
          m.embeds = value.embeds.map(e => e.toJSON());
          m.components = (value.components ?? []).map(r => ({ components: r.components.map(b => ({ customId: b.data.custom_id })) }));
        }
        apply(payload); messages.set(m.id, m); return m;
      },
      async delete() {
        if (c.failDelete) throw Object.assign(new Error('Sem permissão'), { code: 50013 });
        sequence.push(`delete:${id}`); channels.delete(id);
      },
      permissionsFor() { return { has: () => true }; },
      storedMessages: messages,
    };
    channels.set(id, c); return c;
  }

  const guild = { id: config.guildId,
    members: {
      async fetch({ user }) { return { roles: { cache: new Collection(staffUsers.has(user) ? [[config.editRoleId, {}]] : []) } }; },
      async fetchMe() { return { permissions: { has: () => true } }; },
    },
    roles: { async fetch() { return new Collection([[config.editRoleId, {}]]); } },
    channels: { cache: channels,
      async fetch(id) { return id ? channels.get(id) : channels; },
      async create(options) { sequence.push('create'); return channel(`c-${++nextId}`, options); },
    },
  };
  const client = { user: { id: 'bot' }, channels: {
    async fetch(id) {
      if (!channels.has(id)) throw Object.assign(new Error('Canal desconhecido'), { code: 10003 });
      return channels.get(id);
    },
  }, guilds: { async fetch() { return guild; } } };
  channel(config.logChannelId); channel(config.products[0].channelId);
  const discord = new DiscordShop(client, config, shop, store);
  await discord.preflight();
  const context = { config, store, shop, discord, client, ready: true };
  return { ...context, context, channels, channel, sequence, staffUsers };
}

function interaction(context, id, { user = 'comprador', channelId, type = 'button', values = [], fields = {} } = {}) {
  const responses = [];
  return { customId: id, commandName: 'edit', user: { id: user, username: user },
    guild: context.discord.guild, guildId: context.config.guildId, channelId, values,
    fields: { getTextInputValue: key => fields[key] }, deferred: false, replied: false,
    inGuild: () => true,
    isChatInputCommand: () => type === 'command', isButton: () => type === 'button',
    isStringSelectMenu: () => type === 'select', isModalSubmit: () => type === 'modal',
    async deferReply(options) { this.deferred = true; responses.push(options); },
    async editReply(payload) { this.replied = true; responses.push(payload); },
    async reply(payload) { this.replied = true; responses.push(payload); },
    async followUp(payload) { responses.push(payload); },
    async showModal(payload) { this.replied = true; responses.push(payload); }, responses,
  };
}

test('cria carrinho privado e confirma com log antes da exclusão', async t => {
  const ctx = await setup(t);
  await ctx.discord.catalogs();
  const item = 'external-3dias';
  const cart = await ctx.discord.add({ id: 'comprador', username: 'Cliente' }, item);
  const channel = ctx.channels.get(cart.channelId);
  const overwrites = channel.options.permissionOverwrites;
  assert.deepEqual(overwrites[0], { id: ctx.config.guildId, deny: [PermissionFlagsBits.ViewChannel] });
  assert.deepEqual(new Set(overwrites.slice(1).map(o => o.id)), new Set(['bot', 'comprador', ctx.config.editRoleId]));
  assert.equal(ctx.store.read().items[item].stock, 49);

  const pay = interaction(ctx.context, `pay:${cart.id}`, { channelId: cart.channelId });
  await handleInteraction(pay, ctx.context);
  assert.equal(ctx.shop.cart(cart.id).status, 'awaiting_payment');
  const confirm = interaction(ctx.context, `confirm:${cart.id}`, { user: 'staff', channelId: cart.channelId });
  await handleInteraction(confirm, ctx.context);
  assert.equal(ctx.store.read().items[item].stock, 48);
  const paid = ctx.shop.cart(cart.id);
  assert.ok(paid.logMessageId);
  assert.ok(paid.channelDeletedAt);
  const logIndex = ctx.sequence.indexOf(`send:${ctx.config.logChannelId}`);
  const deleteIndex = ctx.sequence.indexOf(`delete:${cart.channelId}`);
  assert.ok(logIndex >= 0 && logIndex < deleteIndex);
  const panel = [...ctx.channels.get(ctx.config.products[0].channelId).storedMessages.values()][0];
  assert.match(panel.embeds[0].description, /48 un/);
});

test('comprador não confirma pagamento nem edita, inclusive enviando modal diretamente', async t => {
  const ctx = await setup(t);
  const c = await ctx.discord.add({ id: 'comprador', username: 'Cliente' }, 'external-3dias');
  await ctx.shop.checkout(c.id, c.buyerId, c.channelId);
  for (const [id, type] of [[`confirm:${c.id}`, 'button'], ['edit', 'command'], ['edit-save:external-3dias', 'modal']]) {
    const i = interaction(ctx.context, id, { channelId: c.channelId, type });
    await handleInteraction(i, ctx.context);
    assert.match(i.responses.at(-1).content, /Somente/);
  }
  assert.equal(ctx.store.read().items['external-3dias'].stock, 49);
  assert.equal(ctx.shop.cart(c.id).status, 'awaiting_payment');
});

test('cargo removido entre abertura e envio do modal impede edição', async t => {
  const ctx = await setup(t);
  const button = interaction(ctx.context, 'edit-open:external-3dias', { user: 'staff' });
  await handleInteraction(button, ctx.context);
  assert.equal(button.responses[0].toJSON().custom_id, 'edit-save:external-3dias:0');
  ctx.staffUsers.delete('staff');
  const save = interaction(ctx.context, 'edit-save:external-3dias:0', {
    user: 'staff', type: 'modal', fields: { price: '1,00', stock: '1', description: 'Inválido' },
  });
  await handleInteraction(save, ctx.context);
  assert.match(save.responses.at(-1).content, /Somente/);
  assert.equal(ctx.store.read().items['external-3dias'].priceCents, 875);
});

test('edição autorizada persiste e atualiza automaticamente o catálogo', async t => {
  const ctx = await setup(t);
  const save = interaction(ctx.context, 'edit-save:external-3dias:0', { user: 'staff', type: 'modal',
    fields: { price: '9,75', stock: '3', description: 'Descrição alterada pela equipe' } });
  await handleInteraction(save, ctx.context);
  assert.equal(ctx.store.read().items['external-3dias'].priceCents, 975);
  const panel = [...ctx.channels.get(ctx.config.products[0].channelId).storedMessages.values()][0];
  assert.match(panel.embeds[0].description, /Descrição alterada pela equipe/);
  assert.match(panel.embeds[0].description, /3 un/);
  assert.match(save.responses.at(-1).content, /salvo e catálogo atualizado/);
});

test('falha de log preserva canal e reprocessamento não reduz estoque novamente', async t => {
  const ctx = await setup(t);
  const c = await ctx.discord.add({ id: 'comprador', username: 'Cliente' }, 'external-3dias');
  await ctx.shop.checkout(c.id, c.buyerId, c.channelId);
  await ctx.shop.confirm(c.id, 'staff', c.channelId);
  ctx.channels.get(ctx.config.logChannelId).failSend = true;
  await assert.rejects(ctx.discord.settle(c.id));
  assert.ok(ctx.channels.has(c.channelId));
  assert.equal(ctx.shop.cart(c.id).logMessageId, null);
  ctx.channels.get(ctx.config.logChannelId).failSend = false;
  await ctx.discord.reconcile();
  assert.ok(ctx.shop.cart(c.id).channelDeletedAt);
  assert.equal(ctx.store.read().items['external-3dias'].stock, 48);
  assert.equal(ctx.channels.get(ctx.config.logChannelId).storedMessages.size, 1);
});

test('falha de exclusão preserva log e próximo processamento não duplica venda', async t => {
  const ctx = await setup(t);
  const c = await ctx.discord.add({ id: 'comprador', username: 'Cliente' }, 'external-3dias');
  await ctx.shop.checkout(c.id, c.buyerId, c.channelId);
  await ctx.shop.confirm(c.id, 'staff', c.channelId);
  ctx.channels.get(c.channelId).failDelete = true;
  await assert.rejects(ctx.discord.settle(c.id));
  assert.ok(ctx.shop.cart(c.id).logMessageId);
  ctx.channels.get(c.channelId).failDelete = false;
  await ctx.discord.settle(c.id);
  assert.equal(ctx.channels.get(ctx.config.logChannelId).storedMessages.size, 1);
  assert.equal(ctx.store.read().items['external-3dias'].stock, 48);
});

test('reinicialização recupera canal criado antes de salvar a mensagem e não duplica canal', async t => {
  const ctx = await setup(t);
  const creating = await ctx.shop.add({ id: 'comprador', username: 'Cliente' }, 'external-3dias');
  const existing = ctx.channel('canal-recuperado', { topic: `zeroms-cart:${creating.id}` });
  await ctx.discord.reconcile();
  assert.equal(ctx.shop.cart(creating.id).channelId, existing.id);
  assert.equal(ctx.shop.cart(creating.id).status, 'open');
  assert.equal(ctx.sequence.includes('create'), false);
});

test('reconciliação repõe painel excluído e não remove carrinho aguardando pagamento', async t => {
  const ctx = await setup(t);
  await ctx.discord.catalogs();
  const channel = ctx.channels.get(ctx.config.products[0].channelId);
  channel.storedMessages.clear();
  const c = await ctx.discord.add({ id: 'comprador', username: 'Cliente' }, 'external-3dias');
  await ctx.shop.checkout(c.id, c.buyerId, c.channelId);
  await ctx.store.transact(data => { data.carts[c.id].createdAt = '2000-01-01T00:00:00Z'; });
  await ctx.discord.reconcile();
  assert.equal(channel.storedMessages.size, 1);
  assert.equal(ctx.shop.cart(c.id).status, 'awaiting_payment');
  assert.ok(ctx.channels.has(c.channelId));
});

test('expiração remove apenas carrinho aberto inativo, sem dar baixa no estoque', async t => {
  const ctx = await setup(t);
  const c = await ctx.discord.add({ id: 'comprador', username: 'Cliente' }, 'external-3dias');
  await ctx.store.transact(data => {
    data.carts[c.id].createdAt = '2000-01-01T00:00:00Z';
    data.carts[c.id].updatedAt = '2000-01-01T00:00:00Z';
  });
  await ctx.discord.reconcile();
  assert.equal(ctx.shop.cart(c.id).status, 'expired');
  assert.ok(ctx.shop.cart(c.id).channelDeletedAt);
  assert.equal(ctx.channels.has(c.channelId), false);
  assert.equal(ctx.store.read().items['external-3dias'].stock, 49);
});

test('painel já publicado recebe nova imagem e descrição, sem criar outro painel', async t => {
  const ctx = await setup(t);
  await ctx.discord.catalogs();
  const channel = ctx.channels.get(ctx.config.products[0].channelId);
  const id = [...channel.storedMessages.keys()][0];
  ctx.config.products[0].description = ':shopping_cart: **Descrição atualizada**';
  ctx.config.products[0].image = 'https://example.com/banner-atualizado.webp';
  await ctx.discord.catalogs();
  assert.equal(channel.storedMessages.size, 1);
  assert.equal(channel.storedMessages.get(id).embeds[0].image.url, 'https://example.com/banner-atualizado.webp');
  assert.ok(channel.storedMessages.get(id).embeds[0].description.includes('🛒 **Descrição atualizada**'));
});

test('recarregar config atualiza imagem e descrições sem repor estoque vendido', async t => {
  const ctx = await setup(t);
  await ctx.shop.edit('external-3dias', { priceCents: 975, stock: 2, description: 'Descrição do painel' });
  await ctx.discord.catalogs();
  const next = structuredClone(ctx.config);
  next.products[0].image = 'https://example.com/imagem-recarregada.webp';
  next.products[0].description = '🛒 **Descrição salva no config**';
  next.products[0].variants.find(v => v.id === 'external-3dias').description = '💎 Detalhes atualizados';
  await reloadCatalog(ctx.context, next);
  assert.equal(ctx.store.read().items['external-3dias'].stock, 2);
  assert.equal(ctx.store.read().items['external-3dias'].priceCents, 975);
  assert.equal(ctx.store.read().items['external-3dias'].description, '💎 Detalhes atualizados');
  const channel = ctx.channels.get(ctx.config.products[0].channelId);
  assert.equal(channel.storedMessages.size, 1);
  const message = [...channel.storedMessages.values()][0];
  assert.equal(message.embeds[0].image.url, next.products[0].image);
  assert.ok(message.embeds[0].description.includes('💎 Detalhes atualizados'));
});

test('recarregamento recusa trocar servidor ou remover produtos sem alterar dados', async t => {
  const ctx = await setup(t);
  const before = ctx.store.read();
  const next = structuredClone(ctx.config);
  next.products = [];
  await assert.rejects(reloadCatalog(ctx.context, next), /reinicie/);
  assert.deepEqual(ctx.store.read(), before);
});

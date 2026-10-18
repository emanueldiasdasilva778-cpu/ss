import { ChannelType, PermissionFlagsBits } from 'discord.js';
import { createHash } from 'node:crypto';
import { UserError, report } from './errors.js';
import { cartPayload, catalogPayload, receiptPayload } from './ui.js';
import { refreshGuildEmojis } from './emojis.js';

const P = PermissionFlagsBits;
const requiredText = [P.ViewChannel, P.SendMessages, P.EmbedLinks, P.ReadMessageHistory];
const missingChannel = error => error?.code === 10003;
const missingMessage = error => error?.code === 10008;
const fingerprint = payload => createHash('sha256').update(JSON.stringify(payload)).digest('hex');

// Discord é um sistema externo: pedidos pagos ficam pendentes até log e fechamento concluírem.
// A fila persistida no próprio pedido é reprocessada na inicialização e periodicamente.
export class DiscordShop {
  constructor(client, config, shop, store) {
    Object.assign(this, { client, config, shop, store });
    this.serial = Promise.resolve();
    this.processing = false;
    this.cartHashes = new Map();
    this.panelHashes = new Map();
  }

  exclusive(task) {
    const job = this.serial.then(task);
    this.serial = job.catch(() => {});
    return job;
  }

  async channel(id) {
    const channel = await this.client.channels.fetch(id);
    if (!channel || channel.guildId !== this.config.guildId || channel.type !== ChannelType.GuildText) {
      throw new UserError('O canal configurado não é um canal de texto deste servidor.');
    }
    return channel;
  }

  async member(interaction) {
    if (!interaction.inGuild() || interaction.guildId !== this.config.guildId) throw new UserError('Use este recurso no servidor configurado.');
    return interaction.guild.members.fetch({ user: interaction.user.id, force: true });
  }

  async staff(interaction, editOnly = false) {
    const member = await this.member(interaction);
    const allowed = editOnly ? [this.config.editRoleId] : this.config.staffRoleIds;
    if (!allowed.some(id => member.roles.cache.has(id))) {
      throw new UserError(editOnly ? 'Somente o cargo autorizado pode usar /edit e seus painéis.' : 'Somente a equipe pode confirmar pagamentos.');
    }
    return member;
  }

  async preflight() {
    const guild = await this.client.guilds.fetch(this.config.guildId);
    const me = await guild.members.fetchMe();
    const roles = await guild.roles.fetch();
    for (const roleId of this.config.staffRoleIds) {
      if (roleId === guild.id || !roles.has(roleId)) throw new UserError('Confira staffRoleIds: cargo ausente ou @everyone não permitido.');
    }
    let category;
    if (this.config.cartCategoryId) {
      category = await guild.channels.fetch(this.config.cartCategoryId);
      if (!category || category.type !== ChannelType.GuildCategory) throw new UserError('Categoria de carrinhos inválida.');
    }
    if (!(category ? category.permissionsFor(me) : me.permissions)?.has([P.ManageChannels, P.ManageRoles])) {
      throw new UserError('O bot precisa de Gerenciar canais e Gerenciar permissões/cargos para criar carrinhos privados.');
    }
    for (const id of [this.config.logChannelId, ...this.config.products.map(p => p.channelId)]) {
      const channel = await this.channel(id);
      if (!channel.permissionsFor(me)?.has(requiredText)) throw new UserError(`Permissões insuficientes no canal ${id}: ver, enviar, inserir links e ler histórico.`);
    }
    this.guild = guild;
    await refreshGuildEmojis(guild, this.config);
    await guild.channels.fetch();
  }

  async add(user, itemId) {
    return this.exclusive(async () => {
      const cart = await this.shop.add(user, itemId);
      if (cart.status === 'creating') await this.ensureCart(cart.id);
      await this.updateCart(cart.id);
      return this.shop.cart(cart.id);
    });
  }

  async ensureCart(id) {
    let cart = this.shop.cart(id);
    if (cart.status !== 'creating') return;
    const marker = `zeroms-cart:${cart.id}`;
    let channel = cart.channelId ? await this.channel(cart.channelId).catch(error => {
      if (missingChannel(error)) return null;
      throw error;
    }) : this.guild.channels.cache.find(c => c.type === ChannelType.GuildText && c.topic === marker);
    if (!channel) {
      const readWrite = [P.ViewChannel, P.SendMessages, P.ReadMessageHistory, P.AttachFiles, P.EmbedLinks];
      channel = await this.guild.channels.create({
        name: `carrinho-${cart.username.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().replace(/[^a-z0-9]/g, '').slice(0, 20) || 'cliente'}-${cart.id.slice(0, 6)}`,
        type: ChannelType.GuildText, parent: this.config.cartCategoryId ?? undefined,
        topic: marker,
        permissionOverwrites: [
          { id: this.guild.id, deny: [P.ViewChannel] },
          { id: this.client.user.id, allow: [...readWrite, P.ManageChannels] },
          { id: cart.buyerId, allow: readWrite },
          ...this.config.staffRoleIds.map(roleId => ({ id: roleId, allow: readWrite })),
        ],
        reason: `Carrinho solicitado pelo comprador ${cart.buyerId}`,
      });
    }
    await this.store.transact(data => { data.carts[id].channelId = channel.id; });
    const recent = await channel.messages.fetch({ limit: 50 });
    let message = recent.find(m => m.author.id === this.client.user.id && m.components.some(r =>
      r.components.some(c => c.customId === `confirm:${cart.id}`)));
    if (!message) message = await channel.send(cartPayload(this.config, cart));
    await this.shop.attach(id, channel.id, message.id);
  }

  async updateCart(id) {
    const cart = this.shop.cart(id);
    if (!cart.channelId || cart.channelDeletedAt) return;
    const channel = await this.channel(cart.channelId);
    const payload = cartPayload(this.config, cart);
    const hash = fingerprint(payload);
    let message = cart.messageId ? await channel.messages.fetch({ message: cart.messageId, force: true, cache: false }).catch(error => {
      if (missingMessage(error)) return null;
      throw error;
    }) : null;
    if (message && this.cartHashes.get(id) === hash) return;
    if (message) await message.edit(payload);
    else {
      message = await channel.send(payload);
      await this.store.transact(data => { data.carts[id].messageId = message.id; });
    }
    this.cartHashes.set(id, hash);
  }

  async publishCatalog(product) {
    const channel = await this.channel(product.channelId);
    const data = this.store.read();
    const payload = catalogPayload(this.config, product, data);
    const hash = fingerprint(payload);
    const saved = data.panels[product.id];
    let message = saved?.channelId === channel.id ? await channel.messages.fetch({ message: saved.messageId, force: true, cache: false }).catch(error => {
      if (missingMessage(error)) return null;
      throw error;
    }) : null;
    if (!message) {
      const marker = `${this.config.footer} | Catálogo ${product.id}`;
      const recent = await channel.messages.fetch({ limit: 100 });
      message = recent.find(m => m.author.id === this.client.user.id && m.embeds[0]?.footer?.text === marker);
    }
    if (message && this.panelHashes.get(product.id) === hash) return;
    if (message) await message.edit(payload);
    else message = await channel.send(payload);
    await this.store.transact(next => { next.panels[product.id] = { channelId: channel.id, messageId: message.id }; });
    this.panelHashes.set(product.id, hash);
  }

  async catalogs() {
    let firstError;
    for (const product of this.config.products) {
      try { await this.publishCatalog(product); }
      catch (error) { report(`Atualização do catálogo ${product.id}`, error); firstError ??= error; }
    }
    if (firstError) throw firstError;
  }

  async deleteCartChannel(id) {
    const cart = this.shop.cart(id);
    if (!cart.channelId || cart.channelDeletedAt) return;
    try {
      const channel = await this.channel(cart.channelId);
      if (channel.topic !== `zeroms-cart:${cart.id}`) throw new UserError('Canal sem marcador de carrinho; exclusão bloqueada.');
      await channel.delete(`Pedido ${cart.id}: ${cart.status}`);
    } catch (error) { if (!missingChannel(error)) throw error; }
    await this.store.transact(data => { data.carts[id].channelDeletedAt = new Date().toISOString(); });
    this.cartHashes.delete(id);
  }

  async settle(id) {
    let cart = this.shop.cart(id);
    if (cart.status !== 'paid') return;
    // A confirmação só fecha o canal depois de o log estar registrado e os catálogos atualizados.
    await this.catalogs();
    if (!cart.logMessageId) {
      const channel = await this.channel(this.config.logChannelId);
      const marker = `${this.config.footer} | Pedido ${cart.id}`;
      const recent = await channel.messages.fetch({ limit: 100 });
      const found = recent.find(m => m.author.id === this.client.user.id && m.embeds[0]?.footer?.text === marker);
      const message = found ?? await channel.send(receiptPayload(this.config, cart));
      await this.store.transact(data => { data.carts[id].logMessageId = message.id; });
    }
    await this.deleteCartChannel(id);
  }

  async reconcile() {
    if (this.processing) return;
    this.processing = true;
    try {
      await this.exclusive(async () => {
        for (const cart of Object.values(this.store.read().carts)) {
          try {
            if (cart.status === 'creating') await this.ensureCart(cart.id);
            else if (cart.status === 'paid' && !cart.channelDeletedAt) { await this.settle(cart.id); continue; }
            else if (['cancelled', 'expired'].includes(cart.status) && !cart.channelDeletedAt) { await this.deleteCartChannel(cart.id); continue; }
            const current = this.shop.cart(cart.id);
            if (current.status === 'open' && Date.now() - Date.parse(current.updatedAt ?? current.createdAt) > this.config.cartIdleMinutes * 60_000) {
              await this.store.transact(data => {
                const latest = data.carts[cart.id];
                if (latest.status === 'open' && Date.now() - Date.parse(latest.updatedAt ?? latest.createdAt) > this.config.cartIdleMinutes * 60_000) {
                  latest.status = 'expired';
                }
              });
              if (this.shop.cart(cart.id).status === 'expired') await this.deleteCartChannel(cart.id);
            } else if (['open', 'awaiting_payment'].includes(current.status)) {
              await this.updateCart(cart.id);
            }
          } catch (error) { report(`Recuperação do pedido ${cart.id}`, error); }
        }
        await this.catalogs();
      });
    } finally { this.processing = false; }
  }
}

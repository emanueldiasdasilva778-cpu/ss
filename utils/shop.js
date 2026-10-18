import { randomUUID } from 'node:crypto';
import { UserError } from './errors.js';
import { totals } from './money.js';
import { imageUrl } from './catalog.js';
import { catalogPayload } from './ui.js';

export class Shop {
  constructor(store, config) { this.store = store; this.config = config; }

  cart(id) {
    const cart = this.store.read().carts[id];
    if (!cart) throw new UserError('Carrinho não encontrado.');
    return cart;
  }

  assertOwner(cart, userId, channelId = cart.channelId) {
    if (cart.buyerId !== userId || cart.channelId !== channelId) throw new UserError('Este carrinho não pertence a você.');
  }

  assertOpen(cart) {
    if (cart.status !== 'open') throw new UserError('Este carrinho está fechado para alterações. Cancele-o para iniciar outra compra.');
  }

  refresh(cart, data) {
    for (const line of cart.lines) {
      const item = data.items[line.itemId];
      if (!item || item.stock < line.quantity) throw new UserError('Estoque insuficiente. Ajuste a quantidade ou aguarde reposição.');
      line.name = item.name;
      line.priceCents = item.priceCents;
    }
    if (cart.coupon && totals(cart.lines).subtotal <= 300) cart.coupon = null;
  }

  async add(user, itemId) {
    return this.store.transact(data => {
      const item = data.items[itemId];
      if (!item || item.stock < 1) throw new UserError('Produto indisponível no momento.');
      let cart = Object.values(data.carts).find(c => c.buyerId === user.id && ['creating', 'open', 'awaiting_payment'].includes(c.status));
      if (cart?.status === 'creating') throw new UserError('Seu carrinho está sendo criado. Aguarde alguns segundos.');
      if (cart) this.assertOpen(cart);
      if (!cart) {
        cart = { id: randomUUID(), buyerId: user.id, username: user.username, status: 'creating',
          channelId: null, messageId: null, coupon: null, lines: [], createdAt: new Date().toISOString() };
        data.carts[cart.id] = cart;
      }
      let line = cart.lines.find(l => l.itemId === itemId);
      if (!line) {
        if (cart.lines.length >= 20) throw new UserError('O carrinho aceita até 20 produtos diferentes.');
        line = { itemId, name: item.name, priceCents: item.priceCents, quantity: 0 };
        cart.lines.push(line);
      }
      if (line.quantity >= item.stock) throw new UserError('A quantidade solicitada ultrapassa o estoque disponível.');
      line.quantity++;
      this.refresh(cart, data);
      cart.updatedAt = new Date().toISOString();
      return cart;
    });
  }

  async attach(id, channelId, messageId) {
    return this.store.transact(data => {
      const cart = data.carts[id];
      if (cart.status !== 'creating') throw new UserError('O carrinho não está em criação.');
      Object.assign(cart, { channelId, messageId, status: 'open' });
      return cart;
    });
  }

  async quantity(id, buyerId, channelId, itemId, quantity) {
    if (!Number.isInteger(quantity) || quantity < 0 || quantity > 999_999) throw new UserError('Quantidade inválida.');
    return this.store.transact(data => {
      const cart = data.carts[id];
      this.assertOwner(cart, buyerId, channelId); this.assertOpen(cart);
      const line = cart.lines.find(l => l.itemId === itemId);
      if (!line) throw new UserError('Produto não encontrado neste carrinho.');
      if (quantity === 0 && cart.lines.length === 1) throw new UserError('Use Cancelar para remover o último produto.');
      line.quantity = quantity;
      cart.lines = cart.lines.filter(l => l.quantity > 0);
      this.refresh(cart, data);
      cart.updatedAt = new Date().toISOString();
      return cart;
    });
  }

  async coupon(id, buyerId, channelId, code) {
    return this.store.transact(data => {
      const cart = data.carts[id];
      this.assertOwner(cart, buyerId, channelId); this.assertOpen(cart); this.refresh(cart, data);
      const normalized = code.trim().toUpperCase();
      if (normalized && normalized !== 'ZEROMS25') throw new UserError('Cupom inválido.');
      if (normalized && totals(cart.lines).subtotal <= 300) throw new UserError('O cupom ZEROMS25 exige subtotal maior que R$ 3,00.');
      cart.coupon = normalized || null;
      cart.updatedAt = new Date().toISOString();
      return cart;
    });
  }

  async checkout(id, buyerId, channelId) {
    return this.store.transact(data => {
      const cart = data.carts[id];
      this.assertOwner(cart, buyerId, channelId); this.assertOpen(cart);
      const previous = JSON.stringify(cart.lines);
      this.refresh(cart, data);
      // O preço visto pelo comprador deve ser o mesmo exibido antes de gerar o pagamento.
      if (JSON.stringify(cart.lines) !== previous) {
        return { cart, priceChanged: true };
      }
      cart.quote = totals(cart.lines, cart.coupon);
      cart.status = 'awaiting_payment';
      cart.checkoutAt = new Date().toISOString();
      return { cart, priceChanged: false };
    });
  }

  async confirm(id, staffId, channelId) {
    return this.store.transact(data => {
      const cart = data.carts[id];
      if (!cart || cart.channelId !== channelId) throw new UserError('Canal de carrinho inválido.');
      if (cart.status === 'paid') return cart; // Idempotência: nunca descontar duas vezes.
      if (cart.status !== 'awaiting_payment') throw new UserError('O comprador precisa clicar em Pagar com Pix antes da confirmação.');
      for (const line of cart.lines) {
        if (!data.items[line.itemId] || data.items[line.itemId].stock < line.quantity) {
          throw new UserError('Estoque insuficiente para confirmar. Confira o pagamento e resolva a reposição ou o reembolso com o comprador.');
        }
      }
      for (const line of cart.lines) {
        data.items[line.itemId].stock -= line.quantity;
        data.items[line.itemId].revision++;
      }
      Object.assign(cart, { status: 'paid', staffId, paidAt: new Date().toISOString(),
        receipt: { ...totals(cart.lines, cart.coupon), coupon: cart.coupon, lines: structuredClone(cart.lines) },
        logMessageId: null, channelDeletedAt: null });
      return cart;
    });
  }

  async cancel(id, actorId, channelId, isStaff = false) {
    return this.store.transact(data => {
      const cart = data.carts[id];
      if (!cart || cart.channelId !== channelId) throw new UserError('Canal de carrinho inválido.');
      if (!isStaff) this.assertOwner(cart, actorId, channelId);
      if (!['open', 'awaiting_payment'].includes(cart.status)) throw new UserError('Este pedido não pode ser cancelado.');
      cart.status = 'cancelled'; cart.cancelledAt = new Date().toISOString();
      return cart;
    });
  }

  async edit(itemId, changes, expectedRevision) {
    return this.store.transact(data => {
      const item = data.items[itemId];
      if (!item) throw new UserError('Produto não encontrado.');
      if (expectedRevision !== undefined && item.revision !== expectedRevision) {
        throw new UserError('O produto mudou desde a abertura do formulário. Reabra /edit antes de salvar para não sobrescrever o estoque atual.');
      }
      if (!Number.isSafeInteger(changes.priceCents) || changes.priceCents < 1 || changes.priceCents > 100_000_000 ||
          !Number.isInteger(changes.stock) || changes.stock < 0 || changes.stock > 999_999 ||
          typeof changes.description !== 'string' || changes.description.length > 180) throw new UserError('Dados de produto inválidos.');
      Object.assign(item, changes);
      item.revision++;
      return item;
    });
  }

  changeEmbed(data, productId, changes, expectedRevision) {
    const product = this.config.products.find(p => p.id === productId);
    const embed = data.embeds?.[productId];
    if (!product || !embed) throw new UserError('Embed do canal não encontrado. Reabra /edit.');
    if (!Number.isSafeInteger(expectedRevision) || embed.revision !== expectedRevision) {
      throw new UserError('O embed mudou desde a abertura do formulário. Reabra /edit para carregar a descrição atual.');
    }
    const image = imageUrl(changes.image);
    if (typeof changes.description !== 'string' || changes.description.length > 4000 || image === undefined) {
      throw new UserError('Use uma descrição de até 4000 caracteres e uma imagem HTTPS válida, ou deixe a imagem vazia para usar o padrão.');
    }
    embed.description = changes.description;
    embed.image = image;
    embed.revision++;
    return product;
  }

  async editEmbed(productId, changes, expectedRevision) {
    return this.store.transact(data => {
      const product = this.changeEmbed(data, productId, changes, expectedRevision);
      catalogPayload(this.config, product, data); // Valida limites antes de gravar.
      return data.embeds[productId];
    });
  }

  async editPublicPanel(itemId, changes, expectedItemRevision, expectedEmbedRevision) {
    return this.store.transact(data => {
      const item = data.items[itemId];
      if (!item || item.revision !== expectedItemRevision) {
        throw new UserError('Valor ou estoque mudou desde a abertura do formulário. Reabra /edit para evitar sobrescrever uma venda.');
      }
      if (!Number.isSafeInteger(changes.priceCents) || changes.priceCents < 1 || changes.priceCents > 100_000_000 ||
          !Number.isInteger(changes.stock) || changes.stock < 0 || changes.stock > 999_999) {
        throw new UserError('Valor ou quantidade em estoque inválidos.');
      }
      const product = this.changeEmbed(data, item.productId, changes, expectedEmbedRevision);
      item.priceCents = changes.priceCents;
      item.stock = changes.stock;
      item.revision++;
      catalogPayload(this.config, product, data);
      return item;
    });
  }
}

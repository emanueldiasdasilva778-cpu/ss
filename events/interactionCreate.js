import { MessageFlags } from 'discord.js';
import * as editCommand from '../commands/edit.js';
import { UserError, report } from '../utils/errors.js';
import { parsePrice, parseQuantity } from '../utils/money.js';
import { editPanel, modal, pixPayload, productSelect, quantitySelect, variantSelect } from '../utils/ui.js';

async function replyError(interaction, error) {
  const content = error instanceof UserError ? error.message : 'Não foi possível concluir agora. A equipe deve verificar as permissões e o registro do bot. Tente novamente em alguns instantes.';
  if (!(error instanceof UserError)) report('Falha ao processar interação', error);
  const payload = { content, embeds: [], components: [], allowedMentions: { parse: [] } };
  if (interaction.deferred && !interaction.replied) await interaction.editReply(payload);
  else if (interaction.replied) await interaction.followUp({ ...payload, flags: MessageFlags.Ephemeral });
  else await interaction.reply({ ...payload, flags: MessageFlags.Ephemeral });
}

// Cada clique e envio de modal revalida cargo/dono; customId nunca é autorização.
export async function handleInteraction(interaction, context) {
  const { config, shop, store, discord } = context;
  if (!(interaction.isChatInputCommand() || interaction.isButton() || interaction.isStringSelectMenu() || interaction.isModalSubmit())) return;
  try {
    if (!interaction.inGuild() || interaction.guildId !== config.guildId) throw new UserError('Use este recurso no servidor configurado.');
    if (!context.ready) throw new UserError('A loja está inicializando. Aguarde alguns instantes.');
    if (interaction.isChatInputCommand()) {
      if (interaction.commandName === 'edit') return await editCommand.execute(interaction, context);
      throw new UserError('Comando desconhecido.');
    }

    const [action, id, itemId] = interaction.customId.split(':');
    // Abertura de modal deve ser a primeira resposta. A checagem de cargo usa consulta atual.
    if (action.startsWith('edit-')) {
      if (action !== 'edit-open') await interaction.deferReply({ flags: MessageFlags.Ephemeral });
      await discord.staff(interaction, true);
      const snapshot = store.read();
      if (action === 'edit-product' && interaction.isStringSelectMenu()) {
        const product = config.products.find(p => p.id === interaction.values[0]);
        if (!product) throw new UserError('Catálogo inválido.');
        return await interaction.editReply(variantSelect(product, snapshot));
      }
      if (action === 'edit-variant' && interaction.isStringSelectMenu()) {
        const product = config.products.find(p => p.id === id);
        const item = snapshot.items[interaction.values[0]];
        if (!item || !product?.variants.some(v => v.id === item.id)) throw new UserError('Produto inválido.');
        return await interaction.editReply(editPanel(config, item));
      }
      const item = snapshot.items[id];
      if (!item) throw new UserError('Produto inválido.');
      if (action === 'edit-open' && interaction.isButton()) {
        return await interaction.showModal(modal(`edit-save:${id}:${item.revision}`, 'Editar produto', [
          { id: 'price', label: 'Preço em reais (ex.: 8,75)', value: (item.priceCents / 100).toFixed(2).replace('.', ','), max: 12 },
          { id: 'stock', label: 'Quantidade em estoque', value: String(item.stock), max: 6 },
          { id: 'description', label: 'Descrição do produto', value: item.description, max: 180, required: false, paragraph: true },
        ]));
      }
      if (action === 'edit-save' && interaction.isModalSubmit()) {
        if (!/^\d+$/.test(itemId ?? '')) throw new UserError('Formulário antigo. Reabra /edit para continuar.');
        await shop.edit(id, { priceCents: parsePrice(interaction.fields.getTextInputValue('price')),
          stock: parseQuantity(interaction.fields.getTextInputValue('stock'), { allowZero: true }),
          description: interaction.fields.getTextInputValue('description').trim() }, Number(itemId));
        try { await discord.exclusive(() => discord.catalogs()); }
        catch {
          return await interaction.editReply({ content: 'Alteração salva. O Discord recusou a atualização do painel; o bot tentará novamente automaticamente. Confira as permissões e os registros.', components: [] });
        }
        return await interaction.editReply({ ...editPanel(config, store.read().items[id]), content: 'Produto salvo e catálogo atualizado.' });
      }
      throw new UserError('Ação de edição inválida.');
    }

    if (action === 'add' || action === 'catalog') {
      await interaction.deferReply({ flags: MessageFlags.Ephemeral });
      let selectedId = id;
      if (action === 'catalog') {
        if (!interaction.isStringSelectMenu()) throw new UserError('Seleção inválida.');
        selectedId = interaction.values[0];
        const product = config.products.find(p => p.id === id);
        if (!product?.variants.some(v => v.id === selectedId)) throw new UserError('Produto inválido.');
      }
      const cart = await discord.add(interaction.user, selectedId);
      return await interaction.editReply(`Produto adicionado! Acesse seu carrinho: <#${cart.channelId}>.`);
    }

    const cart = shop.cart(id);
    if (cart.channelId !== interaction.channelId) throw new UserError('Esta ação só funciona no canal original do carrinho.');
    if (action === 'confirm') {
      await interaction.deferReply({ flags: MessageFlags.Ephemeral });
      await discord.staff(interaction);
      await discord.exclusive(async () => {
        const paid = await shop.confirm(id, interaction.user.id, interaction.channelId);
        await interaction.editReply(`Pagamento do pedido ${paid.id} confirmado. Estoque registrado; o canal será fechado após salvar o log.`);
        try { await discord.updateCart(id); } catch (error) { report('Atualização do carrinho pago', error); }
        try { await discord.settle(id); }
        catch (error) {
          report(`Finalização pendente ${id}`, error);
          await interaction.editReply('Pagamento já registrado e estoque reduzido uma única vez. O log ou fechamento está pendente; o bot tentará novamente automaticamente. Não confirme nem cobre novamente.');
        }
      });
      return;
    }
    if (action === 'cancel') {
      await interaction.deferReply({ flags: MessageFlags.Ephemeral });
      let isStaff = false;
      if (cart.buyerId !== interaction.user.id) { await discord.staff(interaction); isStaff = true; }
      await discord.exclusive(async () => {
        await shop.cancel(id, interaction.user.id, interaction.channelId, isStaff);
        await interaction.editReply('Carrinho cancelado. Se já enviou um Pix, fale com a equipe para resolver o pagamento.');
        await discord.deleteCartChannel(id);
      });
      return;
    }

    shop.assertOwner(cart, interaction.user.id, interaction.channelId);
    if (action === 'coupon' && interaction.isButton()) {
      shop.assertOpen(cart);
      return await interaction.showModal(modal(`coupon-save:${id}`, 'Aplicar cupom de desconto', [
        { id: 'code', label: 'Código (deixe vazio para remover)', max: 30, required: false, value: cart.coupon ?? '' },
      ]));
    }
    if (action === 'quantity-item' && interaction.isStringSelectMenu()) {
      shop.assertOpen(cart);
      const line = cart.lines.find(l => l.itemId === interaction.values[0]);
      if (!line) throw new UserError('Produto fora do carrinho.');
      return await interaction.showModal(modal(`quantity-save:${id}:${line.itemId}`, 'Alterar quantidade', [
        { id: 'quantity', label: 'Quantidade (zero remove a linha)', value: String(line.quantity), max: 6 },
      ]));
    }

    await interaction.deferReply({ flags: MessageFlags.Ephemeral });
    if (action === 'quantity' && interaction.isButton()) {
      shop.assertOpen(cart);
      return await interaction.editReply(quantitySelect(cart));
    }
    if (action === 'coupon-save' && interaction.isModalSubmit()) {
      await shop.coupon(id, interaction.user.id, interaction.channelId, interaction.fields.getTextInputValue('code'));
      await discord.exclusive(() => discord.updateCart(id));
      return await interaction.editReply('Cupom atualizado. Confira o total no carrinho.');
    }
    if (action === 'quantity-save' && interaction.isModalSubmit()) {
      await shop.quantity(id, interaction.user.id, interaction.channelId, itemId,
        parseQuantity(interaction.fields.getTextInputValue('quantity'), { allowZero: true }));
      await discord.exclusive(() => discord.updateCart(id));
      return await interaction.editReply('Quantidade atualizada. Confira o total no carrinho.');
    }
    if (action === 'pay' && interaction.isButton()) {
      let result;
      await discord.exclusive(async () => {
        const current = shop.cart(id);
        if (current.status === 'awaiting_payment') result = { cart: current, priceChanged: false };
        else result = await shop.checkout(id, interaction.user.id, interaction.channelId);
        await discord.updateCart(id);
      });
      if (result.priceChanged) return await interaction.editReply('O preço mudou desde sua última ação. O carrinho foi atualizado. Confira o total e clique novamente em Pagar com Pix.');
      return await interaction.editReply(pixPayload(config, result.cart));
    }
    throw new UserError('Ação desconhecida ou expirada. Abra o painel novamente.');
  } catch (error) {
    try { await replyError(interaction, error); }
    catch (replyFailure) { report('Falha ao responder à interação', replyFailure); }
  }
}

import {
  ActionRowBuilder, ButtonBuilder, ButtonStyle, EmbedBuilder,
  StringSelectMenuBuilder, ModalBuilder, TextInputBuilder, TextInputStyle, escapeMarkdown,
} from 'discord.js';
import { money, totals } from './money.js';
import { formatText, safeText } from './format.js';
import { UserError } from './errors.js';

const row = (...components) => new ActionRowBuilder().addComponents(...components);
const button = (id, label, style, disabled = false) => new ButtonBuilder().setCustomId(id).setLabel(label).setStyle(style).setDisabled(disabled);
const text = value => escapeMarkdown(String(value));
export const noMentions = { parse: [], repliedUser: false };
export const baseEmbed = config => new EmbedBuilder().setColor(config.color).setFooter({ text: config.footer });

export function catalogPayload(config, product, data) {
  const items = product.variants.map(v => data.items[v.id]);
  const cheapest = Math.min(...items.map(v => v.priceCents));
  const description = `${formatText(product.description, config)}\n\n**💰 A partir de ${money(cheapest)}**\n\n**📦 Catálogo completo**\n${items.map(item =>
    `• ${safeText(item.name, config)} – ${money(item.priceCents)} (${item.stock} un)${item.description ? `\n  ${formatText(item.description, config)}` : ''}`).join('\n')}`;
  const parts = [];
  let remaining = description;
  while (remaining.length > 4096) {
    let boundary = remaining.lastIndexOf('\n', 4096);
    if (boundary < 2000) boundary = remaining.lastIndexOf(' ', 4096);
    if (boundary < 1) boundary = 4096;
    if (/[\uD800-\uDBFF]/.test(remaining[boundary - 1])) boundary--;
    parts.push(remaining.slice(0, boundary));
    remaining = remaining.slice(boundary);
  }
  parts.push(remaining);
  const embeds = parts.map((part, index) => baseEmbed(config)
    .setTitle(`${formatText(product.name, config)}${index ? ' — Continuação' : ''}`).setDescription(part)
    // Marcador persistente permite reencontrar o painel após reinicialização.
    .setFooter({ text: `${config.footer} | Catálogo ${product.id}` }));
  const totalLength = embeds.reduce((sum, e) => sum + e.data.title.length + e.data.description.length + e.data.footer.text.length, 0);
  if (totalLength > 6000) throw new UserError(`O catálogo ${product.id} ultrapassa o limite de 6000 caracteres do Discord após resolver os emojis. Reduza a descrição.`);
  if (product.image) embeds[0].setImage(product.image.trim());
  let component;
  if (items.length === 1) {
    component = button(`add:${items[0].id}`, 'Adicionar ao Carrinho', ButtonStyle.Success, items[0].stock === 0);
  } else {
    const available = items.filter(item => item.stock > 0);
    component = new StringSelectMenuBuilder().setCustomId(`catalog:${product.id}`)
      .setPlaceholder(available.length ? 'Selecione o produto para adicionar ao carrinho' : 'Estoque esgotado')
      .setDisabled(!available.length).addOptions((available.length ? available : items.slice(0, 1)).map(item => ({
        label: item.name, value: item.id, description: `${money(item.priceCents)} • ${item.stock} unidades`,
      })));
  }
  return { embeds, components: [row(component)], allowedMentions: noMentions };
}

export function cartPayload(config, cart) {
  const quote = cart.receipt ?? cart.quote ?? totals(cart.lines, cart.coupon);
  const locked = cart.status !== 'open';
  const closed = !['open', 'creating', 'awaiting_payment'].includes(cart.status);
  const embed = baseEmbed(config).setTitle(`🛒 Carrinho de compras de ${cart.username}`)
    .setDescription(`Olá ${text(cart.username)}, seja bem-vindo ao seu carrinho de compras!\n\n` +
      `**📦 Produtos no carrinho**\n${cart.lines.map(line => `• ${line.quantity}x ${text(line.name)} - ${money(line.priceCents * line.quantity)}`).join('\n')}\n\n` +
      `**💵 Subtotal:** ${money(quote.subtotal)}\n` +
      `**💸 Cupom de desconto:** ${cart.coupon ? `${cart.coupon} (25%)` : 'Sem cupom aplicado'}\n` +
      `**🛒 Valor total:** ${money(quote.total)}`);
  const components = [row(
    button(`pay:${cart.id}`, '🟢 Pagar com Pix', ButtonStyle.Success, closed),
    button(`coupon:${cart.id}`, '🎟️ Cupom', ButtonStyle.Primary, locked),
    button(`quantity:${cart.id}`, '📦 Quantidade', ButtonStyle.Secondary, locked),
    button(`cancel:${cart.id}`, '⬛ Cancelar', ButtonStyle.Danger, closed),
  ), row(button(`confirm:${cart.id}`, '✅ Confirmar Pagamento', ButtonStyle.Success, cart.status !== 'awaiting_payment'))];
  return { embeds: [embed], components, allowedMentions: noMentions };
}

export function pixPayload(config, cart) {
  const total = cart.quote?.total ?? totals(cart.lines, cart.coupon).total;
  return { embeds: [baseEmbed(config).setTitle('💠 Pagamento por Pix — confirmação manual')
    .setDescription(`**Pedido:** ${cart.id}\n**Total:** ${money(total)}\n\n` +
      `${config.pix.key ? `**Chave Pix:** ${text(config.pix.key)}\n` : ''}` +
      `${config.pix.beneficiary ? `**Favorecido:** ${text(config.pix.beneficiary)}\n` : ''}` +
      `${text(config.pix.instructions)}\n\nO estoque não está reservado. A equipe precisa conferir o crédito na conta antes de confirmar. ` +
      `Se houver pagamento e falta de estoque, solicite atendimento para reposição ou reembolso. A confirmação não entrega produtos automaticamente.`)],
    allowedMentions: noMentions };
}

export function receiptPayload(config, cart) {
  const receipt = cart.receipt;
  const date = new Intl.DateTimeFormat('pt-BR', { timeZone: config.timezone, dateStyle: 'short', timeStyle: 'long' }).format(new Date(cart.paidAt));
  return { embeds: [baseEmbed(config).setTitle('✅ Venda confirmada manualmente')
    .setDescription(`**Pedido:** ${cart.id}\n**Usuário:** ${text(cart.username)} (${cart.buyerId})\n` +
      `**Responsável:** ${cart.staffId}\n\n**Produtos**\n${receipt.lines.map(line =>
        `• ${line.quantity}x ${text(line.name)} — ${money(line.priceCents)} cada — ${money(line.priceCents * line.quantity)}`).join('\n')}\n\n` +
      `**Valor original:** ${money(receipt.subtotal)}\n**Cupom usado:** ${receipt.coupon ?? 'Sem cupom aplicado'}\n` +
      `**Desconto:** ${money(receipt.discount)}\n**Valor final pago:** ${money(receipt.total)}\n` +
      `**Data e hora:** ${date} (${config.timezone})`)
    .setFooter({ text: `${config.footer} | Pedido ${cart.id}` }).setTimestamp(new Date(cart.paidAt))], allowedMentions: noMentions };
}

export function productSelect(config) {
  return { content: 'Selecione o catálogo que deseja editar.', components: [row(new StringSelectMenuBuilder()
    .setCustomId('edit-product').setPlaceholder('Catálogo').addOptions(config.products.map(p => ({ label: p.name, value: p.id }))))] };
}

export function variantSelect(product, data) {
  return { content: `Selecione a opção de ${product.name}.`, components: [row(new StringSelectMenuBuilder()
    .setCustomId(`edit-variant:${product.id}`).setPlaceholder('Produto').addOptions(product.variants.map(v => ({
      label: data.items[v.id].name, value: v.id,
    }))))] };
}

export function editPanel(config, item) {
  return { content: null, embeds: [baseEmbed(config).setTitle(`Editar: ${item.name}`)
    .setDescription(`**Preço:** ${money(item.priceCents)}\n**Estoque:** ${item.stock}\n**Descrição:** ${formatText(item.description || 'Sem descrição', config)}`)],
  components: [row(button(`edit-open:${item.id}`, '✏️ Editar preço, estoque e descrição', ButtonStyle.Primary)),
    row(new StringSelectMenuBuilder().setCustomId('edit-product').setPlaceholder('Trocar catálogo')
      .addOptions(config.products.map(p => ({ label: p.name, value: p.id }))))] };
}

export function modal(id, title, fields) {
  return new ModalBuilder().setCustomId(id).setTitle(title).addComponents(fields.map(field => row(
    new TextInputBuilder().setCustomId(field.id).setLabel(field.label)
      .setStyle(field.paragraph ? TextInputStyle.Paragraph : TextInputStyle.Short)
      .setRequired(field.required !== false).setMaxLength(field.max ?? 100)
      .setValue(field.value ?? ''),
  )));
}

export function quantitySelect(cart) {
  return { content: 'Selecione o produto. Informe zero para remover uma linha do carrinho.',
    components: [row(new StringSelectMenuBuilder().setCustomId(`quantity-item:${cart.id}`).setPlaceholder('Produto do carrinho')
      .addOptions(cart.lines.map(l => ({ label: l.name, value: l.itemId, description: `Quantidade atual: ${l.quantity}` }))))] };
}

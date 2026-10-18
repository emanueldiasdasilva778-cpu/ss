import { ChannelType, InteractionContextType, MessageFlags, SlashCommandBuilder } from 'discord.js';
import { catalogEditPanel, productSelect } from '../utils/ui.js';
import { UserError } from '../utils/errors.js';

export const data = new SlashCommandBuilder().setName('edit')
  .setDescription('Edita descrição completa e imagem do embed, valor e quantidade em estoque.')
  .addChannelOption(option => option.setName('canal').setDescription('Canal do embed que deseja editar')
    .addChannelTypes(ChannelType.GuildText))
  .setContexts(InteractionContextType.Guild);

export async function execute(interaction, context) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  await context.discord.staff(interaction, true);
  const channel = interaction.options?.getChannel('canal');
  if (channel) {
    const product = context.config.products.find(p => p.channelId === channel.id);
    if (!product) throw new UserError('Esse canal não está cadastrado no catálogo da loja.');
    return interaction.editReply(catalogEditPanel(context.config, product, context.store.read()));
  }
  await interaction.editReply(productSelect(context.config));
}

import { InteractionContextType, MessageFlags, SlashCommandBuilder } from 'discord.js';
import { productSelect } from '../utils/ui.js';

export const data = new SlashCommandBuilder().setName('edit')
  .setDescription('Abre o painel de edição de preço, estoque e descrição dos produtos.')
  .setContexts(InteractionContextType.Guild);

export async function execute(interaction, context) {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  await context.discord.staff(interaction, true);
  await interaction.editReply(productSelect(context.config));
}

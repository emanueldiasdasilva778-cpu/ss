import { report } from './errors.js';

export async function refreshGuildEmojis(guild, config) {
  try {
    const emojis = await guild.emojis.fetch();
    const resolved = {};
    for (const emoji of emojis.values()) {
      if (emoji.available === false || !emoji.name) continue;
      const value = `<${emoji.animated ? 'a' : ''}:${emoji.name}:${emoji.id}>`;
      resolved[emoji.name] = value;
      resolved[emoji.id] = value;
    }
    config.resolvedEmojis = resolved;
  } catch (error) {
    // Uma falha no catálogo de emojis não deve impedir o atendimento da loja.
    report('Não foi possível consultar emojis do servidor; usando os alternativos', error);
  }
}

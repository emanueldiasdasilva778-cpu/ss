import { escapeMarkdown } from 'discord.js';

const defaults = {
  shopping_cart: '🛒', package: '📦', moneybag: '💰', money_with_wings: '💸',
  credit_card: '💳', ticket: '🎟️', white_check_mark: '✅', lock: '🔒',
  snowflake: '❄️', gem: '💎', calendar: '📅', hourglass_flowing_sand: '⏳',
  computer: '💻', video_game: '🎮', gear: '⚙️', pix: '💠', speech_balloon: '💬',
};

// Emojis personalizados precisam do ID real: <:nome:ID> ou <a:nome:ID>.
// Nomes sem ID são resolvidos pelo mapa do config.json; não inventamos IDs.
export function formatText(value, config = {}) {
  const emojis = { ...defaults, ...config.emojis };
  return String(value).replace(/<a?:\w+:\d{17,20}>|:([A-Za-z0-9_]+):/g,
    (match, alias) => alias ? (emojis[alias] ?? match) : match);
}

export function safeText(value, config = {}) {
  return formatText(value, config).split(/(<a?:\w+:\d{17,20}>)/g)
    .map(part => /^<a?:\w+:\d{17,20}>$/.test(part) ? part : escapeMarkdown(part)).join('');
}

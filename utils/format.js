import { escapeMarkdown } from 'discord.js';

const defaults = {
  shopping_cart: '🛒', package: '📦', moneybag: '💰', money_with_wings: '💸',
  credit_card: '💳', ticket: '🎟️', white_check_mark: '✅', lock: '🔒',
  snowflake: '❄️', gem: '💎', calendar: '📅', hourglass_flowing_sand: '⏳',
  computer: '💻', video_game: '🎮', gear: '⚙️', pix: '💠', speech_balloon: '💬',
  Zennify_box: '📦', '9504_purple_flame': '🔥', '44250purplecheck': '✅',
  shoppingcart: '🛒', ant5: '⚡', '1298339916803215483': '💳',
  'star~1': '⭐', fogue: '🔥', '144': 'ℹ️',
};

// Emojis personalizados precisam do ID real: <:nome:ID> ou <a:nome:ID>.
// Nomes sem ID são resolvidos pelo mapa do config.json; não inventamos IDs.
export function formatText(value, config = {}) {
  return String(value).replace(/<a?:\w+:\d{17,20}>|:([A-Za-z0-9_]+(?:~\d+)?):/g,
    (match, alias) => {
      if (!alias) return match;
      const normalized = alias.replace(/~\d+$/, '');
      return config.emojis?.[alias] ?? config.resolvedEmojis?.[alias] ?? config.resolvedEmojis?.[normalized] ??
        defaults[alias] ?? defaults[normalized] ?? match;
    });
}

export function safeText(value, config = {}) {
  return formatText(value, config).split(/(<a?:\w+:\d{17,20}>)/g)
    .map(part => /^<a?:\w+:\d{17,20}>$/.test(part) ? part : escapeMarkdown(part)).join('');
}

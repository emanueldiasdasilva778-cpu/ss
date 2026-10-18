import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { UserError } from '../utils/errors.js';
import { imageUrl } from '../utils/catalog.js';

export const root = fileURLToPath(new URL('../', import.meta.url));
const snowflake = value => typeof value === 'string' && /^\d{17,20}$/.test(value);
function requireSetting(condition, message) { if (!condition) throw new UserError(`Configuração inválida: ${message}`); }

export function validateConfig(config, { requireCredentials = true } = {}) {
  requireSetting(config && typeof config === 'object', 'arquivo JSON inválido.');
  requireSetting(imageUrl(config.defaultImage) !== undefined, 'defaultImage deve ser uma URL HTTPS ou null para usar o banner incluído.');
  if (config.emojis !== undefined) {
    requireSetting(config.emojis && typeof config.emojis === 'object' && !Array.isArray(config.emojis) &&
      Object.entries(config.emojis).every(([name, value]) => /^[A-Za-z0-9_]+(?:~\d+)?$/.test(name) &&
        typeof value === 'string' && value.length > 0 && value.length <= 100), 'emojis deve mapear nomes para emojis Unicode ou códigos reais do Discord.');
  }
  if (requireCredentials) {
    requireSetting(typeof config.token === 'string' && config.token.length > 30 && !config.token.includes('COLOQUE_'), 'preencha token no config.json.');
    requireSetting(snowflake(config.guildId), 'preencha guildId no config.json.');
  }
  requireSetting(snowflake(config.editRoleId) && snowflake(config.logChannelId), 'IDs de edição e logs.');
  requireSetting(Array.isArray(config.staffRoleIds) && config.staffRoleIds.length > 0 && config.staffRoleIds.length <= 50 && config.staffRoleIds.every(snowflake), 'staffRoleIds deve conter cargos válidos.');
  requireSetting(config.staffRoleIds.includes(config.editRoleId), 'o cargo de edição precisa constar em staffRoleIds.');
  requireSetting(!config.cartCategoryId || snowflake(config.cartCategoryId), 'cartCategoryId.');
  requireSetting(/^#[\da-fA-F]{6}$/.test(config.color), 'color deve ser hexadecimal.');
  requireSetting(typeof config.footer === 'string' && config.footer.length <= 200, 'footer deve ter até 200 caracteres.');
  requireSetting(Number.isInteger(config.cartIdleMinutes) && config.cartIdleMinutes >= 5 && config.cartIdleMinutes <= 10080, 'cartIdleMinutes deve ficar entre 5 e 10080.');
  requireSetting(Number.isInteger(config.reconcileIntervalSeconds) && config.reconcileIntervalSeconds >= 10 && config.reconcileIntervalSeconds <= 300, 'reconcileIntervalSeconds deve ficar entre 10 e 300.');
  try { new Intl.DateTimeFormat('pt-BR', { timeZone: config.timezone }).format(); }
  catch { throw new UserError('Configuração inválida: timezone.'); }
  requireSetting(config.pix && ['key', 'beneficiary', 'instructions'].every(k => typeof config.pix[k] === 'string') &&
    config.pix.key.length <= 200 && config.pix.beneficiary.length <= 200 && config.pix.instructions.length <= 1000, 'campos Pix ausentes ou longos demais.');
  requireSetting(Array.isArray(config.products) && config.products.length > 0 && config.products.length <= 25, 'products deve ter entre 1 e 25 produtos.');
  const ids = new Set(), channels = new Set();
  for (const product of config.products) {
    requireSetting(/^[a-z0-9-]{1,40}$/.test(product.id) && !ids.has(product.id), 'ID de produto duplicado/inválido.');
    ids.add(product.id);
    requireSetting(snowflake(product.channelId) && !channels.has(product.channelId) && product.channelId !== config.logChannelId, 'cada produto precisa de canal exclusivo, diferente do canal de logs.');
    channels.add(product.channelId);
    requireSetting(typeof product.name === 'string' && product.name.length > 0 && product.name.length <= 80, 'nome do produto.');
    requireSetting(typeof product.description === 'string' && product.description.length <= 4000, 'descrição do embed deve ter até 4000 caracteres.');
    requireSetting(imageUrl(product.image) !== undefined, 'imagem deve ser uma URL HTTPS válida.');
    requireSetting(Array.isArray(product.variants) && product.variants.length > 0 && product.variants.length <= 10, 'cada catálogo deve ter entre 1 e 10 opções.');
    for (const item of product.variants) {
      requireSetting(/^[a-z0-9-]{1,40}$/.test(item.id) && !ids.has(item.id), 'ID de opção duplicado/inválido.');
      ids.add(item.id);
      requireSetting(typeof item.name === 'string' && item.name.length > 0 && item.name.length <= 80, 'nome da opção.');
      requireSetting(Number.isSafeInteger(item.priceCents) && item.priceCents > 0 && item.priceCents <= 100_000_000, 'preço em centavos.');
      requireSetting(Number.isInteger(item.stock) && item.stock >= 0 && item.stock <= 999_999, 'estoque inteiro não negativo.');
      requireSetting(typeof item.description === 'string' && item.description.length <= 180, 'descrição da opção deve ter até 180 caracteres.');
    }
  }
  return config;
}

export async function loadConfig(options) {
  let config;
  try { config = JSON.parse(await readFile(new URL('../config.json', import.meta.url), 'utf8')); }
  catch { throw new UserError('Não foi possível ler config.json. Copie config.example.json para config.json e preencha os dados.'); }
  return validateConfig(config, options);
}

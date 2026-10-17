import { UserError } from './errors.js';

export const money = cents => new Intl.NumberFormat('pt-BR', {
  style: 'currency', currency: 'BRL',
}).format(cents / 100);

export function parsePrice(text) {
  if (!/^\d{1,7}(?:[,.]\d{1,2})?$/.test(text.trim())) {
    throw new UserError('Informe um preço como 8,75, sem símbolo ou separador de milhares.');
  }
  const [whole, decimals = ''] = text.trim().replace(',', '.').split('.');
  const cents = Number(whole) * 100 + Number(decimals.padEnd(2, '0'));
  if (cents < 1 || cents > 100_000_000) throw new UserError('O preço deve ficar entre R$ 0,01 e R$ 1.000.000,00.');
  return cents;
}

export function parseQuantity(text, { allowZero = false } = {}) {
  if (!/^\d{1,6}$/.test(text.trim())) throw new UserError('Informe uma quantidade inteira válida.');
  const n = Number(text.trim());
  if (n < (allowZero ? 0 : 1) || n > 999_999) throw new UserError('Quantidade fora do limite permitido.');
  return n;
}

// Centavos inteiros evitam erros de ponto flutuante; desconto arredondado ao centavo.
export function totals(lines, coupon = null) {
  const subtotal = lines.reduce((sum, line) => sum + line.priceCents * line.quantity, 0);
  const discount = coupon === 'ZEROMS25' && subtotal > 300 ? Math.round(subtotal / 4) : 0;
  return { subtotal, discount, total: subtotal - discount };
}

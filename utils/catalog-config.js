// Importa somente a apresentação do catálogo. Credenciais, preços e estoques não são copiados.
export function mergeCatalog(current, incoming) {
  if (!Array.isArray(incoming?.products)) throw new Error('CATALOG_PRODUCTS_REQUIRED');
  const result = structuredClone(current);
  if (incoming.emojis) result.emojis = { ...result.emojis, ...incoming.emojis };
  for (const product of result.products) {
    const source = incoming.products.find(p => p.id === product.id);
    if (!source) continue;
    for (const key of ['name', 'description', 'image']) {
      if (Object.hasOwn(source, key)) product[key] = source[key];
    }
    for (const item of product.variants) {
      const variant = source.variants?.find(v => v.id === item.id);
      if (!variant) continue;
      for (const key of ['name', 'description']) {
        if (Object.hasOwn(variant, key)) item[key] = variant[key];
      }
    }
  }
  return result;
}

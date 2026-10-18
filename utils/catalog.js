// O texto público do canal pertence ao catálogo; não é a descrição curta de uma opção.
export function catalogDetails(product, data) {
  const saved = data.embeds?.[product.id];
  return { ...product,
    description: saved?.description ?? product.description,
    image: saved ? saved.image : product.image,
    revision: saved?.revision ?? 0,
  };
}

export function imageUrl(value) {
  const image = String(value ?? '').trim();
  if (!image) return null;
  try {
    const url = new URL(image);
    if (url.protocol !== 'https:' || url.username || url.password || image.length > 1800) return undefined;
    return url.href;
  } catch { return undefined; }
}

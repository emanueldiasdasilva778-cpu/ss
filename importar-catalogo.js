import { open, readFile, rename, unlink } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { randomUUID } from 'node:crypto';
import { root, validateConfig } from './config/load.js';
import { mergeCatalog } from './utils/catalog-config.js';
import { report, UserError } from './utils/errors.js';

const target = join(root, 'config.json');
const temporary = `${target}.${randomUUID()}.tmp`;
try {
  const source = resolve(root, process.argv[2] ?? 'config.example.json');
  const incoming = JSON.parse(await readFile(source, 'utf8'));
  let current;
  try { current = JSON.parse(await readFile(target, 'utf8')); }
  catch (error) {
    if (error.code !== 'ENOENT') throw error;
    current = JSON.parse(await readFile(join(root, 'config.example.json'), 'utf8'));
    console.info('Criando config.json a partir do exemplo. Preencha seu token antes de iniciar.');
  }
  const merged = validateConfig(mergeCatalog(current, incoming), { requireCredentials: false });
  const handle = await open(temporary, 'wx', 0o600);
  try {
    await handle.writeFile(`${JSON.stringify(merged, null, 2)}\n`);
    await handle.sync();
  } finally { await handle.close(); }
  await rename(temporary, target);
  console.info('Nomes, imagens, descrições e emojis importados para config.json. Token, IDs, preços e estoques preservados. O bot ligado atualizará os painéis automaticamente; se estiver desligado, inicie-o.');
} catch (error) {
  if (error instanceof UserError) console.error(error.message);
  else if (error.code === 'ENOENT') console.error('Arquivo não encontrado. Crie config.json a partir do exemplo antes de importar o catálogo.');
  else report('Não foi possível importar o catálogo; a configuração original foi preservada', error);
  process.exitCode = 1;
} finally {
  await unlink(temporary).catch(error => { if (error.code !== 'ENOENT') throw error; });
}

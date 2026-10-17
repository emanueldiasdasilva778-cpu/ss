import { readdir } from 'node:fs/promises';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { loadConfig } from './config/load.js';

async function check(folder) {
  for (const entry of await readdir(folder, { withFileTypes: true })) {
    if (['node_modules', 'data', '.git'].includes(entry.name)) continue;
    const path = join(folder, entry.name);
    if (entry.isDirectory()) await check(path);
    else if (path.endsWith('.js')) {
      const result = spawnSync(process.execPath, ['--check', path], { stdio: 'inherit' });
      if (result.status !== 0) throw new Error(`Sintaxe inválida: ${entry.name}`);
    }
  }
}
await check(fileURLToPath(new URL('.', import.meta.url)));
await loadConfig({ requireCredentials: false });
console.info('Sintaxe JavaScript e estrutura do config.json válidas. Credenciais não foram testadas.');

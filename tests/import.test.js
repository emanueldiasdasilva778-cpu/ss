import test from 'node:test';
import assert from 'node:assert/strict';
import { copyFile, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const run = promisify(execFile);
const root = fileURLToPath(new URL('../', import.meta.url));

test('importador funciona em pasta nova e preserva credenciais nas importações seguintes', async t => {
  const directory = await mkdtemp(join(tmpdir(), 'zeroms-import-'));
  t.after(() => rm(directory, { recursive: true, force: true }));
  for (const name of ['package.json', 'config.example.json', 'importar-catalogo.js', 'config/load.js', 'utils/catalog-config.js', 'utils/errors.js']) {
    await mkdir(dirname(join(directory, name)), { recursive: true });
    await copyFile(join(root, name), join(directory, name));
  }
  const script = join(directory, 'importar-catalogo.js');
  const first = await run(process.execPath, [script], { cwd: directory });
  assert.ok(first.stdout.includes('Criando config.json'));
  const file = join(directory, 'config.json');
  const current = JSON.parse(await readFile(file, 'utf8'));
  assert.equal(current.token, 'COLOQUE_O_TOKEN_DO_BOT_AQUI');
  current.token = 'credencial-local-ficticia-para-teste';
  current.products[0].variants[0].stock = 2;
  await writeFile(file, JSON.stringify(current));
  const second = await run(process.execPath, [script], { cwd: directory });
  const updated = JSON.parse(await readFile(file, 'utf8'));
  assert.equal(updated.token, current.token);
  assert.equal(updated.products[0].variants[0].stock, 2);
  assert.equal(second.stdout.includes(current.token), false);
  assert.ok(updated.products.find(p => p.channelId === '1561129676124983306').name.includes('Guest'));
});

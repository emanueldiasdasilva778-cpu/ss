import { mkdir, open, readFile, rename, unlink } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { randomUUID } from 'node:crypto';

// Uma única instância por diretório: impede dois processos de venderem o mesmo estoque.
// Escrita atômica + fsync do arquivo; em POSIX também sincroniza o diretório.
export class Store {
  constructor(directory, { platform = process.platform, openFile = open } = {}) {
    this.directory = directory;
    this.file = join(directory, 'store.json');
    this.lockFile = join(directory, 'instance.lock');
    this.tail = Promise.resolve();
    this.platform = platform;
    this.openFile = openFile;
  }

  async init(products) {
    await mkdir(this.directory, { recursive: true, mode: 0o700 });
    this.lock = await this.openFile(this.lockFile, 'wx', 0o600).catch(error => {
      if (error.code === 'EEXIST') throw new Error('INSTANCE_LOCKED');
      throw error;
    });
    try {
      await this.lock.writeFile(JSON.stringify({ pid: process.pid, startedAt: new Date().toISOString() }));
      try {
        this.data = JSON.parse(await readFile(this.file, 'utf8'));
        if (this.data.schema !== 1 || !this.data.items || !this.data.carts || !this.data.panels) {
          throw new Error('INVALID_STORE');
        }
      } catch (error) {
        if (error.code !== 'ENOENT') throw error; // Banco corrompido nunca vira estoque novo.
        this.data = { schema: 1, items: {}, carts: {}, panels: {} };
      }
      await this.syncCatalog(products);
    } catch (error) {
      await this.close();
      throw error;
    }
    return this;
  }

  async syncCatalog(products) {
    return this.transact(data => {
      data.catalogSources ??= {};
      data.embeds ??= {};
      for (const product of products) {
        const sourceImage = product.image?.trim() || null;
        const saved = data.embeds[product.id];
        if (!saved) {
          data.embeds[product.id] = { description: product.description, image: sourceImage, revision: 0,
            sourceDescription: product.description, sourceImage };
        } else {
          let changed = false;
          if (saved.sourceDescription !== product.description) {
            saved.description = product.description; changed = true;
          }
          if (saved.sourceImage !== sourceImage) {
            saved.image = sourceImage; changed = true;
          }
          if (changed) saved.revision++;
          saved.sourceDescription = product.description;
          saved.sourceImage = sourceImage;
        }
        for (const item of product.variants) {
          const existing = data.items[item.id];
          const previous = data.catalogSources[item.id];
          data.items[item.id] ??= { ...item, productId: product.id };
          data.items[item.id].revision ??= 0;
          // Alterações explícitas da descrição no config entram sem repor estoque.
          // Sem mudança no config, preserve o que a equipe salvou pelo /edit.
          const importDescription = previous ? previous.description !== item.description : Boolean(item.description);
          if (existing && (existing.name !== item.name || (importDescription && existing.description !== item.description))) {
            existing.revision++;
          }
          if (importDescription) data.items[item.id].description = item.description;
          data.items[item.id].name = item.name;
          data.items[item.id].productId = product.id;
          data.catalogSources[item.id] = { description: item.description };
        }
      }
    });
  }

  read() { return structuredClone(this.data); }

  transact(change) {
    const job = this.tail.then(async () => {
      if (this.failed) throw new Error('STORE_WRITE_DISABLED_AFTER_IO_FAILURE');
      const next = structuredClone(this.data);
      const result = change(next); // Não permite efeitos Discord dentro da transação.
      if (result && typeof result.then === 'function') throw new Error('ASYNC_TRANSACTION_NOT_ALLOWED');
      const temporary = `${this.file}.${randomUUID()}.tmp`;
      try {
        const handle = await this.openFile(temporary, 'wx', 0o600);
        try {
          await handle.writeFile(`${JSON.stringify(next, null, 2)}\n`);
          await handle.sync();
        } finally { await handle.close(); }
        await rename(temporary, this.file);
        this.data = next;
        // Windows não permite abrir diretórios com fs.open para executar fsync (EPERM).
        // O fsync do arquivo e a substituição por rename continuam obrigatórios em todos os SOs.
        if (this.platform !== 'win32') {
          const folder = await this.openFile(dirname(this.file), 'r');
          try { await folder.sync(); } finally { await folder.close(); }
        }
        return structuredClone(result);
      } catch (error) {
        // Falha de disco tem resultado incerto: bloqueie novas vendas até revisão/reinício.
        this.failed = true;
        throw error;
      } finally { await unlink(temporary).catch(error => { if (error.code !== 'ENOENT') throw error; }); }
    });
    this.tail = job.catch(() => {});
    return job;
  }

  async close() {
    await this.tail;
    if (this.lock) {
      await this.lock.close();
      this.lock = null;
      await unlink(this.lockFile);
    }
  }
}

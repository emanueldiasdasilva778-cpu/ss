import { REST, Routes } from 'discord.js';
import { loadConfig } from './config/load.js';
import { data } from './commands/edit.js';
import { UserError, report } from './utils/errors.js';

// Registra somente /edit: não apaga outros comandos já existentes na aplicação.
try {
  const config = await loadConfig();
  const rest = new REST({ version: '10' }).setToken(config.token);
  const application = await rest.get(Routes.oauth2CurrentApplication());
  await rest.post(Routes.applicationGuildCommands(application.id, config.guildId), { body: data.toJSON() });
  console.info('Comando /edit registrado no servidor configurado. O cargo é conferido em cada interação.');
} catch (error) {
  if (error instanceof UserError) console.error(error.message);
  else report('Não foi possível registrar /edit', error);
  process.exitCode = 1;
}

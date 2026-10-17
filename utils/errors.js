export class UserError extends Error {}

// Não registre objetos de interação, configuração ou cabeçalhos: podem conter segredos.
export function report(context, error) {
  console.error(`[${new Date().toISOString()}] ${context}`, {
    name: error?.name ?? 'Error',
    code: error?.code ?? 'SEM_CODIGO',
    // Mensagens internas podem conter URLs autenticadas; detalhes sensíveis não vão ao log.
  });
}

import * as vscode from 'vscode';
import type { LanguageClient } from 'vscode-languageclient/node';
import { createLanguageClient, getServerModule } from './lspClient';

let client: LanguageClient | undefined;

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  const output = vscode.window.createOutputChannel('SEMDL');
  client = createLanguageClient(context, output);
  await client.start();

  const showLanguageServerStatus = vscode.commands.registerCommand('semdl.showLanguageServerStatus', async () => {
    const serverModule = getServerModule(context);
    output.appendLine(`SEMDL language server module: ${serverModule}`);
    output.appendLine('Language registration, TextMate grammars, and runtime LanguageClient wiring are active.');
    output.appendLine('Current LSP slice provides basic syntax diagnostics and top-level document symbols.');
    output.show(true);

    await vscode.window.showInformationMessage('SEMDL LSP is active with basic diagnostics and top-level document symbols.');
  });

  context.subscriptions.push(output, showLanguageServerStatus);
}

export async function deactivate(): Promise<void> {
  if (client) {
    await client.stop();
    client = undefined;
  }
}

import * as vscode from 'vscode';
import type { LanguageClient } from 'vscode-languageclient/node';
import { createLanguageClient, getServerModule } from './lspClient';
import { describeSemdlToolRegistry, registerSemdlLanguageModelTools } from './shared/languageModelTools';

let client: LanguageClient | undefined;

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  const output = vscode.window.createOutputChannel('SEMDL');
  const serverModule = getServerModule(context);
  output.appendLine(`SEMDL extension activated. Server module: ${serverModule}`);
  output.appendLine('Current LSP slice provides richer syntax diagnostics, grammar-derived keyword completion, grammar-derived keyword hover, and top-level document symbols.');
  client = createLanguageClient(context, output);
  await client.start();
  const toolRegistrations = registerSemdlLanguageModelTools(context, output);

  const showLanguageServerStatus = vscode.commands.registerCommand('semdl.showLanguageServerStatus', async () => {
    output.appendLine(`SEMDL language server module: ${serverModule}`);
    output.appendLine('Language registration, TextMate grammars, and runtime LanguageClient wiring are active.');
    output.appendLine('Current LSP slice provides richer syntax diagnostics, grammar-derived keyword completion, grammar-derived keyword hover, and top-level document symbols.');
    output.show(true);

    await vscode.window.showInformationMessage('SEMDL LSP is active with richer diagnostics, keyword completion, keyword hover, and top-level document symbols.');
  });

  const showToolRegistryStatus = vscode.commands.registerCommand('semdl.showToolRegistryStatus', async () => {
    output.appendLine(describeSemdlToolRegistry());
    output.show(true);

    await vscode.window.showInformationMessage('SEMDL tool registry status was written to the SEMDL output channel.');
  });

  context.subscriptions.push(output, showLanguageServerStatus, showToolRegistryStatus, ...toolRegistrations);
}

export async function deactivate(): Promise<void> {
  if (client) {
    await client.stop();
    client = undefined;
  }
}

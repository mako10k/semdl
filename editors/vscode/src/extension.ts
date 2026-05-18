import * as vscode from 'vscode';
import { getServerModule } from './lspClient';

export function activate(context: vscode.ExtensionContext): void {
  const output = vscode.window.createOutputChannel('SEMDL');

  const showScaffoldStatus = vscode.commands.registerCommand('semdl.showLanguageServerScaffoldStatus', async () => {
    const serverModule = getServerModule(context);
    output.appendLine(`SEMDL language server scaffold is available at: ${serverModule}`);
    output.appendLine('Language registration, TextMate grammars, and runtime client wiring are intentionally deferred to later slices.');
    output.show(true);

    await vscode.window.showInformationMessage('SEMDL language server scaffold is present. Runtime activation will be added in a later slice.');
  });

  context.subscriptions.push(output, showScaffoldStatus);
}

export function deactivate(): void {
}

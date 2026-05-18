import * as path from 'node:path';
import * as vscode from 'vscode';
import { LanguageClient, type LanguageClientOptions, type ServerOptions, TransportKind } from 'vscode-languageclient/node';

export function getServerModule(context: vscode.ExtensionContext): string {
  return context.asAbsolutePath(path.join('bundle', 'server.js'));
}

export function createLanguageClient(context: vscode.ExtensionContext, outputChannel: vscode.OutputChannel): LanguageClient {
  const serverModule = getServerModule(context);

  const serverOptions: ServerOptions = {
    run: {
      module: serverModule,
      transport: TransportKind.stdio
    },
    debug: {
      module: serverModule,
      transport: TransportKind.stdio
    }
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'semdl-ssd' },
      { scheme: 'untitled', language: 'semdl-ssd' },
      { scheme: 'file', language: 'semdl-ssm' },
      { scheme: 'untitled', language: 'semdl-ssm' },
      { scheme: 'file', language: 'semdl-ssq' },
      { scheme: 'untitled', language: 'semdl-ssq' },
    ],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{ssd,ssm,ssq}')
    },
    outputChannel
  };

  return new LanguageClient('semdl-language-server', 'SEMDL Language Server', serverOptions, clientOptions);
}

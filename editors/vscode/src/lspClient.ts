import * as path from 'node:path';
import { ExtensionContext } from 'vscode';

export function getServerModule(context: ExtensionContext): string {
  return context.asAbsolutePath(path.join('out', 'server', 'server.js'));
}

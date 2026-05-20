import * as vscode from 'vscode';
import { getSemdlToolContracts, listSemdlToolNames, type SemdlToolContract } from './toolContracts';
import { invokeSemdlTool } from './semdlToolExecutors';
import { validateMcpToolInput } from './mcpSchema';

export function registerSemdlLanguageModelTools(
  context: vscode.ExtensionContext,
  output: vscode.OutputChannel
): vscode.Disposable[] {
  const registrations = getSemdlToolContracts().map((contract) =>
    vscode.lm.registerTool(contract.name, new SemdlLanguageModelTool(contract, output))
  );
  context.subscriptions.push(...registrations);
  output.appendLine(`Registered SEMDL language model tools: ${listSemdlToolNames().join(', ')}`);
  return registrations;
}

export function describeSemdlToolRegistry(): string {
  const configuredBinaryPath = vscode.workspace.getConfiguration('semdl').get<string>('binaryPath')?.trim() ?? '';
  return [
    'SEMDL tool registry is active.',
    `Configured binary path: ${configuredBinaryPath.length > 0 ? configuredBinaryPath : '(auto resolution)'}`,
    `Registered tools: ${listSemdlToolNames().join(', ')}`
  ].join('\n');
}

class SemdlLanguageModelTool implements vscode.LanguageModelTool<Record<string, unknown>> {
  constructor(
    private readonly contract: SemdlToolContract,
    private readonly output: vscode.OutputChannel
  ) {}

  prepareInvocation(
    options: vscode.LanguageModelToolInvocationPrepareOptions<Record<string, unknown>>,
    _token: vscode.CancellationToken
  ): vscode.PreparedToolInvocation {
    return {
      invocationMessage: `${this.contract.displayName} is running`,
      confirmationMessages: {
        title: this.contract.displayName,
        message: new vscode.MarkdownString(buildConfirmationMessage(this.contract, options.input))
      }
    };
  }

  async invoke(
    options: vscode.LanguageModelToolInvocationOptions<Record<string, unknown>>,
    token: vscode.CancellationToken
  ): Promise<vscode.LanguageModelToolResult> {
    const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    const configuredBinaryPath = vscode.workspace.getConfiguration('semdl').get<string>('binaryPath')?.trim() || undefined;
    const signal = cancellationTokenToAbortSignal(token);
    const validatedInput = validateMcpToolInput(this.contract.inputSchema, options.input);
    const execution = await invokeSemdlTool(this.contract.name, validatedInput, {
      binaryPath: configuredBinaryPath,
      workspaceRoot,
      signal
    });

    this.output.appendLine(execution.rendered);

    return new vscode.LanguageModelToolResult([
      new vscode.LanguageModelTextPart(execution.rendered),
      vscode.LanguageModelDataPart.json({
        toolName: execution.toolName,
        ok: execution.ok,
        exitCode: execution.exitCode,
        commandLine: execution.commandLine,
        stdout: execution.stdout,
        stderr: execution.stderr
      })
    ]);
  }
}

function buildConfirmationMessage(contract: SemdlToolContract, input: Record<string, unknown>): string {
  const renderedInput = JSON.stringify(input, null, 2);
  return [
    `${contract.displayName} を実行します。`,
    '',
    contract.userDescription,
    '',
    'Input:',
    '```json',
    renderedInput,
    '```'
  ].join('\n');
}

function cancellationTokenToAbortSignal(token: vscode.CancellationToken): AbortSignal {
  const controller = new AbortController();
  token.onCancellationRequested(() => controller.abort());
  return controller.signal;
}
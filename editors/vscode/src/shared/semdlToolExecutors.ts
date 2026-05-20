import * as path from 'node:path';
import { getSemdlToolContract } from './toolContracts';
import { runSemdlBinary, type SemdlBinaryProxyOptions } from './semdlBinaryProxy';

export interface SemdlToolExecutionResult {
  toolName: string;
  ok: boolean;
  exitCode: number;
  commandLine: string;
  stdout: string;
  stderr: string;
  rendered: string;
}

export interface SemdlToolInvocation {
  args: string[];
  workingDirectory?: string;
}

export async function invokeSemdlTool(
  toolName: string,
  input: Record<string, unknown>,
  options: SemdlBinaryProxyOptions = {}
): Promise<SemdlToolExecutionResult> {
  const invocation = buildSemdlToolInvocation(toolName, input);
  const result = await runSemdlBinary(invocation.args, {
    ...options,
    workingDirectory: options.workingDirectory ?? invocation.workingDirectory
  });

  return {
    toolName,
    ok: result.exitCode === 0,
    exitCode: result.exitCode,
    commandLine: result.commandLine,
    stdout: result.stdout,
    stderr: result.stderr,
    rendered: renderSemdlToolResult(toolName, result.exitCode, result.commandLine, result.stdout, result.stderr)
  };
}

export function buildSemdlToolInvocation(toolName: string, input: Record<string, unknown>): SemdlToolInvocation {
  getSemdlToolContract(toolName);

  switch (toolName) {
    case 'check_semdl_document': {
      const documentPath = requireAbsolutePath(input.document_path, 'document_path');
      return {
        args: ['check', documentPath],
        workingDirectory: path.dirname(documentPath)
      };
    }
    case 'explain_semdl_entity': {
      const entityId = requireNonEmptyString(input.entity_id, 'entity_id');
      const documentPath = requireAbsolutePath(input.document_path, 'document_path');
      return {
        args: ['explain', entityId, documentPath],
        workingDirectory: path.dirname(documentPath)
      };
    }
    case 'search_semdl_query': {
      const queryPath = requireAbsolutePath(input.query_path, 'query_path');
      const documentPath = requireAbsolutePath(input.document_path, 'document_path');
      return {
        args: ['search', queryPath, documentPath],
        workingDirectory: path.dirname(documentPath)
      };
    }
    case 'read_semdl_help': {
      const args = ['help'];
      const topic = optionalNonEmptyString(input.topic);
      const target = optionalNonEmptyString(input.target);
      const format = optionalNonEmptyString(input.format);

      if (target && !topic) {
        throw new Error('target requires topic for read_semdl_help.');
      }

      if (topic) {
        args.push(topic);
      }
      if (target) {
        args.push(target);
      }
      if (format && format !== 'text') {
        if (format !== 'semdl') {
          throw new Error('format must be either text or semdl.');
        }
        args.push('--format', format);
      }

      return { args };
    }
    default:
      throw new Error(`Unknown SEMDL tool executor: ${toolName}`);
  }
}

function renderSemdlToolResult(
  toolName: string,
  exitCode: number,
  commandLine: string,
  stdout: string,
  stderr: string
): string {
  const lines = [
    `tool: ${toolName}`,
    `ok: ${exitCode === 0}`,
    `exitCode: ${exitCode}`,
    `command: ${commandLine}`
  ];

  if (stdout.trim().length > 0) {
    lines.push('', 'stdout:', stdout.trimEnd());
  }
  if (stderr.trim().length > 0) {
    lines.push('', 'stderr:', stderr.trimEnd());
  }

  return lines.join('\n');
}

function requireAbsolutePath(value: unknown, fieldName: string): string {
  const stringValue = requireNonEmptyString(value, fieldName);
  if (!path.isAbsolute(stringValue)) {
    throw new Error(`${fieldName} must be an absolute path.`);
  }
  return stringValue;
}

function requireNonEmptyString(value: unknown, fieldName: string): string {
  if (typeof value !== 'string' || value.trim().length === 0) {
    throw new Error(`${fieldName} must be a non-empty string.`);
  }
  return value.trim();
}

function optionalNonEmptyString(value: unknown): string | undefined {
  if (typeof value !== 'string') {
    return undefined;
  }
  const trimmed = value.trim();
  return trimmed.length === 0 ? undefined : trimmed;
}
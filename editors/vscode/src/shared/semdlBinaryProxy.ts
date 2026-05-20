import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';

export interface SemdlBinaryProxyOptions {
  binaryPath?: string;
  workspaceRoot?: string;
  workingDirectory?: string;
  env?: NodeJS.ProcessEnv;
  signal?: AbortSignal;
}

export interface SemdlBinaryProxyResult {
  binaryPath: string;
  commandLine: string;
  exitCode: number;
  stdout: string;
  stderr: string;
}

export function resolveSemdlBinaryPath(options: SemdlBinaryProxyOptions = {}): string {
  return listSemdlBinaryCandidates(options)[0] ?? 'ssd';
}

export function listSemdlBinaryCandidates(options: SemdlBinaryProxyOptions = {}): string[] {
  const projectRoot = inferSemdlProjectRoot(options);
  const rawCandidates = [
    options.binaryPath,
    process.env.SEMDL_BINARY,
    'ssd',
    projectRoot ? path.join(projectRoot, 'build', 'ssd') : undefined
  ];

  const candidates: string[] = [];
  for (const candidate of rawCandidates) {
    if (!candidate || candidate.trim().length === 0) {
      continue;
    }
    if (candidate === 'ssd') {
      if (!candidates.includes(candidate)) {
        candidates.push(candidate);
      }
      continue;
    }
    if (path.isAbsolute(candidate) && fs.existsSync(candidate) && !candidates.includes(candidate)) {
      candidates.push(candidate);
    }
  }

  return candidates.length > 0 ? candidates : ['ssd'];
}

export async function runSemdlBinary(args: string[], options: SemdlBinaryProxyOptions = {}): Promise<SemdlBinaryProxyResult> {
  const candidates = listSemdlBinaryCandidates(options);
  const workingDirectory = options.workingDirectory ?? inferSemdlProjectRoot(options) ?? process.cwd();
  let lastError: Error | undefined;

  for (const binaryPath of candidates) {
    try {
      return await runSemdlBinaryOnce(binaryPath, args, workingDirectory, options);
    } catch (error) {
      if (!(error instanceof Error) || !error.message.includes('ENOENT') || binaryPath !== 'ssd') {
        throw error;
      }
      lastError = error;
    }
  }

  throw lastError ?? new Error('SEMDL binary not found.');
}

function quoteArg(argument: string): string {
  return /[^A-Za-z0-9_./:-]/.test(argument) ? JSON.stringify(argument) : argument;
}

function inferSemdlProjectRoot(options: SemdlBinaryProxyOptions): string | undefined {
  const candidate = options.workspaceRoot
    ?? options.env?.SEMDL_PROJECT_ROOT
    ?? process.env.SEMDL_PROJECT_ROOT;
  if (!candidate || candidate.trim().length === 0) {
    return undefined;
  }
  return path.resolve(candidate);
}

async function runSemdlBinaryOnce(
  binaryPath: string,
  args: string[],
  workingDirectory: string,
  options: SemdlBinaryProxyOptions
): Promise<SemdlBinaryProxyResult> {
  const commandLine = [binaryPath, ...args].map(quoteArg).join(' ');

  return await new Promise<SemdlBinaryProxyResult>((resolve, reject) => {
    const child = spawn(binaryPath, args, {
      cwd: workingDirectory,
      env: { ...process.env, ...options.env },
      stdio: ['ignore', 'pipe', 'pipe']
    });

    let stdout = '';
    let stderr = '';
    let settled = false;

    const abortHandler = () => {
      child.kill('SIGTERM');
      if (!settled) {
        settled = true;
        reject(new Error(`SEMDL binary execution was cancelled: ${commandLine}`));
      }
    };

    if (options.signal) {
      if (options.signal.aborted) {
        abortHandler();
        return;
      }
      options.signal.addEventListener('abort', abortHandler, { once: true });
    }

    child.stdout.on('data', (chunk: Buffer | string) => {
      stdout += chunk.toString();
    });

    child.stderr.on('data', (chunk: Buffer | string) => {
      stderr += chunk.toString();
    });

    child.on('error', (error: NodeJS.ErrnoException) => {
      if (settled) {
        return;
      }
      settled = true;
      const detail = error.code === 'ENOENT'
        ? `SEMDL binary not found. Set semdl.binaryPath, SEMDL_BINARY, or place ssd on PATH. Attempted command: ${commandLine}`
        : `Failed to execute SEMDL binary: ${commandLine}`;
      reject(new Error(`${detail}\n${error.message}`));
    });

    child.on('close', (code) => {
      if (settled) {
        return;
      }
      settled = true;
      resolve({
        binaryPath,
        commandLine,
        exitCode: code ?? -1,
        stdout,
        stderr
      });
    });
  });
}
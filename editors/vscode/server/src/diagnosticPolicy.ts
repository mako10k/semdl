import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import type { Diagnostic } from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';

export const expectedDiagnosticsConfigFileName = '.semdl-diagnostics.json';

interface ExpectedDiagnosticRule {
  file: string;
  allow: Array<{
    line: number;
    message: string;
  }>;
}

export function filterExpectedDiagnostics(document: TextDocument, diagnostics: Diagnostic[]): Diagnostic[] {
  if (diagnostics.length === 0 || !document.uri.startsWith('file://')) {
    return diagnostics;
  }

  const filePath = toFilePath(document.uri);
  if (!filePath) {
    return diagnostics;
  }

  const expectedRules = loadExpectedDiagnosticRules(path.dirname(filePath));
  if (!expectedRules) {
    return diagnostics;
  }

  const allowedDiagnostics = expectedRules.get(path.basename(filePath));
  if (!allowedDiagnostics || allowedDiagnostics.size === 0) {
    return diagnostics;
  }

  return diagnostics.filter((diagnostic) => !allowedDiagnostics.has(makeDiagnosticKey(diagnostic.range.start.line + 1, diagnostic.message)));
}

function loadExpectedDiagnosticRules(directoryPath: string): Map<string, Set<string>> | undefined {
  const configPath = path.join(directoryPath, expectedDiagnosticsConfigFileName);
  if (!fs.existsSync(configPath)) {
    return undefined;
  }

  let parsedConfig: unknown;
  try {
    parsedConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));
  } catch {
    return undefined;
  }

  if (!isRecord(parsedConfig) || !Array.isArray(parsedConfig.expected_diagnostics)) {
    return undefined;
  }

  const rules = new Map<string, Set<string>>();
  for (const rawRule of parsedConfig.expected_diagnostics) {
    const rule = parseExpectedDiagnosticRule(rawRule);
    if (!rule) {
      continue;
    }

    const allowedDiagnostics = rules.get(rule.file) ?? new Set<string>();
    for (const expectedDiagnostic of rule.allow) {
      allowedDiagnostics.add(makeDiagnosticKey(expectedDiagnostic.line, expectedDiagnostic.message));
    }
    rules.set(rule.file, allowedDiagnostics);
  }

  return rules;
}

function parseExpectedDiagnosticRule(value: unknown): ExpectedDiagnosticRule | undefined {
  if (!isRecord(value) || typeof value.file !== 'string' || !Array.isArray(value.allow)) {
    return undefined;
  }

  const file = value.file.trim();
  if (!isNarrowFileName(file)) {
    return undefined;
  }

  const allow = value.allow
    .map((entry) => parseExpectedDiagnosticEntry(entry))
    .filter((entry): entry is { line: number; message: string } => entry !== undefined);

  if (allow.length === 0) {
    return undefined;
  }

  return { file, allow };
}

function parseExpectedDiagnosticEntry(value: unknown): { line: number; message: string } | undefined {
  if (!isRecord(value) || typeof value.line !== 'number' || typeof value.message !== 'string') {
    return undefined;
  }

  if (!Number.isInteger(value.line) || value.line <= 0) {
    return undefined;
  }

  const message = value.message.trim();
  if (message.length === 0) {
    return undefined;
  }

  return {
    line: value.line,
    message
  };
}

function isNarrowFileName(value: string): boolean {
  if (value.length === 0 || path.basename(value) !== value) {
    return false;
  }

  return !/[\\/*?[\]{}]/.test(value);
}

function makeDiagnosticKey(line: number, message: string): string {
  return `${line}\u0000${message}`;
}

function toFilePath(uri: string): string | undefined {
  try {
    return fileURLToPath(uri);
  } catch {
    return undefined;
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null;
}
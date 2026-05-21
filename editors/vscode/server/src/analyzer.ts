import * as path from 'node:path';
import {
  CompletionItem,
  CompletionItemKind,
  DiagnosticSeverity,
  DiagnosticRelatedInformation,
  DocumentSymbol,
  Location,
  Position,
  Range,
  SymbolKind,
  type Diagnostic
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';

export interface AnalysisResult {
  diagnostics: Diagnostic[];
  symbols: DocumentSymbol[];
}

interface LineInfo {
  lineNumber: number;
  text: string;
  startOffset: number;
}

interface SemdlStackEntry {
  kind: string;
  line: LineInfo;
  symbol?: DocumentSymbol;
}

interface Token {
  kind: 'identifier' | 'string' | 'number' | 'boolean' | 'operator' | 'lparen' | 'rparen' | 'eof';
  value: string;
  start: number;
  end: number;
}

interface DiagnosticOptions {
  code?: string;
  relatedInformation?: DiagnosticRelatedInformation[];
}

interface SemdlCompletionContext {
  stack: string[];
  seenDocumentBlock: boolean;
  seenTopLevelBlock: boolean;
}

interface SsqCompletionContext {
  queryStarted: boolean;
  queryClosed: boolean;
  seenEntries: Set<string>;
  lastEntryOrder: number;
}

const semdlTopLevelKinds = new Map<string, SymbolKind>([
  ['document', SymbolKind.Module],
  ['resource', SymbolKind.Object],
  ['segment', SymbolKind.String],
  ['assertion', SymbolKind.Interface],
  ['hypothesis', SymbolKind.Enum],
  ['alternative', SymbolKind.EnumMember],
  ['document_meta', SymbolKind.Struct],
  ['meta', SymbolKind.Property]
]);

const ssdAllowedTopLevelKinds = new Set<string>([
  'document',
  'resource',
  'segment',
  'assertion',
  'hypothesis',
  'alternative',
  'document_meta',
  'meta'
]);

const ssmAllowedTopLevelKinds = new Set<string>(['document_meta', 'meta']);

const topLevelBlockPattern = /^(document|resource|segment|assertion|hypothesis|alternative|document_meta|meta) +([A-Za-z][A-Za-z0-9_-]*) +\{$/;
const nestedBlockPattern = /^(meta|embedding) +\{$/;
const scalarValuePattern = /^(?:"(?:[^"\\]|\\.)*"|-?\d+(?:\.\d+)?|true|false|[A-Za-z][A-Za-z0-9_-]*)$/;
const fieldLinePattern = /^([A-Za-z][A-Za-z0-9_-]*):( *)(.+)$/;
const queryHeaderPattern = /^query +\{$/;
const queryEntryPattern = /^(select|where|similar|return):( *)(.+)$/;
const entryOrder = new Map<string, number>([
  ['select', 0],
  ['where', 1],
  ['similar', 2],
  ['return', 3]
]);

const topLevelCompletionOrder = ['document', 'resource', 'segment', 'assertion', 'hypothesis', 'alternative', 'document_meta', 'meta'];
const queryEntryCompletionOrder = ['select', 'where', 'similar', 'return'];

export function analyzeDocument(document: TextDocument): AnalysisResult {
  if (document.languageId === 'semdl-ssq') {
    return analyzeSsqDocument(document);
  }
  if (document.languageId === 'semdl-ssm') {
    return analyzeSemdlDocument(document, ssmAllowedTopLevelKinds, false);
  }

  const extension = path.extname(document.uri).toLowerCase();
  if (extension === '.ssq') {
    return analyzeSsqDocument(document);
  }
  return analyzeSemdlDocument(
    document,
    extension === '.ssm' ? ssmAllowedTopLevelKinds : ssdAllowedTopLevelKinds,
    extension !== '.ssm'
  );
}

export function getKeywordCompletionItems(document: TextDocument, position: Position): CompletionItem[] {
  const beforeCursor = document.getText(Range.create(Position.create(position.line, 0), position));
  if (beforeCursor.includes(':')) {
    return [];
  }

  const prefix = getCompletionPrefix(beforeCursor);
  if (document.languageId === 'semdl-ssq' || path.extname(document.uri).toLowerCase() === '.ssq') {
    return getSsqKeywordCompletionItems(document, position, prefix);
  }

  const extension = path.extname(document.uri).toLowerCase();
  const allowedTopLevelKinds = extension === '.ssm' || document.languageId === 'semdl-ssm' ? ssmAllowedTopLevelKinds : ssdAllowedTopLevelKinds;
  const requireDocumentFirst = !(extension === '.ssm' || document.languageId === 'semdl-ssm');
  return getSemdlKeywordCompletionItems(document, position, allowedTopLevelKinds, requireDocumentFirst, prefix);
}

function analyzeSemdlDocument(document: TextDocument, allowedTopLevelKinds: Set<string>, requireDocumentFirst: boolean): AnalysisResult {
  const diagnostics: Diagnostic[] = [];
  const symbols: DocumentSymbol[] = [];
  const stack: SemdlStackEntry[] = [];
  let seenDocumentBlock = false;
  let seenTopLevelBlock = false;
  let firstDocumentLine: LineInfo | undefined;

  for (const line of buildLineInfos(document)) {
    const trimmed = line.text.trim();
    if (trimmed.length === 0 || trimmed.startsWith('#')) {
      continue;
    }

    if (trimmed === '}') {
      const entry = stack.pop();
      if (!entry) {
        diagnostics.push(makeDiagnostic(lineRange(line), 'Unexpected closing brace.', DiagnosticSeverity.Error));
        continue;
      }
      if (entry.symbol) {
        entry.symbol.range = Range.create(entry.symbol.range.start, Position.create(line.lineNumber, line.text.length));
      }
      continue;
    }

    if (trimmed.endsWith('{')) {
      if (stack.length === 0) {
        const topLevelMatch = trimmed.match(topLevelBlockPattern);
        if (!topLevelMatch) {
          diagnostics.push(makeDiagnostic(lineRange(line), 'Expected a top-level SEMDL block declaration.', DiagnosticSeverity.Error));
          continue;
        }

        const kind = topLevelMatch[1];
        const name = topLevelMatch[2];
        if (!allowedTopLevelKinds.has(kind)) {
          diagnostics.push(makeDiagnostic(lineRange(line), `Top-level block \`${kind}\` is not allowed in this file type.`, DiagnosticSeverity.Error));
          continue;
        }
        if (requireDocumentFirst && !seenTopLevelBlock && kind !== 'document') {
          diagnostics.push(makeDiagnostic(lineRange(line), 'A `.ssd` document must start with a `document` block.', DiagnosticSeverity.Error));
          continue;
        }
        if (kind === 'document') {
          if (seenDocumentBlock || seenTopLevelBlock) {
            diagnostics.push(
              makeDiagnostic(
                tokenRange(line, kind),
                'The `document` block is only allowed once and must appear first.',
                DiagnosticSeverity.Error,
                {
                  code: 'duplicate-document-block',
                  relatedInformation: firstDocumentLine
                    ? [
                        makeRelatedInformation(
                          document,
                          tokenRange(firstDocumentLine, 'document'),
                          'The first `document` block appears here.'
                        )
                      ]
                    : undefined
                }
              )
            );
            continue;
          }
          seenDocumentBlock = true;
          firstDocumentLine = line;
        }
        seenTopLevelBlock = true;

        const symbol = DocumentSymbol.create(
          `${kind} ${name}`,
          '',
          semdlTopLevelKinds.get(kind) ?? SymbolKind.Object,
          lineRange(line),
          tokenRange(line, name),
          []
        );
        symbols.push(symbol);
        stack.push({ kind, line, symbol });
        continue;
      }

      const nestedMatch = trimmed.match(nestedBlockPattern);
      if (!nestedMatch) {
        diagnostics.push(makeDiagnostic(lineRange(line), 'Unexpected nested block declaration.', DiagnosticSeverity.Error));
        continue;
      }

      const nestedKind = nestedMatch[1];
      const parentKind = stack[stack.length - 1].kind;
      const parentAllowsMeta = parentKind === 'assertion' || parentKind === 'hypothesis';
      const parentAllowsEmbedding = parentKind === 'meta';

      if (nestedKind === 'meta' && !parentAllowsMeta) {
        diagnostics.push(makeDiagnostic(lineRange(line), 'Inline `meta` blocks are only allowed inside `assertion` or `hypothesis` blocks.', DiagnosticSeverity.Error));
        continue;
      }
      if (nestedKind === 'embedding' && !parentAllowsEmbedding) {
        diagnostics.push(makeDiagnostic(lineRange(line), '`embedding` blocks are only allowed inside `meta` blocks.', DiagnosticSeverity.Error));
        continue;
      }

      stack.push({ kind: nestedKind, line });
      continue;
    }

    if (stack.length === 0) {
      diagnostics.push(makeDiagnostic(lineRange(line), 'Content must appear inside a top-level block.', DiagnosticSeverity.Error));
      continue;
    }

    const fieldDiagnostic = validateSemdlFieldLine(document, line);
    if (fieldDiagnostic) {
      diagnostics.push(fieldDiagnostic);
    }
  }

  if (requireDocumentFirst && !seenDocumentBlock) {
    diagnostics.push(makeDiagnostic(fullDocumentRange(document), 'A `.ssd` document requires a top-level `document` block.', DiagnosticSeverity.Error));
  }

  while (stack.length > 0) {
    const entry = stack.pop()!;
    diagnostics.push(makeDiagnostic(lineRange(entry.line), `Unclosed block \`${entry.kind}\`.`, DiagnosticSeverity.Error));
    if (entry.symbol) {
      entry.symbol.range = Range.create(entry.symbol.range.start, document.positionAt(document.getText().length));
    }
  }

  return { diagnostics, symbols };
}

function analyzeSsqDocument(document: TextDocument): AnalysisResult {
  const diagnostics: Diagnostic[] = [];
  const symbols: DocumentSymbol[] = [];
  const seenEntries = new Map<string, LineInfo>();
  let queryHeader: LineInfo | undefined;
  let queryClosed = false;
  let lastEntryOrder = -1;
  let lastOrderedEntry: { key: string; line: LineInfo } | undefined;

  for (const line of buildLineInfos(document)) {
    const trimmed = line.text.trim();
    if (trimmed.length === 0 || trimmed.startsWith('#')) {
      continue;
    }

    if (!queryHeader) {
      if (!queryHeaderPattern.test(trimmed)) {
        diagnostics.push(makeDiagnostic(lineRange(line), 'Expected `query {` as the first non-comment content.', DiagnosticSeverity.Error));
        continue;
      }
      queryHeader = line;
      symbols.push(DocumentSymbol.create('query', '', SymbolKind.Module, lineRange(line), tokenRange(line, 'query'), []));
      continue;
    }

    if (trimmed === '}') {
      if (queryClosed) {
        diagnostics.push(makeDiagnostic(lineRange(line), 'Unexpected closing brace after the end of the query block.', DiagnosticSeverity.Error));
        continue;
      }
      queryClosed = true;
      symbols[0].range = Range.create(symbols[0].range.start, Position.create(line.lineNumber, line.text.length));
      continue;
    }

    if (queryClosed) {
      diagnostics.push(makeDiagnostic(lineRange(line), 'No content is allowed after the closing query brace.', DiagnosticSeverity.Error));
      continue;
    }

    const entryMatch = trimmed.match(queryEntryPattern);
    if (!entryMatch) {
      diagnostics.push(makeDiagnostic(lineRange(line), 'Expected a query entry in the form `select: ...`, `where: ...`, `similar: ...`, or `return: ...`.', DiagnosticSeverity.Error));
      continue;
    }

    const key = entryMatch[1];
    const value = entryMatch[3].trim();
    const order = entryOrder.get(key) ?? -1;
    const keyRange = tokenRange(line, key);

    const firstEntry = seenEntries.get(key);
    if (firstEntry) {
      diagnostics.push(
        makeDiagnostic(keyRange, `Duplicate query entry \`${key}\`.`, DiagnosticSeverity.Error, {
          code: 'duplicate-query-entry',
          relatedInformation: [
            makeRelatedInformation(document, tokenRange(firstEntry, key), `The first \`${key}\` entry appears here.`)
          ]
        })
      );
    } else {
      seenEntries.set(key, line);
    }

    if (key !== 'select' && seenEntries.size === 1) {
      diagnostics.push(makeDiagnostic(lineRange(line), 'The first query entry must be `select:`.', DiagnosticSeverity.Error));
    }

    if (order < lastEntryOrder) {
      diagnostics.push(
        makeDiagnostic(keyRange, 'Query entries must appear in `select`, `where`, `similar`, `return` order.', DiagnosticSeverity.Error, {
          code: 'query-entry-order',
          relatedInformation: lastOrderedEntry
            ? [
                makeRelatedInformation(
                  document,
                  tokenRange(lastOrderedEntry.line, lastOrderedEntry.key),
                  `The previous query entry \`${lastOrderedEntry.key}\` established the current order boundary.`
                )
              ]
            : undefined
        })
      );
    } else {
      lastEntryOrder = Math.max(lastEntryOrder, order);
      lastOrderedEntry = { key, line };
    }

    validateSsqEntryValue(document, line, key, value, diagnostics);
  }

  if (!queryHeader) {
    diagnostics.push(makeDiagnostic(fullDocumentRange(document), 'A `.ssq` document must contain a `query { ... }` block.', DiagnosticSeverity.Error));
    return { diagnostics, symbols };
  }

  if (!queryClosed) {
    diagnostics.push(makeDiagnostic(lineRange(queryHeader), 'Unclosed `query` block.', DiagnosticSeverity.Error));
    symbols[0].range = Range.create(symbols[0].range.start, document.positionAt(document.getText().length));
  }

  if (!seenEntries.has('select')) {
    diagnostics.push(makeDiagnostic(lineRange(queryHeader), 'Query documents require a `select:` entry.', DiagnosticSeverity.Error));
  }

  return { diagnostics, symbols };
}

function getSemdlKeywordCompletionItems(
  document: TextDocument,
  position: Position,
  allowedTopLevelKinds: Set<string>,
  requireDocumentFirst: boolean,
  prefix: string
): CompletionItem[] {
  const context = buildSemdlCompletionContext(document, position, allowedTopLevelKinds);

  if (context.stack.length === 0) {
    const keywords = topLevelCompletionOrder.filter((kind) => {
      if (!allowedTopLevelKinds.has(kind)) {
        return false;
      }
      if (requireDocumentFirst && !context.seenTopLevelBlock) {
        return kind === 'document';
      }
      if (kind === 'document' && context.seenDocumentBlock) {
        return false;
      }
      return true;
    });
    return buildKeywordCompletionItems(keywords, prefix, 'block');
  }

  const parentKind = context.stack[context.stack.length - 1];
  if (parentKind === 'assertion' || parentKind === 'hypothesis') {
    return buildKeywordCompletionItems(['meta'], prefix, 'nested-block');
  }
  if (parentKind === 'meta') {
    return buildKeywordCompletionItems(['embedding'], prefix, 'nested-block');
  }

  return [];
}

function getSsqKeywordCompletionItems(document: TextDocument, position: Position, prefix: string): CompletionItem[] {
  const context = buildSsqCompletionContext(document, position);
  if (!context.queryStarted) {
    return buildKeywordCompletionItems(['query'], prefix, 'query-header');
  }
  if (context.queryClosed) {
    return [];
  }

  const keywords = queryEntryCompletionOrder.filter((key) => {
    if (context.seenEntries.has(key)) {
      return false;
    }
    return (entryOrder.get(key) ?? -1) >= context.lastEntryOrder;
  });
  return buildKeywordCompletionItems(keywords, prefix, 'query-entry');
}

function validateSsqEntryValue(document: TextDocument, line: LineInfo, key: string, value: string, diagnostics: Diagnostic[]): void {
  if (value.length === 0) {
    diagnostics.push(makeDiagnostic(lineRange(line), `Query entry \`${key}\` requires a value.`, DiagnosticSeverity.Error));
    return;
  }

  if (key === 'select') {
    if (!isIdentifier(value) && !isQuotedString(value)) {
      diagnostics.push(makeDiagnostic(lineRange(line), '`select:` must contain an identifier or a quoted string.', DiagnosticSeverity.Error));
    }
    return;
  }

  if (key === 'similar') {
    if (!isIdentifier(value)) {
      diagnostics.push(makeDiagnostic(lineRange(line), '`similar:` must reference an identifier target.', DiagnosticSeverity.Error));
    }
    return;
  }

  if (key === 'return') {
    if (value !== 'matches' && value !== 'subgraph') {
      diagnostics.push(makeDiagnostic(lineRange(line), '`return:` must be `matches` or `subgraph`.', DiagnosticSeverity.Error));
    }
    return;
  }

  if (key === 'where') {
    const parseError = parseFilterExpression(value);
    if (parseError) {
      diagnostics.push(makeDiagnostic(lineRange(line), parseError, DiagnosticSeverity.Error));
    }
  }
}

function buildSemdlCompletionContext(document: TextDocument, position: Position, allowedTopLevelKinds: Set<string>): SemdlCompletionContext {
  const context: SemdlCompletionContext = {
    stack: [],
    seenDocumentBlock: false,
    seenTopLevelBlock: false
  };

  for (const line of buildLineInfos(document)) {
    if (line.lineNumber >= position.line) {
      break;
    }

    const trimmed = line.text.trim();
    if (trimmed.length === 0 || trimmed.startsWith('#')) {
      continue;
    }
    if (trimmed === '}') {
      if (context.stack.length > 0) {
        context.stack.pop();
      }
      continue;
    }
    if (!trimmed.endsWith('{')) {
      continue;
    }

    if (context.stack.length === 0) {
      const topLevelMatch = trimmed.match(topLevelBlockPattern);
      if (!topLevelMatch) {
        continue;
      }
      const kind = topLevelMatch[1];
      if (!allowedTopLevelKinds.has(kind)) {
        continue;
      }
      context.seenTopLevelBlock = true;
      if (kind === 'document') {
        context.seenDocumentBlock = true;
      }
      context.stack.push(kind);
      continue;
    }

    const nestedMatch = trimmed.match(nestedBlockPattern);
    if (nestedMatch) {
      context.stack.push(nestedMatch[1]);
    }
  }

  return context;
}

function buildSsqCompletionContext(document: TextDocument, position: Position): SsqCompletionContext {
  const context: SsqCompletionContext = {
    queryStarted: false,
    queryClosed: false,
    seenEntries: new Set<string>(),
    lastEntryOrder: -1
  };

  for (const line of buildLineInfos(document)) {
    if (line.lineNumber >= position.line) {
      break;
    }

    const trimmed = line.text.trim();
    if (trimmed.length === 0 || trimmed.startsWith('#')) {
      continue;
    }
    if (!context.queryStarted) {
      if (queryHeaderPattern.test(trimmed)) {
        context.queryStarted = true;
      }
      continue;
    }
    if (trimmed === '}') {
      context.queryClosed = true;
      continue;
    }
    if (context.queryClosed) {
      continue;
    }

    const entryMatch = trimmed.match(queryEntryPattern);
    if (!entryMatch) {
      continue;
    }

    const key = entryMatch[1];
    context.seenEntries.add(key);
    context.lastEntryOrder = Math.max(context.lastEntryOrder, entryOrder.get(key) ?? -1);
  }

  return context;
}

function buildLineInfos(document: TextDocument): LineInfo[] {
  const text = document.getText();
  const rawLines = text.split(/\r?\n/);
  const lines: LineInfo[] = [];
  let startOffset = 0;

  for (let index = 0; index < rawLines.length; index += 1) {
    const line = rawLines[index];
    lines.push({ lineNumber: index, text: line, startOffset });
    startOffset += line.length + 1;
  }

  return lines;
}

function lineRange(line: LineInfo): Range {
  return Range.create(Position.create(line.lineNumber, 0), Position.create(line.lineNumber, line.text.length));
}

function getCompletionPrefix(beforeCursor: string): string {
  const match = beforeCursor.match(/([A-Za-z][A-Za-z0-9_-]*)$/);
  return match?.[1] ?? '';
}

function tokenRange(line: LineInfo, token: string): Range {
  const column = Math.max(0, line.text.indexOf(token));
  return Range.create(Position.create(line.lineNumber, column), Position.create(line.lineNumber, column + token.length));
}

function fullDocumentRange(document: TextDocument): Range {
  const end = document.positionAt(document.getText().length);
  return Range.create(Position.create(0, 0), end);
}

function makeDiagnostic(range: Range, message: string, severity: DiagnosticSeverity, options: DiagnosticOptions = {}): Diagnostic {
  return {
    range,
    message,
    severity,
    code: options.code,
    relatedInformation: options.relatedInformation,
    source: 'semdl-lsp'
  };
}

function makeRelatedInformation(document: TextDocument, range: Range, message: string): DiagnosticRelatedInformation {
  return DiagnosticRelatedInformation.create(Location.create(document.uri, range), message);
}

function buildKeywordCompletionItems(keywords: string[], prefix: string, category: 'block' | 'nested-block' | 'query-header' | 'query-entry'): CompletionItem[] {
  return keywords
    .filter((keyword) => prefix.length === 0 || keyword.startsWith(prefix))
    .map((keyword, index) => ({
      label: keyword,
      kind: CompletionItemKind.Keyword,
      detail: describeCompletionCategory(category),
      insertText: completionInsertText(keyword, category),
      sortText: `${index}`.padStart(2, '0')
    }));
}

function completionInsertText(keyword: string, category: 'block' | 'nested-block' | 'query-header' | 'query-entry'): string {
  if (category === 'query-header') {
    return 'query {';
  }
  if (category === 'query-entry') {
    return `${keyword}: `;
  }
  if (category === 'nested-block') {
    return `${keyword} {`;
  }
  return `${keyword} `;
}

function describeCompletionCategory(category: 'block' | 'nested-block' | 'query-header' | 'query-entry'): string {
  if (category === 'block') {
    return 'SEMDL top-level block keyword';
  }
  if (category === 'nested-block') {
    return 'SEMDL nested block keyword';
  }
  if (category === 'query-header') {
    return 'SEMDL query header keyword';
  }
  return 'SEMDL query entry keyword';
}

function validateSemdlFieldLine(document: TextDocument, line: LineInfo): Diagnostic | undefined {
  const trimmed = line.text.trim();
  const separatorIndex = trimmed.indexOf(':');
  if (separatorIndex === -1) {
    return makeDiagnostic(lineRange(line), 'Expected `:` separating a field name and value.', DiagnosticSeverity.Error, {
      code: 'field-missing-separator'
    });
  }

  const fieldName = trimmed.slice(0, separatorIndex).trim();
  const rawValue = trimmed.slice(separatorIndex + 1);
  const value = rawValue.trim();

  if (!/^[A-Za-z][A-Za-z0-9_-]*$/.test(fieldName)) {
    return makeDiagnostic(
      Range.create(Position.create(line.lineNumber, 0), Position.create(line.lineNumber, separatorIndex)),
      'Field names must be identifiers in the form `name`.',
      DiagnosticSeverity.Error,
      { code: 'field-invalid-name' }
    );
  }

  if (value.length === 0) {
    return makeDiagnostic(lineRange(line), `Field \`${fieldName}\` requires a scalar value.`, DiagnosticSeverity.Error, {
      code: 'field-missing-value'
    });
  }

  if (!scalarValuePattern.test(value)) {
    const valueStart = Math.max(separatorIndex + 1, trimmed.length - value.length);
    return makeDiagnostic(
      Range.create(Position.create(line.lineNumber, valueStart), Position.create(line.lineNumber, valueStart + value.length)),
      `Field \`${fieldName}\` must contain a quoted string, number, boolean, or identifier.`,
      DiagnosticSeverity.Error,
      { code: 'field-invalid-scalar' }
    );
  }

  return undefined;
}

function isIdentifier(value: string): boolean {
  return /^[A-Za-z][A-Za-z0-9_-]*$/.test(value);
}

function isQuotedString(value: string): boolean {
  return /^"(?:[^"\\]|\\.)*"$/.test(value);
}

function parseFilterExpression(input: string): string | undefined {
  try {
    const parser = new FilterExpressionParser(tokenizeFilterExpression(input));
    parser.parseExpression();
    parser.expect('eof');
    return undefined;
  } catch (error) {
    return error instanceof Error ? error.message : 'Invalid filter expression.';
  }
}

function tokenizeFilterExpression(input: string): Token[] {
  const tokens: Token[] = [];
  let index = 0;

  while (index < input.length) {
    const char = input[index];
    if (/\s/.test(char)) {
      index += 1;
      continue;
    }
    if (char === '(') {
      tokens.push({ kind: 'lparen', value: char, start: index, end: index + 1 });
      index += 1;
      continue;
    }
    if (char === ')') {
      tokens.push({ kind: 'rparen', value: char, start: index, end: index + 1 });
      index += 1;
      continue;
    }
    if (char === '>' || char === '<' || char === '=') {
      const next = input[index + 1];
      const value = (char === '>' || char === '<') && next === '=' ? `${char}=` : char;
      tokens.push({ kind: 'operator', value, start: index, end: index + value.length });
      index += value.length;
      continue;
    }
    if (char === '"') {
      let cursor = index + 1;
      let escaped = false;
      while (cursor < input.length) {
        const current = input[cursor];
        if (escaped) {
          escaped = false;
        } else if (current === '\\') {
          escaped = true;
        } else if (current === '"') {
          break;
        }
        cursor += 1;
      }
      if (cursor >= input.length || input[cursor] !== '"') {
        throw new Error('Malformed quoted string in `where:` expression.');
      }
      const value = input.slice(index, cursor + 1);
      tokens.push({ kind: 'string', value, start: index, end: cursor + 1 });
      index = cursor + 1;
      continue;
    }
    const numberMatch = input.slice(index).match(/^-?\d+(?:\.\d+)?/);
    if (numberMatch) {
      const value = numberMatch[0];
      tokens.push({ kind: 'number', value, start: index, end: index + value.length });
      index += value.length;
      continue;
    }
    const identifierMatch = input.slice(index).match(/^[A-Za-z][A-Za-z0-9_-]*/);
    if (identifierMatch) {
      const value = identifierMatch[0];
      const lower = value.toLowerCase();
      if (lower === 'true' || lower === 'false') {
        tokens.push({ kind: 'boolean', value: lower, start: index, end: index + value.length });
      } else {
        tokens.push({ kind: 'identifier', value, start: index, end: index + value.length });
      }
      index += value.length;
      continue;
    }

    throw new Error(`Unexpected token \`${char}\` in \`where:\` expression.`);
  }

  tokens.push({ kind: 'eof', value: '', start: input.length, end: input.length });
  return tokens;
}

class FilterExpressionParser {
  private readonly tokens: Token[];
  private index = 0;

  constructor(tokens: Token[]) {
    this.tokens = tokens;
  }

  parseExpression(): void {
    this.parseOrExpression();
  }

  expect(kind: Token['kind']): Token {
    const token = this.peek();
    if (token.kind !== kind) {
      throw new Error('Invalid filter expression.');
    }
    this.index += 1;
    return token;
  }

  private parseOrExpression(): void {
    this.parseAndExpression();
    while (this.peekIdentifierValue('or')) {
      this.index += 1;
      this.parseAndExpression();
    }
  }

  private parseAndExpression(): void {
    this.parseUnaryExpression();
    while (this.peekIdentifierValue('and')) {
      this.index += 1;
      this.parseUnaryExpression();
    }
  }

  private parseUnaryExpression(): void {
    if (this.peekIdentifierValue('not')) {
      this.index += 1;
      this.parseUnaryExpression();
      return;
    }
    this.parsePrimaryExpression();
  }

  private parsePrimaryExpression(): void {
    const token = this.peek();
    if (token.kind === 'lparen') {
      this.index += 1;
      this.parseExpression();
      if (this.peek().kind !== 'rparen') {
        throw new Error('Unmatched parenthesis in `where:` expression.');
      }
      this.index += 1;
      return;
    }
    this.parseFilterTerm();
  }

  private parseFilterTerm(): void {
    const field = this.peek();
    if (field.kind !== 'identifier') {
      throw new Error('Filter terms must begin with an identifier field reference.');
    }
    if (field.value === 'not') {
      throw new Error('`not` is reserved and cannot be used as a field name.');
    }
    this.index += 1;

    const next = this.peek();
    if (next.kind === 'operator') {
      this.index += 1;
      if (next.value === '=') {
        const value = this.peek();
        if (!['string', 'number', 'boolean'].includes(value.kind)) {
          throw new Error('Equality filters require a quoted string, number, or boolean value.');
        }
        this.index += 1;
        return;
      }

      const value = this.peek();
      if (value.kind !== 'number') {
        throw new Error('Range filters require a numeric value.');
      }
      this.index += 1;
      return;
    }
  }

  private peek(): Token {
    return this.tokens[this.index];
  }

  private peekIdentifierValue(value: string): boolean {
    const token = this.peek();
    return token.kind === 'identifier' && token.value === value;
  }
}

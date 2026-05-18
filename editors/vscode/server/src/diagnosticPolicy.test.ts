import test from 'node:test';
import assert from 'node:assert/strict';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { pathToFileURL } from 'node:url';
import { DiagnosticSeverity, Range } from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { expectedDiagnosticsConfigFileName, filterExpectedDiagnostics } from './diagnosticPolicy';

function makeDiagnostic(line: number, message: string) {
  return {
    range: Range.create(line - 1, 0, line - 1, 10),
    message,
    severity: DiagnosticSeverity.Error,
    source: 'semdl-lsp'
  };
}

test('directory-local expected diagnostics suppress only exact file line and message matches', () => {
  const tempDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'semdl-diagnostic-policy-'));
  fs.writeFileSync(
    path.join(tempDirectory, expectedDiagnosticsConfigFileName),
    JSON.stringify(
      {
        expected_diagnostics: [
          {
            file: 'invalid-range-quoted-number-filter.ssq',
            allow: [
              {
                line: 3,
                message: 'Range filters require a numeric value.'
              }
            ]
          }
        ]
      },
      null,
      2
    )
  );

  const filePath = path.join(tempDirectory, 'invalid-range-quoted-number-filter.ssq');
  fs.writeFileSync(filePath, 'query {\n  where: confidence > "0.8"\n}\n');

  const document = TextDocument.create(pathToFileURL(filePath).toString(), 'semdl-ssq', 1, fs.readFileSync(filePath, 'utf8'));
  const diagnostics = [
    makeDiagnostic(3, 'Range filters require a numeric value.'),
    makeDiagnostic(3, 'Query entries must appear in `select`, `where`, `similar`, `return` order.'),
    makeDiagnostic(2, 'Range filters require a numeric value.')
  ];

  const filteredDiagnostics = filterExpectedDiagnostics(document, diagnostics);
  assert.deepEqual(
    filteredDiagnostics.map((diagnostic) => `${diagnostic.range.start.line + 1}:${diagnostic.message}`),
    [
      '3:Query entries must appear in `select`, `where`, `similar`, `return` order.',
      '2:Range filters require a numeric value.'
    ]
  );
});

test('broad file patterns are ignored', () => {
  const tempDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'semdl-diagnostic-policy-'));
  fs.writeFileSync(
    path.join(tempDirectory, expectedDiagnosticsConfigFileName),
    JSON.stringify(
      {
        expected_diagnostics: [
          {
            file: '*.ssq',
            allow: [
              {
                line: 3,
                message: 'Range filters require a numeric value.'
              }
            ]
          }
        ]
      },
      null,
      2
    )
  );

  const filePath = path.join(tempDirectory, 'invalid-range-quoted-number-filter.ssq');
  fs.writeFileSync(filePath, 'query {\n  where: confidence > "0.8"\n}\n');

  const document = TextDocument.create(pathToFileURL(filePath).toString(), 'semdl-ssq', 1, fs.readFileSync(filePath, 'utf8'));
  const diagnostics = [makeDiagnostic(3, 'Range filters require a numeric value.')];

  const filteredDiagnostics = filterExpectedDiagnostics(document, diagnostics);
  assert.equal(filteredDiagnostics.length, 1);
});

test('parent-directory configs are not inherited', () => {
  const tempDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'semdl-diagnostic-policy-'));
  const nestedDirectory = path.join(tempDirectory, 'nested');
  fs.mkdirSync(nestedDirectory);
  fs.writeFileSync(
    path.join(tempDirectory, expectedDiagnosticsConfigFileName),
    JSON.stringify(
      {
        expected_diagnostics: [
          {
            file: 'invalid-range-quoted-number-filter.ssq',
            allow: [
              {
                line: 3,
                message: 'Range filters require a numeric value.'
              }
            ]
          }
        ]
      },
      null,
      2
    )
  );

  const filePath = path.join(nestedDirectory, 'invalid-range-quoted-number-filter.ssq');
  fs.writeFileSync(filePath, 'query {\n  where: confidence > "0.8"\n}\n');

  const document = TextDocument.create(pathToFileURL(filePath).toString(), 'semdl-ssq', 1, fs.readFileSync(filePath, 'utf8'));
  const diagnostics = [makeDiagnostic(3, 'Range filters require a numeric value.')];

  const filteredDiagnostics = filterExpectedDiagnostics(document, diagnostics);
  assert.equal(filteredDiagnostics.length, 1);
});

test('same-directory policy can suppress exact diagnostics for invalid .ssd fixtures', () => {
  const tempDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'semdl-diagnostic-policy-'));
  fs.writeFileSync(
    path.join(tempDirectory, expectedDiagnosticsConfigFileName),
    JSON.stringify(
      {
        expected_diagnostics: [
          {
            file: 'multiple-documents-invalid.ssd',
            allow: [
              {
                line: 4,
                message: 'The `document` block is only allowed once and must appear first.'
              },
              {
                line: 5,
                message: 'Unexpected closing brace.'
              }
            ]
          }
        ]
      },
      null,
      2
    )
  );

  const filePath = path.join(tempDirectory, 'multiple-documents-invalid.ssd');
  fs.writeFileSync(filePath, 'document D1 {\n}\n\ndocument D2 {\n}\n');

  const document = TextDocument.create(pathToFileURL(filePath).toString(), 'semdl-ssd', 1, fs.readFileSync(filePath, 'utf8'));
  const diagnostics = [
    makeDiagnostic(4, 'The `document` block is only allowed once and must appear first.'),
    makeDiagnostic(5, 'Unexpected closing brace.'),
    makeDiagnostic(1, 'A `.ssd` document requires a top-level `document` block.')
  ];

  const filteredDiagnostics = filterExpectedDiagnostics(document, diagnostics);
  assert.deepEqual(
    filteredDiagnostics.map((diagnostic) => `${diagnostic.range.start.line + 1}:${diagnostic.message}`),
    ['1:A `.ssd` document requires a top-level `document` block.']
  );
});
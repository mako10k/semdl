import test from 'node:test';
import assert from 'node:assert/strict';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { analyzeDocument } from './analyzer';

test('valid .ssd document has no diagnostics and exposes top-level symbols', () => {
  const document = TextDocument.create(
    'file:///valid.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '  title: "Sample"',
      '}',
      '',
      'resource R1 {',
      '  type: document',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.equal(result.diagnostics.length, 0);
  assert.deepEqual(result.symbols.map((symbol) => symbol.name), ['document D1', 'resource R1']);
});

test('invalid .ssd document without a leading document block reports a diagnostic', () => {
  const document = TextDocument.create(
    'file:///invalid.ssd',
    'semdl-ssd',
    1,
    [
      'resource R1 {',
      '  type: document',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.message.includes('must start with a `document` block')));
});

test('invalid nested embedding outside meta reports a diagnostic', () => {
  const document = TextDocument.create(
    'file:///invalid-nested.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '  title: "Sample"',
      '}',
      'assertion A1 {',
      '  embedding {',
      '    model: "x"',
      '  }',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.message.includes('`embedding` blocks are only allowed inside `meta` blocks')));
});

test('valid .ssm document has no diagnostics', () => {
  const document = TextDocument.create(
    'file:///valid.ssm',
    'semdl-ssm',
    1,
    [
      'document_meta D1 {',
      '  version: "0.1"',
      '}',
      'meta A1 {',
      '  confidence: 0.91',
      '  embedding {',
      '    dimensions: 3072',
      '  }',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.equal(result.diagnostics.length, 0);
});

test('invalid whitespace-relaxed .ssd syntax is rejected', () => {
  const document = TextDocument.create(
    'file:///invalid-whitespace.ssd',
    'semdl-ssd',
    1,
    [
      'document D1{',
      '  title : "Sample"',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.length >= 1);
});

test('invalid whitespace-relaxed .ssm syntax is rejected', () => {
  const document = TextDocument.create(
    'file:///invalid-whitespace.ssm',
    'semdl-ssm',
    1,
    [
      'document_meta D1{',
      '  version : "0.1"',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.length >= 1);
});

test('valid .ssq query has no diagnostics and exposes a query symbol', () => {
  const document = TextDocument.create(
    'file:///valid.ssq',
    'semdl-ssq',
    1,
    [
      'query {',
      '  select: assertion',
      '  where: source_presence and confidence > 0.8',
      '  return: matches',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.equal(result.diagnostics.length, 0);
  assert.deepEqual(result.symbols.map((symbol) => symbol.name), ['query']);
});

test('invalid .ssq query catches quoted similar targets', () => {
  const document = TextDocument.create(
    'file:///invalid-similar.ssq',
    'semdl-ssq',
    1,
    [
      'query {',
      '  select: assertion',
      '  similar: "A1"',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.message.includes('`similar:` must reference an identifier target')));
});

test('invalid .ssq query catches out-of-order entries', () => {
  const document = TextDocument.create(
    'file:///invalid-order.ssq',
    'semdl-ssq',
    1,
    [
      'query {',
      '  select: assertion',
      '  return: matches',
      '  where: source_presence',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.message.includes('must appear in `select`, `where`, `similar`, `return` order')));
});

test('invalid whitespace-relaxed .ssq syntax is rejected', () => {
  const document = TextDocument.create(
    'file:///invalid-whitespace.ssq',
    'semdl-ssq',
    1,
    [
      'query{',
      '  select : assertion',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.length >= 1);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { MarkupContent, Position } from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { analyzeDocument, getCompletionItems, getKeywordCompletionItems, getKeywordHover } from './analyzer';

function hoverMarkdownValue(hover: ReturnType<typeof getKeywordHover>): string {
  const contents = hover?.contents;
  if (!contents || Array.isArray(contents) || typeof contents === 'string') {
    return '';
  }
  return (contents as MarkupContent).value;
}

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

test('invalid field lines report more specific diagnostics', () => {
  const document = TextDocument.create(
    'file:///invalid-fields.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '  title',
      '  label:',
      '  kind: [not-a-scalar]',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.code === 'field-missing-separator' && diagnostic.message.includes('Expected `:` separating')));
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.code === 'field-missing-value' && diagnostic.message.includes('Field `label` requires a scalar value.')));
  assert.ok(result.diagnostics.some((diagnostic) => diagnostic.code === 'field-invalid-scalar' && diagnostic.message.includes('Field `kind` must contain a quoted string, number, boolean, or identifier.')));
});

test('duplicate document blocks point back to the first document block', () => {
  const document = TextDocument.create(
    'file:///duplicate-document.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      '',
      'document D2 {',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  const duplicateDiagnostic = result.diagnostics.find((diagnostic) => diagnostic.code === 'duplicate-document-block');

  assert.ok(duplicateDiagnostic);
  assert.equal(duplicateDiagnostic?.range.start.line, 3);
  assert.equal(duplicateDiagnostic?.relatedInformation?.[0]?.location.range.start.line, 0);
});

test('empty .ssd document offers only the leading document keyword', () => {
  const document = TextDocument.create('file:///empty.ssd', 'semdl-ssd', 1, '');

  const items = getKeywordCompletionItems(document, Position.create(0, 0));
  assert.deepEqual(items.map((item) => item.label), ['document']);
});

test('inside assertion blocks only meta is offered as a nested keyword', () => {
  const document = TextDocument.create(
    'file:///assertion-context.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '}', '', 'assertion A1 {', '  '].join('\n')
  );

  const items = getKeywordCompletionItems(document, Position.create(4, 2));
  assert.deepEqual(items.map((item) => item.label), ['meta']);
});

test('empty .ssq document offers the query keyword', () => {
  const document = TextDocument.create('file:///empty.ssq', 'semdl-ssq', 1, '');

  const items = getKeywordCompletionItems(document, Position.create(0, 0));
  assert.deepEqual(items.map((item) => item.label), ['query']);
});

test('query entry completion follows grammar order and excludes identifiers', () => {
  const document = TextDocument.create(
    'file:///query-completion.ssq',
    'semdl-ssq',
    1,
    ['query {', '  select: assertion', '  '].join('\n')
  );

  const items = getKeywordCompletionItems(document, Position.create(2, 2));
  assert.deepEqual(items.map((item) => item.label), ['where', 'similar', 'return']);
  assert.ok(items.every((item) => item.kind === 14));
});

test('keyword completion does not offer suggestions after a field separator', () => {
  const document = TextDocument.create(
    'file:///field-context.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '  title: do', '}'].join('\n')
  );

  const items = getKeywordCompletionItems(document, Position.create(1, 11));
  assert.equal(items.length, 0);
});

test('document-local identifier completion offers matching resource ids for segment.source', () => {
  const document = TextDocument.create(
    'file:///identifier-source.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'resource R1 {',
      '}',
      'resource R2 {',
      '}',
      'segment S1 {',
      '  source: R',
      '}'
    ].join('\n')
  );

  const items = getCompletionItems(document, Position.create(7, 11));
  assert.deepEqual(items.map((item) => item.label), ['R1', 'R2']);
  assert.ok(items.every((item) => item.detail === 'SEMDL document-local resource identifier'));
});

test('document-local identifier completion offers assertion ids for hypothesis.about', () => {
  const document = TextDocument.create(
    'file:///identifier-about.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'assertion A1 {',
      '}',
      'assertion A2 {',
      '}',
      'hypothesis H1 {',
      '  about: A',
      '}'
    ].join('\n')
  );

  const items = getCompletionItems(document, Position.create(7, 10));
  assert.deepEqual(items.map((item) => item.label), ['A1', 'A2']);
});

test('document-local identifier completion offers resource and segment ids for the remaining eligible fields', () => {
  const subjectDocument = TextDocument.create(
    'file:///identifier-subject.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'resource R1 {',
      '}',
      'resource R2 {',
      '}',
      'assertion A1 {',
      '  subject: R',
      '}'
    ].join('\n')
  );
  const sourceSegmentDocument = TextDocument.create(
    'file:///identifier-source-segment.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'segment S1 {',
      '}',
      'segment S2 {',
      '}',
      'assertion A1 {',
      '  source_segment: S',
      '}'
    ].join('\n')
  );

  assert.deepEqual(getCompletionItems(subjectDocument, Position.create(7, 12)).map((item) => item.label), ['R1', 'R2']);
  assert.deepEqual(getCompletionItems(sourceSegmentDocument, Position.create(7, 19)).map((item) => item.label), ['S1', 'S2']);
});

test('document-local identifier completion stays out of non-reference and quoted-string fields', () => {
  const predicateDocument = TextDocument.create(
    'file:///identifier-negative-predicate.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'resource R1 {',
      '}',
      'assertion A1 {',
      '  predicate: re',
      '}'
    ].join('\n')
  );
  const quotedDocument = TextDocument.create(
    'file:///identifier-negative-quoted.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'resource R1 {',
      '}',
      'assertion A1 {',
      '  subject: "R',
      '}'
    ].join('\n')
  );
  const nestedMetaDocument = TextDocument.create(
    'file:///identifier-negative-nested-meta.ssd',
    'semdl-ssd',
    1,
    [
      'document D1 {',
      '}',
      'resource R1 {',
      '}',
      'assertion A1 {',
      '  meta {',
      '    subject: R',
      '  }',
      '}'
    ].join('\n')
  );

  assert.equal(getCompletionItems(predicateDocument, Position.create(5, 15)).length, 0);
  assert.equal(getCompletionItems(quotedDocument, Position.create(5, 13)).length, 0);
  assert.equal(getCompletionItems(nestedMetaDocument, Position.create(6, 14)).length, 0);
});

test('top-level block keywords expose grammar-derived hover in .ssd documents', () => {
  const document = TextDocument.create(
    'file:///hover-document.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '}'].join('\n')
  );

  const hover = getKeywordHover(document, Position.create(0, 2));
  assert.equal((hover?.contents as MarkupContent | undefined)?.kind, 'markdown');
  assert.match(hoverMarkdownValue(hover), /SEMDL Canvas top-level block keyword/);
  assert.match(hoverMarkdownValue(hover), /document-block = "document"/);
});

test('nested block keywords expose grammar-derived hover in document-local context', () => {
  const document = TextDocument.create(
    'file:///hover-nested.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '}', '', 'assertion A1 {', '  meta {', '  }', '}'].join('\n')
  );

  const hover = getKeywordHover(document, Position.create(4, 4));
  assert.match(hoverMarkdownValue(hover), /SEMDL Canvas nested block keyword/);
  assert.match(hoverMarkdownValue(hover), /inline-meta-block = "meta"/);
});

test('top-level block keywords expose sidekick hover in .ssm documents', () => {
  const document = TextDocument.create(
    'file:///hover-document.ssm',
    'semdl-ssm',
    1,
    ['document_meta D1 {', '}'].join('\n')
  );

  const hover = getKeywordHover(document, Position.create(0, 4));
  assert.match(hoverMarkdownValue(hover), /SEMDL Sidekick top-level block keyword/);
  assert.match(hoverMarkdownValue(hover), /document-meta-block = "document_meta"/);
});

test('query header and query entry keywords expose lens hover', () => {
  const document = TextDocument.create(
    'file:///hover-query.ssq',
    'semdl-ssq',
    1,
    ['query {', '  similar: A1', '}'].join('\n')
  );

  const headerHover = getKeywordHover(document, Position.create(0, 2));
  const entryHover = getKeywordHover(document, Position.create(1, 3));
  assert.match(hoverMarkdownValue(headerHover), /SEMDL Lens query header keyword/);
  assert.match(hoverMarkdownValue(headerHover), /query-block = "query"/);
  assert.match(hoverMarkdownValue(entryHover), /SEMDL Lens query entry keyword/);
  assert.match(hoverMarkdownValue(entryHover), /similar-entry = "similar"/);
});

test('hover is not returned for field names or field values', () => {
  const document = TextDocument.create(
    'file:///hover-field.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '  title: Sample', '}'].join('\n')
  );

  assert.equal(getKeywordHover(document, Position.create(1, 2)), null);
  assert.equal(getKeywordHover(document, Position.create(1, 10)), null);
});

test('hover is not returned for invalid nested-block placement', () => {
  const document = TextDocument.create(
    'file:///hover-invalid-nested.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '}', '', 'resource R1 {', '  meta {', '  }', '}'].join('\n')
  );

  assert.equal(getKeywordHover(document, Position.create(4, 3)), null);
});

test('hover is not returned for invalid top-level block placement', () => {
  const sidekickDocument = TextDocument.create(
    'file:///hover-invalid-top-level.ssm',
    'semdl-ssm',
    1,
    'resource R1 {'
  );
  const secondDocument = TextDocument.create(
    'file:///hover-second-document.ssd',
    'semdl-ssd',
    1,
    ['document D1 {', '}', '', 'document D2 {'].join('\n')
  );

  assert.equal(getKeywordHover(sidekickDocument, Position.create(0, 2)), null);
  assert.equal(getKeywordHover(secondDocument, Position.create(3, 2)), null);
});

test('hover is not returned for out-of-context query entry lines', () => {
  const document = TextDocument.create(
    'file:///hover-invalid-query.ssq',
    'semdl-ssq',
    1,
    'select: assertion'
  );

  assert.equal(getKeywordHover(document, Position.create(0, 2)), null);
});

test('hover is not returned for duplicate or out-of-order query entries', () => {
  const duplicateDocument = TextDocument.create(
    'file:///hover-duplicate-query.ssq',
    'semdl-ssq',
    1,
    ['query {', '  select: assertion', '  where: source_presence', '  where: confidence > 0.8', '}'].join('\n')
  );
  const outOfOrderDocument = TextDocument.create(
    'file:///hover-out-of-order-query.ssq',
    'semdl-ssq',
    1,
    ['query {', '  select: assertion', '  return: matches', '  where: source_presence', '}'].join('\n')
  );

  assert.equal(getKeywordHover(duplicateDocument, Position.create(3, 3)), null);
  assert.equal(getKeywordHover(outOfOrderDocument, Position.create(3, 3)), null);
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

test('duplicate .ssq query entries point back to the first occurrence', () => {
  const document = TextDocument.create(
    'file:///duplicate-query-entry.ssq',
    'semdl-ssq',
    1,
    [
      'query {',
      '  select: assertion',
      '  where: source_presence',
      '  where: confidence > 0.8',
      '}'
    ].join('\n')
  );

  const result = analyzeDocument(document);
  const duplicateDiagnostic = result.diagnostics.find((diagnostic) => diagnostic.code === 'duplicate-query-entry');

  assert.ok(duplicateDiagnostic);
  assert.equal(duplicateDiagnostic?.range.start.line, 3);
  assert.equal(duplicateDiagnostic?.relatedInformation?.[0]?.location.range.start.line, 2);
});

test('out-of-order .ssq query entries point back to the order boundary', () => {
  const document = TextDocument.create(
    'file:///out-of-order-query-entry.ssq',
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
  const orderDiagnostic = result.diagnostics.find((diagnostic) => diagnostic.code === 'query-entry-order');

  assert.ok(orderDiagnostic);
  assert.equal(orderDiagnostic?.range.start.line, 3);
  assert.equal(orderDiagnostic?.relatedInformation?.[0]?.location.range.start.line, 2);
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

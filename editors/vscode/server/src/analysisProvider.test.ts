import test from 'node:test';
import assert from 'node:assert/strict';
import { MarkupContent, Position } from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { createAnalysisProvider } from './analysisProvider';

function hoverMarkdownValue(hover: Awaited<ReturnType<ReturnType<typeof createAnalysisProvider>['getHover']>>): string {
  const contents = hover?.contents;
  if (!contents || Array.isArray(contents) || typeof contents === 'string') {
    return '';
  }
  return (contents as MarkupContent).value;
}

test('default analysis provider serves diagnostics and symbols through the provider boundary', async () => {
  const provider = createAnalysisProvider({});
  const document = TextDocument.create(
    'file:///provider-test.ssd',
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

  const diagnostics = await provider.getDiagnostics(document);
  const symbols = await provider.getDocumentSymbols(document);

  assert.equal(diagnostics.length, 0);
  assert.deepEqual(symbols.map((symbol) => symbol.name), ['document D1', 'resource R1']);
  assert.deepEqual(provider.getSourceMetadata(), {
    requestedSource: 'auto',
    activeSource: 'local-ts-analyzer',
    targetSource: 'binary-audit',
    fallbackReason: 'binary audit source planned'
  });
});

test('binary-requested analysis provider reports fallback metadata while keeping the current provider surface stable', async () => {
  const provider = createAnalysisProvider({ SEMDL_LSP_ANALYSIS_SOURCE: 'binary' });
  const document = TextDocument.create(
    'file:///provider-test.ssq',
    'semdl-ssq',
    1,
    [
      'query {',
      '  similar: "A1"',
      '}'
    ].join('\n')
  );

  const diagnostics = await provider.getDiagnostics(document);

  assert.ok(diagnostics.some((diagnostic) => diagnostic.message.includes('`similar:` must reference an identifier target.')));
  assert.deepEqual(provider.getSourceMetadata(), {
    requestedSource: 'binary',
    activeSource: 'local-ts-analyzer',
    targetSource: 'binary-audit',
    fallbackReason: 'binary audit source requested but not implemented yet'
  });
  assert.match(provider.describeSource(), /local-ts-analyzer fallback/);
});

test('analysis provider exposes grammar-derived keyword completion through the provider boundary', async () => {
  const provider = createAnalysisProvider({});
  const document = TextDocument.create(
    'file:///provider-completion.ssq',
    'semdl-ssq',
    1,
    ['query {', '  select: assertion', '  '].join('\n')
  );

  const items = await provider.getCompletionItems(document, Position.create(2, 2));
  assert.deepEqual(items.map((item) => item.label), ['where', 'similar', 'return']);
});

test('analysis provider exposes document-local identifier completion through the provider boundary', async () => {
  const provider = createAnalysisProvider({});
  const document = TextDocument.create(
    'file:///provider-identifier-completion.ssd',
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

  const items = await provider.getCompletionItems(document, Position.create(7, 11));
  assert.deepEqual(items.map((item) => item.label), ['R1', 'R2']);
});

test('analysis provider exposes grammar-derived keyword hover through the provider boundary', async () => {
  const provider = createAnalysisProvider({});
  const document = TextDocument.create(
    'file:///provider-hover.ssq',
    'semdl-ssq',
    1,
    ['query {', '  return: matches', '}'].join('\n')
  );

  const hover = await provider.getHover(document, Position.create(1, 3));
  assert.match(hoverMarkdownValue(hover), /SEMDL Lens query entry keyword/);
  assert.match(hoverMarkdownValue(hover), /return-entry = "return"/);
});
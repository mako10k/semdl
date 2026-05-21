import test from 'node:test';
import assert from 'node:assert/strict';
import { Position } from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { createAnalysisProvider } from './analysisProvider';

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

  const items = await provider.getKeywordCompletionItems(document, Position.create(2, 2));
  assert.deepEqual(items.map((item) => item.label), ['where', 'similar', 'return']);
});
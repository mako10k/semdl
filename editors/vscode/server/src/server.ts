import {
  createConnection,
  ProposedFeatures,
  TextDocumentSyncKind,
  TextDocuments,
  type DocumentSymbolParams,
  type InitializeParams,
  type InitializeResult
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { analyzeDocument } from './analyzer';
import { filterExpectedDiagnostics } from './diagnosticPolicy';

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

function publishDiagnostics(document: TextDocument): void {
  const result = analyzeDocument(document);
  connection.sendDiagnostics({ uri: document.uri, diagnostics: filterExpectedDiagnostics(document, result.diagnostics) });
}

connection.onInitialize((_params: InitializeParams): InitializeResult => {
  return {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,
      documentSymbolProvider: true
    },
    serverInfo: {
      name: 'semdl-language-server',
      version: '0.0.1'
    }
  };
});

connection.onInitialized(() => {
  connection.console.info('SEMDL language server initialized with basic diagnostics and document symbols.');
});

documents.onDidOpen((change) => {
  publishDiagnostics(change.document);
});

documents.onDidChangeContent((change) => {
  publishDiagnostics(change.document);
});

documents.onDidClose((change) => {
  connection.sendDiagnostics({ uri: change.document.uri, diagnostics: [] });
});

connection.onDocumentSymbol((params: DocumentSymbolParams) => {
  const document = documents.get(params.textDocument.uri);
  if (!document) {
    return [];
  }
  return analyzeDocument(document).symbols;
});

documents.listen(connection);
connection.listen();

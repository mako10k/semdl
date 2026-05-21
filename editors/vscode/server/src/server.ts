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
import { createAnalysisProvider } from './analysisProvider';
import { filterExpectedDiagnostics } from './diagnosticPolicy';

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);
const analysisProvider = createAnalysisProvider();

async function publishDiagnostics(document: TextDocument): Promise<void> {
  const diagnostics = await analysisProvider.getDiagnostics(document);
  connection.sendDiagnostics({ uri: document.uri, diagnostics: filterExpectedDiagnostics(document, diagnostics) });
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
  connection.console.info(`SEMDL language server initialized with basic diagnostics and document symbols. Analysis source: ${analysisProvider.describeSource()}`);
});

documents.onDidOpen((change) => {
  void publishDiagnostics(change.document);
});

documents.onDidChangeContent((change) => {
  void publishDiagnostics(change.document);
});

documents.onDidClose((change) => {
  connection.sendDiagnostics({ uri: change.document.uri, diagnostics: [] });
});

connection.onDocumentSymbol(async (params: DocumentSymbolParams) => {
  const document = documents.get(params.textDocument.uri);
  if (!document) {
    return [];
  }
  return analysisProvider.getDocumentSymbols(document);
});

documents.listen(connection);
connection.listen();

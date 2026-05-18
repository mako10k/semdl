import { createConnection, ProposedFeatures, TextDocumentSyncKind, TextDocuments, type InitializeParams, type InitializeResult } from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

connection.onInitialize((_params: InitializeParams): InitializeResult => {
  return {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,
      documentSymbolProvider: false
    },
    serverInfo: {
      name: 'semdl-language-server',
      version: '0.0.1'
    }
  };
});

connection.onInitialized(() => {
  connection.console.info('SEMDL language server scaffold initialized. Features are intentionally deferred.');
});

documents.listen(connection);
connection.listen();

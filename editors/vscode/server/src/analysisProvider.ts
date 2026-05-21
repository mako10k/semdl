import { TextDocument } from 'vscode-languageserver-textdocument';
import type { Diagnostic, DocumentSymbol } from 'vscode-languageserver/node';
import { analyzeDocument } from './analyzer';

export interface AnalysisSourceMetadata {
  requestedSource: 'auto' | 'binary';
  activeSource: 'local-ts-analyzer';
  targetSource: 'local-ts-analyzer' | 'binary-audit';
  fallbackReason?: string;
}

export interface AnalysisProvider {
  getDiagnostics(document: TextDocument): Promise<Diagnostic[]>;
  getDocumentSymbols(document: TextDocument): Promise<DocumentSymbol[]>;
  describeSource(): string;
  getSourceMetadata(): AnalysisSourceMetadata;
}

class LocalAnalyzerProvider implements AnalysisProvider {
  constructor(private readonly sourceMetadata: AnalysisSourceMetadata) {}

  async getDiagnostics(document: TextDocument): Promise<Diagnostic[]> {
    return analyzeDocument(document).diagnostics;
  }

  async getDocumentSymbols(document: TextDocument): Promise<DocumentSymbol[]> {
    return analyzeDocument(document).symbols;
  }

  describeSource(): string {
    if (this.sourceMetadata.fallbackReason) {
      return `${this.sourceMetadata.activeSource} fallback (${this.sourceMetadata.fallbackReason})`;
    }
    return this.sourceMetadata.activeSource;
  }

  getSourceMetadata(): AnalysisSourceMetadata {
    return this.sourceMetadata;
  }
}

export function createAnalysisProvider(env: NodeJS.ProcessEnv = process.env): AnalysisProvider {
  const requestedSource = env.SEMDL_LSP_ANALYSIS_SOURCE?.trim().toLowerCase();
  if (requestedSource === 'binary') {
    return new LocalAnalyzerProvider({
      requestedSource: 'binary',
      activeSource: 'local-ts-analyzer',
      targetSource: 'binary-audit',
      fallbackReason: 'binary audit source requested but not implemented yet'
    });
  }

  return new LocalAnalyzerProvider({
    requestedSource: 'auto',
    activeSource: 'local-ts-analyzer',
    targetSource: 'binary-audit',
    fallbackReason: 'binary audit source planned'
  });
}
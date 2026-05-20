import { TextDocument } from 'vscode-languageserver-textdocument';
import { analyzeDocument, type AnalysisResult } from './analyzer';

export interface AnalysisProvider {
  analyzeDocument(document: TextDocument): Promise<AnalysisResult>;
  describeSource(): string;
}

class LocalAnalyzerProvider implements AnalysisProvider {
  constructor(private readonly description: string) {}

  async analyzeDocument(document: TextDocument): Promise<AnalysisResult> {
    return analyzeDocument(document);
  }

  describeSource(): string {
    return this.description;
  }
}

export function createAnalysisProvider(): AnalysisProvider {
  const requestedSource = process.env.SEMDL_LSP_ANALYSIS_SOURCE?.trim().toLowerCase();
  if (requestedSource === 'binary') {
    return new LocalAnalyzerProvider('local-ts-analyzer fallback (binary audit source requested but not implemented yet)');
  }
  return new LocalAnalyzerProvider('local-ts-analyzer fallback (binary audit source planned)');
}
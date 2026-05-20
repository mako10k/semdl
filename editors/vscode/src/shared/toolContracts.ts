import toolCatalogJson from '../../tooling/semdl-tool-contracts.json';

export interface SemdlJsonStringSchema {
  type: 'string';
  description?: string;
  enum?: string[];
  default?: string;
}

export interface SemdlJsonObjectSchema {
  type: 'object';
  additionalProperties?: boolean;
  properties: Record<string, SemdlJsonSchema>;
  required?: string[];
}

export type SemdlJsonSchema = SemdlJsonStringSchema | SemdlJsonObjectSchema;

export interface SemdlToolContract {
  name: string;
  displayName: string;
  toolReferenceName?: string;
  userDescription: string;
  modelDescription: string;
  canBeReferencedInPrompt?: boolean;
  when?: string;
  inputSchema: SemdlJsonObjectSchema;
}

export interface SemdlToolCatalog {
  version: number;
  tools: SemdlToolContract[];
}

const toolCatalog = toolCatalogJson as unknown as SemdlToolCatalog;

export function getSemdlToolCatalog(): SemdlToolCatalog {
  return toolCatalog;
}

export function getSemdlToolContracts(): readonly SemdlToolContract[] {
  return toolCatalog.tools;
}

export function getSemdlToolContract(name: string): SemdlToolContract {
  const contract = toolCatalog.tools.find((candidate) => candidate.name === name);
  if (!contract) {
    throw new Error(`Unknown SEMDL tool contract: ${name}`);
  }
  return contract;
}

export function listSemdlToolNames(): string[] {
  return toolCatalog.tools.map((tool) => tool.name);
}

export function toLanguageModelContribution(contract: SemdlToolContract): Record<string, unknown> {
  return {
    name: contract.name,
    displayName: contract.displayName,
    userDescription: contract.userDescription,
    modelDescription: contract.modelDescription,
    inputSchema: contract.inputSchema,
    canBeReferencedInPrompt: contract.canBeReferencedInPrompt,
    toolReferenceName: contract.toolReferenceName,
    when: contract.when
  };
}
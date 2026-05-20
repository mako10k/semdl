import test from 'node:test';
import assert from 'node:assert/strict';
import { getSemdlToolContracts, listSemdlToolNames, toLanguageModelContribution } from './toolContracts';

test('shared tool catalog exposes the expected read-only tools', () => {
  const toolNames = listSemdlToolNames();
  assert.deepEqual(toolNames, [
    'check_semdl_document',
    'explain_semdl_entity',
    'search_semdl_query',
    'read_semdl_help'
  ]);
  assert.equal(new Set(toolNames).size, toolNames.length);
});

test('language model contribution mirrors the shared contract', () => {
  const contract = getSemdlToolContracts()[0];
  const contribution = toLanguageModelContribution(contract);
  assert.equal(contribution.name, contract.name);
  assert.equal(contribution.displayName, contract.displayName);
  assert.deepEqual(contribution.inputSchema, contract.inputSchema);
});
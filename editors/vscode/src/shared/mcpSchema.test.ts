import test from 'node:test';
import assert from 'node:assert/strict';
import { getSemdlToolContract } from './toolContracts';
import { jsonObjectSchemaToZodObject, validateMcpToolInput } from './mcpSchema';

test('mcp schema conversion preserves additionalProperties false', () => {
  const contract = getSemdlToolContract('check_semdl_document');
  const schema = jsonObjectSchemaToZodObject(contract.inputSchema);

  const accepted = schema.safeParse({ document_path: '/tmp/example.ssd' });
  assert.equal(accepted.success, true);

  const rejected = schema.safeParse({ document_path: '/tmp/example.ssd', extra: 'x' });
  assert.equal(rejected.success, false);
});

test('runtime validation rejects extra MCP tool input properties', () => {
  const contract = getSemdlToolContract('check_semdl_document');
  assert.throws(
    () => validateMcpToolInput(contract.inputSchema, { document_path: '/tmp/example.ssd', extra: 'x' }),
    /Unrecognized key/
  );
});
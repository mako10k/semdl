import test from 'node:test';
import assert from 'node:assert/strict';
import { buildSemdlToolInvocation } from './semdlToolExecutors';

test('build explain tool invocation with absolute paths', () => {
  const invocation = buildSemdlToolInvocation('explain_semdl_entity', {
    entity_id: 'A1',
    document_path: '/tmp/example.ssd'
  });

  assert.deepEqual(invocation.args, ['explain', 'A1', '/tmp/example.ssd']);
  assert.equal(invocation.workingDirectory, '/tmp');
});

test('read help requires topic when target is provided', () => {
  assert.throws(
    () => buildSemdlToolInvocation('read_semdl_help', { target: 'inline' }),
    /target requires topic/
  );
});

test('document inputs must be absolute paths', () => {
  assert.throws(
    () => buildSemdlToolInvocation('check_semdl_document', { document_path: 'relative.ssd' }),
    /absolute path/
  );
});
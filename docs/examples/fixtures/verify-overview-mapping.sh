#!/bin/sh
set -eu

readme_path="${1:-docs/examples/fixtures/README.md}"
ssd_path="${2:-docs/examples/fixtures/ollama-embeddings-overview.ssd}"
expected_source_ref="docs/examples/fixtures/ollama-embeddings.psv"

readme_entries="$(mktemp)"
ssd_entries="$(mktemp)"
diff_output="$(mktemp)"
trap 'rm -f "$readme_entries" "$ssd_entries" "$diff_output"' EXIT

awk -F '|' '
  function trim(value) {
    gsub(/^[ \t]+|[ \t]+$/, "", value)
    return value
  }

  /^\| `[^`]+` / {
    target_id = trim($2)
    gsub(/`/, "", target_id)
    kind = trim($3)
    gsub(/`/, "", kind)
    source_field = trim($4)
    gsub(/`/, "", source_field)
    prompt = trim($5)
    gsub(/`/, "", prompt)
    raw_capture = trim($6)
    gsub(/`/, "", raw_capture)
    provider = trim($7)
    gsub(/`/, "", provider)
    model = trim($8)
    gsub(/`/, "", model)
    dimensions = trim($9)
    gsub(/`/, "", dimensions)
    print target_id "|" kind "|" source_field "|" prompt "|" raw_capture "|" provider "|" model "|" dimensions
  }
' "$readme_path" > "$readme_entries"

awk -v expected_source_ref="$expected_source_ref" '
  function trim(value) {
    gsub(/^[ \t]+|[ \t]+$/, "", value)
    return value
  }

  function flush_entry() {
    if (target_id == "") {
      return
    }
    if (block_kind != kind) {
      printf("kind mismatch for %s: comment=%s block=%s\n", target_id, kind, block_kind) > "/dev/stderr"
      exit 1
    }
    if (block_id != target_id) {
      printf("id mismatch for %s: block=%s\n", target_id, block_id) > "/dev/stderr"
      exit 1
    }
    if (prompt == "") {
      printf("missing prompt field %s for %s\n", source_field, target_id) > "/dev/stderr"
      exit 1
    }
    print target_id "|" kind "|" source_field "|" prompt "|" raw_capture "|" provider "|" model "|" dimensions
    target_id = ""
    kind = ""
    source_field = ""
    raw_capture = ""
    provider = ""
    model = ""
    dimensions = ""
    block_kind = ""
    block_id = ""
    prompt = ""
  }

  /^document [^ ]+ \{/ {
    next
  }

  /source_ref: "/ {
    source_ref = $0
    sub(/^[^"]*"/, "", source_ref)
    sub(/".*$/, "", source_ref)
    source_ref = trim(source_ref)
    next
  }

  /^# target_id:/ {
    flush_entry()
    field_count = split(substr($0, 3), fields, / \| /)
    for (field_index = 1; field_index <= field_count; field_index++) {
      key = fields[field_index]
      sub(/: .*/, "", key)
      key = trim(key)
      value = fields[field_index]
      sub(/^[^:]*: /, "", value)
      value = trim(value)
      if (key == "target_id") {
        target_id = value
      } else if (key == "kind") {
        kind = value
      } else if (key == "source_field") {
        source_field = value
      } else if (key == "raw_capture") {
        raw_capture = value
      } else if (key == "provider") {
        provider = value
      } else if (key == "model") {
        model = value
      } else if (key == "dimensions") {
        dimensions = value
      }
    }
    next
  }

  /^(resource|segment|assertion|hypothesis|alternative) [^ ]+ \{/ {
    block_line = $0
    sub(/ \{$/, "", block_line)
    split(block_line, block_parts, / /)
    block_kind = block_parts[1]
    block_id = block_parts[2]
    next
  }

  /^[[:space:]]*[A-Za-z_]+: "/ {
    field_line = trim($0)
    field_name = field_line
    sub(/: .*/, "", field_name)
    if (field_name == source_field) {
      prompt = field_line
      sub(/^[^"]*"/, "", prompt)
      sub(/".*$/, "", prompt)
    }
    next
  }

  /^}/ {
    flush_entry()
  }

  END {
    flush_entry()
    if (source_ref != expected_source_ref) {
      printf("source_ref mismatch: %s\n", source_ref) > "/dev/stderr"
      exit 1
    }
  }
' "$ssd_path" > "$ssd_entries"

if ! diff -u "$readme_entries" "$ssd_entries" > "$diff_output"; then
  cat "$diff_output" >&2
  exit 1
fi

entry_count="$(wc -l < "$readme_entries" | tr -d ' ')"
printf 'OK %s %s entries: %s\n' "$readme_path" "$ssd_path" "$entry_count"
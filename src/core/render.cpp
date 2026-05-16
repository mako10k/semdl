#include "semdl/core/render.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace semdl::core {

namespace {

void append_field_if_present(std::ostringstream& output,
                             const std::map<std::string, std::string>& fields,
                             std::vector<std::string>& emitted,
                             std::string_view field_name,
                             std::string_view indent,
                             std::string_view rendered_name = "");

void append_remaining_fields(std::ostringstream& output,
                             const std::map<std::string, std::string>& fields,
                             const std::vector<std::string>& emitted,
                             std::string_view indent,
                             std::string_view excluded_prefix = "");

std::vector<std::string> collect_sorted_ids_for_kind(const DocumentData& document, std::string_view kind) {
    std::vector<std::string> ids;
    for (const auto& [id, entity] : document.entities) {
        if (entity.kind == kind) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool is_document_sidecar_field(std::string_view field_name) {
    return field_name == "version" || field_name == "generator";
}

bool is_entity_sidecar_field(std::string_view field_name) {
    return field_name == "confidence" || field_name == "provenance_kind" || field_name == "rationale" ||
           field_name == "caveat" || field_name == "todo" || field_name == "status" || field_name == "explanation" ||
           field_name.starts_with("embedding.");
}

std::map<std::string, std::string> collect_inline_document_fields(const DocumentData& document, const std::string& id) {
    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(id); entity != nullptr) {
        for (const auto& [name, value] : entity->fields) {
            if (!is_document_sidecar_field(name)) {
                fields[name] = value;
            }
        }
    }
    return fields;
}

std::map<std::string, std::string> collect_document_sidecar_fields(const DocumentData& document, const std::string& id) {
    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(id); entity != nullptr) {
        for (const auto& [name, value] : entity->fields) {
            if (is_document_sidecar_field(name)) {
                fields[name] = value;
            }
        }
    }
    if (const auto* metadata = document.find_metadata(id); metadata != nullptr) {
        for (const auto& [name, value] : metadata->fields) {
            fields[name] = value;
        }
    }
    return fields;
}

std::map<std::string, std::string> collect_inline_entity_fields(const DocumentData& document, const std::string& id) {
    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(id); entity != nullptr) {
        for (const auto& [name, value] : entity->fields) {
            if (!is_entity_sidecar_field(name)) {
                fields[name] = value;
            }
        }
    }
    return fields;
}

std::map<std::string, std::string> collect_entity_sidecar_fields(const DocumentData& document, const std::string& id) {
    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(id); entity != nullptr) {
        for (const auto& [name, value] : entity->fields) {
            if (is_entity_sidecar_field(name)) {
                fields[name] = value;
            }
        }
    }
    if (const auto* metadata = document.find_metadata(id); metadata != nullptr) {
        for (const auto& [name, value] : metadata->fields) {
            fields[name] = value;
        }
    }
    return fields;
}

void append_preview_item(std::vector<std::string>& items,
                         std::string_view scope,
                         const std::string& id,
                         std::string_view field_name) {
    items.push_back(std::string(scope) + " " + id + "." + std::string(field_name));
}

void append_document_sidecar_preview(const std::string& id,
                                     const std::map<std::string, std::string>& fields,
                                     SplitDocumentOutput& output) {
    if (fields.empty()) {
        return;
    }

    if (fields.contains("version")) {
        append_preview_item(output.moved_items, "document_meta", id, "version");
        ++output.moved_count;
    }
    if (fields.contains("generator")) {
        append_preview_item(output.moved_items, "document_meta", id, "generator");
        ++output.moved_count;
    }
    for (const auto& [name, value] : fields) {
        (void)value;
        if (name == "version" || name == "generator") {
            continue;
        }
        append_preview_item(output.moved_items, "document_meta", id, name);
        ++output.moved_count;
    }
}

void append_entity_sidecar_preview(const std::string& id,
                                   const std::map<std::string, std::string>& fields,
                                   SplitDocumentOutput& output) {
    if (fields.empty()) {
        return;
    }

    static constexpr std::array ordered_meta_fields = {
        std::string_view{"confidence"},
        std::string_view{"provenance_kind"},
        std::string_view{"rationale"},
        std::string_view{"caveat"},
        std::string_view{"todo"},
        std::string_view{"status"},
        std::string_view{"explanation"},
    };

    for (const auto field_name : ordered_meta_fields) {
        if (fields.contains(std::string(field_name))) {
            append_preview_item(output.moved_items, "meta", id, field_name);
            ++output.moved_count;
        }
    }

    bool has_embedding = false;
    for (const auto& [name, value] : fields) {
        (void)value;
        if (name.starts_with("embedding.")) {
            has_embedding = true;
            continue;
        }
        if (std::find(ordered_meta_fields.begin(), ordered_meta_fields.end(), name) != ordered_meta_fields.end()) {
            continue;
        }
        append_preview_item(output.moved_items, "meta", id, name);
        ++output.moved_count;
    }
    if (has_embedding) {
        append_preview_item(output.moved_items, "meta", id, "embedding");
        ++output.moved_count;
    }
}

void append_entity_preview_items(const DocumentData& document,
                                 const std::string& id,
                                 SplitDocumentOutput& output) {
    const auto inline_fields = collect_inline_entity_fields(document, id);
    for (const auto& [name, value] : inline_fields) {
        (void)value;
        append_preview_item(output.kept_items, document.find_entity(id)->kind, id, name);
    }
    append_entity_sidecar_preview(id, collect_entity_sidecar_fields(document, id), output);
}

void append_document_meta_block(std::ostringstream& output,
                                const std::string& id,
                                const std::map<std::string, std::string>& fields) {
    if (fields.empty()) {
        return;
    }

    output << "document_meta " << id << " {\n";
    std::vector<std::string> emitted;
    append_field_if_present(output, fields, emitted, "version", "  ");
    append_field_if_present(output, fields, emitted, "generator", "  ");
    append_remaining_fields(output, fields, emitted, "  ");
    output << "}\n";
}

void append_sidecar_meta_block(std::ostringstream& output,
                               const std::string& id,
                               const std::map<std::string, std::string>& fields) {
    if (fields.empty()) {
        return;
    }

    output << "meta " << id << " {\n";

    std::vector<std::string> emitted;
    append_field_if_present(output, fields, emitted, "confidence", "  ");
    append_field_if_present(output, fields, emitted, "provenance_kind", "  ");
    append_field_if_present(output, fields, emitted, "rationale", "  ");
    append_field_if_present(output, fields, emitted, "caveat", "  ");
    append_field_if_present(output, fields, emitted, "todo", "  ");
    append_field_if_present(output, fields, emitted, "status", "  ");
    append_field_if_present(output, fields, emitted, "explanation", "  ");
    append_remaining_fields(output, fields, emitted, "  ", "embedding.");

    std::vector<std::string> embedding_emitted;
    std::ostringstream embedding_output;
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.model", "    ", "model");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.dimensions", "    ", "dimensions");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.generated_at", "    ", "generated_at");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.provider", "    ", "provider");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.source_field", "    ", "source_field");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.vector", "    ", "vector");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.vector_ref", "    ", "vector_ref");
    for (const auto& [name, value] : fields) {
        if (!name.starts_with("embedding.")) {
            continue;
        }
        if (std::find(embedding_emitted.begin(), embedding_emitted.end(), name) != embedding_emitted.end()) {
            continue;
        }
        embedding_output << "    " << name.substr(10) << ": " << value << "\n";
    }

    const std::string embedding_text = embedding_output.str();
    if (!embedding_text.empty()) {
        output << "\n  embedding {\n" << embedding_text << "  }\n";
    }

    output << "}\n";
}

void append_field_if_present(std::ostringstream& output,
                             const std::map<std::string, std::string>& fields,
                             std::vector<std::string>& emitted,
                             std::string_view field_name,
                             std::string_view indent,
                             std::string_view rendered_name) {
    const auto it = fields.find(std::string(field_name));
    if (it == fields.end()) {
        return;
    }
    const std::string_view name = rendered_name.empty() ? field_name : rendered_name;
    output << indent << name << ": " << it->second << "\n";
    emitted.push_back(std::string(field_name));
}

void append_remaining_fields(std::ostringstream& output,
                             const std::map<std::string, std::string>& fields,
                             const std::vector<std::string>& emitted,
                             std::string_view indent,
                             std::string_view excluded_prefix) {
    for (const auto& [name, value] : fields) {
        if (std::find(emitted.begin(), emitted.end(), name) != emitted.end()) {
            continue;
        }
        if (!excluded_prefix.empty() && name.starts_with(excluded_prefix)) {
            continue;
        }
        output << indent << name << ": " << value << "\n";
    }
}

std::map<std::string, std::string> document_level_fields(const DocumentData& document, const std::string& id) {
    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(id); entity != nullptr) {
        fields.insert(entity->fields.begin(), entity->fields.end());
    }
    if (const auto* metadata = document.find_metadata(id); metadata != nullptr) {
        fields.insert(metadata->fields.begin(), metadata->fields.end());
    }
    return fields;
}

void append_meta_block(std::ostringstream& output, const DocumentData& document, const std::string& id) {
    const auto* metadata = document.find_metadata(id);
    if (metadata == nullptr || metadata->kind == "document_meta") {
        return;
    }

    const auto& fields = metadata->fields;
    std::vector<std::string> emitted;
    std::ostringstream meta_output;
    append_field_if_present(meta_output, fields, emitted, "confidence", "    ");
    append_field_if_present(meta_output, fields, emitted, "provenance_kind", "    ");
    append_field_if_present(meta_output, fields, emitted, "rationale", "    ");
    append_field_if_present(meta_output, fields, emitted, "caveat", "    ");
    append_remaining_fields(meta_output, fields, emitted, "    ", "embedding.");

    std::vector<std::string> embedding_emitted;
    std::ostringstream embedding_output;
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.model", "      ", "model");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.dimensions", "      ", "dimensions");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.generated_at", "      ", "generated_at");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.provider", "      ", "provider");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.source_field", "      ", "source_field");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.vector", "      ", "vector");
    append_field_if_present(embedding_output, fields, embedding_emitted, "embedding.vector_ref", "      ", "vector_ref");
    for (const auto& [name, value] : fields) {
        if (!name.starts_with("embedding.")) {
            continue;
        }
        if (std::find(embedding_emitted.begin(), embedding_emitted.end(), name) != embedding_emitted.end()) {
            continue;
        }
        embedding_output << "      " << name.substr(10) << ": " << value << "\n";
    }

    const std::string meta_text = meta_output.str();
    const std::string embedding_text = embedding_output.str();
    if (meta_text.empty() && embedding_text.empty()) {
        return;
    }

    output << "\n  meta {\n" << meta_text;
    if (!embedding_text.empty()) {
        output << "\n    embedding {\n" << embedding_text << "    }\n";
    }
    output << "  }\n";
}

void append_entity_block(std::ostringstream& output,
                         const DocumentData* document,
                         const std::string& id,
                         std::string_view kind,
                         const std::map<std::string, std::string>& fields,
                         const std::vector<std::string_view>& ordered_fields,
                         bool include_meta_block) {
    if (fields.empty()) {
        return;
    }

    output << kind << ' ' << id << " {\n";
    std::vector<std::string> emitted;
    for (const auto field_name : ordered_fields) {
        append_field_if_present(output, fields, emitted, field_name, "  ");
    }
    append_remaining_fields(output, fields, emitted, "  ");
    if (include_meta_block && document != nullptr) {
        append_meta_block(output, *document, id);
    }
    output << "}\n";
}

}  // namespace

std::string render_canonical_inline_document(const DocumentData& document) {
    std::ostringstream output;

    const auto document_ids = collect_sorted_ids_for_kind(document, "document");
    bool first_block = true;
    for (const auto& id : document_ids) {
        if (!first_block) {
            output << "\n";
        }
        first_block = false;
        output << "document " << id << " {\n";
        const auto fields = document_level_fields(document, id);
        std::vector<std::string> emitted;
        append_field_if_present(output, fields, emitted, "title", "  ");
        append_field_if_present(output, fields, emitted, "source_ref", "  ");
        append_field_if_present(output, fields, emitted, "version", "  ");
        append_field_if_present(output, fields, emitted, "generator", "  ");
        append_remaining_fields(output, fields, emitted, "  ");
        output << "}\n";
    }

    const std::array kind_order = {
        std::pair{"resource", std::vector<std::string_view>{"type", "label"}},
        std::pair{"segment", std::vector<std::string_view>{"source", "text_quote"}},
        std::pair{"assertion", std::vector<std::string_view>{"subject", "predicate", "object", "label", "source_segment", "source_presence", "embedding_presence"}},
        std::pair{"hypothesis", std::vector<std::string_view>{"about", "kind", "summary", "alternative_group"}},
        std::pair{"alternative", std::vector<std::string_view>{"group", "label"}},
    };

    for (const auto& [kind, fields] : kind_order) {
        const auto ids = collect_sorted_ids_for_kind(document, kind);
        for (const auto& id : ids) {
            if (!first_block) {
                output << "\n";
            }
            first_block = false;
            append_entity_block(output, &document, id, kind, collect_inline_entity_fields(document, id), fields, true);
        }
    }

    return output.str();
}

SplitDocumentOutput render_split_document(const DocumentData& document) {
    SplitDocumentOutput result;

    std::ostringstream inline_output;
    std::ostringstream sidecar_output;

    const auto document_ids = collect_sorted_ids_for_kind(document, "document");
    bool first_inline_block = true;
    bool first_sidecar_block = true;
    for (const auto& id : document_ids) {
        const auto inline_fields = collect_inline_document_fields(document, id);
        const auto sidecar_fields = collect_document_sidecar_fields(document, id);

        for (const auto& [name, value] : inline_fields) {
            (void)value;
            append_preview_item(result.kept_items, "document", id, name);
        }
        append_document_sidecar_preview(id, sidecar_fields, result);

        if (!inline_fields.empty()) {
            if (!first_inline_block) {
                inline_output << "\n";
            }
            first_inline_block = false;
            inline_output << "document " << id << " {\n";
            std::vector<std::string> emitted;
            append_field_if_present(inline_output, inline_fields, emitted, "title", "  ");
            append_field_if_present(inline_output, inline_fields, emitted, "source_ref", "  ");
            append_remaining_fields(inline_output, inline_fields, emitted, "  ");
            inline_output << "}\n";
        }

        if (!sidecar_fields.empty()) {
            if (!first_sidecar_block) {
                sidecar_output << "\n";
            }
            first_sidecar_block = false;
            append_document_meta_block(sidecar_output, id, sidecar_fields);
        }
    }

    const std::array kind_order = {
        std::pair{"resource", std::vector<std::string_view>{"type", "label"}},
        std::pair{"segment", std::vector<std::string_view>{"source", "text_quote"}},
        std::pair{"assertion", std::vector<std::string_view>{"subject", "predicate", "object", "label", "source_segment", "source_presence", "embedding_presence"}},
        std::pair{"hypothesis", std::vector<std::string_view>{"about", "kind", "summary", "alternative_group"}},
        std::pair{"alternative", std::vector<std::string_view>{"group", "label"}},
    };

    for (const auto& [kind, ordered_fields] : kind_order) {
        const auto ids = collect_sorted_ids_for_kind(document, kind);
        for (const auto& id : ids) {
            const auto inline_fields = collect_inline_entity_fields(document, id);
            const auto sidecar_fields = collect_entity_sidecar_fields(document, id);

            append_entity_preview_items(document, id, result);

            if (!inline_fields.empty()) {
                if (!first_inline_block) {
                    inline_output << "\n";
                }
                first_inline_block = false;
                append_entity_block(inline_output, nullptr, id, kind, inline_fields, ordered_fields, false);
            }

            if (!sidecar_fields.empty()) {
                if (!first_sidecar_block) {
                    sidecar_output << "\n";
                }
                first_sidecar_block = false;
                append_sidecar_meta_block(sidecar_output, id, sidecar_fields);
            }
        }
    }

    result.inline_document = inline_output.str();
    result.sidecar_document = sidecar_output.str();
    return result;
}

}  // namespace semdl::core
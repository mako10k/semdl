#include "semdl/core/render.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

namespace semdl::core {

namespace {

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

void append_field_if_present(std::ostringstream& output,
                             const std::map<std::string, std::string>& fields,
                             std::vector<std::string>& emitted,
                             std::string_view field_name,
                             std::string_view indent,
                             std::string_view rendered_name = "") {
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
                             std::string_view excluded_prefix = "") {
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
                         const DocumentData& document,
                         const std::string& id,
                         std::string_view kind,
                         const std::vector<std::string_view>& ordered_fields) {
    const auto* entity = document.find_entity(id);
    if (entity == nullptr) {
        return;
    }

    output << kind << ' ' << id << " {\n";
    std::vector<std::string> emitted;
    for (const auto field_name : ordered_fields) {
        append_field_if_present(output, entity->fields, emitted, field_name, "  ");
    }
    append_remaining_fields(output, entity->fields, emitted, "  ");
    append_meta_block(output, document, id);
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
            append_entity_block(output, document, id, kind, fields);
        }
    }

    return output.str();
}

}  // namespace semdl::core
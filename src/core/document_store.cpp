#include "semdl/core/document_store.hpp"

#include <fstream>
#include <sstream>

namespace {

std::filesystem::path derive_sidecar_path(const std::filesystem::path& input_file) {
    auto sidecar = input_file;
    sidecar.replace_extension(".ssm");
    return sidecar;
}

std::string trim_copy(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_words(const std::string& value) {
    std::istringstream stream(value);
    std::vector<std::string> parts;
    std::string item;

    while (stream >> item) {
        parts.push_back(item);
    }

    return parts;
}

void increment_kind_count(semdl::core::DocumentData& data, const std::string& kind) {
    if (kind == "document") {
        ++data.document_count;
    } else if (kind == "resource") {
        ++data.resource_count;
    } else if (kind == "segment") {
        ++data.segment_count;
    } else if (kind == "assertion") {
        ++data.assertion_count;
    } else if (kind == "hypothesis") {
        ++data.hypothesis_count;
    } else if (kind == "alternative") {
        ++data.alternative_count;
    }
}

void parse_file(const std::filesystem::path& file_path, semdl::core::DocumentData& data, bool is_sidecar) {
    std::ifstream input(file_path);
    if (!input.is_open()) {
        data.issues.push_back("failed to open " + file_path.generic_string());
        return;
    }

    std::string line;
    semdl::core::EntityData* current_entity = nullptr;
    semdl::core::EntityData* current_metadata = nullptr;
    semdl::core::EntityData* current_fields = nullptr;
    std::string current_entity_id;
    std::string nested_prefix;

    while (std::getline(input, line)) {
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }

        if (trimmed == "}") {
            if (!nested_prefix.empty()) {
                nested_prefix.clear();
            } else if (current_fields == current_metadata && current_metadata != nullptr) {
                current_fields = current_entity;
                current_metadata = nullptr;
            } else {
                current_fields = nullptr;
                current_entity = nullptr;
                current_entity_id.clear();
            }
            continue;
        }

        if (trimmed.ends_with('{')) {
            const std::string header = trim_copy(trimmed.substr(0, trimmed.size() - 1));
            if (header == "embedding" && current_fields != nullptr) {
                nested_prefix = "embedding.";
                continue;
            }

            if (header == "meta" && current_entity != nullptr) {
                const std::string metadata_kind = current_entity->kind == "document" ? "document_meta" : "meta";
                auto [it, inserted] = data.metadata_entities.emplace(current_entity_id, semdl::core::EntityData{});
                if (inserted || it->second.kind.empty()) {
                    it->second.kind = metadata_kind;
                }
                current_metadata = &it->second;
                current_fields = current_metadata;
                continue;
            }

            const auto words = split_words(header);
            if (words.size() != 2U) {
                continue;
            }

            const auto& kind = words[0];
            const auto& id = words[1];
            if (is_sidecar && (kind == "meta" || kind == "document_meta")) {
                auto [it, inserted] = data.metadata_entities.emplace(id, semdl::core::EntityData{});
                if (inserted) {
                    it->second.kind = kind;
                }
                current_entity = nullptr;
                current_entity_id.clear();
                current_metadata = &it->second;
                current_fields = current_metadata;
            } else {
                auto [it, inserted] = data.entities.emplace(id, semdl::core::EntityData{});
                if (inserted) {
                    it->second.kind = kind;
                    increment_kind_count(data, kind);
                }
                current_entity = &it->second;
                current_entity_id = id;
                current_metadata = nullptr;
                current_fields = current_entity;
            }
            continue;
        }

        if (current_fields == nullptr) {
            continue;
        }

        const auto colon = trimmed.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(trimmed.substr(0, colon));
        const std::string value = trim_copy(trimmed.substr(colon + 1));
        current_fields->fields[nested_prefix + key] = value;
    }
}

}  // namespace

namespace semdl::core {

bool DocumentData::contains_entity_id(std::string_view id) const {
    return entities.contains(std::string(id));
}

const EntityData* DocumentData::find_entity(std::string_view id) const {
    const auto it = entities.find(std::string(id));
    return it == entities.end() ? nullptr : &it->second;
}

const EntityData* DocumentData::find_metadata(std::string_view id) const {
    const auto it = metadata_entities.find(std::string(id));
    return it == metadata_entities.end() ? nullptr : &it->second;
}

bool DocumentData::is_valid() const {
    return issues.empty();
}

DocumentData DocumentStore::load_document(const std::filesystem::path& input_file) const {
    DocumentData data;
    data.input_file = input_file;
    data.sidecar_file = derive_sidecar_path(input_file);
    data.has_sidecar = std::filesystem::exists(data.sidecar_file);

    parse_file(input_file, data, false);
    if (data.has_sidecar) {
        parse_file(data.sidecar_file, data, true);
    }

    if (data.document_count == 0) {
        data.issues.push_back("missing document block");
    } else if (data.document_count > 1) {
        data.issues.push_back("multiple document blocks");
    }

    return data;
}

DocumentSourceViews DocumentStore::load_document_source_views(const std::filesystem::path& input_file) const {
    DocumentSourceViews views;

    views.inline_source.input_file = input_file;
    views.inline_source.sidecar_file = derive_sidecar_path(input_file);
    views.inline_source.has_sidecar = std::filesystem::exists(views.inline_source.sidecar_file);
    parse_file(input_file, views.inline_source, false);
    if (views.inline_source.document_count == 0) {
        views.inline_source.issues.push_back("missing document block");
    } else if (views.inline_source.document_count > 1) {
        views.inline_source.issues.push_back("multiple document blocks");
    }

    views.sidecar_source.input_file = input_file;
    views.sidecar_source.sidecar_file = views.inline_source.sidecar_file;
    views.sidecar_source.has_sidecar = views.inline_source.has_sidecar;
    if (views.sidecar_source.has_sidecar) {
        parse_file(views.sidecar_source.sidecar_file, views.sidecar_source, true);
    }

    return views;
}

DocumentSummary DocumentStore::load_summary(const std::filesystem::path& input_file) const {
    const auto document = load_document(input_file);

    DocumentSummary summary;
    summary.primary_document_id = input_file.stem().string();
    summary.has_sidecar = document.has_sidecar;

    for (const auto& [entity_id, entity] : document.entities) {
        (void)entity;
        summary.entity_ids.push_back(entity_id);
    }

    return summary;
}

}  // namespace semdl::core
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace semdl::core {

struct EntityData {
    std::string kind;
    std::map<std::string, std::string> fields;
};

struct DocumentData {
    std::filesystem::path input_file;
    std::filesystem::path sidecar_file;
    bool has_sidecar = false;
    int document_count = 0;
    int resource_count = 0;
    int segment_count = 0;
    int assertion_count = 0;
    int hypothesis_count = 0;
    int alternative_count = 0;
    std::vector<std::string> issues;
    std::unordered_map<std::string, EntityData> entities;
    std::unordered_map<std::string, EntityData> metadata_entities;

    [[nodiscard]] bool contains_entity_id(std::string_view id) const;
    [[nodiscard]] const EntityData* find_entity(std::string_view id) const;
    [[nodiscard]] const EntityData* find_metadata(std::string_view id) const;
    [[nodiscard]] bool is_valid() const;
};

struct DocumentSummary {
    std::string primary_document_id;
    bool has_sidecar = false;
    std::vector<std::string> entity_ids;
};

class DocumentStore {
public:
    [[nodiscard]] DocumentData load_document(const std::filesystem::path& input_file) const;
    [[nodiscard]] DocumentSummary load_summary(const std::filesystem::path& input_file) const;
};

}  // namespace semdl::core
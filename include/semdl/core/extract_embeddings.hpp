#pragma once

#include "semdl/core/document_store.hpp"

#include <string>
#include <vector>

namespace semdl::core {

struct ExtractEmbeddingTarget {
    std::string id;
    std::string kind;
    std::string source_field;
    std::string source_text;
};

struct ExtractEmbeddingPlan {
    std::vector<ExtractEmbeddingTarget> targets;
    int skipped_count = 0;
};

struct GeneratedEmbeddingRecord {
    std::string id;
    std::string model;
    int dimensions = 0;
    std::string generated_at;
    std::string provider;
    std::string source_field;
    std::string vector;
};

[[nodiscard]] ExtractEmbeddingPlan build_extract_embedding_plan(const DocumentData& document);
[[nodiscard]] std::string render_extract_sidecar(const std::vector<GeneratedEmbeddingRecord>& records);

}  // namespace semdl::core
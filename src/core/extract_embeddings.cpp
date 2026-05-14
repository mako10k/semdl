#include "semdl/core/extract_embeddings.hpp"

#include <algorithm>
#include <sstream>

namespace semdl::core {

namespace {

std::string unquote_scalar(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

const std::string* canonical_source_value(const EntityData& entity, std::string& source_field) {
    const auto find_field = [&](const char* field_name) -> const std::string* {
        const auto it = entity.fields.find(field_name);
        if (it == entity.fields.end()) {
            return nullptr;
        }
        source_field = field_name;
        return &it->second;
    };

    if (entity.kind == "segment") {
        return find_field("text_quote");
    }
    if (entity.kind == "assertion") {
        return find_field("object");
    }
    if (entity.kind == "hypothesis") {
        return find_field("summary");
    }
    if (entity.kind == "alternative") {
        return find_field("label");
    }
    if (entity.kind == "resource") {
        return find_field("label");
    }
    return nullptr;
}

int kind_priority(std::string_view kind) {
    if (kind == "resource") {
        return 0;
    }
    if (kind == "segment") {
        return 1;
    }
    if (kind == "assertion") {
        return 2;
    }
    if (kind == "hypothesis") {
        return 3;
    }
    if (kind == "alternative") {
        return 4;
    }
    return 5;
}

}  // namespace

ExtractEmbeddingPlan build_extract_embedding_plan(const DocumentData& document) {
    ExtractEmbeddingPlan plan;
    for (const auto& [entity_id, entity] : document.entities) {
        std::string source_field;
        const auto* source_value = canonical_source_value(entity, source_field);
        if (source_value == nullptr) {
            ++plan.skipped_count;
            continue;
        }

        plan.targets.push_back(ExtractEmbeddingTarget{
            .id = entity_id,
            .kind = entity.kind,
            .source_field = source_field,
            .source_text = unquote_scalar(*source_value),
        });
    }

    std::sort(plan.targets.begin(), plan.targets.end(), [](const ExtractEmbeddingTarget& left, const ExtractEmbeddingTarget& right) {
        const int left_priority = kind_priority(left.kind);
        const int right_priority = kind_priority(right.kind);
        if (left_priority != right_priority) {
            return left_priority < right_priority;
        }
        return left.id < right.id;
    });

    return plan;
}

std::string render_extract_sidecar(const std::vector<GeneratedEmbeddingRecord>& records) {
    std::ostringstream output;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        output << "meta " << record.id << " {\n";
        output << "  embedding {\n";
        output << "    model: \"" << record.model << "\"\n";
        output << "    dimensions: " << record.dimensions << "\n";
        output << "    generated_at: \"" << record.generated_at << "\"\n";
        output << "    provider: \"" << record.provider << "\"\n";
        output << "    source_field: \"" << record.source_field << "\"\n";
        output << "    vector: \"" << record.vector << "\"\n";
        output << "  }\n";
        output << "}";
        output << (index + 1 < records.size() ? "\n\n" : "\n");
    }
    return output.str();
}

}  // namespace semdl::core
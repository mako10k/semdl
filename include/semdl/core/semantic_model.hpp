#pragma once

#include "semdl/core/document_store.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace semdl::core {

struct ExplainField {
    std::string name;
    std::string value;
};

struct ExplainView {
    std::string id;
    std::string kind;
    std::vector<ExplainField> fields;
};

enum class SelectorKind {
    id,
    type,
    path,
    meta,
    doc_self,
    unknown,
};

struct Selector {
    SelectorKind kind = SelectorKind::unknown;
    std::string raw;
    std::string entity_id;
    std::string field_path;
};

enum class ResolveError {
    none,
    invalid_selector_syntax,
    target_not_found,
    multiple_targets,
    wrong_layer,
};

struct ResolveResult {
    ResolveError error = ResolveError::none;
    std::string target_id;
    std::string target_kind;
    std::string field_path;
    int matched_count = 0;
};

[[nodiscard]] Selector parse_selector(std::string_view raw_selector);
[[nodiscard]] ResolveResult resolve_selector(const DocumentData& document, const Selector& selector);
[[nodiscard]] ExplainView build_explain_view(const DocumentData& document, std::string_view entity_id);

}  // namespace semdl::core
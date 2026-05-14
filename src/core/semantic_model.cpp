#include "semdl/core/semantic_model.hpp"

namespace semdl::core {

namespace {

bool contains_field(const EntityData& entity, const std::string& field_name) {
    return entity.fields.find(field_name) != entity.fields.end();
}

int count_entities_of_kind(const DocumentData& document, std::string_view kind, std::string* first_id) {
    int count = 0;
    for (const auto& [entity_id, entity] : document.entities) {
        if (entity.kind == kind) {
            ++count;
            if (count == 1 && first_id != nullptr) {
                *first_id = entity_id;
            }
        }
    }
    return count;
}

void push_field(ExplainView& view, const EntityData& entity, const std::string& field_name) {
    const auto it = entity.fields.find(field_name);
    if (it != entity.fields.end()) {
        view.fields.push_back(ExplainField{.name = field_name, .value = it->second});
    }
}

}  // namespace

Selector parse_selector(std::string_view raw_selector) {
    Selector selector;
    selector.raw = std::string(raw_selector);

    if (raw_selector == "doc:self") {
        selector.kind = SelectorKind::doc_self;
        return selector;
    }

    const auto colon = raw_selector.find(':');
    if (colon == std::string_view::npos) {
        return selector;
    }

    const std::string_view prefix = raw_selector.substr(0, colon);
    const std::string_view rest = raw_selector.substr(colon + 1);

    if (prefix == "id") {
        selector.kind = SelectorKind::id;
        selector.entity_id = std::string(rest);
        return selector;
    }

    if (prefix == "type") {
        selector.kind = SelectorKind::type;
        selector.entity_id = std::string(rest);
        return selector;
    }

    if (prefix == "path" || prefix == "meta") {
        const auto dot = rest.find('.');
        if (dot == std::string_view::npos) {
            selector.kind = prefix == "path" ? SelectorKind::path : SelectorKind::meta;
            return selector;
        }

        selector.kind = prefix == "path" ? SelectorKind::path : SelectorKind::meta;
        selector.entity_id = std::string(rest.substr(0, dot));
        selector.field_path = std::string(rest.substr(dot + 1));
    }

    return selector;
}

ResolveResult resolve_selector(const DocumentData& document, const Selector& selector) {
    ResolveResult result;

    if (selector.kind == SelectorKind::id) {
        const auto* entity = document.find_entity(selector.entity_id);
        if (entity == nullptr) {
            result.error = ResolveError::target_not_found;
            return result;
        }

        result.target_id = selector.entity_id;
        result.target_kind = entity->kind;
        result.matched_count = 1;
        return result;
    }

    if (selector.kind == SelectorKind::type) {
        std::string first_id;
        const int matched = count_entities_of_kind(document, selector.entity_id, &first_id);
        if (matched == 0) {
            result.error = ResolveError::target_not_found;
            return result;
        }
        if (matched > 1) {
            result.error = ResolveError::multiple_targets;
            result.matched_count = matched;
            result.target_kind = selector.entity_id;
            return result;
        }

        result.target_id = first_id;
        result.target_kind = selector.entity_id;
        result.matched_count = 1;
        return result;
    }

    if (selector.kind == SelectorKind::path) {
        if (selector.field_path.empty()) {
            result.error = ResolveError::invalid_selector_syntax;
            return result;
        }

        const auto* entity = document.find_entity(selector.entity_id);
        if (entity == nullptr) {
            result.error = ResolveError::target_not_found;
            return result;
        }

        if (!contains_field(*entity, selector.field_path)) {
            const auto* meta = document.find_metadata(selector.entity_id);
            if (meta != nullptr && contains_field(*meta, selector.field_path)) {
                result.error = ResolveError::wrong_layer;
                result.target_id = selector.entity_id;
                result.field_path = selector.field_path;
                return result;
            }

            result.error = ResolveError::target_not_found;
            return result;
        }

        result.target_id = selector.entity_id;
        result.target_kind = entity->kind;
        result.field_path = selector.field_path;
        result.matched_count = 1;
        return result;
    }

    if (selector.kind == SelectorKind::meta) {
        if (selector.field_path.empty()) {
            result.error = ResolveError::invalid_selector_syntax;
            return result;
        }

        const auto* meta = document.find_metadata(selector.entity_id);
        if (meta == nullptr) {
            const auto* entity = document.find_entity(selector.entity_id);
            if (entity != nullptr && contains_field(*entity, selector.field_path)) {
                result.error = ResolveError::wrong_layer;
                result.target_id = selector.entity_id;
                result.field_path = selector.field_path;
                return result;
            }

            result.error = ResolveError::target_not_found;
            return result;
        }

        if (!contains_field(*meta, selector.field_path)) {
            const auto* entity = document.find_entity(selector.entity_id);
            if (entity != nullptr && contains_field(*entity, selector.field_path)) {
                result.error = ResolveError::wrong_layer;
                result.target_id = selector.entity_id;
                result.field_path = selector.field_path;
                return result;
            }

            result.error = ResolveError::target_not_found;
            return result;
        }

        result.target_id = selector.entity_id;
        result.target_kind = meta->kind;
        result.field_path = selector.field_path;
        result.matched_count = 1;
        return result;
    }

    if (selector.kind == SelectorKind::doc_self) {
        result.target_id = "self";
        result.target_kind = "document";
        result.matched_count = 1;
        return result;
    }

    result.error = ResolveError::invalid_selector_syntax;
    return result;
}

ExplainView build_explain_view(const DocumentData& document, std::string_view entity_id) {
    ExplainView view;
    view.id = std::string(entity_id);

    const auto* entity = document.find_entity(entity_id);
    if (entity == nullptr) {
        return view;
    }

    view.kind = entity->kind;
    push_field(view, *entity, "subject");
    push_field(view, *entity, "predicate");
    push_field(view, *entity, "object");
    push_field(view, *entity, "label");
    push_field(view, *entity, "source_segment");
    push_field(view, *entity, "source_presence");

    if (const auto* metadata = document.find_metadata(entity_id); metadata != nullptr) {
        push_field(view, *metadata, "confidence");
        push_field(view, *metadata, "provenance_kind");
        push_field(view, *metadata, "rationale");
        push_field(view, *metadata, "embedding.model");
        push_field(view, *metadata, "embedding.dimensions");
        push_field(view, *metadata, "embedding.generated_at");
    }

    return view;
}

}  // namespace semdl::core
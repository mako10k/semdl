#include "semdl/core/sidecar_fields.hpp"

namespace semdl::core {

bool is_document_sidecar_field(std::string_view field_name) {
    return field_name == "version" || field_name == "generator";
}

bool is_entity_sidecar_field(std::string_view field_name) {
    return field_name == "confidence" || field_name == "provenance_kind" || field_name == "rationale" ||
           field_name == "caveat" || field_name == "todo" || field_name == "status" || field_name == "explanation" ||
           field_name.starts_with("embedding.");
}

}  // namespace semdl::core
#pragma once

#include <string_view>

namespace semdl::core {

[[nodiscard]] bool is_document_sidecar_field(std::string_view field_name);
[[nodiscard]] bool is_entity_sidecar_field(std::string_view field_name);

}  // namespace semdl::core
#pragma once

#include "semdl/core/document_store.hpp"

#include <string>

namespace semdl::core {

[[nodiscard]] std::string render_canonical_inline_document(const DocumentData& document);

}  // namespace semdl::core
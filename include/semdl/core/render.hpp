#pragma once

#include "semdl/core/document_store.hpp"

#include <string>
#include <vector>

namespace semdl::core {

struct SplitDocumentOutput {
	std::string inline_document;
	std::string sidecar_document;
	std::vector<std::string> kept_items;
	std::vector<std::string> moved_items;
	int moved_count = 0;
};

[[nodiscard]] std::string render_canonical_inline_document(const DocumentData& document);
[[nodiscard]] SplitDocumentOutput render_split_document(const DocumentData& document);

}  // namespace semdl::core
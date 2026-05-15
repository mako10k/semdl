#pragma once

#include "semdl/core/document_store.hpp"

#include <filesystem>
#include <string>

namespace semdl::core {

enum class SimilarityError {
    none,
    target_not_found,
    embedding_missing,
    model_mismatch,
    dimensions_mismatch,
    malformed_vector,
    unreadable_vector_ref,
    undefined_cosine_similarity,
};

struct SimilarityResult {
    SimilarityError error = SimilarityError::none;
    std::string left_id;
    std::string right_id;
    std::string model;
    std::string left_model;
    std::string right_model;
    int dimensions = 0;
    int left_dimensions = 0;
    int right_dimensions = 0;
    double score = 0.0;
    std::string target;
    std::string source;
    std::string vector_ref;
};

[[nodiscard]] SimilarityResult compare_pairwise_similarity(const DocumentData& document,
                                                           const std::filesystem::path& input_file,
                                                           std::string_view left_id,
                                                           std::string_view right_id);
[[nodiscard]] SimilarityResult compare_pairwise_similarity(const DocumentData& left_document,
                                                           const std::filesystem::path& left_input_file,
                                                           std::string_view left_id,
                                                           const DocumentData& right_document,
                                                           const std::filesystem::path& right_input_file,
                                                           std::string_view right_id);

}  // namespace semdl::core
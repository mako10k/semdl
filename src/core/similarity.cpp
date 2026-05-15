#include "semdl/core/similarity.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

namespace semdl::core {

namespace {

std::string trim_copy(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string unquote_scalar(std::string value) {
    value = trim_copy(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::optional<int> parse_int_field(const std::string& value) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return static_cast<int>(parsed);
}

std::optional<std::vector<double>> parse_vector_text(const std::string& raw_text) {
    const std::string trimmed = trim_copy(raw_text);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        return std::nullopt;
    }

    const std::string inner = trimmed.substr(1, trimmed.size() - 2);
    std::vector<double> values;
    std::stringstream stream(inner);
    std::string part;
    while (std::getline(stream, part, ',')) {
        const std::string token = trim_copy(part);
        if (token.empty()) {
            return std::nullopt;
        }
        char* end = nullptr;
        const double parsed = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            return std::nullopt;
        }
        values.push_back(parsed);
    }

    if (values.empty()) {
        return std::nullopt;
    }
    return values;
}

std::optional<std::string> read_text_file(const std::filesystem::path& file_path) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

struct ResolvedEmbedding {
    std::string model;
    int dimensions = 0;
    std::vector<double> vector;
};

std::optional<ResolvedEmbedding> resolve_embedding(const DocumentData& document,
                                                   const std::filesystem::path& input_file,
                                                   std::string_view target_id,
                                                   SimilarityResult& result) {
    const auto* entity = document.find_entity(target_id);
    if (entity == nullptr) {
        result.error = SimilarityError::target_not_found;
        result.target = std::string(target_id);
        return std::nullopt;
    }

    const auto* metadata = document.find_metadata(target_id);
    if (metadata == nullptr) {
        result.error = SimilarityError::embedding_missing;
        result.target = std::string(target_id);
        return std::nullopt;
    }

    const auto model_it = metadata->fields.find("embedding.model");
    const auto dimensions_it = metadata->fields.find("embedding.dimensions");
    if (model_it == metadata->fields.end() || dimensions_it == metadata->fields.end()) {
        result.error = SimilarityError::embedding_missing;
        result.target = std::string(target_id);
        return std::nullopt;
    }

    const auto parsed_dimensions = parse_int_field(dimensions_it->second);
    if (!parsed_dimensions.has_value()) {
        result.error = SimilarityError::malformed_vector;
        result.target = std::string(target_id);
        result.source = "inline vector";
        return std::nullopt;
    }

    std::string vector_text;
    if (const auto vector_it = metadata->fields.find("embedding.vector"); vector_it != metadata->fields.end()) {
        vector_text = unquote_scalar(vector_it->second);
        result.source = "inline vector";
    } else if (const auto vector_ref_it = metadata->fields.find("embedding.vector_ref"); vector_ref_it != metadata->fields.end()) {
        result.vector_ref = unquote_scalar(vector_ref_it->second);
        std::filesystem::path resolved = result.vector_ref;
        if (!resolved.is_absolute()) {
            const auto declaring_file = document.has_sidecar ? document.sidecar_file : input_file;
            const auto candidate = declaring_file.parent_path() / resolved;
            resolved = std::filesystem::exists(candidate) ? candidate : resolved;
        }
        const auto loaded = read_text_file(resolved);
        if (!loaded.has_value()) {
            result.error = SimilarityError::unreadable_vector_ref;
            result.target = std::string(target_id);
            return std::nullopt;
        }
        vector_text = trim_copy(*loaded);
        result.source = "vector_ref";
    } else {
        result.error = SimilarityError::embedding_missing;
        result.target = std::string(target_id);
        return std::nullopt;
    }

    auto parsed_vector = parse_vector_text(vector_text);
    if (!parsed_vector.has_value() || static_cast<int>(parsed_vector->size()) != *parsed_dimensions) {
        result.error = SimilarityError::malformed_vector;
        result.target = std::string(target_id);
        if (result.source.empty()) {
            result.source = "inline vector";
        }
        return std::nullopt;
    }

    return ResolvedEmbedding{
        .model = unquote_scalar(model_it->second),
        .dimensions = *parsed_dimensions,
        .vector = std::move(*parsed_vector),
    };
}

double vector_norm(const std::vector<double>& values) {
    double sum = 0.0;
    for (const double value : values) {
        sum += value * value;
    }
    return std::sqrt(sum);
}

double dot_product(const std::vector<double>& left, const std::vector<double>& right) {
    double sum = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        sum += left[index] * right[index];
    }
    return sum;
}

}  // namespace

SimilarityResult compare_pairwise_similarity(const DocumentData& document,
                                             const std::filesystem::path& input_file,
                                             std::string_view left_id,
                                             std::string_view right_id) {
    return compare_pairwise_similarity(document, input_file, left_id, document, input_file, right_id);
}

SimilarityResult compare_pairwise_similarity(const DocumentData& left_document,
                                             const std::filesystem::path& left_input_file,
                                             std::string_view left_id,
                                             const DocumentData& right_document,
                                             const std::filesystem::path& right_input_file,
                                             std::string_view right_id) {
    SimilarityResult result;
    result.left_id = std::string(left_id);
    result.right_id = std::string(right_id);

    auto left = resolve_embedding(left_document, left_input_file, left_id, result);
    if (!left.has_value()) {
        return result;
    }

    result.source.clear();
    result.vector_ref.clear();
    auto right = resolve_embedding(right_document, right_input_file, right_id, result);
    if (!right.has_value()) {
        return result;
    }

    result.left_model = left->model;
    result.right_model = right->model;
    result.left_dimensions = left->dimensions;
    result.right_dimensions = right->dimensions;

    if (left->model != right->model) {
        result.error = SimilarityError::model_mismatch;
        return result;
    }
    if (left->dimensions != right->dimensions) {
        result.error = SimilarityError::dimensions_mismatch;
        return result;
    }

    const double left_norm = vector_norm(left->vector);
    if (left_norm == 0.0) {
        result.error = SimilarityError::undefined_cosine_similarity;
        result.target = result.left_id;
        return result;
    }

    const double right_norm = vector_norm(right->vector);
    if (right_norm == 0.0) {
        result.error = SimilarityError::undefined_cosine_similarity;
        result.target = result.right_id;
        return result;
    }

    result.model = left->model;
    result.dimensions = left->dimensions;
    result.score = dot_product(left->vector, right->vector) / (left_norm * right_norm);
    return result;
}

}  // namespace semdl::core
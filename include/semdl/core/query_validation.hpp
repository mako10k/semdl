#pragma once

#include "semdl/core/similarity.hpp"

#include "semdl/core/document_store.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace semdl::core {

struct QueryValidationIssue {
    bool valid = true;
    std::string clause = "where";
    std::string expression;
    std::string reason;
};

[[nodiscard]] QueryValidationIssue validate_initial_ssq_profile(const std::filesystem::path& query_file);

struct SearchQuery {
    std::string select;
    std::optional<std::string> where_expression;
    std::optional<std::string> similar_expression;
    std::string result_mode = "matches";
};

struct SearchMatch {
    std::string file;
    std::string id;
    std::string kind;
    std::optional<double> score;
};

struct SearchSubgraph {
    SearchMatch match;
    std::vector<SearchMatch> context_nodes;
};

struct SearchResult {
    SearchQuery query;
    std::vector<SearchMatch> matches;
    std::vector<SearchSubgraph> subgraphs;
    std::string anchor_id;
    std::string metric;
    std::string model;
    int dimensions = 0;
    SimilarityResult similarity_failure;
};

[[nodiscard]] SearchQuery parse_initial_search_query(const std::filesystem::path& query_file);
[[nodiscard]] SearchResult execute_initial_search_query(const SearchQuery& query,
                                                        const std::vector<std::filesystem::path>& input_files,
                                                        const DocumentStore& store);

}  // namespace semdl::core
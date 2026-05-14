#pragma once

#include <filesystem>
#include <string>

namespace semdl::core {

struct QueryValidationIssue {
    bool valid = true;
    std::string clause = "where";
    std::string expression;
    std::string reason;
};

[[nodiscard]] QueryValidationIssue validate_initial_ssq_profile(const std::filesystem::path& query_file);

}  // namespace semdl::core
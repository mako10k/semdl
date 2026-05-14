#pragma once

#include "semdl/runner/manifest.hpp"

#include <filesystem>
#include <string>

namespace semdl::runner {

struct ExecutionResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

struct CaseResult {
    std::string id;
    bool passed = false;
    std::string failure_reason;
};

[[nodiscard]] ExecutionResult execute_case(const std::filesystem::path& repo_root, const TestCaseSpec& test_case);
[[nodiscard]] CaseResult compare_case_result(const std::filesystem::path& repo_root, const TestCaseSpec& test_case, const ExecutionResult& execution);

}  // namespace semdl::runner
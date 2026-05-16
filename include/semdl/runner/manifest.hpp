#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "semdl/runner/discovery.hpp"

namespace semdl::runner {

struct TestCaseSpec {
    std::string id;
    std::string command;
    std::vector<std::string> argv;
    std::filesystem::path cwd;
    std::string stdin_text;
    std::map<std::string, std::string> environment;
    std::vector<std::filesystem::path> setup_files;
    std::map<std::filesystem::path, std::filesystem::path> expected_files;
    std::vector<std::filesystem::path> expected_absent_files;
    int expected_exit = 0;
    std::filesystem::path expected_stdout;
    std::filesystem::path expected_stderr;
    std::string expected_output_kind;
    std::string notes;
};

struct TestManifest {
    std::string name;
    std::filesystem::path manifest_path;
    std::vector<TestCaseSpec> cases;
};

[[nodiscard]] TestManifest load_manifest(const TestCaseManifest& manifest, const std::filesystem::path& repo_root);

}  // namespace semdl::runner
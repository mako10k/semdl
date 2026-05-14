#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace semdl::runner {

struct TestCaseManifest {
    std::string name;
    std::filesystem::path manifest_path;
};

[[nodiscard]] std::vector<TestCaseManifest> discover_default_manifests(const std::filesystem::path& repo_root);

}  // namespace semdl::runner
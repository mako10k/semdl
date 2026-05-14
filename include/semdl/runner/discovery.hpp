#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace semdl::runner {

struct TestCaseManifest {
    std::string name;
    std::filesystem::path manifest_path;
};

struct DiscoveryResult {
    std::filesystem::path repo_root;
    std::vector<TestCaseManifest> manifests;
};

[[nodiscard]] std::vector<TestCaseManifest> discover_default_manifests(const std::filesystem::path& repo_root);
[[nodiscard]] DiscoveryResult discover_with_repo_root(const std::filesystem::path& candidate_root);

}  // namespace semdl::runner
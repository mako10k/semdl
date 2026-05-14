#include "semdl/runner/discovery.hpp"

namespace semdl::runner {

std::vector<TestCaseManifest> discover_default_manifests(const std::filesystem::path& repo_root) {
    return {
        TestCaseManifest{
            .name = "cli-success",
            .manifest_path = repo_root / "docs/examples/testcases/cli-success.json",
        },
        TestCaseManifest{
            .name = "cli-failure",
            .manifest_path = repo_root / "docs/examples/testcases/cli-failure.json",
        },
    };
}

DiscoveryResult discover_with_repo_root(const std::filesystem::path& candidate_root) {
    return DiscoveryResult{
        .repo_root = candidate_root,
        .manifests = discover_default_manifests(candidate_root),
    };
}

}  // namespace semdl::runner
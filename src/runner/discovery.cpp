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

}  // namespace semdl::runner
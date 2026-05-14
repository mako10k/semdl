#include "semdl/runner/executor.hpp"
#include "semdl/runner/discovery.hpp"
#include "semdl/runner/manifest.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const std::filesystem::path repo_root = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(SEMDL_REPO_ROOT);
    const auto discovery = semdl::runner::discover_with_repo_root(repo_root);

    std::size_t passed = 0;
    std::size_t total = 0;

    for (const auto& manifest_ref : discovery.manifests) {
        const auto manifest = semdl::runner::load_manifest(manifest_ref, discovery.repo_root);
        for (const auto& test_case : manifest.cases) {
            ++total;
            const auto execution = semdl::runner::execute_case(discovery.repo_root, test_case);
            const auto result = semdl::runner::compare_case_result(discovery.repo_root, test_case, execution);
            if (result.passed) {
                ++passed;
            } else {
                std::cerr << "FAIL " << result.id << ": " << result.failure_reason << "\n";
            }
        }
    }

    std::cout << "runner: passed " << passed << "/" << total << " cases\n";
    return passed == total ? 0 : 1;
}
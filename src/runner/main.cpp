#include "semdl/runner/discovery.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const std::filesystem::path repo_root = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(SEMDL_REPO_ROOT);
    const auto manifests = semdl::runner::discover_default_manifests(repo_root);
    std::cout << "runner skeleton: discovered " << manifests.size() << " manifests\n";
    return 0;
}
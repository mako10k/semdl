#include "semdl/cli/cli_app.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);

    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    const semdl::cli::CliApp app;
    const auto result = app.run(args);

    if (!result.stdout_text.empty()) {
        std::cout << result.stdout_text;
    }

    if (!result.stderr_text.empty()) {
        std::cerr << result.stderr_text;
    }

    return result.exit_code;
}
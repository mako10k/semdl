#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace semdl::cli {

struct CommandResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

class CliApp {
public:
    [[nodiscard]] CommandResult run(const std::vector<std::string_view>& args) const;
};

}  // namespace semdl::cli
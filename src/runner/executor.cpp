#include "semdl/runner/executor.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace semdl::runner {
namespace {

std::string read_file_or_empty(const std::filesystem::path& file_path) {
    if (file_path.empty()) {
        return "";
    }

    std::ifstream input(file_path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string read_required_file(const std::filesystem::path& file_path) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("missing file: " + file_path.generic_string());
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string read_from_fd(int fd) {
    std::array<char, 4096> buffer{};
    std::string data;
    while (true) {
        const ssize_t count = ::read(fd, buffer.data(), buffer.size());
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("failed to read pipe");
        }
        data.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return data;
}

std::string expected_output_text(const std::string& kind, const std::filesystem::path& file_path) {
    if (kind == "empty") {
        return "";
    }
    if (kind == "exact" || kind == "fixture-file") {
        return read_file_or_empty(file_path);
    }

    throw std::runtime_error("unsupported expected_output_kind: " + kind);
}

std::filesystem::path resolve_program(const std::filesystem::path& repo_root, const std::vector<std::string>& argv) {
    if (!argv.empty() && argv[0] == "ssd") {
        return std::filesystem::absolute(repo_root / "build/ssd");
    }
    return argv.empty() ? std::filesystem::path{} : std::filesystem::absolute(std::filesystem::path(argv[0]));
}

std::filesystem::path create_sandbox_root() {
    std::string templ = (std::filesystem::temp_directory_path() / "semdl-runner-XXXXXX").string();
    std::vector<char> buffer(templ.begin(), templ.end());
    buffer.push_back('\0');
    char* created = ::mkdtemp(buffer.data());
    if (created == nullptr) {
        throw std::runtime_error("failed to create sandbox directory");
    }
    return std::filesystem::path(created);
}

void copy_file_into_sandbox(const std::filesystem::path& repo_root,
                            const std::filesystem::path& sandbox_root,
                            const std::filesystem::path& relative_path) {
    const auto source = repo_root / relative_path;
    const auto destination = sandbox_root / relative_path;
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
}

bool case_uses_sandbox(const TestCaseSpec& test_case) {
    return !test_case.setup_files.empty() || !test_case.expected_files.empty() || !test_case.expected_absent_files.empty();
}

}  // namespace

ExecutionResult execute_case(const std::filesystem::path& repo_root, const TestCaseSpec& test_case) {
    const auto absolute_repo_root = std::filesystem::absolute(repo_root);
    int stdout_pipe[2];
    int stderr_pipe[2];
    int stdin_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0 || ::pipe(stdin_pipe) != 0) {
        throw std::runtime_error("failed to create pipes");
    }

    ExecutionResult result;
    std::filesystem::path execution_cwd = test_case.cwd;
    if (case_uses_sandbox(test_case)) {
        result.sandbox_root = create_sandbox_root();
        for (const auto& relative_path : test_case.setup_files) {
            copy_file_into_sandbox(absolute_repo_root, result.sandbox_root, relative_path);
        }
        const auto relative_cwd = std::filesystem::relative(std::filesystem::absolute(test_case.cwd), absolute_repo_root);
        execution_cwd = result.sandbox_root / relative_cwd;
        std::filesystem::create_directories(execution_cwd);
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error("failed to fork");
    }

    if (pid == 0) {
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::close(stdin_pipe[1]);

        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);

        ::close(stdin_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        if (!execution_cwd.empty() && ::chdir(execution_cwd.c_str()) != 0) {
            _exit(126);
        }

        for (const auto& [key, value] : test_case.environment) {
            ::setenv(key.c_str(), value.c_str(), 1);
        }

        const auto program = resolve_program(absolute_repo_root, test_case.argv);
        std::vector<std::string> argv_storage = test_case.argv;
        if (!argv_storage.empty()) {
            argv_storage[0] = program.string();
        }

        std::vector<char*> exec_argv;
        exec_argv.reserve(argv_storage.size() + 1);
        for (auto& arg : argv_storage) {
            exec_argv.push_back(arg.data());
        }
        exec_argv.push_back(nullptr);

        ::execv(program.c_str(), exec_argv.data());
        _exit(127);
    }

    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);
    ::close(stdin_pipe[0]);

    if (!test_case.stdin_text.empty()) {
        (void)::write(stdin_pipe[1], test_case.stdin_text.data(), test_case.stdin_text.size());
    }
    ::close(stdin_pipe[1]);

    result.stdout_text = read_from_fd(stdout_pipe[0]);
    result.stderr_text = read_from_fd(stderr_pipe[0]);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = 128;
    }

    return result;
}

CaseResult compare_case_result(const std::filesystem::path& repo_root, const TestCaseSpec& test_case, const ExecutionResult& execution) {
    CaseResult result{.id = test_case.id, .passed = true};

    if (execution.exit_code != test_case.expected_exit) {
        result.passed = false;
        result.failure_reason = "exit code mismatch: expected " + std::to_string(test_case.expected_exit) +
                                " actual " + std::to_string(execution.exit_code);
        return result;
    }

    const std::string expected_stdout = expected_output_text(test_case.expected_output_kind, test_case.expected_stdout);
    const std::string expected_stderr = test_case.expected_stderr.empty() ? std::string{} : expected_output_text(test_case.expected_output_kind, test_case.expected_stderr);
    (void)repo_root;

    if (execution.stdout_text != expected_stdout) {
        result.passed = false;
        result.failure_reason = "stdout mismatch";
        return result;
    }

    if (execution.stderr_text != expected_stderr) {
        result.passed = false;
        result.failure_reason = "stderr mismatch";
        return result;
    }

    for (const auto& [runtime_path, expected_fixture] : test_case.expected_files) {
        if (execution.sandbox_root.empty()) {
            result.passed = false;
            result.failure_reason = "sandbox missing for expected_files";
            return result;
        }

        const auto actual_path = execution.sandbox_root / runtime_path;
        try {
            const auto actual_text = read_required_file(actual_path);
            const auto expected_text = read_required_file(expected_fixture);
            if (actual_text != expected_text) {
                result.passed = false;
                result.failure_reason = "file output mismatch: " + runtime_path.generic_string();
                return result;
            }
        } catch (const std::runtime_error&) {
            result.passed = false;
            result.failure_reason = "file output missing: " + runtime_path.generic_string();
            return result;
        }
    }

    for (const auto& runtime_path : test_case.expected_absent_files) {
        if (execution.sandbox_root.empty()) {
            result.passed = false;
            result.failure_reason = "sandbox missing for expected_absent_files";
            return result;
        }

        const auto actual_path = execution.sandbox_root / runtime_path;
        if (std::filesystem::exists(actual_path)) {
            result.passed = false;
            result.failure_reason = "file should be absent: " + runtime_path.generic_string();
            return result;
        }
    }

    return result;
}

}  // namespace semdl::runner
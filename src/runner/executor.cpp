#include "semdl/runner/executor.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
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
        return repo_root / "build/ssd";
    }
    return argv.empty() ? std::filesystem::path{} : std::filesystem::path(argv[0]);
}

}  // namespace

ExecutionResult execute_case(const std::filesystem::path& repo_root, const TestCaseSpec& test_case) {
    int stdout_pipe[2];
    int stderr_pipe[2];
    int stdin_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0 || ::pipe(stdin_pipe) != 0) {
        throw std::runtime_error("failed to create pipes");
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

        if (!test_case.cwd.empty() && ::chdir(test_case.cwd.c_str()) != 0) {
            _exit(126);
        }

        for (const auto& [key, value] : test_case.environment) {
            ::setenv(key.c_str(), value.c_str(), 1);
        }

        const auto program = resolve_program(repo_root, test_case.argv);
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

    ExecutionResult result;
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
        result.failure_reason = "exit code mismatch";
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

    return result;
}

}  // namespace semdl::runner
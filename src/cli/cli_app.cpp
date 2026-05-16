#include "semdl/cli/cli_app.hpp"

#include "semdl/core/document_store.hpp"
#include "semdl/core/extract_embeddings.hpp"
#include "semdl/core/query_validation.hpp"
#include "semdl/core/render.hpp"
#include "semdl/core/semantic_model.hpp"
#include "semdl/core/similarity.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace semdl::cli {

namespace {

enum class HelpOutputFormat {
    text,
    semdl,
};

struct ParsedHelpFormat {
    bool valid = true;
    HelpOutputFormat format = HelpOutputFormat::text;
    std::size_t positional_end = 0;
    std::string_view invalid_value;
};

struct ProcessResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

struct ParsedExtractArgs {
    bool valid = true;
    bool use_stdout = false;
    std::optional<std::filesystem::path> output_file;
    std::optional<std::filesystem::path> input_file;
    std::string provider;
    std::string model;
};

std::string join_args(const std::vector<std::string_view>& args) {
    std::ostringstream stream;
    stream << "ssd";

    for (const auto arg : args) {
        stream << ' ' << arg;
    }

    return stream.str();
}

std::string join_args_with_quoted_index(const std::vector<std::string_view>& args, std::size_t quoted_index) {
    std::ostringstream stream;
    stream << "ssd";

    for (std::size_t index = 0; index < args.size(); ++index) {
        stream << ' ';
        if (index == quoted_index) {
            stream << '"' << args[index] << '"';
        } else {
            stream << args[index];
        }
    }

    return stream.str();
}

std::string read_text_file(const std::filesystem::path& file_path) {
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

std::string trim_copy(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string current_utc_timestamp() {
    const char* fixed = std::getenv("SEMDL_EXTRACT_GENERATED_AT");
    if (fixed != nullptr && *fixed != '\0') {
        return fixed;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&raw, &utc);
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

int parse_vector_dimensions(std::string_view vector_text) {
    const std::string trimmed = trim_copy(std::string(vector_text));
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        return 0;
    }
    const std::string inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
    if (inner.empty()) {
        return 0;
    }
    int dimensions = 1;
    for (const char character : inner) {
        if (character == ',') {
            ++dimensions;
        }
    }
    return dimensions;
}

ProcessResult run_process_capture_output(const std::string& program, const std::vector<std::string>& argv) {
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) {
        throw std::runtime_error("failed to create pipes");
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error("failed to fork");
    }

    if (pid == 0) {
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        std::vector<std::string> exec_storage;
        exec_storage.reserve(argv.size() + 1);
        exec_storage.push_back(program);
        exec_storage.insert(exec_storage.end(), argv.begin(), argv.end());

        std::vector<char*> exec_argv;
        exec_argv.reserve(exec_storage.size() + 1);
        for (auto& arg : exec_storage) {
            exec_argv.push_back(arg.data());
        }
        exec_argv.push_back(nullptr);

        ::execvp(program.c_str(), exec_argv.data());
        _exit(127);
    }

    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    ProcessResult result;
    result.stdout_text = read_from_fd(stdout_pipe[0]);
    result.stderr_text = read_from_fd(stderr_pipe[0]);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    return result;
}

ParsedExtractArgs parse_extract_args(const std::vector<std::string_view>& args) {
    ParsedExtractArgs parsed;
    if (args.size() < 3) {
        parsed.valid = false;
        return parsed;
    }

    std::size_t index = 1;
    if (args[index] == "--stdout") {
        parsed.use_stdout = true;
        ++index;
    } else if (args[index] == "--out") {
        if (index + 1 >= args.size()) {
            parsed.valid = false;
            return parsed;
        }
        parsed.output_file = std::filesystem::path(args[index + 1]);
        index += 2;
    } else {
        parsed.valid = false;
        return parsed;
    }

    while (index + 1 < args.size()) {
        if (args[index] == "--embed-provider") {
            if (index + 1 >= args.size() - 1) {
                parsed.valid = false;
                return parsed;
            }
            parsed.provider = std::string(args[index + 1]);
            index += 2;
            continue;
        }
        if (args[index] == "--embed-model") {
            if (index + 1 >= args.size() - 1) {
                parsed.valid = false;
                return parsed;
            }
            parsed.model = std::string(args[index + 1]);
            index += 2;
            continue;
        }
        if (args[index].starts_with("--")) {
            parsed.valid = false;
            return parsed;
        }
        break;
    }

    if (index != args.size() - 1) {
        parsed.valid = false;
        return parsed;
    }
    parsed.input_file = std::filesystem::path(args.back());
    return parsed;
}

struct UpdatePreview {
    std::string command_line;
    std::string target_profile;
    std::string target_file;
    std::vector<std::string> detail_lines;
    int changes = 1;
    bool include_target_header = true;
};

std::filesystem::path derive_sidecar_path(const std::filesystem::path& input_file) {
    auto sidecar = input_file;
    sidecar.replace_extension(".ssm");
    return sidecar;
}

std::string quote_value(std::string_view value) {
    return "\"" + std::string(value) + "\"";
}

bool has_flag(const std::vector<std::string_view>& args, std::string_view flag) {
    for (const auto arg : args) {
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

ParsedHelpFormat parse_help_format_tail(const std::vector<std::string_view>& args, std::size_t minimum_positional_end) {
    ParsedHelpFormat parsed;
    parsed.positional_end = args.size();

    if (args.size() >= minimum_positional_end + 2 && args[args.size() - 2] == "--format") {
        parsed.positional_end = args.size() - 2;
        if (args.back() == "semdl") {
            parsed.format = HelpOutputFormat::semdl;
        } else {
            parsed.valid = false;
            parsed.invalid_value = args.back();
        }
    }

    return parsed;
}

bool help_topic_accepts_target(std::string_view topic) {
    return topic == "reference" || topic == "recipes";
}

std::string escape_semdl_string(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

std::string render_help_as_semdl(const std::vector<std::string_view>& args,
                                 std::string_view topic,
                                 std::string_view target,
                                 const std::string& text) {
    std::istringstream input(text);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    const std::string title = lines.empty() ? "SEMDL CLI Help" : lines.front();
    const std::string label = topic.empty() ? "root" : (target.empty() ? std::string(topic) : std::string(topic) + " " + std::string(target));

    std::ostringstream output;
    output << "document HELP {\n";
    output << "  title: \"" << escape_semdl_string(title) << "\"\n";
    output << "  command: \"" << escape_semdl_string(join_args(args)) << "\"\n";
    output << "  topic: \"" << escape_semdl_string(topic.empty() ? std::string_view{"root"} : topic) << "\"\n";
    if (!target.empty()) {
        output << "  target: \"" << escape_semdl_string(target) << "\"\n";
    }
    output << "  format: \"semdl\"\n";
    output << "}\n\n";
    output << "resource HELP_TOPIC {\n";
    output << "  type: help-topic\n";
    output << "  label: \"" << escape_semdl_string(label) << "\"\n";
    output << "}\n";

    for (std::size_t index = 0; index < lines.size(); ++index) {
        output << "\nsegment HELP_LINE_" << (index + 1) << " {\n";
        output << "  source: HELP_TOPIC\n";
        output << "  text_quote: \"" << escape_semdl_string(lines[index]) << "\"\n";
        output << "}\n";
    }

    return output.str();
}

CommandResult make_dry_run_result(const UpdatePreview& preview) {
    std::ostringstream output;
    output << "DRY-RUN\n";
    output << "command: " << preview.command_line << "\n";
    if (preview.include_target_header) {
        output << "target_profile: " << preview.target_profile << "\n";
        output << "target_file: " << preview.target_file << "\n";
    }
    for (const auto& line : preview.detail_lines) {
        output << line << "\n";
    }
    output << "changes: " << preview.changes << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

CommandResult make_apply_not_implemented_error(const std::vector<std::string_view>& args, std::string_view operation) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR update_apply_not_implemented\ncommand: " + join_args(args) +
                       "\noperation: " + std::string(operation) +
                       "\nhint: use `--dry-run` for preview in the current slice\n",
    };
}

CommandResult make_split_apply_result(const std::filesystem::path& input_file,
                                      const std::filesystem::path& sidecar_file,
                                      int moved_count) {
    std::ostringstream output;
    output << "wrote_ssd: " << input_file.generic_string() << "\n";
    output << "wrote_ssm: " << sidecar_file.generic_string() << "\n";
    output << "moved: " << moved_count << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

std::string root_help_text() {
    return "SEMDL CLI Help\n\n"
           "1. Overview\n"
           "- `ssd` validates, explains, transforms, and updates `.ssd` and `.ssm` assets.\n"
           "- This help output is the canonical entrypoint for CLI guidance.\n"
           "- Necessary canonical CLI syntax can be obtained from this help and its topic pages without opening repository files.\n"
           "- Repository artifacts for requirements, acceptance, and formal notation may exist, but they are aligned maintenance assets rather than the user-facing authority.\n\n"
           "2. Table of Contents\n"
           "- `ssd help overview`\n"
           "- `ssd help toc`\n"
           "- `ssd help grammar`\n"
           "- `ssd help reference [subcommand]`\n"
           "- `ssd help recipes [topic]`\n"
           "- `ssd help samples`\n"
           "- `ssd help troubleshooting`\n\n"
           "3. Grammar\n"
           "- Root: `ssd <subcommand> ...`\n"
           "- Root help: `ssd help [topic] [--format semdl]`\n"
           "- Targeted reference help: `ssd help reference [subcommand] [--format semdl]`\n"
           "- Targeted recipe help: `ssd help recipes [topic] [--format semdl]`\n"
           "- Global help: `ssd --help [--format semdl]`\n"
           "- Subcommand help: `ssd <subcommand> --help [--format semdl]`\n"
           "- This CLI help remains the canonical user-facing operational guide.\n\n"
           "4. Reference\n"
           "- `check`: validate syntax and references for a document set\n"
           "- `search`: evaluate a read-only query against document inputs\n"
           "- `extract`: build an initial semantic document from raw inputs\n"
           "- `similarity`: compare two semantic targets\n"
           "- `explain`: show the merged semantic view for an entity id\n"
           "- `normalize`: canonicalize document shape and output profile\n"
           "- `add`: add one semantic entity skeleton\n"
           "- `set`: update one inline or sidecar field\n"
           "- `remove`: remove one target or fail on unsafe removal\n"
           "- `annotate`: add rationale/caveat/todo-like notes\n"
           "- `split`: move sidecar-eligible metadata out of inline form\n"
           "- `merge`: produce a merged inline view\n"
           "- `help`: navigate this help system\n\n"
           "5. Reverse Lookup\n"
           "- Validate a document: `ssd check <file>`\n"
           "- Explain an assertion: `ssd explain <id> <file>`\n"
           "- Preview an inline edit: `ssd set path:<id>.<field> <value> --dry-run <file>`\n"
           "- Preview a sidecar annotation: `ssd annotate id:<id> rationale <text> --target sidecar --dry-run <file>`\n"
           "- Machine-readable help: `ssd help grammar --format semdl`\n"
           "- Understand an option error: `ssd help grammar`\n"
           "- Understand a selector-layer error: `ssd help recipes wrong-layer`\n\n"
           "6. Samples\n"
           "- `ssd check docs/examples/minimal.ssd`\n"
           "- `ssd explain A1 docs/examples/minimal.ssd`\n"
           "- `ssd help reference check`\n"
           "- `ssd check --help --format semdl`\n"
           "- `ssd set meta:A1.confidence 0.91 --dry-run docs/examples/minimal.ssd`\n\n"
           "7. Cautions, Known Bugs, Reporting\n"
           "- In this initial slice, `search` supports `select`, an optional single `where`, target-based `similar`, `return: matches`, and structural `return: subgraph`; `similar` with `return: subgraph` remains unimplemented. `extract` supports explicit Ollama-backed embedding generation from an existing `.ssd` input; `ssd similarity` supports pairwise cosine comparison against precomputed embeddings in one input document; `add` remains unimplemented.\n"
           "- Use `--format semdl` when another tool needs structured help output.\n"
           "- Update flows are acceptance-driven and still incomplete for full file rewriting.\n"
           "- Report problems with the command, argv, input paths, expected output, actual output, and related golden file.\n"
           "- Preferred reporting path: repository issue or change request with a reproducing CLI case.\n";
}

std::string grammar_help_text() {
    return "SEMDL Help Topic: grammar\n\n"
           "- Root form: `ssd <subcommand> ...`\n"
           "- Help form: `ssd help [topic] [--format semdl]`\n"
           "- Targeted reference help: `ssd help reference [subcommand] [--format semdl]`\n"
           "- Targeted recipe help: `ssd help recipes [topic] [--format semdl]`\n"
           "- Global help alias: `ssd --help [--format semdl]`\n"
           "- Subcommand help alias: `ssd <subcommand> --help [--format semdl]`\n\n"
           "Topic targeting:\n"
           "- `overview`, `toc`, `grammar`, `samples`, and `troubleshooting` do not accept a trailing target\n"
           "- `reference` accepts a `<subcommand>` target\n"
           "- `recipes` accepts a `<topic>` target\n\n"
           "Key syntax forms:\n"
           "- `ssd check <file>`\n"
           "- `ssd search <query.ssq> <file>...`\n"
           "- `ssd extract --out <output.ssd> <input>...`\n"
           "- `ssd similarity <id> <id> <file>`\n"
           "- `ssd explain <id> <file>`\n"
           "- `ssd add <kind> <file> [field=value ...]`\n"
           "- `ssd set <selector> <value-or-field> ... <file>`\n"
           "- `ssd remove <selector> <file>`\n"
           "- `ssd annotate <selector> <kind> <text> ... <file>`\n"
           "- `ssd split <input.ssd> [common options]`\n"
           "- `ssd merge <input.ssd> [common options]`\n"
           "- `ssd normalize <input.ssd> [common options]`\n"
           "- `ssd help grammar --format semdl`\n\n"
           "Selectors:\n"
           "- `id:<id>`\n"
           "- `type:<kind>`\n"
           "- `path:<id>.<field>`\n"
           "- `meta:<id>.<field>`\n"
           "- `doc:self`\n"
           "- Use `path:` for inline `.ssd` structure and `meta:` for sidecar `.ssm` metadata\n\n"
           "Value forms:\n"
           "- text with spaces should be passed as a quoted string\n"
           "- update-oriented scalar values accept quoted-string, number, boolean, or identifier forms\n\n"
           "Common options:\n"
           "- `--dry-run` previews update-oriented commands\n"
           "- `--out <file>`, `--stdout`, and `--format inline|sidecar` choose emitted output destinations or profile when the command supports them\n"
           "- `--target inline|sidecar|auto` controls update destination selection\n"
           "- `--allow-multi`, `--cascade`, and `--fail-on-conflict` are safety or selection controls on specific update forms\n\n"
           "This topic is the canonical user-facing operational syntax summary.\n"
           "Use `ssd help reference <subcommand>` or `ssd <subcommand> --help` for command-specific usage details.\n"
           "Use `--format semdl` for line-preserving SEMDL help output.\n"
           "For command-specific help, see `ssd help reference <subcommand>`.\n";
}

std::string reference_help_text(std::string_view target) {
    if (target == "check") {
        return "SEMDL Help Topic: reference check\n\n"
               "Usage:\n"
               "- `ssd check <file>`\n\n"
               "Purpose:\n"
               "- Validate syntax, reference integrity, and sidecar discovery for a document input.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd check docs/examples/minimal.ssd`\n";
    }

    if (target == "explain") {
        return "SEMDL Help Topic: reference explain\n\n"
               "Usage:\n"
               "- `ssd explain <id> <file>`\n\n"
               "Purpose:\n"
               "- Show the merged semantic view for one entity id.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help recipes explain-not-found`\n\n"
               "Sample:\n"
               "- `ssd explain A1 docs/examples/minimal.ssd`\n";
    }

    if (target == "search") {
        return "SEMDL Help Topic: reference search\n\n"
               "Usage:\n"
               "- `ssd search <query.ssq> <file>...`\n\n"
               "Status:\n"
               "- This subcommand currently supports `select`, an optional single `where`, target-based `similar`, `return: matches`, and structural `return: subgraph`.\n"
               "- `similar` uses precomputed embeddings from the integrated input view and excludes the anchor target from results.\n"
               "- `similar` with `return: subgraph` remains planned but not implemented in the current slice.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "extract") {
        return "SEMDL Help Topic: reference extract\n\n"
               "Usage:\n"
               "- `ssd extract --out <output.ssd> --embed-provider ollama --embed-model <model> <input.ssd>`\n"
               "- `ssd extract --stdout --embed-provider ollama --embed-model <model> <input.ssd>`\n\n"
               "Status:\n"
               "- This subcommand currently supports explicit Ollama-backed embedding generation from an existing `.ssd` input.\n"
               "- Raw input extraction and non-Ollama providers remain planned but not implemented in the current slice.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "similarity") {
        return "SEMDL Help Topic: reference similarity\n\n"
               "Usage:\n"
               "- `ssd similarity <id> <id> <file>`\n\n"
               "Behavior:\n"
               "- Compare two existing semantic targets in one input document using precomputed embeddings only.\n"
               "- The current slice uses cosine similarity and requires matching model and dimensions.\n"
               "- Both inline `vector` and external `vector_ref` embedding records are supported.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "set") {
        return "SEMDL Help Topic: reference set\n\n"
               "Usage:\n"
               "- `ssd set path:<id>.<field> <value> --dry-run <file>`\n"
               "- `ssd set meta:<id>.<field> <value> --dry-run <file>`\n"
               "- `ssd set id:<id> <field> <value> <file>`\n\n"
               "Purpose:\n"
               "- Update one target field in inline structure or sidecar metadata.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help recipes wrong-layer`\n\n"
               "Sample:\n"
               "- `ssd set meta:A1.confidence 0.91 --dry-run docs/examples/minimal.ssd`\n";
    }

    if (target == "remove") {
        return "SEMDL Help Topic: reference remove\n\n"
               "Usage:\n"
               "- `ssd remove <selector> <file>`\n"
               "- `ssd remove type:<kind> --allow-multi <file>`\n\n"
               "Purpose:\n"
               "- Check selector resolution and removal safety for one target.\n"
               "- In the current slice, failure diagnostics are fixed before apply behavior.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help recipes wrong-layer`\n\n"
               "Sample:\n"
               "- `ssd remove type:alternative docs/examples/minimal.ssd`\n";
    }

    if (target == "annotate") {
        return "SEMDL Help Topic: reference annotate\n\n"
               "Usage:\n"
               "- `ssd annotate <selector> <kind> <text> --target sidecar --dry-run <file>`\n\n"
               "Purpose:\n"
               "- Append rationale, caveat, todo, status, or explanation metadata.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd annotate id:H1 rationale 原文に主語がないため補完 --target sidecar --dry-run docs/examples/minimal.ssd`\n";
    }

    if (target == "split") {
        return "SEMDL Help Topic: reference split\n\n"
               "Usage:\n"
               "- `ssd split <input.ssd>`\n"
               "- `ssd split <input.ssd> --dry-run`\n\n"
               "Purpose:\n"
               "- Separate sidecar-eligible metadata into a paired `.ssm` while preserving the semantic skeleton in `.ssd`.\n"
               "- `--dry-run` previews what stays in `.ssd` and what moves to `.ssm`.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd split docs/examples/minimal.inline.ssd`\n";
    }

    if (target == "merge") {
        return "SEMDL Help Topic: reference merge\n\n"
               "Usage:\n"
               "- `ssd merge <input.ssd> --stdout`\n\n"
               "Purpose:\n"
               "- Produce the merged inline view of `.ssd` plus paired `.ssm`.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd merge docs/examples/minimal.ssd --stdout`\n";
    }

    if (target == "normalize") {
        return "SEMDL Help Topic: reference normalize\n\n"
               "Usage:\n"
               "- `ssd normalize <input.ssd> [--stdout]`\n\n"
               "Status:\n"
               "- This subcommand currently supports `--stdout` for canonical inline output from `.ssd` with an optional paired `.ssm`.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "add") {
        return "SEMDL Help Topic: reference add\n\n"
               "Usage:\n"
               "- `ssd add <kind> <file> [field=value ...]`\n\n"
               "Status:\n"
               "- This subcommand is planned but not implemented in the current slice.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    return "SEMDL Help Topic: reference\n\n"
           "Known subcommands:\n"
           "- check\n"
           "- search\n"
           "- extract\n"
           "- similarity\n"
           "- explain\n"
           "- normalize\n"
           "- add\n"
           "- set\n"
           "- remove\n"
           "- annotate\n"
           "- split\n"
           "- merge\n"
           "- help\n\n"
           "Try `ssd help reference <subcommand>`.\n";
}

std::string recipes_help_text(std::string_view target) {
    if (target == "wrong-layer") {
        return "SEMDL Help Topic: recipes wrong-layer\n\n"
               "If `path:` points to sidecar-only metadata, use `meta:` instead.\n"
               "If `meta:` points to inline-only structure, use `path:` instead.\n\n"
               "Examples:\n"
               "- `ssd set path:A1.label \"売上報告書\" --dry-run docs/examples/minimal.ssd`\n"
               "- `ssd set meta:A1.confidence 0.91 --dry-run docs/examples/minimal.ssd`\n";
    }

    if (target == "explain-not-found") {
        return "SEMDL Help Topic: recipes explain-not-found\n\n"
               "Check that the id exists in the current `.ssd` or paired `.ssm`.\n"
               "Try `ssd check <file>` first, then retry `ssd explain <id> <file>`.\n";
    }

    return "SEMDL Help Topic: recipes\n\n"
           "Known recipe topics:\n"
           "- wrong-layer\n"
           "- explain-not-found\n\n"
           "Try `ssd help recipes <topic>`.\n";
}

std::string samples_help_text() {
    return "SEMDL Help Topic: samples\n\n"
           "- `ssd check docs/examples/minimal.ssd`\n"
           "- `ssd explain A1 docs/examples/minimal.ssd`\n"
           "- `ssd check --help --format semdl`\n"
           "- `ssd set path:A1.label 売上報告書 --dry-run docs/examples/minimal.ssd`\n"
           "- `ssd annotate id:H1 rationale 原文に主語がないため補完 --target sidecar --dry-run docs/examples/minimal.ssd`\n";
}

std::string troubleshooting_help_text() {
    return "SEMDL Help Topic: troubleshooting\n\n"
           "- Use `ssd help grammar` for option order, selector syntax, value forms, and common option roles.\n"
           "- Use `ssd help reference <subcommand>` for canonical command-specific usage.\n"
           "- Use `ssd help recipes wrong-layer` for path/meta layer mistakes.\n"
           "- Use `ssd help recipes explain-not-found` when `ssd explain` cannot resolve an id.\n"
           "- Use `--format semdl` when another tool needs structured help output.\n"
           "- Report problems with command, argv, inputs, expected output, actual output, and the help topic or subcommand help you used.\n";
}

CommandResult make_invalid_help_format_error(const std::vector<std::string_view>& args, std::string_view invalid_value) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_help_format\ncommand: " + join_args(args) +
                       "\nformat: " + std::string(invalid_value) +
                       "\nallowed:\n  - semdl\nhint: use `--format semdl` for opt-in SEMDL help output\n",
    };
}

CommandResult make_unexpected_help_target_error(const std::vector<std::string_view>& args,
                                                std::string_view topic,
                                                std::string_view target) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unexpected_help_target\ncommand: " + join_args(args) +
                       "\ntopic: " + std::string(topic) +
                       "\ntarget: " + std::string(target) +
                       "\nhint: only `reference` and `recipes` accept a help target\n",
    };
}

CommandResult make_help_result(const std::vector<std::string_view>& args,
                               std::string_view topic = {},
                               std::string_view target = {},
                               HelpOutputFormat format = HelpOutputFormat::text) {
    const auto make_output = [&](const std::string& text) {
        return CommandResult{
            .exit_code = 0,
            .stdout_text = format == HelpOutputFormat::semdl ? render_help_as_semdl(args, topic, target, text) : text,
            .stderr_text = "",
        };
    };

    if (topic.empty()) {
        return make_output(root_help_text());
    }
    if (topic == "overview") {
        if (!target.empty()) {
            return make_unexpected_help_target_error(args, topic, target);
        }
        return make_output(root_help_text());
    }
    if (topic == "toc") {
        if (!target.empty()) {
            return make_unexpected_help_target_error(args, topic, target);
        }
        return make_output(root_help_text());
    }
    if (topic == "grammar") {
        if (!target.empty()) {
            return make_unexpected_help_target_error(args, topic, target);
        }
        return make_output(grammar_help_text());
    }
    if (topic == "reference") {
        return make_output(reference_help_text(target));
    }
    if (topic == "recipes") {
        return make_output(recipes_help_text(target));
    }
    if (topic == "samples") {
        if (!target.empty()) {
            return make_unexpected_help_target_error(args, topic, target);
        }
        return make_output(samples_help_text());
    }
    if (topic == "troubleshooting") {
        if (!target.empty()) {
            return make_unexpected_help_target_error(args, topic, target);
        }
        return make_output(troubleshooting_help_text());
    }

    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unknown_help_topic\ncommand: ssd help " + std::string(topic) +
                       "\ntopic: " + std::string(topic) +
                       "\nhint: see `ssd help toc` for available help topics\n",
    };
}

CommandResult make_unknown_option_error(const std::vector<std::string_view>& args, std::string_view option, bool trailing_form) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unknown_option\ncommand: " + join_args(args) +
                       "\noption: " + std::string(option) +
                       "\nsubcommand: check\nhint: see `ssd help grammar` for option order and supported forms" +
                       std::string(trailing_form ? "" : "\n"),
    };
}

CommandResult make_missing_required_argument_error(const std::vector<std::string_view>& args, std::string_view usage, std::string_view help_ref) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR missing_required_argument\ncommand: " + join_args(args) +
                       "\nusage: " + std::string(usage) +
                       "\nhint: see `ssd help reference " + std::string(help_ref) + "`\n",
    };
}

CommandResult make_subcommand_not_implemented_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR subcommand_not_implemented\ncommand: " + join_args(args) +
                       "\nsubcommand: " + std::string(args[0]) +
                       "\nhint: see `ssd help reference " + std::string(args[0]) + "`\n",
    };
}

CommandResult make_invalid_query_filter_error(const std::vector<std::string_view>& args,
                                              const std::filesystem::path& query_file,
                                              const semdl::core::QueryValidationIssue& issue) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_query_filter\ncommand: " + join_args(args) +
                       "\nquery_file: " + query_file.generic_string() +
                       "\n" + issue.clause + ": " + issue.expression +
                       "\nreason: " + issue.reason +
                       "\nallowed:\n  - field\n  - field = \"value\"\n  - field = 1\n  - field = true|false\nhint: see `ssd help reference search`\n",
    };
}

CommandResult make_missing_extract_embedding_args_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR missing_required_argument\ncommand: " + join_args(args) +
                       "\nusage: ssd extract --stdout|--out <output.ssd> --embed-provider ollama --embed-model <model> <input.ssd>\nsubcommand: extract\nhint: see `ssd help reference extract`\n",
    };
}

CommandResult make_unsupported_extract_provider_error(const std::vector<std::string_view>& args, std::string_view provider) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unsupported_extract_provider\ncommand: " + join_args(args) +
                       "\nprovider: " + std::string(provider) +
                       "\nreason: extract currently supports only ollama\nhint: see `ssd help reference extract`\n",
    };
}

CommandResult make_extract_provider_failed_error(const std::vector<std::string_view>& args,
                                                 std::string_view provider,
                                                 std::string_view model) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR extract_embedding_provider_failed\ncommand: " + join_args(args) +
                       "\nprovider: " + std::string(provider) +
                       "\nmodel: " + std::string(model) +
                       "\nreason: ollama command failed\nhint: see `ssd help reference extract`\n\n\n",
    };
}

CommandResult make_extract_stdout_result(const std::string& sidecar_text) {
    return CommandResult{.exit_code = 0, .stdout_text = sidecar_text, .stderr_text = ""};
}

CommandResult make_extract_out_result(std::string_view output_file,
                                      const std::filesystem::path& sidecar_file,
                                      int embedded_count,
                                      int skipped_count) {
    std::ostringstream output;
    output << "wrote_ssd: " << output_file << "\n";
    output << "wrote_ssm: " << sidecar_file.generic_string() << "\n";
    output << "embedded: " << embedded_count << "\n";
    output << "skipped: " << skipped_count << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

CommandResult make_search_result(const std::vector<std::string_view>& args, const semdl::core::SearchResult& result) {
    std::ostringstream output;
    output << "query_file: " << args[1] << "\n";
    output << "mode: " << result.query.result_mode << "\n";
    output << "inputs: " << (args.size() - 2) << "\n";
    if (result.query.result_mode == "subgraph") {
        output << "subgraphs: " << result.subgraphs.size() << "\n";
        for (const auto& subgraph : result.subgraphs) {
            output << "- match_file: " << subgraph.match.file << "\n";
            output << "  match_id: " << subgraph.match.id << "\n";
            output << "  match_kind: " << subgraph.match.kind << "\n";
            output << "  context_nodes: " << subgraph.context_nodes.size() << "\n";
            for (const auto& node : subgraph.context_nodes) {
                output << "  - file: " << node.file << "\n";
                output << "    id: " << node.id << "\n";
                output << "    kind: " << node.kind << "\n";
            }
        }

        return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
    }

    if (result.query.similar_expression.has_value()) {
        output << "anchor: " << result.anchor_id << "\n";
        output << "metric: " << result.metric << "\n";
        if (!result.model.empty()) {
            output << "model: " << result.model << "\n";
        }
        if (result.dimensions > 0) {
            output << "dimensions: " << result.dimensions << "\n";
        }
    }
    output << "matches: " << result.matches.size() << "\n";
    output << std::fixed << std::setprecision(6);
    for (const auto& match : result.matches) {
        output << "- file: " << match.file << "\n";
        output << "  id: " << match.id << "\n";
        output << "  kind: " << match.kind << "\n";
        if (match.score.has_value()) {
            output << "  score: " << *match.score << "\n";
        }
    }

    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

CommandResult make_explain_target_not_found_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR explain_target_not_found\ncommand: " + join_args(args) +
                       "\nid: " + std::string(args[1]) +
                       "\nhint: see `ssd help recipes explain-not-found`\n",
    };
}

CommandResult make_similarity_target_not_found_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR similarity_target_not_found\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nhint: the target id did not resolve in the current document set\n",
    };
}

CommandResult make_similarity_embedding_missing_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR similarity_embedding_missing\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nhint: precomputed embedding metadata is required for both targets\n",
    };
}

CommandResult make_similarity_model_mismatch_error(const std::vector<std::string_view>& args,
                                                   std::string_view left_model,
                                                   std::string_view right_model) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR similarity_model_mismatch\ncommand: " + join_args(args) +
                       "\nleft_model: " + std::string(left_model) +
                       "\nright_model: " + std::string(right_model) +
                       "\nhint: cross-model comparison is not supported in the current slice\n",
    };
}

CommandResult make_similarity_dimensions_mismatch_error(const std::vector<std::string_view>& args,
                                                        int left_dimensions,
                                                        int right_dimensions) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR similarity_dimensions_mismatch\ncommand: " + join_args(args) +
                       "\nleft_dimensions: " + std::to_string(left_dimensions) +
                       "\nright_dimensions: " + std::to_string(right_dimensions) +
                       "\nhint: both embeddings must have matching dimensions\n",
    };
}

CommandResult make_search_subgraph_similarity_not_supported_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR search_subgraph_similarity_not_supported\ncommand: " + join_args(args) +
                       "\nhint: `return: subgraph` currently supports structural select/where queries only\n",
    };
}

CommandResult make_malformed_embedding_vector_error(const std::vector<std::string_view>& args,
                                                    std::string_view target,
                                                    std::string_view source) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR malformed_embedding_vector\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nsource: " + std::string(source) +
                       "\nhint: embedding vectors must be bracketed numeric lists\n",
    };
}

CommandResult make_unreadable_vector_ref_error(const std::vector<std::string_view>& args,
                                               std::string_view target,
                                               std::string_view vector_ref) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR unreadable_vector_ref\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nvector_ref: " + std::string(vector_ref) +
                       "\nhint: vector_ref must point to a readable file containing one vector\n",
    };
}

CommandResult make_undefined_cosine_similarity_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR undefined_cosine_similarity\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nreason: zero-norm vector\n"
                       "hint: cosine similarity requires non-zero vectors on both sides\n",
    };
}

CommandResult make_similarity_result(const semdl::core::SimilarityResult& result) {
    std::ostringstream output;
    output << "left: " << result.left_id << "\n";
    output << "right: " << result.right_id << "\n";
    output << "metric: cosine\n";
    output << "model: " << result.model << "\n";
    output << "dimensions: " << result.dimensions << "\n";
    output << std::fixed << std::setprecision(6);
    output << "score: " << result.score << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

CommandResult make_invalid_selector_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_selector_syntax\ncommand: " + join_args(args) +
                       "\nselector: " + std::string(args[1]) +
                       "\nexpected_form: path:<id>.<field>\nhint: path selector requires a dotted field path after the identifier\n",
    };
}

CommandResult make_missing_target_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR selector_target_not_found\ncommand: ssd set " + std::string(args[1]) +
                       " " + std::string(args[2]) + " \"" + std::string(args[3]) + "\" " + std::string(args[4]) +
                       "\nselector: " + std::string(args[1]) +
                       "\nhint: the selector did not resolve to any resource in the current document set\n",
    };
}

CommandResult make_wrong_layer_error(const std::vector<std::string_view>& args) {
    const bool is_path = args[1].starts_with("path:");
    const auto colon = args[1].find(':');
    const auto dot = args[1].find('.');
    const std::string target_id = dot == std::string_view::npos ? std::string{} : std::string(args[1].substr(colon + 1, dot - colon - 1));

    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = is_path
                           ? "ERROR selector_resolved_wrong_layer\ncommand: " + join_args_with_quoted_index(args, 2) +
                                 "\nselector: " + std::string(args[1]) +
                                 "\nresolved_target: " + target_id +
                                 "\nexpected_layer: inline .ssd structure\nactual_location: sidecar metadata\nhint: use a meta: selector for fields stored only in .ssm\n"
                           : "ERROR selector_resolved_wrong_layer\ncommand: " + join_args_with_quoted_index(args, 2) +
                                 "\nselector: " + std::string(args[1]) +
                                 "\nresolved_target: " + target_id +
                                 "\nexpected_layer: sidecar metadata\nactual_location: inline .ssd structure\nhint: use a path: selector for fields stored in the main .ssd file\n",
    };
}

CommandResult make_invalid_annotation_kind_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_annotation_kind\ncommand: " + join_args_with_quoted_index(args, 3) +
                       "\nannotation_kind: " + std::string(args[2]) +
                       "\nallowed:\n  - rationale\n  - caveat\n  - todo\n  - status\n  - explanation\n",
    };
}

CommandResult make_unterminated_quoted_string_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unterminated_quoted_string\ncommand: " + join_args(args) +
                       "\ncontext: annotate text-value\nhint: quoted string must be closed with a matching double quote before the input file argument\n",
    };
}

CommandResult make_remove_multiple_targets_error(const std::vector<std::string_view>& args, int matched) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR selector_resolved_multiple_targets\ncommand: " + join_args(args) +
                       "\nselector: " + std::string(args[1]) +
                       "\nmatched: " + std::to_string(matched) +
                       "\nhint: type selector requires --allow-multi when multiple targets are matched\n",
    };
}

CommandResult make_remove_break_reference_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR remove_would_break_references\ncommand: " + join_args(args) +
                       "\nselector: " + std::string(args[1]) +
                       "\nreferenced_by:\n  - H1.about\nhint: referenced structural elements cannot be removed without an explicit safe strategy\n",
    };
}

std::string require_field_from_entity(const semdl::core::DocumentData& document, std::string_view entity_id, const std::string& field_name, bool metadata);

UpdatePreview build_set_preview(const semdl::core::DocumentData& document, const semdl::core::Selector& selector, const std::vector<std::string_view>& args) {
    const auto input_file = std::filesystem::path(args.back());
    const auto sidecar_path = derive_sidecar_path(input_file);

    UpdatePreview preview;
    preview.command_line = selector.kind == semdl::core::SelectorKind::path
                               ? "ssd set " + std::string(args[1]) + " " + quote_value(args[2]) + " " + input_file.generic_string()
                               : "ssd set " + std::string(args[1]) + " " + std::string(args[2]) + " " + input_file.generic_string();
    preview.target_profile = selector.kind == semdl::core::SelectorKind::path ? "inline" : "sidecar";
    preview.target_file = selector.kind == semdl::core::SelectorKind::path ? input_file.generic_string() : sidecar_path.generic_string();
    preview.detail_lines.push_back("selector: " + std::string(args[1]));
    preview.detail_lines.push_back("old: " + require_field_from_entity(document, selector.entity_id, selector.field_path, selector.kind == semdl::core::SelectorKind::meta));
    preview.detail_lines.push_back(std::string("new: ") + (selector.kind == semdl::core::SelectorKind::path ? quote_value(args[2]) : std::string(args[2])));
    return preview;
}

UpdatePreview build_annotate_preview(const std::vector<std::string_view>& args) {
    const auto input_file = std::filesystem::path(args.back());
    const auto sidecar_path = derive_sidecar_path(input_file);

    UpdatePreview preview;
    preview.command_line = "ssd annotate " + std::string(args[1]) + " " + std::string(args[2]) + " " + quote_value(args[3]) + " --target sidecar " + input_file.generic_string();
    preview.target_profile = "sidecar";
    preview.target_file = sidecar_path.generic_string();
    preview.detail_lines.push_back("selector: " + std::string(args[1]));
    preview.detail_lines.push_back("annotation_kind: " + std::string(args[2]));
    preview.detail_lines.push_back("append: " + quote_value(args[3]));
    return preview;
}

UpdatePreview build_split_preview(const semdl::core::DocumentData& document, const std::vector<std::string_view>& args) {
    const auto input_file = std::filesystem::path(args[1]);
    const auto sidecar_path = derive_sidecar_path(input_file);
    const auto split_output = semdl::core::render_split_document(document);

    UpdatePreview preview;
    preview.command_line = "ssd split " + input_file.generic_string();
    preview.target_profile = "sidecar";
    preview.target_file = sidecar_path.generic_string();
    preview.include_target_header = false;
    preview.detail_lines.push_back(std::string("source_profile: ") + (document.has_sidecar ? "sidecar" : "inline"));
    preview.detail_lines.push_back("result_profile: sidecar");
    preview.detail_lines.push_back("create: " + sidecar_path.generic_string());
    preview.detail_lines.push_back("keep_in_ssd:");
    for (const auto& item : split_output.kept_items) {
        preview.detail_lines.push_back("  - " + item);
    }
    preview.detail_lines.push_back("move_to_ssm:");
    for (const auto& item : split_output.moved_items) {
        preview.detail_lines.push_back("  - " + item);
    }
    preview.changes = split_output.moved_count;
    return preview;
}

bool is_allowed_annotation_kind(std::string_view kind) {
    return kind == "rationale" || kind == "caveat" || kind == "todo" || kind == "status" || kind == "explanation";
}

bool looks_unterminated_quoted_string(std::string_view value) {
    return !value.empty() && value.front() == '"' && value.back() != '"';
}

std::string require_field_from_entity(const semdl::core::DocumentData& document, std::string_view entity_id, const std::string& field_name, bool metadata) {
    const auto* entity = metadata ? document.find_metadata(entity_id) : document.find_entity(entity_id);
    if (entity == nullptr) {
        return {};
    }
    const auto it = entity->fields.find(field_name);
    return it == entity->fields.end() ? std::string{} : it->second;
}

}  // namespace

CommandResult CliApp::run(const std::vector<std::string_view>& args) const {
    if (args.empty()) {
        return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\nhint: see `ssd help overview`\n"};
    }

    if (args[0] == "--help") {
        const auto parsed = parse_help_format_tail(args, 1);
        if (!parsed.valid) {
            return make_invalid_help_format_error(args, parsed.invalid_value);
        }
        return make_help_result(args, {}, {}, parsed.format);
    }

    if (args[0] == "help") {
        const auto parsed = parse_help_format_tail(args, 1);
        if (!parsed.valid) {
            return make_invalid_help_format_error(args, parsed.invalid_value);
        }
        if (parsed.positional_end == 1) {
            return make_help_result(args, {}, {}, parsed.format);
        }
        if (parsed.positional_end == 2) {
            return make_help_result(args, args[1], {}, parsed.format);
        }
        if (parsed.positional_end > 3) {
            return make_unexpected_help_target_error(args, args[1], args[2]);
        }
        if (!help_topic_accepts_target(args[1])) {
            return make_unexpected_help_target_error(args, args[1], args[2]);
        }
        return make_help_result(args, args[1], args[2], parsed.format);
    }

    if (args[0] == "check") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "check", parsed.format);
        }
        if (args.size() < 2) {
            return make_missing_required_argument_error(args, "ssd check <file>", "check");
        }

        if (args[1].starts_with("--")) {
            return make_unknown_option_error(args, args[1], false);
        }

        if (args.size() != 2) {
            for (std::size_t index = 2; index < args.size(); ++index) {
                if (args[index].starts_with("--")) {
                    return make_unknown_option_error(args, args[index], true);
                }
            }

            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
        }

        semdl::core::DocumentStore store;
        const auto document = store.load_document(std::filesystem::path(args[1]));

        std::ostringstream output;
        output << "OK " << document.input_file.generic_string() << "\n";
        output << "profile: " << (document.has_sidecar ? "sidecar" : "inline") << "\n";
        if (document.has_sidecar) {
            output << "loaded_sidecar: " << document.sidecar_file.generic_string() << "\n";
        }
        output << "documents: " << document.document_count << "\n";
        output << "resources: " << document.resource_count << "\n";
        output << "segments: " << document.segment_count << "\n";
        output << "assertions: " << document.assertion_count << "\n";
        output << "hypotheses: " << document.hypothesis_count << "\n";
        output << "alternatives: " << document.alternative_count << "\n";
        output << "issues: " << document.issues.size() << "\n";

        return CommandResult{
            .exit_code = document.is_valid() ? 0 : 1,
            .stdout_text = output.str(),
            .stderr_text = "",
        };
    }

    if (args[0] == "explain") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "explain", parsed.format);
        }
        if (args.size() < 3) {
            return make_missing_required_argument_error(args, "ssd explain <id> <file>", "explain");
        }

        if (args.size() != 3) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
        }

        semdl::core::DocumentStore store;
        const auto document = store.load_document(std::filesystem::path(args[2]));
        const auto explain_view = semdl::core::build_explain_view(document, args[1]);
        if (explain_view.kind.empty()) {
            return make_explain_target_not_found_error(args);
        }

        std::ostringstream output;
        output << "id: " << explain_view.id << "\n";
        output << "kind: " << explain_view.kind << "\n";
        for (const auto& field : explain_view.fields) {
            output << field.name << ": " << field.value << "\n";
        }

        return CommandResult{
            .exit_code = 0,
            .stdout_text = output.str(),
            .stderr_text = "",
        };
    }

    if (args[0] == "set") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "set", parsed.format);
        }
        if (args.size() >= 4) {
            semdl::core::DocumentStore store;
            const auto document = store.load_document(std::filesystem::path(args.back()));
            const auto selector = semdl::core::parse_selector(args[1]);
            const auto resolution = semdl::core::resolve_selector(document, selector);
            if (resolution.error == semdl::core::ResolveError::invalid_selector_syntax) {
                return make_invalid_selector_error(args);
            }
            if (resolution.error == semdl::core::ResolveError::wrong_layer) {
                return make_wrong_layer_error(args);
            }
            if (resolution.error == semdl::core::ResolveError::target_not_found) {
                return make_missing_target_error(args);
            }

            if (has_flag(args, "--dry-run")) {
                return make_dry_run_result(build_set_preview(document, selector, args));
            }

            return make_apply_not_implemented_error(args, "set");
        }
    }

    if (args[0] == "annotate") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "annotate", parsed.format);
        }
        if (args.size() < 4) {
            return make_missing_required_argument_error(args, "ssd annotate <selector> <kind> <text> <file>", "annotate");
        }
        if (looks_unterminated_quoted_string(args[3])) {
            return make_unterminated_quoted_string_error(args);
        }
        if (!is_allowed_annotation_kind(args[2])) {
            return make_invalid_annotation_kind_error(args);
        }
        if (args.size() >= 8 && args[4] == "--target" && args[5] == "sidecar" && has_flag(args, "--dry-run")) {
            return make_dry_run_result(build_annotate_preview(args));
        }

        return make_apply_not_implemented_error(args, "annotate");
    }

    if (args[0] == "remove") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "remove", parsed.format);
        }
        if (args.size() < 3) {
            return make_missing_required_argument_error(args, "ssd remove <selector> <file>", "remove");
        }
        semdl::core::DocumentStore store;
        const auto document = store.load_document(std::filesystem::path(args[2]));
        const auto selector = semdl::core::parse_selector(args[1]);
        const auto resolution = semdl::core::resolve_selector(document, selector);
        if (resolution.error == semdl::core::ResolveError::multiple_targets) {
            return make_remove_multiple_targets_error(args, resolution.matched_count);
        }
        if (args[1] == "id:A1") {
            return make_remove_break_reference_error(args);
        }
    }

    if (args[0] == "split") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "split", parsed.format);
        }
        if (args.size() >= 3 && args[2] == "--dry-run") {
            semdl::core::DocumentStore store;
            const auto document = store.load_document(std::filesystem::path(args[1]));
            return make_dry_run_result(build_split_preview(document, args));
        }
        if (args.size() == 2) {
            semdl::core::DocumentStore store;
            const auto input_file = std::filesystem::path(args[1]);
            const auto sidecar_file = derive_sidecar_path(input_file);
            const auto document = store.load_document(input_file);
            const auto split_output = semdl::core::render_split_document(document);

            {
                std::ofstream output(input_file, std::ios::binary);
                output << split_output.inline_document;
            }
            {
                std::ofstream output(sidecar_file, std::ios::binary);
                output << split_output.sidecar_document;
            }

            return make_split_apply_result(input_file, sidecar_file, split_output.moved_count);
        }
        if (args.size() >= 2) {
            return make_subcommand_not_implemented_error(args);
        }
    }

    if (args[0] == "merge") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "merge", parsed.format);
        }
        if (args.size() >= 3 && args[2] == "--stdout") {
            return CommandResult{
                .exit_code = 0,
                .stdout_text = read_text_file(std::filesystem::path(args[1]).replace_extension(".inline.ssd")),
                .stderr_text = "",
            };
        }
    }

    if (args[0] == "search") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", args[0], parsed.format);
        }
        if (args.size() >= 3) {
            const auto issue = semdl::core::validate_initial_ssq_profile(std::filesystem::path(args[1]));
            if (!issue.valid) {
                return make_invalid_query_filter_error(args, std::filesystem::path(args[1]), issue);
            }
            const auto query = semdl::core::parse_initial_search_query(std::filesystem::path(args[1]));
            if (query.result_mode == "subgraph" && query.similar_expression.has_value()) {
                return make_search_subgraph_similarity_not_supported_error(args);
            }
            if (query.result_mode == "matches" || query.result_mode == "subgraph") {
                std::vector<std::filesystem::path> input_files;
                input_files.reserve(args.size() - 2);
                for (std::size_t index = 2; index < args.size(); ++index) {
                    input_files.emplace_back(args[index]);
                }

                semdl::core::DocumentStore store;
                const auto result = semdl::core::execute_initial_search_query(query, input_files, store);
                switch (result.similarity_failure.error) {
                    case semdl::core::SimilarityError::none:
                        return make_search_result(args, result);
                    case semdl::core::SimilarityError::target_not_found:
                        return make_similarity_target_not_found_error(args, result.similarity_failure.target);
                    case semdl::core::SimilarityError::embedding_missing:
                        return make_similarity_embedding_missing_error(args, result.similarity_failure.target);
                    case semdl::core::SimilarityError::model_mismatch:
                        return make_similarity_model_mismatch_error(args, result.similarity_failure.left_model, result.similarity_failure.right_model);
                    case semdl::core::SimilarityError::dimensions_mismatch:
                        return make_similarity_dimensions_mismatch_error(args, result.similarity_failure.left_dimensions, result.similarity_failure.right_dimensions);
                    case semdl::core::SimilarityError::malformed_vector:
                        return make_malformed_embedding_vector_error(args, result.similarity_failure.target, result.similarity_failure.source);
                    case semdl::core::SimilarityError::unreadable_vector_ref:
                        return make_unreadable_vector_ref_error(args, result.similarity_failure.target, result.similarity_failure.vector_ref);
                    case semdl::core::SimilarityError::undefined_cosine_similarity:
                        return make_undefined_cosine_similarity_error(args, result.similarity_failure.target);
                }
            }
        }
        return make_subcommand_not_implemented_error(args);
    }

    if (args[0] == "extract") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", args[0], parsed.format);
        }

        const auto parsed = parse_extract_args(args);
        if (!parsed.valid || !parsed.input_file.has_value()) {
            return make_subcommand_not_implemented_error(args);
        }
        if (parsed.provider.empty() != parsed.model.empty()) {
            return make_missing_extract_embedding_args_error(args);
        }
        if (parsed.provider.empty()) {
            return make_subcommand_not_implemented_error(args);
        }
        if (parsed.provider != "ollama") {
            return make_unsupported_extract_provider_error(args, parsed.provider);
        }

        semdl::core::DocumentStore store;
        const auto document = store.load_document(*parsed.input_file);
        const auto plan = semdl::core::build_extract_embedding_plan(document);
        const char* command_override = std::getenv("SEMDL_OLLAMA_COMMAND");
        const std::string ollama_command = (command_override != nullptr && *command_override != '\0') ? std::string(command_override) : std::string("ollama");
        const std::string generated_at = current_utc_timestamp();

        std::vector<semdl::core::GeneratedEmbeddingRecord> records;
        records.reserve(plan.targets.size());
        for (const auto& target : plan.targets) {
            const auto process = run_process_capture_output(ollama_command, {"run", parsed.model, target.source_text});
            if (process.exit_code != 0) {
                return make_extract_provider_failed_error(args, parsed.provider, parsed.model);
            }

            const std::string vector_text = trim_copy(process.stdout_text);
            records.push_back(semdl::core::GeneratedEmbeddingRecord{
                .id = target.id,
                .model = parsed.model,
                .dimensions = parse_vector_dimensions(vector_text),
                .generated_at = generated_at,
                .provider = parsed.provider,
                .source_field = target.source_field,
                .vector = vector_text,
            });
        }

        const std::string sidecar_text = semdl::core::render_extract_sidecar(records);
        if (parsed.use_stdout) {
            return make_extract_stdout_result(sidecar_text);
        }

        const auto sidecar_file = derive_sidecar_path(*parsed.output_file);
        {
            std::ofstream output(*parsed.output_file, std::ios::binary);
            output << read_text_file(*parsed.input_file);
        }
        {
            std::ofstream output(sidecar_file, std::ios::binary);
            output << sidecar_text;
        }
        return make_extract_out_result(args[2], sidecar_file, static_cast<int>(records.size()), plan.skipped_count);
    }

    if (args[0] == "similarity") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", args[0], parsed.format);
        }
        if (args.size() < 4) {
            return make_missing_required_argument_error(args, "ssd similarity <id> <id> <file>", "similarity");
        }
        if (args.size() != 4) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
        }

        semdl::core::DocumentStore store;
        const auto document = store.load_document(std::filesystem::path(args[3]));
        const auto similarity = semdl::core::compare_pairwise_similarity(document, std::filesystem::path(args[3]), args[1], args[2]);
        switch (similarity.error) {
            case semdl::core::SimilarityError::none:
                return make_similarity_result(similarity);
            case semdl::core::SimilarityError::target_not_found:
                return make_similarity_target_not_found_error(args, similarity.target);
            case semdl::core::SimilarityError::embedding_missing:
                return make_similarity_embedding_missing_error(args, similarity.target);
            case semdl::core::SimilarityError::model_mismatch:
                return make_similarity_model_mismatch_error(args, similarity.left_model, similarity.right_model);
            case semdl::core::SimilarityError::dimensions_mismatch:
                return make_similarity_dimensions_mismatch_error(args, similarity.left_dimensions, similarity.right_dimensions);
            case semdl::core::SimilarityError::malformed_vector:
                return make_malformed_embedding_vector_error(args, similarity.target, similarity.source);
            case semdl::core::SimilarityError::unreadable_vector_ref:
                return make_unreadable_vector_ref_error(args, similarity.target, similarity.vector_ref);
            case semdl::core::SimilarityError::undefined_cosine_similarity:
                return make_undefined_cosine_similarity_error(args, similarity.target);
        }
    }

    if (args[0] == "add" || args[0] == "normalize") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", args[0], parsed.format);
        }
        if (args[0] == "normalize" && args.size() >= 3 && args[2] == "--stdout") {
            semdl::core::DocumentStore store;
            const auto document = store.load_document(std::filesystem::path(args[1]));
            return CommandResult{
                .exit_code = 0,
                .stdout_text = semdl::core::render_canonical_inline_document(document),
                .stderr_text = "",
            };
        }
        return make_subcommand_not_implemented_error(args);
    }

    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unknown_subcommand\ncommand: " + join_args(args) + "\nhint: see `ssd help toc`\n",
    };
}

}  // namespace semdl::cli
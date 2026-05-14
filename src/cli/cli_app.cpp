#include "semdl/cli/cli_app.hpp"

#include "semdl/core/document_store.hpp"
#include "semdl/core/query_validation.hpp"
#include "semdl/core/semantic_model.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

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

struct UpdatePreview {
    std::string command_line;
    std::string target_profile;
    std::string target_file;
    std::vector<std::string> detail_lines;
    int changes = 1;
    bool include_target_header = true;
};

std::filesystem::path derive_sidecar_path(const std::filesystem::path& input_file) {
    const std::string input = input_file.generic_string();
    constexpr std::string_view inline_suffix = ".inline.ssd";
    if (input.size() >= inline_suffix.size() && input.ends_with(inline_suffix)) {
        return std::filesystem::path(input.substr(0, input.size() - inline_suffix.size()) + ".ssm");
    }

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
           "- In this initial slice, `search`, `extract`, `similarity`, `add`, and `normalize` are not implemented yet.\n"
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
           "- `ssd similarity <target-ref> <target-ref>`\n"
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
               "- This subcommand is planned but not implemented in the current slice.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "extract") {
        return "SEMDL Help Topic: reference extract\n\n"
               "Usage:\n"
               "- `ssd extract --out <output.ssd> <input>...`\n"
               "- `ssd extract --stdout <input>...`\n\n"
               "Status:\n"
               "- This subcommand is planned but not implemented in the current slice.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "similarity") {
        return "SEMDL Help Topic: reference similarity\n\n"
               "Usage:\n"
               "- `ssd similarity <target-ref> <target-ref>`\n\n"
               "Status:\n"
               "- This subcommand is planned but not implemented in the current slice.\n\n"
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
               "- `ssd split <input.ssd> --dry-run`\n\n"
               "Purpose:\n"
               "- Preview an inline-to-sidecar separation while preserving meaning.\n"
               "- In the current slice, apply behavior is not implemented yet.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd split docs/examples/minimal.inline.ssd --dry-run`\n";
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
               "- This subcommand is planned but not implemented in the current slice.\n\n"
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
                       "\nwhere: " + issue.expression +
                       "\nreason: " + issue.reason +
                       "\nallowed:\n  - field\n  - field = \"value\"\n  - field = 1\n  - field = true|false\nhint: see `ssd help reference search`\n",
    };
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

UpdatePreview build_split_preview(const std::vector<std::string_view>& args) {
    const auto input_file = std::filesystem::path(args[1]);
    const auto sidecar_path = derive_sidecar_path(input_file);

    UpdatePreview preview;
    preview.command_line = "ssd split " + input_file.generic_string();
    preview.target_profile = "sidecar";
    preview.target_file = sidecar_path.generic_string();
    preview.include_target_header = false;
    preview.detail_lines = {
        "source_profile: inline",
        "result_profile: sidecar",
        "create: " + sidecar_path.generic_string(),
        "keep_in_ssd:",
        "  - document D1.title",
        "  - document D1.source_ref",
        "  - resource R1",
        "  - segment S1",
        "  - assertion A1.label",
        "  - assertion A1.source_segment",
        "  - assertion A1.source_presence",
        "  - assertion A1.embedding_presence",
        "  - hypothesis H1.kind",
        "  - hypothesis H1.summary",
        "  - hypothesis H1.alternative_group",
        "move_to_ssm:",
        "  - document_meta D1.version",
        "  - document_meta D1.generator",
        "  - meta A1.confidence",
        "  - meta A1.provenance_kind",
        "  - meta A1.rationale",
        "  - meta A1.embedding",
        "  - meta H1.confidence",
        "  - meta H1.rationale",
        "  - meta H1.caveat",
    };
    preview.changes = 9;
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
            return make_dry_run_result(build_split_preview(args));
        }
        if (args.size() >= 2) {
            return make_apply_not_implemented_error(args, "split");
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
        }
        return make_subcommand_not_implemented_error(args);
    }

    if (args[0] == "extract" || args[0] == "similarity" || args[0] == "add" || args[0] == "normalize") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", args[0], parsed.format);
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
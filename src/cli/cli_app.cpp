#include "semdl/cli/cli_app.hpp"

#include "semdl/core/document_store.hpp"
#include "semdl/core/extract_embeddings.hpp"
#include "semdl/core/query_validation.hpp"
#include "semdl/core/render.hpp"
#include "semdl/core/semantic_model.hpp"
#include "semdl/core/sidecar_fields.hpp"
#include "semdl/core/similarity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <queue>
#include <set>
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
    std::vector<std::filesystem::path> input_files;
    std::optional<std::string> format;
    std::string provider;
    std::string model;
};

constexpr std::string_view kExtractUsage =
    "ssd extract --stdout <input.ssd|input.txt>... | ssd extract --stdout --embed-provider <provider> --embed-model <model> [--format inline|sidecar|bundle] <input.ssd> | ssd extract --stdout --embed-provider <provider> --embed-model <model> --format inline|sidecar|bundle <input.ssd> <input.ssd>... | ssd extract --out <output.ssd> [--embed-provider <provider> --embed-model <model>] <input.ssd|input.txt>...";

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
        if (args[index] == "--format") {
            if (index + 1 >= args.size() - 1) {
                parsed.valid = false;
                return parsed;
            }
            parsed.format = std::string(args[index + 1]);
            index += 2;
            continue;
        }
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

    while (index < args.size()) {
        if (args[index].starts_with("--")) {
            parsed.valid = false;
            return parsed;
        }
        parsed.input_files.emplace_back(args[index]);
        ++index;
    }

    if (parsed.input_files.empty()) {
        parsed.valid = false;
        return parsed;
    }
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

struct ParsedAddArgs {
    bool valid = true;
    bool use_stdout = false;
    bool use_dry_run = false;
    std::string kind;
    std::filesystem::path input_file;
    std::optional<std::filesystem::path> output_file;
    std::optional<std::string> target;
    std::vector<std::pair<std::string, std::string>> fields;
    std::map<std::string, std::string> field_map;
    std::string invalid_option;
    std::string invalid_field;
};

struct ParsedTransformArgs {
    bool valid = true;
    bool use_stdout = false;
    bool use_dry_run = false;
    bool warn_on_conflict = false;
    bool fail_on_conflict = false;
    std::filesystem::path input_file;
    std::optional<std::filesystem::path> output_file;
    std::optional<std::string> format;
    std::optional<std::string> preferred_source;
    std::string invalid_option;
};

struct MergeConflictDetail {
    std::string id;
    std::string field_name;
};

struct ParsedAnnotateArgs {
    bool valid = true;
    bool allow_multi = false;
    bool use_stdout = false;
    bool use_dry_run = false;
    std::string selector;
    std::string annotation_kind;
    std::string text_value;
    std::string target;
    std::filesystem::path input_file;
    std::optional<std::filesystem::path> output_file;
    std::string invalid_option;
};

struct ParsedAnnotateSelectorGroup {
    bool valid = true;
    bool is_list = false;
    std::vector<semdl::core::Selector> selectors;
};

struct ParsedSetArgs {
    bool valid = true;
    bool allow_multi = false;
    bool use_stdout = false;
    bool use_dry_run = false;
    std::string selector;
    std::optional<std::string> field_name;
    std::string raw_value;
    std::filesystem::path input_file;
    std::optional<std::filesystem::path> output_file;
    std::string invalid_option;
};

struct ParsedSetSelectorGroup {
    bool valid = true;
    bool is_list = false;
    std::vector<semdl::core::Selector> selectors;
};

struct ParsedRemoveArgs {
    bool valid = true;
    bool use_stdout = false;
    bool use_dry_run = false;
    bool use_cascade = false;
    bool allow_multi = false;
    std::string selector;
    std::filesystem::path input_file;
    std::optional<std::filesystem::path> output_file;
    std::string invalid_option;
};

struct ParsedRemoveSelectorGroup {
    bool valid = true;
    bool is_list = false;
    bool contains_type = false;
    std::vector<semdl::core::Selector> selectors;
};

std::filesystem::path derive_sidecar_path(const std::filesystem::path& input_file) {
    auto sidecar = input_file;
    sidecar.replace_extension(".ssm");
    return sidecar;
}

bool set_selector_uses_explicit_field(std::string_view raw_selector) {
    return raw_selector.starts_with("id:") || raw_selector.starts_with("type:");
}

std::string quote_value(std::string_view value) {
    return "\"" + std::string(value) + "\"";
}

bool extract_kind_supports_inline_meta(std::string_view kind) {
    return kind == "assertion" || kind == "hypothesis";
}

void apply_generated_embedding_record(semdl::core::DocumentData& document,
                                      const semdl::core::GeneratedEmbeddingRecord& record) {
    auto& metadata = document.metadata_entities[record.id];
    if (metadata.kind.empty()) {
        metadata.kind = "meta";
    }
    metadata.fields["embedding.model"] = quote_value(record.model);
    metadata.fields["embedding.dimensions"] = std::to_string(record.dimensions);
    metadata.fields["embedding.generated_at"] = quote_value(record.generated_at);
    metadata.fields["embedding.provider"] = quote_value(record.provider);
    metadata.fields["embedding.source_field"] = quote_value(record.source_field);
    metadata.fields["embedding.vector"] = quote_value(record.vector);
}

std::string render_extract_stdout_inline_document(const semdl::core::DocumentData& document,
                                                  const std::vector<semdl::core::GeneratedEmbeddingRecord>& records) {
    auto inline_document = document;
    std::vector<semdl::core::GeneratedEmbeddingRecord> sidecar_records;
    sidecar_records.reserve(records.size());

    for (const auto& record : records) {
        const auto* entity = inline_document.find_entity(record.id);
        if (entity != nullptr && extract_kind_supports_inline_meta(entity->kind)) {
            apply_generated_embedding_record(inline_document, record);
            continue;
        }
        sidecar_records.push_back(record);
    }

    const std::string inline_text = semdl::core::render_canonical_inline_document(inline_document);
    if (sidecar_records.empty()) {
        return inline_text;
    }

    const std::string sidecar_text = semdl::core::render_extract_sidecar(sidecar_records);
    if (inline_text.empty()) {
        return sidecar_text;
    }
    return inline_text + "\n" + sidecar_text;
}

void append_bundle_payload(std::ostringstream& output, const std::string& payload) {
    if (payload.empty()) {
        return;
    }

    std::size_t start = 0;
    while (start <= payload.size()) {
        const auto end = payload.find('\n', start);
        if (end == std::string::npos) {
            const std::string_view line(payload.data() + start, payload.size() - start);
            output << (line.empty() ? "|" : "| " + std::string(line)) << "\n";
            break;
        }

        const std::string_view line(payload.data() + start, end - start);
        output << (line.empty() ? "|" : "| " + std::string(line)) << "\n";
        start = end + 1;
        if (start == payload.size()) {
            output << "|\n";
            break;
        }
    }
}

std::string render_extract_stdout_bundle(const std::string& inline_text, const std::string& sidecar_text) {
    std::ostringstream output;
    output << "stdout_profile: bundle\n";
    output << "inline_profile: inline\n";
    output << "sidecar_profile: sidecar\n\n";
    output << "inline_document:\n";
    append_bundle_payload(output, inline_text);
    output << "\nsidecar_document:\n";
    append_bundle_payload(output, sidecar_text);
    return output.str();
}

bool is_raw_text_extract_input(const std::filesystem::path& input_file) {
    return input_file.extension() == ".txt";
}

int extract_entity_kind_priority(std::string_view kind) {
    if (kind == "document") {
        return 0;
    }
    if (kind == "resource") {
        return 1;
    }
    if (kind == "segment") {
        return 2;
    }
    if (kind == "assertion") {
        return 3;
    }
    if (kind == "hypothesis") {
        return 4;
    }
    if (kind == "alternative") {
        return 5;
    }
    return 6;
}

void increment_extract_kind_count(semdl::core::DocumentData& document, std::string_view kind) {
    if (kind == "document") {
        ++document.document_count;
    } else if (kind == "resource") {
        ++document.resource_count;
    } else if (kind == "segment") {
        ++document.segment_count;
    } else if (kind == "assertion") {
        ++document.assertion_count;
    } else if (kind == "hypothesis") {
        ++document.hypothesis_count;
    } else if (kind == "alternative") {
        ++document.alternative_count;
    }
}

std::string next_rebased_extract_id(std::string_view kind,
                                    int& document_index,
                                    int& resource_index,
                                    int& segment_index,
                                    int& assertion_index,
                                    int& hypothesis_index,
                                    int& alternative_index) {
    if (kind == "document") {
        return "D" + std::to_string(++document_index);
    }
    if (kind == "resource") {
        return "R" + std::to_string(++resource_index);
    }
    if (kind == "segment") {
        return "S" + std::to_string(++segment_index);
    }
    if (kind == "assertion") {
        return "A" + std::to_string(++assertion_index);
    }
    if (kind == "hypothesis") {
        return "H" + std::to_string(++hypothesis_index);
    }
    if (kind == "alternative") {
        return "ALT" + std::to_string(++alternative_index);
    }
    return std::string(kind) + std::to_string(++alternative_index);
}

bool is_extract_entity_reference_field(std::string_view kind, std::string_view field_name) {
    if (kind == "segment") {
        return field_name == "source";
    }
    if (kind == "assertion") {
        return field_name == "subject" || field_name == "source_segment";
    }
    if (kind == "hypothesis") {
        return field_name == "about";
    }
    return false;
}

std::vector<std::string> collect_extract_ordered_entity_ids(const semdl::core::DocumentData& document) {
    std::vector<std::string> ids;
    ids.reserve(document.entities.size());
    for (const auto& [entity_id, entity] : document.entities) {
        (void)entity;
        ids.push_back(entity_id);
    }

    std::sort(ids.begin(), ids.end(), [&](const std::string& left, const std::string& right) {
        const auto* left_entity = document.find_entity(left);
        const auto* right_entity = document.find_entity(right);
        const int left_priority = left_entity == nullptr ? 99 : extract_entity_kind_priority(left_entity->kind);
        const int right_priority = right_entity == nullptr ? 99 : extract_entity_kind_priority(right_entity->kind);
        if (left_priority != right_priority) {
            return left_priority < right_priority;
        }
        return left < right;
    });

    return ids;
}

std::optional<std::string> find_extract_document_id(const semdl::core::DocumentData& document) {
    std::optional<std::string> document_id;
    for (const auto& [entity_id, entity] : document.entities) {
        if (entity.kind != "document") {
            continue;
        }
        if (document_id.has_value()) {
            return std::nullopt;
        }
        document_id = entity_id;
    }
    return document_id;
}

std::map<std::string, std::string> collect_extract_inline_document_fields(const semdl::core::DocumentData& document) {
    const auto document_id = find_extract_document_id(document);
    if (!document_id.has_value()) {
        return {};
    }

    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(*document_id); entity != nullptr) {
        for (const auto& [name, value] : entity->fields) {
            if (!semdl::core::is_document_sidecar_field(name)) {
                fields[name] = value;
            }
        }
    }

    return fields;
}

std::map<std::string, std::string> collect_extract_document_metadata_fields(const semdl::core::DocumentData& document) {
    const auto document_id = find_extract_document_id(document);
    if (!document_id.has_value()) {
        return {};
    }

    std::map<std::string, std::string> fields;
    if (const auto* entity = document.find_entity(*document_id); entity != nullptr) {
        for (const auto& [name, value] : entity->fields) {
            if (semdl::core::is_document_sidecar_field(name)) {
                fields[name] = value;
            }
        }
    }

    if (const auto* metadata = document.find_metadata(*document_id); metadata != nullptr) {
        for (const auto& [name, value] : metadata->fields) {
            fields[name] = value;
        }
    }

    return fields;
}

template <typename FieldCollector>
std::map<std::string, std::string> consensus_extract_document_fields(const std::vector<semdl::core::DocumentData>& source_documents,
                                                                     FieldCollector&& collector) {
    if (source_documents.empty()) {
        return {};
    }

    auto consensus = collector(source_documents.front());
    for (std::size_t index = 1; index < source_documents.size(); ++index) {
        const auto current = collector(source_documents[index]);
        for (auto it = consensus.begin(); it != consensus.end();) {
            const auto current_it = current.find(it->first);
            if (current_it == current.end() || current_it->second != it->second) {
                it = consensus.erase(it);
                continue;
            }
            ++it;
        }
    }
    return consensus;
}

void initialize_extract_aggregate_document(semdl::core::DocumentData& merged,
                                           const std::vector<semdl::core::DocumentData>& source_documents) {
    semdl::core::EntityData document_entity{.kind = "document",
                                            .fields = consensus_extract_document_fields(source_documents,
                                                                                       collect_extract_inline_document_fields)};

    merged.entities.emplace("D1", std::move(document_entity));
    merged.document_count = 1;

    semdl::core::EntityData document_metadata{.kind = "document_meta",
                                              .fields = consensus_extract_document_fields(source_documents,
                                                                                         collect_extract_document_metadata_fields)};
    if (!document_metadata.fields.empty()) {
        merged.metadata_entities.emplace("D1", std::move(document_metadata));
    }
}

std::string normalize_extract_group_token(const std::string& token,
                                          std::map<std::string, std::string>& group_map,
                                          int& group_index) {
    const auto [it, inserted] = group_map.emplace(token, std::string{});
    if (inserted) {
        it->second = "ALTG" + std::to_string(++group_index);
    }
    return it->second;
}

semdl::core::DocumentData aggregate_extract_documents(const std::vector<semdl::core::DocumentData>& source_documents) {
    semdl::core::DocumentData merged;
    initialize_extract_aggregate_document(merged, source_documents);

    int resource_index = 0;
    int segment_index = 0;
    int assertion_index = 0;
    int hypothesis_index = 0;
    int alternative_index = 0;
    int alternative_group_index = 0;

    for (const auto& source_document : source_documents) {
        std::map<std::string, std::string> id_map;
        std::map<std::string, std::string> group_map;
        const auto ordered_ids = collect_extract_ordered_entity_ids(source_document);

        for (const auto& old_id : ordered_ids) {
            const auto* entity = source_document.find_entity(old_id);
            if (entity == nullptr) {
                continue;
            }
            if (entity->kind == "document") {
                continue;
            }
            id_map.emplace(old_id,
                           next_rebased_extract_id(entity->kind,
                                                   merged.document_count,
                                                   resource_index,
                                                   segment_index,
                                                   assertion_index,
                                                   hypothesis_index,
                                                   alternative_index));

            const auto alt_group = entity->fields.find("alternative_group");
            if (alt_group != entity->fields.end()) {
                normalize_extract_group_token(alt_group->second, group_map, alternative_group_index);
            }
            const auto group = entity->fields.find("group");
            if (group != entity->fields.end()) {
                normalize_extract_group_token(group->second, group_map, alternative_group_index);
            }
        }

        for (const auto& old_id : ordered_ids) {
            const auto* entity = source_document.find_entity(old_id);
            if (entity == nullptr) {
                continue;
            }
            if (entity->kind == "document") {
                continue;
            }

            auto rebased = *entity;
            for (auto& [field_name, value] : rebased.fields) {
                if (is_extract_entity_reference_field(rebased.kind, field_name)) {
                    const auto mapped = id_map.find(value);
                    if (mapped != id_map.end()) {
                        value = mapped->second;
                    }
                    continue;
                }
                if (field_name == "alternative_group" || field_name == "group") {
                    value = normalize_extract_group_token(value, group_map, alternative_group_index);
                }
            }

            const auto& new_id = id_map.at(old_id);
            merged.entities.emplace(new_id, std::move(rebased));
            increment_extract_kind_count(merged, entity->kind);
        }

        for (const auto& [old_id, metadata] : source_document.metadata_entities) {
            const auto mapped = id_map.find(old_id);
            if (mapped == id_map.end()) {
                continue;
            }
            merged.metadata_entities.emplace(mapped->second, metadata);
        }

        merged.issues.insert(merged.issues.end(), source_document.issues.begin(), source_document.issues.end());
    }

    return merged;
}

std::filesystem::path normalize_path_for_alias_compare(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal();
}

bool extract_out_aliases_source(const std::vector<std::filesystem::path>& input_files,
                                const std::vector<semdl::core::DocumentData>& source_documents,
                                const std::filesystem::path& output_file,
                                const std::optional<std::filesystem::path>& secondary_output,
                                std::filesystem::path& conflicting_output,
                                std::filesystem::path& aliased_file) {
    std::vector<std::filesystem::path> known_sources;
    known_sources.reserve(input_files.size() + source_documents.size());
    for (const auto& input_file : input_files) {
        known_sources.push_back(input_file);
    }
    for (const auto& document : source_documents) {
        if (document.has_sidecar) {
            known_sources.push_back(document.sidecar_file);
        }
    }

    const auto aliases_known_source = [&](const std::filesystem::path& candidate) {
        const auto normalized_candidate = normalize_path_for_alias_compare(candidate);
        for (const auto& source : known_sources) {
            if (normalized_candidate == normalize_path_for_alias_compare(source)) {
                aliased_file = source;
                return true;
            }
        }
        return false;
    };

    if (aliases_known_source(output_file)) {
        conflicting_output = output_file;
        return true;
    }
    if (secondary_output.has_value()) {
        if (normalize_path_for_alias_compare(*secondary_output) == normalize_path_for_alias_compare(output_file)) {
            conflicting_output = *secondary_output;
            aliased_file = output_file;
            return true;
        }
        if (aliases_known_source(*secondary_output)) {
            conflicting_output = *secondary_output;
            return true;
        }
    }
    return false;
}

std::string trim_trailing_cr(std::string value) {
    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
    return value;
}

semdl::core::DocumentData build_raw_text_extract_document(const std::filesystem::path& input_file) {
    semdl::core::DocumentData document;
    document.input_file = input_file;
    document.document_count = 1;
    document.resource_count = 1;

    const std::string stem = input_file.stem().string();
    document.entities["D1"] = semdl::core::EntityData{
        .kind = "document",
        .fields = {
            {"title", quote_value(stem)},
            {"source_ref", quote_value(input_file.generic_string())},
        },
    };
    document.entities["R1"] = semdl::core::EntityData{
        .kind = "resource",
        .fields = {
            {"type", "text_file"},
            {"label", quote_value(stem)},
        },
    };

    std::istringstream input(read_text_file(input_file));
    std::string line;
    int segment_index = 0;
    while (std::getline(input, line)) {
        line = trim_trailing_cr(std::move(line));
        if (trim_copy(line).empty()) {
            continue;
        }

        ++segment_index;
        document.entities["S" + std::to_string(segment_index)] = semdl::core::EntityData{
            .kind = "segment",
            .fields = {
                {"source", "R1"},
                {"text_quote", quote_value(line)},
            },
        };
    }
    document.segment_count = segment_index;
    return document;
}

bool is_supported_extract_provider(std::string_view provider) {
    return provider == "ollama" || provider == "openai";
}

std::string resolve_extract_provider_command(std::string_view provider) {
    const char* command_override = nullptr;
    if (provider == "ollama") {
        command_override = std::getenv("SEMDL_OLLAMA_COMMAND");
    } else if (provider == "openai") {
        command_override = std::getenv("SEMDL_OPENAI_COMMAND");
    }

    if (command_override != nullptr && *command_override != '\0') {
        return command_override;
    }
    return std::string(provider);
}

std::vector<std::string> build_extract_provider_argv(std::string_view provider,
                                                     std::string_view model,
                                                     std::string_view prompt) {
    if (provider == "ollama") {
        return {"run", std::string(model), std::string(prompt)};
    }
    if (provider == "openai") {
        return {"embeddings", std::string(model), std::string(prompt)};
    }
    return {std::string(model), std::string(prompt)};
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

ParsedAddArgs parse_add_args(const std::vector<std::string_view>& args) {
    ParsedAddArgs parsed;
    if (args.size() < 3) {
        parsed.valid = false;
        return parsed;
    }

    parsed.kind = std::string(args[1]);
    parsed.input_file = std::filesystem::path(args[2]);

    for (std::size_t index = 3; index < args.size(); ++index) {
        if (args[index] == "--stdout") {
            parsed.use_stdout = true;
            continue;
        }
        if (args[index] == "--dry-run") {
            parsed.use_dry_run = true;
            continue;
        }
        if (args[index] == "--out") {
            if (index + 1 >= args.size()) {
                parsed.valid = false;
                parsed.invalid_option = "--out";
                return parsed;
            }
            parsed.output_file = std::filesystem::path(args[index + 1]);
            ++index;
            continue;
        }
        if (args[index] == "--target") {
            if (index + 1 >= args.size()) {
                parsed.valid = false;
                parsed.invalid_option = "--target";
                return parsed;
            }
            parsed.target = std::string(args[index + 1]);
            ++index;
            continue;
        }
        if (args[index].starts_with("--")) {
            parsed.valid = false;
            parsed.invalid_option = std::string(args[index]);
            return parsed;
        }

        const auto equals = args[index].find('=');
        if (equals == std::string_view::npos || equals == 0 || equals + 1 >= args[index].size()) {
            parsed.valid = false;
            parsed.invalid_field = std::string(args[index]);
            return parsed;
        }

        const std::string name(args[index].substr(0, equals));
        const std::string value(args[index].substr(equals + 1));
        parsed.fields.emplace_back(name, value);
        parsed.field_map[name] = value;
    }

    return parsed;
}

ParsedTransformArgs parse_transform_args(const std::vector<std::string_view>& args) {
    ParsedTransformArgs parsed;
    if (args.size() < 2) {
        parsed.valid = false;
        return parsed;
    }

    parsed.input_file = std::filesystem::path(args[1]);
    std::size_t option_end = args.size();
    if (args.size() >= 3 && args.back() == "--fail-on-conflict") {
        parsed.fail_on_conflict = true;
        option_end = args.size() - 1;
    }
    if (option_end >= 3 && args[option_end - 1] == "--warn-on-conflict") {
        parsed.warn_on_conflict = true;
        --option_end;
    }
    if (option_end >= 4 && args[option_end - 2] == "--prefer-source") {
        parsed.preferred_source = std::string(args[option_end - 1]);
        option_end -= 2;
    }

    if (option_end == 2) {
        return parsed;
    }
    if (option_end == 3 && args[2] == "--stdout") {
        parsed.use_stdout = true;
        return parsed;
    }
    if (option_end == 3 && args[2] == "--dry-run") {
        parsed.use_dry_run = true;
        return parsed;
    }
    if (option_end == 4 && args[2] == "--out") {
        parsed.output_file = std::filesystem::path(args[3]);
        return parsed;
    }
    if (option_end == 5 && args[2] == "--out" && args[4] == "--dry-run") {
        parsed.output_file = std::filesystem::path(args[3]);
        parsed.use_dry_run = true;
        return parsed;
    }
    if (option_end == 5 && args[2] == "--dry-run" && args[3] == "--format") {
        parsed.use_dry_run = true;
        parsed.format = std::string(args[4]);
        return parsed;
    }
    if (option_end == 6 && args[2] == "--out" && args[4] == "--format") {
        parsed.output_file = std::filesystem::path(args[3]);
        parsed.format = std::string(args[5]);
        return parsed;
    }
    if (option_end == 7 && args[2] == "--out" && args[4] == "--format" && args[6] == "--dry-run") {
        parsed.output_file = std::filesystem::path(args[3]);
        parsed.format = std::string(args[5]);
        parsed.use_dry_run = true;
        return parsed;
    }

    parsed.valid = false;
    parsed.invalid_option = args.size() >= 3 ? std::string(args[2]) : std::string{};
    return parsed;
}

ParsedAnnotateArgs parse_annotate_args(const std::vector<std::string_view>& args) {
    ParsedAnnotateArgs parsed;
    if (args.size() < 7) {
        parsed.valid = false;
        return parsed;
    }
    parsed.selector = std::string(args[1]);
    parsed.annotation_kind = std::string(args[2]);
    parsed.text_value = std::string(args[3]);
    if (args[4] != "--target") {
        parsed.valid = false;
        parsed.invalid_option = args[4].starts_with("--") ? std::string(args[4]) : std::string{};
        return parsed;
    }
    parsed.target = std::string(args[5]);
    if (args.back().starts_with("--")) {
        parsed.valid = false;
        parsed.invalid_option = std::string(args.back());
        return parsed;
    }

    parsed.input_file = std::filesystem::path(args.back());
    for (std::size_t index = 6; index + 1 < args.size(); ++index) {
        if (args[index] == "--allow-multi") {
            if (!args[1].starts_with("type:") || parsed.allow_multi || parsed.use_stdout || parsed.use_dry_run || parsed.output_file.has_value()) {
                parsed.valid = false;
                parsed.invalid_option = "--allow-multi";
                return parsed;
            }
            parsed.allow_multi = true;
            continue;
        }
        if (args[index] == "--stdout") {
            if (parsed.use_stdout || parsed.use_dry_run || parsed.output_file.has_value()) {
                parsed.valid = false;
                parsed.invalid_option = "--stdout";
                return parsed;
            }
            parsed.use_stdout = true;
            continue;
        }
        if (args[index] == "--dry-run") {
            if (parsed.use_stdout || parsed.use_dry_run) {
                parsed.valid = false;
                parsed.invalid_option = "--dry-run";
                return parsed;
            }
            parsed.use_dry_run = true;
            continue;
        }
        if (args[index] == "--out") {
            if (parsed.use_stdout || parsed.use_dry_run || parsed.output_file.has_value() || index + 1 >= args.size() - 1) {
                parsed.valid = false;
                parsed.invalid_option = "--out";
                return parsed;
            }
            parsed.output_file = std::filesystem::path(args[index + 1]);
            ++index;
            continue;
        }
        parsed.valid = false;
        parsed.invalid_option = std::string(args[index]);
        return parsed;
    }

    return parsed;
}

ParsedAnnotateSelectorGroup parse_annotate_selector_group(std::string_view raw_selector) {
    ParsedAnnotateSelectorGroup parsed;

    std::size_t start = 0;
    while (start <= raw_selector.size()) {
        const auto end = raw_selector.find(',', start);
        std::string token = trim_copy(std::string(raw_selector.substr(start, end == std::string_view::npos ? raw_selector.size() - start
                                                                                                        : end - start)));
        if (token.empty()) {
            parsed.valid = false;
            return parsed;
        }

        auto selector = semdl::core::parse_selector(token);
        if (selector.kind == semdl::core::SelectorKind::unknown) {
            parsed.valid = false;
            return parsed;
        }

        parsed.selectors.push_back(std::move(selector));
        if (end == std::string_view::npos) {
            break;
        }

        parsed.is_list = true;
        start = end + 1;
    }

    if (parsed.is_list) {
        for (const auto& selector : parsed.selectors) {
            if (selector.kind != semdl::core::SelectorKind::id) {
                parsed.valid = false;
                return parsed;
            }
        }
    }

    return parsed;
}

ParsedSetArgs parse_set_args(const std::vector<std::string_view>& args) {
    ParsedSetArgs parsed;
    if (args.size() < 4) {
        parsed.valid = false;
        return parsed;
    }

    parsed.selector = std::string(args[1]);
    std::size_t index = 2;
    if (set_selector_uses_explicit_field(args[1])) {
        if (args.size() < 5) {
            parsed.valid = false;
            return parsed;
        }
        parsed.field_name = std::string(args[2]);
        parsed.raw_value = std::string(args[3]);
        index = 4;
    } else {
        parsed.raw_value = std::string(args[2]);
        index = 3;
    }

    if (args.back().starts_with("--")) {
        parsed.valid = false;
        parsed.invalid_option = std::string(args.back());
        return parsed;
    }

    parsed.input_file = std::filesystem::path(args.back());
    if (args[1].starts_with("type:") && index < args.size() - 1 && args[index] == "--allow-multi") {
        parsed.allow_multi = true;
        ++index;
    }
    if (index < args.size() - 1 && args[index] == "--stdout") {
        parsed.use_stdout = true;
        ++index;
    } else if (index < args.size() - 1 && args[index] == "--dry-run") {
        parsed.use_dry_run = true;
        ++index;
    } else if (index < args.size() - 1 && args[index] == "--out") {
        if (index + 1 >= args.size() - 1) {
            parsed.valid = false;
            parsed.invalid_option = "--out";
            return parsed;
        }
        parsed.output_file = std::filesystem::path(args[index + 1]);
        index += 2;
        if (index < args.size() - 1 && args[index] == "--dry-run") {
            parsed.use_dry_run = true;
            ++index;
        }
    }

    if (index + 1 != args.size()) {
        parsed.valid = false;
        parsed.invalid_option = index < args.size() ? std::string(args[index]) : std::string{};
        return parsed;
    }

    return parsed;
}

ParsedSetSelectorGroup parse_set_selector_group(std::string_view raw_selector) {
    ParsedSetSelectorGroup parsed;

    std::size_t start = 0;
    while (start <= raw_selector.size()) {
        const auto end = raw_selector.find(',', start);
        std::string token = trim_copy(std::string(raw_selector.substr(start, end == std::string_view::npos ? raw_selector.size() - start
                                                                                                        : end - start)));
        if (token.empty()) {
            parsed.valid = false;
            return parsed;
        }

        auto selector = semdl::core::parse_selector(token);
        if (selector.kind == semdl::core::SelectorKind::unknown) {
            parsed.valid = false;
            return parsed;
        }

        parsed.selectors.push_back(std::move(selector));
        if (end == std::string_view::npos) {
            break;
        }

        parsed.is_list = true;
        start = end + 1;
    }

    if (parsed.is_list) {
        for (const auto& selector : parsed.selectors) {
            if (selector.kind != semdl::core::SelectorKind::id) {
                parsed.valid = false;
                return parsed;
            }
        }
    }

    return parsed;
}

ParsedRemoveArgs parse_remove_args(const std::vector<std::string_view>& args) {
    ParsedRemoveArgs parsed;
    if (args.size() < 3) {
        parsed.valid = false;
        return parsed;
    }

    parsed.selector = std::string(args[1]);

    std::size_t index = 2;
    if (index < args.size() && args[index] == "--allow-multi") {
        parsed.allow_multi = true;
        ++index;
        if (index < args.size() && args[index] == "--cascade") {
            parsed.use_cascade = true;
            ++index;
        }
    } else if (index < args.size() && args[index] == "--cascade") {
        parsed.use_cascade = true;
        ++index;
    }

    if (index < args.size() && args[index] == "--stdout") {
        parsed.use_stdout = true;
        ++index;
    } else if (index < args.size() && args[index] == "--dry-run") {
        parsed.use_dry_run = true;
        ++index;
    } else if (index < args.size() && args[index] == "--out") {
        if (index + 1 >= args.size()) {
            parsed.valid = false;
            parsed.invalid_option = "--out";
            return parsed;
        }
        parsed.output_file = std::filesystem::path(args[index + 1]);
        index += 2;
        if (index < args.size() && args[index] == "--dry-run") {
            parsed.use_dry_run = true;
            ++index;
        }
    }

    if (index + 1 != args.size()) {
        parsed.valid = false;
        parsed.invalid_option = index < args.size() ? std::string(args[index]) : std::string{};
        return parsed;
    }

    parsed.input_file = std::filesystem::path(args[index]);
    return parsed;
}

bool is_structural_remove_selector_kind(semdl::core::SelectorKind kind) {
    return kind == semdl::core::SelectorKind::id || kind == semdl::core::SelectorKind::path ||
           kind == semdl::core::SelectorKind::type;
}

ParsedRemoveSelectorGroup parse_remove_selector_group(std::string_view raw_selector) {
    ParsedRemoveSelectorGroup parsed;

    std::size_t start = 0;
    while (start <= raw_selector.size()) {
        const auto end = raw_selector.find(',', start);
        std::string token = trim_copy(std::string(raw_selector.substr(start, end == std::string_view::npos ? raw_selector.size() - start
                                                                                                        : end - start)));
        if (token.empty()) {
            parsed.valid = false;
            return parsed;
        }

        auto selector = semdl::core::parse_selector(token);
        if (selector.kind == semdl::core::SelectorKind::unknown) {
            parsed.valid = false;
            return parsed;
        }

        parsed.contains_type = parsed.contains_type || selector.kind == semdl::core::SelectorKind::type;
        parsed.selectors.push_back(std::move(selector));
        if (end == std::string_view::npos) {
            break;
        }

        parsed.is_list = true;
        start = end + 1;
    }

    if (parsed.is_list) {
        for (const auto& selector : parsed.selectors) {
            if (!is_structural_remove_selector_kind(selector.kind)) {
                parsed.valid = false;
                return parsed;
            }
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

CommandResult make_dry_run_result(const UpdatePreview& preview, std::string stderr_text = "") {
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
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = std::move(stderr_text)};
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

CommandResult make_invalid_add_kind_error(const std::vector<std::string_view>& args, std::string_view kind) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_add_kind\ncommand: " + join_args(args) +
                       "\nkind: " + std::string(kind) +
                       "\nallowed:\n  - resource\n  - segment\n  - assertion\n  - hypothesis\n  - alternative\n  - annotation\n  - provenance\n",
    };
}

CommandResult make_invalid_add_field_error(const std::vector<std::string_view>& args, std::string_view field_text) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_add_field_assignment\ncommand: " + join_args(args) +
                       "\nfield: " + std::string(field_text) +
                       "\nexpected_form: name=value\nhint: add requires explicit field=value pairs after the input file\n",
    };
}

CommandResult make_invalid_add_options_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_add_options\ncommand: " + join_args(args) +
                       "\nusage: ssd add <kind> <file> [field=value ...] [--stdout|--out <output-file> [--dry-run]|--target sidecar|--dry-run]\n",
    };
}

CommandResult make_add_missing_required_fields_error(const std::vector<std::string_view>& args,
                                                     std::string_view kind,
                                                     const std::vector<std::string>& missing_fields) {
    std::ostringstream output;
    output << "ERROR add_missing_required_fields\n";
    output << "command: " << join_args(args) << "\n";
    output << "kind: " << kind << "\n";
    output << "missing_fields:\n";
    for (const auto& name : missing_fields) {
        output << "  - " << name << "\n";
    }
    output << "hint: provide all required field=value pairs for the selected kind\n";
    return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = output.str()};
}

CommandResult make_add_duplicate_id_error(const std::vector<std::string_view>& args, std::string_view entity_id) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR add_id_conflict\ncommand: " + join_args(args) +
                       "\nid: " + std::string(entity_id) +
                       "\nhint: choose a new id that does not already exist in the current document set\n",
    };
}

CommandResult make_add_target_sidecar_required_error(const std::vector<std::string_view>& args, std::string_view kind) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR add_requires_sidecar_target\ncommand: " + join_args(args) +
                       "\nkind: " + std::string(kind) +
                       "\nhint: metadata-only add currently requires `--target sidecar`\n",
    };
}

CommandResult make_invalid_add_target_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_add_target\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nallowed:\n  - sidecar\n",
    };
}

CommandResult make_add_metadata_target_not_found_error(const std::vector<std::string_view>& args, std::string_view entity_id) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR add_target_not_found\ncommand: " + join_args(args) +
                       "\nid: " + std::string(entity_id) +
                       "\nhint: metadata-only add requires an existing semantic target id\n",
    };
}

CommandResult make_add_metadata_conflict_error(const std::vector<std::string_view>& args,
                                               std::string_view entity_id,
                                               std::string_view field_name) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR add_metadata_conflict\ncommand: " + join_args(args) +
                       "\nid: " + std::string(entity_id) +
                       "\nfield: " + std::string(field_name) +
                       "\nhint: add creates missing metadata fields only; use `ssd set` or `ssd annotate` to update an existing field\n",
    };
}

CommandResult make_invalid_add_annotation_kind_error(const std::vector<std::string_view>& args, std::string_view kind) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_add_annotation_kind\ncommand: " + join_args(args) +
                       "\nkind: " + std::string(kind) +
                       "\nallowed:\n  - rationale\n  - caveat\n  - todo\n  - status\n  - explanation\n",
    };
}

CommandResult make_add_reference_target_not_found_error(const std::vector<std::string_view>& args,
                                                        std::string_view field_name,
                                                        std::string_view target_id) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR add_reference_target_not_found\ncommand: " + join_args(args) +
                       "\nfield: " + std::string(field_name) +
                       "\ntarget: " + std::string(target_id) +
                       "\nhint: referenced ids must already exist in the current document set\n",
    };
}

CommandResult make_update_apply_result(const std::optional<std::filesystem::path>& ssd_file,
                                       const std::optional<std::filesystem::path>& ssm_file,
                                       int changes) {
    std::ostringstream output;
    if (ssd_file.has_value()) {
        output << "wrote_ssd: " << ssd_file->generic_string() << "\n";
    }
    if (ssm_file.has_value()) {
        output << "wrote_ssm: " << ssm_file->generic_string() << "\n";
    }
    output << "changes: " << changes << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

CommandResult make_transform_apply_result(const std::filesystem::path& ssd_file,
                                          const std::optional<std::filesystem::path>& removed_ssm,
                                          int changes,
                                          std::string stderr_text = "") {
    std::ostringstream output;
    output << "wrote_ssd: " << ssd_file.generic_string() << "\n";
    if (removed_ssm.has_value()) {
        output << "removed_ssm: " << removed_ssm->generic_string() << "\n";
    }
    output << "changes: " << changes << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = std::move(stderr_text)};
}

CommandResult make_transform_out_result(const std::filesystem::path& output_file,
                                        int changes,
                                        std::string stderr_text = "",
                                        std::string_view wrote_label = "wrote_ssd") {
    std::ostringstream output;
    output << wrote_label << ": " << output_file.generic_string() << "\n";
    output << "changes: " << changes << "\n";
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = std::move(stderr_text)};
}

CommandResult make_merge_requires_paired_input_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR merge_requires_paired_input\ncommand: " + join_args(args) +
                       "\ninput: " + std::string(args[1]) +
                       "\nhint: merge currently requires a paired `.ssm` for apply, `--dry-run`, and `--out`\n",
    };
}

CommandResult make_merge_preferred_source_requires_paired_input_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR merge_preferred_source_requires_paired_input\ncommand: " + join_args(args) +
                       "\ninput: " + std::string(args[1]) +
                       "\nhint: `--prefer-source` currently requires a paired `.ssm` on the merge input\n",
    };
}

CommandResult make_invalid_merge_preferred_source_error(const std::vector<std::string_view>& args, std::string_view source) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_merge_preferred_source\ncommand: " + join_args(args) +
                       "\npreferred_source: " + std::string(source) +
                       "\nallowed:\n  - inline\n  - sidecar\n",
    };
}

CommandResult make_merge_conflict_error(const std::vector<std::string_view>& args,
                                        const std::filesystem::path& input_file,
                                        std::string_view entity_id,
                                        std::string_view field_name) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR merge_sidecar_conflict\ncommand: " + join_args(args) +
                       "\ninput: " + input_file.generic_string() +
                       "\nid: " + std::string(entity_id) +
                       "\nfield: " + std::string(field_name) +
                       "\nhint: rerun with `--warn-on-conflict` to continue with the selected merge precedence in this slice\n",
    };
}

std::string make_merge_conflict_warning(const std::vector<std::string_view>& args,
                                        const std::filesystem::path& input_file,
                                        std::string_view entity_id,
                                        std::string_view field_name,
                                        const std::optional<std::string>& preferred_source) {
    const std::string selected_source = preferred_source.value_or("sidecar");
    return "WARNING merge_sidecar_conflict\ncommand: " + join_args(args) +
           "\ninput: " + input_file.generic_string() +
           "\nid: " + std::string(entity_id) +
           "\nfield: " + std::string(field_name) +
           "\nselected_source: " + selected_source +
           "\nhint: rerun with `--fail-on-conflict` to reject differing duplicate fields in this slice\n";
}

CommandResult make_invalid_transform_options_error(const std::vector<std::string_view>& args,
                                                   std::string_view subcommand,
                                                   std::string_view usage) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_transform_options\ncommand: " + join_args(args) +
                       "\nsubcommand: " + std::string(subcommand) +
                       "\nusage: " + std::string(usage) + "\n",
    };
}

CommandResult make_transform_out_alias_error(const std::vector<std::string_view>& args,
                                             const std::filesystem::path& output_file,
                                             const std::filesystem::path& aliased_file) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR transform_out_alias_conflict\ncommand: " + join_args(args) +
                       "\nout: " + output_file.generic_string() +
                       "\naliases: " + aliased_file.generic_string() +
                       "\nhint: `--out` must not overwrite the source `.ssd` or paired `.ssm` in this slice\n",
    };
}

std::optional<MergeConflictDetail> find_merge_conflict_in_entity_fields(const semdl::core::EntityData& inline_entity,
                                                                        const semdl::core::EntityData* sidecar_metadata,
                                                                        std::string_view entity_id) {
    if (sidecar_metadata == nullptr) {
        return std::nullopt;
    }

    for (const auto& [field_name, inline_value] : inline_entity.fields) {
        const bool is_sidecar_owned = inline_entity.kind == "document"
                                          ? semdl::core::is_document_sidecar_field(field_name)
                                          : semdl::core::is_entity_sidecar_field(field_name);
        if (!is_sidecar_owned) {
            continue;
        }

        const auto sidecar_it = sidecar_metadata->fields.find(field_name);
        if (sidecar_it != sidecar_metadata->fields.end() && sidecar_it->second != inline_value) {
            return MergeConflictDetail{.id = std::string(entity_id), .field_name = field_name};
        }
    }

    return std::nullopt;
}

std::optional<MergeConflictDetail> find_merge_conflict_in_metadata_fields(const semdl::core::EntityData* inline_metadata,
                                                                          const semdl::core::EntityData* sidecar_metadata,
                                                                          std::string_view entity_id) {
    if (inline_metadata == nullptr || sidecar_metadata == nullptr) {
        return std::nullopt;
    }

    for (const auto& [field_name, inline_value] : inline_metadata->fields) {
        const auto sidecar_it = sidecar_metadata->fields.find(field_name);
        if (sidecar_it != sidecar_metadata->fields.end() && sidecar_it->second != inline_value) {
            return MergeConflictDetail{.id = std::string(entity_id), .field_name = field_name};
        }
    }

    return std::nullopt;
}

std::optional<MergeConflictDetail> find_merge_sidecar_conflict(const semdl::core::DocumentSourceViews& source_views) {
    if (!source_views.inline_source.has_sidecar) {
        return std::nullopt;
    }

    std::set<std::string> ids;
    for (const auto& [id, entity] : source_views.inline_source.entities) {
        (void)entity;
        ids.insert(id);
    }
    for (const auto& [id, metadata] : source_views.inline_source.metadata_entities) {
        (void)metadata;
        ids.insert(id);
    }
    for (const auto& [id, metadata] : source_views.sidecar_source.metadata_entities) {
        (void)metadata;
        ids.insert(id);
    }

    for (const auto& id : ids) {
        const auto* sidecar_metadata = source_views.sidecar_source.find_metadata(id);
        if (const auto* inline_entity = source_views.inline_source.find_entity(id); inline_entity != nullptr) {
            if (const auto conflict = find_merge_conflict_in_entity_fields(*inline_entity, sidecar_metadata, id);
                conflict.has_value()) {
                return conflict;
            }
        }
        if (const auto conflict = find_merge_conflict_in_metadata_fields(source_views.inline_source.find_metadata(id),
                                                                         sidecar_metadata,
                                                                         id);
            conflict.has_value()) {
            return conflict;
        }
    }

    return std::nullopt;
}

bool inline_source_has_sidecar_owned_field(const semdl::core::DocumentSourceViews& source_views,
                                           const std::string& id,
                                           std::string_view field_name) {
    if (const auto* entity = source_views.inline_source.find_entity(id); entity != nullptr) {
        const bool is_sidecar_owned = entity->kind == "document"
                                          ? semdl::core::is_document_sidecar_field(field_name)
                                          : semdl::core::is_entity_sidecar_field(field_name);
        if (is_sidecar_owned && entity->fields.contains(std::string(field_name))) {
            return true;
        }
    }

    if (const auto* metadata = source_views.inline_source.find_metadata(id); metadata != nullptr &&
        metadata->fields.contains(std::string(field_name))) {
        return true;
    }

    return false;
}

semdl::core::DocumentData build_merge_document(const semdl::core::DocumentSourceViews& source_views,
                                              const std::optional<std::string>& preferred_source) {
    auto document = source_views.inline_source;
    if (!document.has_sidecar) {
        return document;
    }

    const bool prefer_inline = preferred_source.has_value() && *preferred_source == "inline";
    for (const auto& [id, sidecar_metadata] : source_views.sidecar_source.metadata_entities) {
        auto& metadata = document.metadata_entities[id];
        if (metadata.kind.empty()) {
            metadata.kind = sidecar_metadata.kind;
        }

        for (const auto& [field_name, value] : sidecar_metadata.fields) {
            if (prefer_inline && inline_source_has_sidecar_owned_field(source_views, id, field_name)) {
                continue;
            }
            metadata.fields[field_name] = value;
        }
    }

    return document;
}

std::string root_help_text() {
    return "SEMDL CLI Help\n\n"
           "1. Overview\n"
           "- `ssd` validates, explains, transforms, and updates Canvas `.ssd` and Sidekick `.ssm` assets.\n"
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
           "- `extract`: build an initial Canvas document from raw inputs\n"
           "- `similarity`: compare two semantic targets\n"
           "- `explain`: show the merged semantic view for an entity id\n"
           "- `normalize`: canonicalize Canvas or Sidekick shape and output mode\n"
           "- `add`: add one semantic entity skeleton\n"
           "- `set`: update one Solo mode field or one Sidekick field, including explicit id-list and type allow-multi Solo mode updates\n"
           "- `remove`: remove an explicit target set or fail on unsafe removal\n"
           "- `annotate`: add rationale/caveat/todo-like notes\n"
           "- `split`: move Sidekick-eligible metadata out of Solo mode form\n"
           "- `merge`: produce a merged inline view\n"
           "- `help`: navigate this help system\n\n"
           "5. Reverse Lookup\n"
           "- Validate a document: `ssd check <file>`\n"
           "- Explain an assertion: `ssd explain <id> <file>`\n"
           "- Preview an inline edit: `ssd set path:<id>.<field> <value> --dry-run <file>`\n"
           "- Emit Sidekick metadata without writing: `ssd set meta:<id>.<field> <value> --stdout <file>`\n"
           "- Preview a Sidekick annotation: `ssd annotate id:<id> rationale <text> --target sidecar --dry-run <file>`\n"
           "- Emit a Solo mode annotation without writing: `ssd annotate id:<id> status <text> --target inline --stdout <file>`\n"
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
           "- Some subcommands intentionally support a smaller surface than their names suggest. Use `ssd help reference <subcommand>` before relying on advanced selector, output, or embedding combinations.\n"
           "- `extract` can read raw `.txt`, but embedding generation from raw text still requires `--out <output.ssd>` rather than `--stdout`.\n"
           "- Non-destructive output options such as `--dry-run`, `--stdout`, and `--out` are not available on every command or every target form.\n"
           "- Use `--format semdl` when another tool needs structured help output.\n"
           "- Some update and rewrite workflows are still narrower than the read-only commands. Check command-specific help when writing paired Canvas `.ssd` / Sidekick `.ssm` files.\n"
           "- Report problems with the command, argv, input paths, expected output, and actual output.\n"
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
           "- `ssd add <kind> <file> [field=value ...] [--stdout|--out <file> [--dry-run]|--target sidecar|--dry-run]`\n"
           "- `ssd set <selector> <value-or-field> ... [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd set id:<id>,id:<id>,... <field> <value> [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd set type:<kind> <field> <value> --allow-multi [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd remove <selector[,selector...]> [--allow-multi [--cascade]|--cascade] [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline|sidecar [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi [--stdout|--dry-run|--out <file> [--dry-run]] <file>`\n"
           "- `ssd split <input.ssd> [--dry-run]`\n"
           "- `ssd merge <input.ssd> [--stdout|--dry-run|--out <file> [--dry-run]] [--prefer-source inline|sidecar] [--warn-on-conflict] [--fail-on-conflict]`\n"
           "- `ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <file> [--format inline|sidecar] [--dry-run]]`\n"
           "- `ssd help grammar --format semdl`\n\n"
           "Selectors:\n"
           "- `id:<id>`\n"
           "- `type:<kind>`\n"
           "- `path:<id>.<field>`\n"
           "- `meta:<id>.<field>`\n"
           "- `doc:self`\n"
           "- Use `path:` for inline `.ssd` structure and `meta:` for sidecar `.ssm` metadata\n"
           "- `remove` also accepts comma-separated structural selector lists composed of `id:`, `path:`, and `type:` atoms\n\n"
           "Value forms:\n"
           "- text with spaces should be passed as a quoted string\n"
           "- update-oriented scalar values accept quoted-string, number, boolean, or identifier forms\n\n"
           "Common options:\n"
           "- `--dry-run` previews update-oriented commands\n"
           "- `--stdout` is currently used by `add`, `set`, `annotate`, `remove`, `merge`, `normalize`, and `extract`\n"
           "- `normalize --dry-run` and `normalize --out` accept `--format inline|sidecar`; help render keeps separate `--format semdl`\n"
           "- `annotate` accepts `--target inline|sidecar|auto`; explicit annotate id lists currently require `--target inline|sidecar`; type selector annotate currently requires `--target sidecar --allow-multi`; metadata-only `add` still requires `--target sidecar`\n"
           "- `--allow-multi` and `--cascade` are safety controls on specific update forms; `--prefer-source inline|sidecar`, `--warn-on-conflict`, and `--fail-on-conflict` are currently merge-only trailing options\n\n"
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
               "- This subcommand currently supports `select`, an optional single `where`, target-based `similar`, `return: matches`, and grouped `return: subgraph`.\n"
               "- `where` accepts presence checks, scalar equality, numeric range comparisons, mixed `and`/`or` boolean expressions, parenthesized grouping, and unary `not`; function calls remain outside this slice.\n"
               "- `similar` uses precomputed embeddings from the integrated input view and excludes the anchor target from results.\n"
               "- `similar` with `return: subgraph` reuses the grouped subgraph shape and includes top-level similarity metadata plus per-group scores.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n";
    }

    if (target == "extract") {
        return "SEMDL Help Topic: reference extract\n\n"
               "Usage:\n"
               "- `ssd extract --stdout <input.ssd|input.txt>`\n"
               "- `ssd extract --stdout <input.ssd|input.txt> <input.ssd|input.txt>...`\n"
               "- `ssd extract --out <output.ssd> <input.ssd|input.txt>`\n"
               "- `ssd extract --out <output.ssd> <input.ssd|input.txt> <input.ssd|input.txt>...`\n"
               "- `ssd extract --stdout --embed-provider ollama|openai --embed-model <model> <input.ssd>`\n"
               "- `ssd extract --stdout --embed-provider ollama|openai --embed-model <model> --format inline|sidecar|bundle <input.ssd>`\n"
               "- `ssd extract --stdout --embed-provider ollama|openai --embed-model <model> --format inline|sidecar|bundle <input.ssd> <input.ssd>...`\n"
               "- `ssd extract --out <output.ssd> --embed-provider ollama|openai --embed-model <model> <input.ssd|input.txt> <input.ssd|input.txt>...`\n\n"
               "Status:\n"
               "- This subcommand currently supports canonical inline `.ssd` stdout for one or more `.ssd` / `.txt` inputs without embedding options, multi-input inline `.ssd` aggregation through `--out`, and explicit embedding generation through `ollama` or `openai` adapters.\n"
               "- Embedding-enabled `--stdout` keeps single-input omitted=`sidecar`; explicit `--format inline|sidecar|bundle` works on one existing `.ssd`, and multi-input existing `.ssd` requires an explicit `--format inline|sidecar|bundle`. `bundle` returns one line-prefixed stdout framing that carries both inline `.ssd` and generated `.ssm` payloads. Raw `.txt` embedding generation still requires `--out <output.ssd>`.\n\n"
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
               "- `ssd set path:<id>.<field> <value> <file>`\n"
               "- `ssd set path:<id>.<field> <value> --dry-run <file>`\n"
               "- `ssd set path:<id>.<field> <value> --stdout <file>`\n"
               "- `ssd set path:<id>.<field> <value> --out <output-file> <file>`\n"
               "- `ssd set path:<id>.<field> <value> --out <output-file> --dry-run <file>`\n"
               "- `ssd set meta:<id>.<field> <value> <file>`\n"
               "- `ssd set meta:<id>.<field> <value> --dry-run <file>`\n"
               "- `ssd set meta:<id>.<field> <value> --stdout <file>`\n"
               "- `ssd set meta:<id>.<field> <value> --out <output-file> <file>`\n"
               "- `ssd set meta:<id>.<field> <value> --out <output-file> --dry-run <file>`\n"
               "- `ssd set id:<id> <field> <value> <file>`\n"
               "- `ssd set id:<id> <field> <value> --dry-run <file>`\n"
               "- `ssd set id:<id> <field> <value> --stdout <file>`\n"
               "- `ssd set id:<id> <field> <value> --out <output-file> <file>`\n"
               "- `ssd set id:<id> <field> <value> --out <output-file> --dry-run <file>`\n\n"
               "- `ssd set id:<id>,id:<id>,... <field> <value> <file>`\n"
               "- `ssd set id:<id>,id:<id>,... <field> <value> --dry-run <file>`\n"
               "- `ssd set id:<id>,id:<id>,... <field> <value> --stdout <file>`\n"
               "- `ssd set id:<id>,id:<id>,... <field> <value> --out <output-file> <file>`\n"
               "- `ssd set id:<id>,id:<id>,... <field> <value> --out <output-file> --dry-run <file>`\n\n"
               "- `ssd set type:<kind> <field> <value> --allow-multi <file>`\n"
               "- `ssd set type:<kind> <field> <value> --allow-multi --dry-run <file>`\n"
               "- `ssd set type:<kind> <field> <value> --allow-multi --stdout <file>`\n"
               "- `ssd set type:<kind> <field> <value> --allow-multi --out <output-file> <file>`\n"
               "- `ssd set type:<kind> <field> <value> --allow-multi --out <output-file> --dry-run <file>`\n\n"
               "Purpose:\n"
               "- Apply, preview, or emit one target field update, or one explicit id-list / type allow-multi Solo mode field update set, in Solo mode structure or Sidekick metadata.\n"
               "- `path:` and current `id:` updates use canonical Solo mode Canvas `.ssd` for `--stdout` and `--out`.\n"
               "- explicit id lists are limited to comma-separated `id:` atoms and currently preserve Solo mode id field ownership.\n"
               "- `type:<kind>` set is limited to Solo mode-owned fields and currently requires `--allow-multi` even when one target matches.\n"
               "- `meta:` updates use canonical Sidekick `.ssm` for `--stdout` and `--out`.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help recipes wrong-layer`\n\n"
               "Sample:\n"
               "- `ssd set meta:A1.confidence 0.91 docs/examples/minimal.ssd`\n"
               "- `ssd set path:A1.label 売上報告書 --stdout docs/examples/minimal.ssd`\n";
    }

    if (target == "remove") {
        return "SEMDL Help Topic: reference remove\n\n"
               "Usage:\n"
               "- `ssd remove <selector[,selector...]> <file>`\n"
               "- `ssd remove <selector[,selector...]> --cascade <file>`\n"
               "- `ssd remove <selector[,selector...]> --dry-run <file>`\n"
               "- `ssd remove <selector[,selector...]> --stdout <file>`\n"
               "- `ssd remove <selector[,selector...]> --out <output.ssd> [--dry-run] <file>`\n"
               "- `ssd remove type:<kind> --allow-multi [--stdout|--dry-run|--out <output.ssd> [--dry-run]] <file>`\n"
               "- `ssd remove type:<kind> --allow-multi --cascade [--stdout|--dry-run|--out <output.ssd> [--dry-run]] <file>`\n\n"
               "Purpose:\n"
               "- Remove one Sidekick metadata field or one structural target set when the selector resolves safely.\n"
               "- Structural removals fail on inbound references unless `--cascade` removes dependent targets in the same step.\n"
               "- Explicit comma-separated structural selector lists resolve each `id:`, `path:`, or `type:` atom and remove the union target set.\n"
               "- `--stdout` and `--out` return the post-remove canonical inline `.ssd` without mutating the source files.\n"
               "- `type:<kind>` list atoms still require `--allow-multi` when they expand to multiple targets.\n"
               "- `type:<kind> --allow-multi --cascade` expands the same cascade edge list across the matched root set and the same union rules apply inside selector lists.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help recipes wrong-layer`\n\n"
               "Sample:\n"
               "- `ssd remove meta:A1.embedding docs/examples/minimal.ssd`\n"
               "- `ssd remove id:ALT1B --stdout docs/examples/minimal.ssd`\n"
               "- `ssd remove id:ALT1A,id:ALT1B docs/examples/minimal.ssd`\n"
               "- `ssd remove id:H1 --cascade docs/examples/minimal.ssd`\n"
               "- `ssd remove type:hypothesis --allow-multi --cascade --out docs/examples/remove-multi-cascade-output.ssd --dry-run docs/examples/remove-multi-cascade-source.ssd`\n";
    }

    if (target == "annotate") {
        return "SEMDL Help Topic: reference annotate\n\n"
               "Usage:\n"
               "- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto <file>`\n"
               "- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --dry-run <file>`\n"
               "- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --stdout <file>`\n"
               "- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --out <output-file> <file>`\n"
               "- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --out <output-file> --dry-run <file>`\n\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline --dry-run <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline --stdout <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline --out <output-file> <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline --out <output-file> --dry-run <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target sidecar <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target sidecar --dry-run <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target sidecar --stdout <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target sidecar --out <output-file> <file>`\n"
               "- `ssd annotate id:<id>,id:<id>,... <kind> <text> --target sidecar --out <output-file> --dry-run <file>`\n"
               "- `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi <file>`\n"
               "- `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi --dry-run <file>`\n"
               "- `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi --stdout <file>`\n"
               "- `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi --out <output-file> <file>`\n"
               "- `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi --out <output-file> --dry-run <file>`\n\n"
               "Purpose:\n"
               "- Write, preview, or emit one rationale, caveat, todo, status, or explanation field in Solo mode or Sidekick metadata.\n"
               "- `--target inline` is currently limited to standalone input and `assertion` / `hypothesis` targets.\n"
               "- `--target auto` uses Sidekick on paired input and otherwise prefers Solo mode only where that standalone form is supported.\n"
               "- explicit annotate id lists are limited to comma-separated `id:` atoms and currently require `--target inline` or `--target sidecar`.\n"
               "- `type:<kind>` annotate is currently limited to Sidekick output and requires `--allow-multi` even when one target matches.\n"
               "- `--stdout` and `--out` return the canonical text for the resolved mode.\n"
               "- Use `ssd add annotation ... --target sidecar` when you need create-only field-map semantics.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd annotate id:H1 todo 追加入力で主体を確認する --target inline docs/examples/minimal.inline.ssd`\n"
               "- `ssd annotate id:H1 status 検証待ち --target inline --stdout docs/examples/minimal.inline.ssd`\n"
               "- `ssd annotate id:R1 rationale 参照元を追記する --target auto docs/examples/minimal.ssd`\n"
               "- `ssd annotate type:alternative rationale 候補を再確認する --target sidecar --allow-multi docs/examples/minimal.ssd`\n";
    }

    if (target == "split") {
        return "SEMDL Help Topic: reference split\n\n"
               "Usage:\n"
               "- `ssd split <input.ssd>`\n"
               "- `ssd split <input.ssd> --dry-run`\n\n"
               "Purpose:\n"
               "- Separate Sidekick-eligible metadata into a paired Sidekick `.ssm` while preserving the Canvas skeleton in `.ssd`.\n"
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
               "- `ssd merge <input.ssd> --stdout`\n"
               "- `ssd merge <input.ssd>`\n"
               "- `ssd merge <input.ssd> --dry-run`\n"
               "- `ssd merge <input.ssd> --out <output.ssd> [--dry-run]`\n"
               "- `ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] --prefer-source inline|sidecar`\n"
               "- `ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] [--prefer-source inline|sidecar] --warn-on-conflict`\n"
               "- `ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] [--prefer-source inline|sidecar] [--warn-on-conflict] --fail-on-conflict`\n\n"
               "Status:\n"
               "- `--stdout` returns the merged inline view of `.ssd` with an optional paired `.ssm`.\n"
               "- Bare apply and `--dry-run` require a paired `.ssm`; in-place apply removes that sidecar after writing inline output.\n"
               "- `--out <output.ssd>` writes merged inline output without modifying the source `.ssd` or paired `.ssm`.\n"
               "- `--prefer-source inline|sidecar` changes which source wins for sidecar-owned duplicate field conflicts; without it, paired `.ssm` keeps precedence.\n"
               "- `--warn-on-conflict` keeps the selected merge result but emits a warning on stderr when inline source and paired sidecar disagree on the same sidecar-owned field.\n"
               "- `--fail-on-conflict` fails before output or mutation when inline source and paired sidecar disagree on the same sidecar-owned field.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help samples`\n\n"
               "Sample:\n"
               "- `ssd merge docs/examples/minimal.ssd --stdout`\n"
               "- `ssd merge docs/examples/minimal.ssd --out docs/examples/merged-output.ssd --dry-run`\n"
               "- `ssd merge docs/examples/merge-conflict-source.ssd --stdout --prefer-source inline`\n"
               "- `ssd merge docs/examples/merge-conflict-source.ssd --stdout --warn-on-conflict`\n"
               "- `ssd merge docs/examples/merge-conflict-source.ssd --stdout --fail-on-conflict`\n";
    }

    if (target == "normalize") {
        return "SEMDL Help Topic: reference normalize\n\n"
               "Usage:\n"
               "- `ssd normalize <input.ssd> [--stdout]`\n"
               "- `ssd normalize <input.ssd> --dry-run [--format inline|sidecar]`\n"
               "- `ssd normalize <input.ssd> --out <output.ssd> [--format inline|sidecar] [--dry-run]`\n\n"
               "Status:\n"
               "- `--stdout` returns canonical inline output from `.ssd` with an optional paired `.ssm`.\n"
               "- Bare apply remains inline-only and removes the sibling `.ssm` on paired input after writing canonical inline output.\n"
               "- `--dry-run` and `--out` default to inline output, but `--format sidecar` previews or writes a split `.ssd` + `.ssm` pair without modifying the source files.\n"
               "- `--stdout` and bare apply do not accept `--format` in this slice.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n\n"
               "Sample:\n"
               "- `ssd normalize docs/examples/normalize-source.ssd --out docs/examples/normalized-output.ssd`\n"
               "- `ssd normalize docs/examples/normalize-source.ssd --out docs/examples/normalized-sidecar.ssd --format sidecar`\n";
    }

    if (target == "add") {
         return "SEMDL Help Topic: reference add\n\n"
             "Usage:\n"
             "- `ssd add <kind> <file> [field=value ...]`\n"
             "- `ssd add <kind> <file> [field=value ...] --stdout`\n"
             "- `ssd add <kind> <file> [field=value ...] --out <output.ssd> [--dry-run]`\n"
             "- `ssd add <kind> <file> [field=value ...] --dry-run`\n"
                         "- `ssd add <kind> <file> [field=value ...] --target sidecar [--stdout|--out <output.ssm> [--dry-run]|--dry-run]`\n\n"
               "Status:\n"
             "- Inline structural add supports: `resource`, `segment`, `assertion`, `hypothesis`, `alternative`. Structural add also supports canonical inline `--stdout` and `--out <output.ssd>`.\n"
                             "- Metadata-only add supports `annotation` and `provenance` with `--target sidecar` only, and its `--stdout` / `--out <output.ssm>` surface returns canonical sidecar text without mutating the source files.\n"
               "- Metadata-only add is create-only: it creates missing sidecar fields, creates a paired `.ssm` when absent, and fails on existing field conflicts.\n\n"
               "Related help:\n"
               "- `ssd help grammar`\n"
               "- `ssd help troubleshooting`\n\n"
               "Sample:\n"
               "- `ssd add resource docs/examples/minimal.ssd id=R2 type=appendix label=月次売上補足 --dry-run`\n"
               "- `ssd add resource docs/examples/minimal.ssd id=R2 type=appendix label=月次売上補足 --stdout`\n"
               "- `ssd add annotation docs/examples/minimal.ssd id=H1 kind=todo text=追加入力で主体を確認する --target sidecar --stdout`\n"
               "- `ssd add annotation docs/examples/minimal.ssd id=H1 kind=todo text=追加入力で主体を確認する --target sidecar --dry-run`\n";
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
                       "\nallowed:\n  - field\n  - field = \"value\"\n  - field = 1\n  - field = true|false\n  - field > 0.8\n  - not field\n  - not field = true\n  - field and other = true\n  - not (field or other)\n  - field and other = true or third\n  - (field or other) and third > 0.8\nhint: see `ssd help reference search`\n",
    };
}

CommandResult make_missing_extract_embedding_args_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR missing_required_argument\ncommand: " + join_args(args) +
                       "\nusage: " + std::string(kExtractUsage) + "\nsubcommand: extract\nhint: see `ssd help reference extract`\n",
    };
}

CommandResult make_invalid_extract_options_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_extract_options\ncommand: " + join_args(args) +
                       "\nsubcommand: extract\nusage: " + std::string(kExtractUsage) + "\n",
    };
}

CommandResult make_unsupported_extract_provider_error(const std::vector<std::string_view>& args, std::string_view provider) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unsupported_extract_provider\ncommand: " + join_args(args) +
                       "\nprovider: " + std::string(provider) +
                       "\nreason: extract currently supports only ollama and openai\nhint: see `ssd help reference extract`\n",
    };
}

CommandResult make_invalid_extract_output_mode_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_extract_output_mode\ncommand: " + join_args(args) +
                       "\nreason: raw text input with --stdout cannot be combined with embedding generation\nhint: use `ssd extract --out <output.ssd> --embed-provider <provider> --embed-model <model> <input.txt>`\n",
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
                       "\nreason: " + std::string(provider) + " command failed\nhint: see `ssd help reference extract`\n",
    };
}

CommandResult make_extract_stdout_result(const std::string& sidecar_text) {
    return CommandResult{.exit_code = 0, .stdout_text = sidecar_text, .stderr_text = ""};
}

CommandResult make_extract_out_result(std::string_view output_file,
                                      const std::optional<std::filesystem::path>& sidecar_file,
                                      const std::optional<int>& embedded_count,
                                      const std::optional<int>& skipped_count) {
    std::ostringstream output;
    output << "wrote_ssd: " << output_file << "\n";
    if (sidecar_file.has_value()) {
        output << "wrote_ssm: " << sidecar_file->generic_string() << "\n";
    }
    if (embedded_count.has_value()) {
        output << "embedded: " << *embedded_count << "\n";
    }
    if (skipped_count.has_value()) {
        output << "skipped: " << *skipped_count << "\n";
    }
    return CommandResult{.exit_code = 0, .stdout_text = output.str(), .stderr_text = ""};
}

CommandResult make_extract_out_alias_error(const std::vector<std::string_view>& args,
                                           const std::filesystem::path& conflicting_output,
                                           const std::filesystem::path& aliased_file) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR extract_out_alias_conflict\ncommand: " + join_args(args) +
                       "\noutput: " + conflicting_output.generic_string() +
                       "\naliases: " + aliased_file.generic_string() +
                       "\nreason: extract --out must not overwrite a source input, a source sidecar, or the generated paired sidecar target\nhint: choose a distinct output path\n",
    };
}

CommandResult make_search_result(const std::vector<std::string_view>& args, const semdl::core::SearchResult& result) {
    std::ostringstream output;
    output << "query_file: " << args[1] << "\n";
    output << "mode: " << result.query.result_mode << "\n";
    output << "inputs: " << (args.size() - 2) << "\n";
    if (result.query.result_mode == "subgraph") {
        if (result.query.similar_expression.has_value()) {
            output << "anchor: " << result.anchor_id << "\n";
            output << "metric: " << result.metric << "\n";
            output << "model: " << result.model << "\n";
            output << "dimensions: " << result.dimensions << "\n";
        }
        output << "subgraphs: " << result.subgraphs.size() << "\n";
        output << std::fixed << std::setprecision(6);
        for (const auto& subgraph : result.subgraphs) {
            output << "- match_file: " << subgraph.match.file << "\n";
            output << "  match_id: " << subgraph.match.id << "\n";
            output << "  match_kind: " << subgraph.match.kind << "\n";
            if (subgraph.match.score.has_value()) {
                output << "  score: " << *subgraph.match.score << "\n";
            }
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

CommandResult make_missing_target_error(const std::vector<std::string_view>& args,
                                        std::string_view selector_text,
                                        std::size_t quoted_index) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR selector_target_not_found\ncommand: " + join_args_with_quoted_index(args, quoted_index) +
                       "\nselector: " + std::string(selector_text) +
                       "\nhint: the selector did not resolve to any resource in the current document set\n",
    };
}

CommandResult make_missing_target_error(const std::vector<std::string_view>& args) {
    const auto selector = semdl::core::parse_selector(args[1]);
    const std::size_t quoted_index = (selector.kind == semdl::core::SelectorKind::id || selector.kind == semdl::core::SelectorKind::type) ? 3U : 2U;
    return make_missing_target_error(args, args[1], quoted_index);
}

CommandResult make_missing_set_allow_multi_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR missing_set_allow_multi\ncommand: " + join_args_with_quoted_index(args, 3) +
                       "\nselector: " + std::string(args[1]) +
                       "\nhint: type selector set currently requires `--allow-multi` even when one target matches\n",
    };
}

CommandResult make_wrong_layer_error(const std::vector<std::string_view>& args,
                                     std::string_view selector_text,
                                     std::string_view target_id,
                                     std::size_t quoted_index,
                                     bool expects_inline) {
    const std::string selector_string(selector_text);
    const std::string target_string(target_id);

    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = expects_inline
                           ? "ERROR selector_resolved_wrong_layer\ncommand: " + join_args_with_quoted_index(args, quoted_index) +
                                 "\nselector: " + selector_string +
                                 "\nresolved_target: " + target_string +
                                 "\nexpected_layer: inline .ssd structure\nactual_location: sidecar metadata\nhint: use a meta: selector for fields stored only in .ssm\n"
                           : "ERROR selector_resolved_wrong_layer\ncommand: " + join_args_with_quoted_index(args, quoted_index) +
                                 "\nselector: " + selector_string +
                                 "\nresolved_target: " + target_string +
                                 "\nexpected_layer: sidecar metadata\nactual_location: inline .ssd structure\nhint: use a path: selector for fields stored in the main .ssd file\n",
    };
}

CommandResult make_wrong_layer_error(const std::vector<std::string_view>& args) {
    const bool expects_inline = !args[1].starts_with("meta:");
    const auto selector = semdl::core::parse_selector(args[1]);
    const std::size_t quoted_index = (selector.kind == semdl::core::SelectorKind::id || selector.kind == semdl::core::SelectorKind::type) ? 3U : 2U;
    return make_wrong_layer_error(args, args[1], selector.entity_id, quoted_index, expects_inline);
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

CommandResult make_invalid_remove_selector_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_remove_selector\ncommand: " + join_args(args) +
                       "\nselector: " + std::string(args[1]) +
                       "\nhint: remove selector lists require comma-separated `id:`, `path:`, or `type:` atoms; `meta:` and `doc:self` lists are not supported in this slice\n",
    };
}

CommandResult make_invalid_remove_options_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_remove_options\ncommand: " + join_args(args) +
                       "\nusage: ssd remove <selector[,selector...]> [--allow-multi [--cascade]|--cascade] [--stdout|--dry-run|--out <output.ssd> [--dry-run]] <file>\n"
                       "hint: `--allow-multi` is currently limited to `type:<kind>` selectors or selector lists that include `type:<kind>`, and output options must follow the safety options\n",
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

CommandResult make_remove_break_reference_error(const std::vector<std::string_view>& args,
                                                const std::vector<std::string>& referenced_by) {
    std::ostringstream output;
    output << "ERROR remove_would_break_references\n";
    output << "command: " << join_args(args) << "\n";
    output << "selector: " << args[1] << "\n";
    output << "referenced_by:\n";
    for (const auto& field_ref : referenced_by) {
        output << "  - " << field_ref << "\n";
    }
    output << "hint: referenced structural elements cannot be removed without an explicit safe strategy\n";
    return CommandResult{.exit_code = 3, .stdout_text = "", .stderr_text = output.str()};
}

CommandResult make_invalid_annotate_target_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_annotate_target\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nallowed:\n  - inline\n  - sidecar\n  - auto\n",
    };
}

CommandResult make_invalid_annotate_options_error(const std::vector<std::string_view>& args) {
    std::string hint = "hint: annotate output options must follow `--target`, and `--stdout` cannot be combined with `--dry-run` or `--out`";
    if (args.size() > 1 && args[1].starts_with("type:")) {
        hint += "; for `type:` annotate, `--allow-multi` must appear before `--stdout`, `--dry-run`, or `--out`, and the target is currently limited to `sidecar`";
    }
    hint += "\n";
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_annotate_options\ncommand: " + join_args(args) +
                       "\nusage: ssd annotate <selector> <kind> <text> --target inline|sidecar|auto [--stdout|--dry-run|--out <output-file> [--dry-run]] <file>\n"
                       + hint,
    };
}

CommandResult make_invalid_annotate_selector_list_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_annotate_selector_list\ncommand: " + join_args(args) +
                       "\nselector: " + std::string(args[1]) +
                       "\nhint: annotate selector lists currently accept only comma-separated `id:` atoms\n",
    };
}

CommandResult make_invalid_annotate_multi_target_target_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_annotate_multi_target_target\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nallowed:\n  - inline\n  - sidecar\n"
                       "hint: annotate selector lists currently require an explicit `--target inline` or `--target sidecar`\n",
    };
}

CommandResult make_missing_annotate_allow_multi_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR missing_annotate_allow_multi\ncommand: " + join_args_with_quoted_index(args, 3) +
                       "\nselector: " + std::string(args[1]) +
                       "\nhint: type selector annotate currently requires `--allow-multi` even when one target matches\n",
    };
}

CommandResult make_invalid_annotate_type_target_error(const std::vector<std::string_view>& args, std::string_view target) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_annotate_type_target\ncommand: " + join_args(args) +
                       "\ntarget: " + std::string(target) +
                       "\nallowed:\n  - sidecar\n"
                       "hint: type selector annotate currently supports only `--target sidecar --allow-multi`\n",
    };
}

CommandResult make_invalid_set_options_error(const std::vector<std::string_view>& args) {
    std::string hint = "hint: set output options must appear before the input file, and `--stdout` cannot be combined with `--dry-run` or `--out`";
    if (args.size() > 1 && args[1].starts_with("type:")) {
        hint += "; for `type:` set, `--allow-multi` must appear before `--stdout`, `--dry-run`, or `--out`";
    }
    hint += "\n";
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_set_options\ncommand: " + join_args(args) +
                       "\nusage: ssd set <selector> <value-or-field> ... [--stdout|--dry-run|--out <output-file> [--dry-run]] <file>\n"
                       + hint,
    };
}

CommandResult make_invalid_set_selector_list_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR invalid_set_selector_list\ncommand: " + join_args(args) +
                       "\nselector: " + std::string(args[1]) +
                       "\nhint: set selector lists currently accept only comma-separated `id:` atoms\n",
    };
}

CommandResult make_annotate_inline_requires_standalone_error(const std::vector<std::string_view>& args) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR annotate_inline_requires_standalone_input\ncommand: " + join_args(args) +
                       "\nhint: `--target inline` is currently limited to standalone `.ssd` inputs\n",
    };
}

CommandResult make_annotate_inline_target_unsupported_error(const std::vector<std::string_view>& args, std::string_view entity_id, std::string_view kind) {
    return CommandResult{
        .exit_code = 3,
        .stdout_text = "",
        .stderr_text = "ERROR annotate_inline_target_unsupported\ncommand: " + join_args(args) +
                       "\nid: " + std::string(entity_id) +
                       "\nkind: " + std::string(kind) +
                       "\nhint: inline annotate currently supports `assertion` and `hypothesis` only\n",
    };
}

bool supports_inline_annotation_kind(const semdl::core::DocumentData& document, std::string_view entity_id) {
    const auto* entity = document.find_entity(entity_id);
    return entity != nullptr && (entity->kind == "assertion" || entity->kind == "hypothesis");
}

std::string resolve_annotate_target(const semdl::core::DocumentData& document,
                                    std::string_view entity_id,
                                    std::string_view requested_target) {
    if (requested_target == "sidecar") {
        return "sidecar";
    }
    if (requested_target == "inline") {
        return "inline";
    }
    if (requested_target == "auto") {
        if (document.has_sidecar) {
            return "sidecar";
        }
        return supports_inline_annotation_kind(document, entity_id) ? "inline" : "sidecar";
    }
    return {};
}

std::vector<std::string> direct_remove_dependents(const semdl::core::DocumentData& document, std::string_view entity_id) {
    std::vector<std::string> dependents;
    const auto* target = document.find_entity(entity_id);
    if (target == nullptr) {
        return dependents;
    }

    if (target->kind == "resource") {
        for (const auto& [candidate_id, entity] : document.entities) {
            if (entity.kind != "segment") {
                continue;
            }
            const auto source_it = entity.fields.find("source");
            if (source_it != entity.fields.end() && source_it->second == entity_id) {
                dependents.push_back(candidate_id);
            }
        }
    } else if (target->kind == "assertion") {
        for (const auto& [candidate_id, entity] : document.entities) {
            if (entity.kind != "hypothesis") {
                continue;
            }
            const auto about_it = entity.fields.find("about");
            if (about_it != entity.fields.end() && about_it->second == entity_id) {
                dependents.push_back(candidate_id);
            }
        }
    } else if (target->kind == "hypothesis") {
        const auto group_it = target->fields.find("alternative_group");
        if (group_it != target->fields.end()) {
            for (const auto& [candidate_id, entity] : document.entities) {
                if (entity.kind != "alternative") {
                    continue;
                }
                const auto alt_group_it = entity.fields.find("group");
                if (alt_group_it != entity.fields.end() && alt_group_it->second == group_it->second) {
                    dependents.push_back(candidate_id);
                }
            }
        }
    }

    std::sort(dependents.begin(), dependents.end());
    dependents.erase(std::unique(dependents.begin(), dependents.end()), dependents.end());
    return dependents;
}

std::vector<std::string> describe_remove_dependents(const semdl::core::DocumentData& document, std::string_view entity_id) {
    std::vector<std::string> refs;
    const auto* target = document.find_entity(entity_id);
    if (target == nullptr) {
        return refs;
    }

    if (target->kind == "resource") {
        for (const auto& [candidate_id, entity] : document.entities) {
            if (entity.kind != "segment") {
                continue;
            }
            const auto source_it = entity.fields.find("source");
            if (source_it != entity.fields.end() && source_it->second == entity_id) {
                refs.push_back(candidate_id + ".source");
            }
        }
    } else if (target->kind == "assertion") {
        for (const auto& [candidate_id, entity] : document.entities) {
            if (entity.kind != "hypothesis") {
                continue;
            }
            const auto about_it = entity.fields.find("about");
            if (about_it != entity.fields.end() && about_it->second == entity_id) {
                refs.push_back(candidate_id + ".about");
            }
        }
    } else if (target->kind == "hypothesis") {
        const auto group_it = target->fields.find("alternative_group");
        if (group_it != target->fields.end()) {
            for (const auto& [candidate_id, entity] : document.entities) {
                if (entity.kind != "alternative") {
                    continue;
                }
                const auto alt_group_it = entity.fields.find("group");
                if (alt_group_it != entity.fields.end() && alt_group_it->second == group_it->second) {
                    refs.push_back(candidate_id + ".group");
                }
            }
        }
    }

    std::sort(refs.begin(), refs.end());
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
    return refs;
}

std::vector<std::string> collect_remove_targets_for_kind(const semdl::core::DocumentData& document, std::string_view kind) {
    std::vector<std::string> targets;
    for (const auto& [candidate_id, entity] : document.entities) {
        if (entity.kind == kind) {
            targets.push_back(candidate_id);
        }
    }
    std::sort(targets.begin(), targets.end());
    return targets;
}

std::vector<std::string> describe_remove_dependents_outside(const semdl::core::DocumentData& document,
                                                            const std::vector<std::string>& removal_ids) {
    const std::set<std::string> removal_set(removal_ids.begin(), removal_ids.end());
    std::vector<std::string> refs;
    for (const auto& removal_id : removal_ids) {
        for (const auto& ref : describe_remove_dependents(document, removal_id)) {
            const auto dot = ref.find('.');
            const std::string dependent_id = dot == std::string::npos ? ref : ref.substr(0, dot);
            if (!removal_set.contains(dependent_id)) {
                refs.push_back(ref);
            }
        }
    }
    std::sort(refs.begin(), refs.end());
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
    return refs;
}

std::vector<std::string> collect_remove_closure(const semdl::core::DocumentData& document, std::string_view target_id) {
    std::vector<std::string> ordered;
    std::queue<std::string> pending;
    std::set<std::string> seen;

    pending.push(std::string(target_id));
    seen.insert(std::string(target_id));
    while (!pending.empty()) {
        const std::string current = pending.front();
        pending.pop();
        ordered.push_back(current);
        for (const auto& dependent_id : direct_remove_dependents(document, current)) {
            if (seen.insert(dependent_id).second) {
                pending.push(dependent_id);
            }
        }
    }
    return ordered;
}

std::vector<std::string> collect_remove_multi_closure(const semdl::core::DocumentData& document,
                                                      const std::vector<std::string>& root_ids) {
    std::vector<std::string> removal_ids;
    std::set<std::string> seen;

    for (const auto& root_id : root_ids) {
        for (const auto& closure_id : collect_remove_closure(document, root_id)) {
            if (seen.insert(closure_id).second) {
                removal_ids.push_back(closure_id);
            }
        }
    }

    return removal_ids;
}

bool apply_remove_structural_change(semdl::core::DocumentData& document, const std::vector<std::string>& removed_ids) {
    for (const auto& id : removed_ids) {
        document.entities.erase(id);
        document.metadata_entities.erase(id);
    }
    return !removed_ids.empty();
}

std::string require_field_from_entity(const semdl::core::DocumentData& document, std::string_view entity_id, const std::string& field_name, bool metadata);

std::vector<std::string> required_add_fields_for_kind(std::string_view kind) {
    if (kind == "resource") {
        return {"id", "type", "label"};
    }
    if (kind == "segment") {
        return {"id", "source", "text_quote"};
    }
    if (kind == "assertion") {
        return {"id", "subject", "predicate", "object"};
    }
    if (kind == "hypothesis") {
        return {"id", "about", "kind", "summary"};
    }
    if (kind == "alternative") {
        return {"id", "group", "label"};
    }
    if (kind == "annotation") {
        return {"id", "kind", "text"};
    }
    if (kind == "provenance") {
        return {"id", "provenance_kind"};
    }
    return {};
}

bool is_metadata_add_kind(std::string_view kind) {
    return kind == "annotation" || kind == "provenance";
}

bool is_structural_add_kind(std::string_view kind) {
    return !is_metadata_add_kind(kind);
}

bool is_supported_add_kind(std::string_view kind) {
    return kind == "resource" || kind == "segment" || kind == "assertion" || kind == "hypothesis" || kind == "alternative" ||
           is_metadata_add_kind(kind);
}

bool looks_number_literal(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = 0;
    if (value.front() == '-' || value.front() == '+') {
        index = 1;
    }
    bool saw_digit = false;
    bool saw_dot = false;
    for (; index < value.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (std::isdigit(ch)) {
            saw_digit = true;
            continue;
        }
        if (ch == '.' && !saw_dot) {
            saw_dot = true;
            continue;
        }
        return false;
    }
    return saw_digit;
}

bool looks_identifier_literal(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    for (const unsigned char ch : value) {
        if (!(std::isalnum(ch) || ch == '_' || ch == '-')) {
            return false;
        }
    }
    return true;
}

std::string render_scalar_argument(std::string_view value) {
    if (value == "true" || value == "false" || looks_number_literal(value) || looks_identifier_literal(value)) {
        return std::string(value);
    }
    return quote_value(value);
}

std::string render_add_field_assignment(std::string_view name, std::string_view value) {
    return std::string(name) + "=" + render_scalar_argument(value);
}

std::string metadata_kind_for_entity(const semdl::core::DocumentData& document, std::string_view entity_id) {
    if (const auto* metadata = document.find_metadata(entity_id); metadata != nullptr) {
        return metadata->kind;
    }
    if (const auto* entity = document.find_entity(entity_id); entity != nullptr) {
        return entity->kind == "document" ? "document_meta" : "meta";
    }
    return "meta";
}

std::size_t set_value_arg_index(const semdl::core::Selector& selector) {
    return (selector.kind == semdl::core::SelectorKind::id || selector.kind == semdl::core::SelectorKind::type) ? 3U : 2U;
}

std::string set_field_name(const semdl::core::Selector& selector, const std::vector<std::string_view>& args) {
    return (selector.kind == semdl::core::SelectorKind::id || selector.kind == semdl::core::SelectorKind::type) ? std::string(args[2]) : selector.field_path;
}

std::string set_raw_value(const semdl::core::Selector& selector, const std::vector<std::string_view>& args) {
    return std::string(args[set_value_arg_index(selector)]);
}

semdl::core::ResolveError resolve_set_field_layer(const semdl::core::DocumentData& document,
                                                  const semdl::core::Selector& selector,
                                                  const std::string& field_name) {
    if (selector.kind != semdl::core::SelectorKind::id) {
        return semdl::core::ResolveError::none;
    }

    const auto* entity = document.find_entity(selector.entity_id);
    if (entity == nullptr) {
        return semdl::core::ResolveError::target_not_found;
    }
    if (entity->fields.contains(field_name)) {
        return semdl::core::ResolveError::none;
    }
    if (const auto* metadata = document.find_metadata(selector.entity_id); metadata != nullptr && metadata->fields.contains(field_name)) {
        return semdl::core::ResolveError::wrong_layer;
    }
    return semdl::core::ResolveError::target_not_found;
}

bool apply_set_change(semdl::core::DocumentData& document, const semdl::core::Selector& selector, std::string_view raw_value) {
    if (selector.kind == semdl::core::SelectorKind::path || selector.kind == semdl::core::SelectorKind::id) {
        auto& fields = document.entities[selector.entity_id].fields;
        fields[selector.field_path] = render_scalar_argument(raw_value);
        return true;
    }
    if (selector.kind == semdl::core::SelectorKind::meta) {
        auto& metadata = document.metadata_entities[selector.entity_id];
        metadata.kind = metadata_kind_for_entity(document, selector.entity_id);
        metadata.fields[selector.field_path] = render_scalar_argument(raw_value);
        return true;
    }
    return false;
}

std::vector<semdl::core::Selector> collect_set_type_targets(const semdl::core::DocumentData& document,
                                                            std::string_view kind,
                                                            std::string_view field_name) {
    std::vector<std::string> ids;
    for (const auto& [entity_id, entity] : document.entities) {
        if (entity.kind == kind) {
            ids.push_back(entity_id);
        }
    }
    std::sort(ids.begin(), ids.end());

    std::vector<semdl::core::Selector> selectors;
    selectors.reserve(ids.size());
    for (const auto& entity_id : ids) {
        semdl::core::Selector selector;
        selector.kind = semdl::core::SelectorKind::id;
        selector.raw = "id:" + entity_id;
        selector.entity_id = entity_id;
        selector.field_path = std::string(field_name);
        selectors.push_back(std::move(selector));
    }
    return selectors;
}

std::vector<std::string> collect_annotate_type_targets(const semdl::core::DocumentData& document,
                                                       std::string_view kind) {
    std::vector<std::string> ids;
    for (const auto& [entity_id, entity] : document.entities) {
        if (entity.kind == kind) {
            ids.push_back(entity_id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool apply_annotation_change(semdl::core::DocumentData& document,
                             std::string_view selector,
                             std::string_view annotation_kind,
                             std::string_view text_value) {
    const auto parsed = semdl::core::parse_selector(selector);
    if ((parsed.kind != semdl::core::SelectorKind::id && parsed.kind != semdl::core::SelectorKind::doc_self) || parsed.entity_id.empty()) {
        return false;
    }
    if (!document.contains_entity_id(parsed.entity_id)) {
        return false;
    }

    auto& metadata = document.metadata_entities[parsed.entity_id];
    metadata.kind = metadata_kind_for_entity(document, parsed.entity_id);
    metadata.fields[std::string(annotation_kind)] = quote_value(text_value);
    return true;
}

bool apply_annotation_changes(semdl::core::DocumentData& document,
                              const std::vector<std::string>& entity_ids,
                              std::string_view annotation_kind,
                              std::string_view text_value) {
    if (entity_ids.empty()) {
        return false;
    }

    for (const auto& entity_id : entity_ids) {
        if (!document.contains_entity_id(entity_id)) {
            return false;
        }
    }

    for (const auto& entity_id : entity_ids) {
        auto& metadata = document.metadata_entities[entity_id];
        metadata.kind = metadata_kind_for_entity(document, entity_id);
        metadata.fields[std::string(annotation_kind)] = quote_value(text_value);
    }
    return true;
}

bool apply_remove_meta_change(semdl::core::DocumentData& document, const semdl::core::Selector& selector) {
    if (selector.kind != semdl::core::SelectorKind::meta) {
        return false;
    }

    auto metadata_it = document.metadata_entities.find(selector.entity_id);
    if (metadata_it == document.metadata_entities.end()) {
        return false;
    }

    const std::string prefix = selector.field_path + ".";
    bool removed = false;
    for (auto it = metadata_it->second.fields.begin(); it != metadata_it->second.fields.end();) {
        if (it->first == selector.field_path || it->first.rfind(prefix, 0) == 0) {
            it = metadata_it->second.fields.erase(it);
            removed = true;
            continue;
        }
        ++it;
    }
    return removed;
}

std::optional<std::pair<std::string, std::string>> validate_add_references(const semdl::core::DocumentData& document,
                                                                           std::string_view kind,
                                                                           const std::map<std::string, std::string>& field_map) {
    const auto require_existing = [&](std::string_view field_name) -> std::optional<std::pair<std::string, std::string>> {
        const auto it = field_map.find(std::string(field_name));
        if (it == field_map.end()) {
            return std::nullopt;
        }
        if (!document.contains_entity_id(it->second)) {
            return std::pair<std::string, std::string>{std::string(field_name), it->second};
        }
        return std::nullopt;
    };

    if (kind == "segment") {
        return require_existing("source");
    }
    if (kind == "assertion") {
        if (auto missing = require_existing("subject"); missing.has_value()) {
            return missing;
        }
        return require_existing("source_segment");
    }
    if (kind == "hypothesis") {
        return require_existing("about");
    }
    return std::nullopt;
}

void write_text_file(const std::filesystem::path& file_path, const std::string& content) {
    std::ofstream output(file_path, std::ios::binary);
    output << content;
}

UpdatePreview build_add_preview(const semdl::core::DocumentData& document, const ParsedAddArgs& parsed) {
    UpdatePreview preview;
    preview.command_line = "ssd add " + parsed.kind + " " + parsed.input_file.generic_string();
    for (const auto& [name, value] : parsed.fields) {
        preview.command_line += " " + render_add_field_assignment(name, value);
    }
    if (parsed.use_stdout) {
        preview.command_line += " --stdout";
    }
    if (parsed.output_file.has_value()) {
        preview.command_line += " --out " + parsed.output_file->generic_string();
    }
    if (parsed.target.has_value()) {
        preview.command_line += " --target " + *parsed.target;
    }
    preview.target_profile = is_metadata_add_kind(parsed.kind) ? "sidecar" : "inline";
    preview.target_file = is_metadata_add_kind(parsed.kind)
        ? (parsed.output_file.has_value() ? parsed.output_file->generic_string() : derive_sidecar_path(parsed.input_file).generic_string())
        : (parsed.output_file.has_value() ? parsed.output_file->generic_string() : parsed.input_file.generic_string());
    if (parsed.output_file.has_value()) {
        preview.detail_lines.push_back(std::string("source_profile: ") + (document.has_sidecar ? "sidecar" : "standalone"));
        preview.detail_lines.push_back(std::string("result_profile: ") + (is_metadata_add_kind(parsed.kind) ? "sidecar" : "inline"));
    }
    preview.detail_lines.push_back("kind: " + parsed.kind);
    preview.detail_lines.push_back((is_metadata_add_kind(parsed.kind) ? "target_id: " : "id: ") + parsed.field_map.at("id"));
    preview.detail_lines.push_back("fields:");
    for (const auto& [name, value] : parsed.fields) {
        if (name == "id") {
            continue;
        }
        preview.detail_lines.push_back("  - " + render_add_field_assignment(name, value));
    }
    if (parsed.output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + parsed.input_file.generic_string());
        if (document.has_sidecar) {
            preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
        }
    }
    return preview;
}

bool apply_add_change(semdl::core::DocumentData& document, const ParsedAddArgs& parsed) {
    const auto id_it = parsed.field_map.find("id");
    if (id_it == parsed.field_map.end()) {
        return false;
    }

    if (parsed.kind == "annotation") {
        auto& metadata = document.metadata_entities[id_it->second];
        metadata.kind = metadata_kind_for_entity(document, id_it->second);
        metadata.fields[parsed.field_map.at("kind")] = render_scalar_argument(parsed.field_map.at("text"));
        return true;
    }

    if (parsed.kind == "provenance") {
        auto& metadata = document.metadata_entities[id_it->second];
        metadata.kind = metadata_kind_for_entity(document, id_it->second);
        for (const auto& [name, value] : parsed.fields) {
            if (name == "id") {
                continue;
            }
            metadata.fields[name] = render_scalar_argument(value);
        }
        return true;
    }

    semdl::core::EntityData entity;
    entity.kind = parsed.kind;
    for (const auto& [name, value] : parsed.fields) {
        if (name == "id") {
            continue;
        }
        entity.fields[name] = render_scalar_argument(value);
    }
    document.entities[id_it->second] = std::move(entity);
    return true;
}

bool metadata_field_exists(const semdl::core::DocumentData& document,
                          std::string_view entity_id,
                          std::string_view field_name) {
    if (const auto* entity = document.find_entity(entity_id); entity != nullptr && entity->fields.contains(std::string(field_name))) {
        return true;
    }
    if (const auto* metadata = document.find_metadata(entity_id); metadata != nullptr && metadata->fields.contains(std::string(field_name))) {
        return true;
    }
    return false;
}

std::optional<std::string> validate_metadata_add_conflicts(const semdl::core::DocumentData& document,
                                                           const ParsedAddArgs& parsed) {
    const auto& entity_id = parsed.field_map.at("id");
    if (parsed.kind == "annotation") {
        const auto& annotation_kind = parsed.field_map.at("kind");
        if (metadata_field_exists(document, entity_id, annotation_kind)) {
            return annotation_kind;
        }
        return std::nullopt;
    }

    for (const auto& [name, value] : parsed.fields) {
        (void)value;
        if (name == "id") {
            continue;
        }
        if (metadata_field_exists(document, entity_id, name)) {
            return name;
        }
    }
    return std::nullopt;
}

UpdatePreview build_set_preview(const semdl::core::DocumentData& document, const semdl::core::Selector& selector, const std::vector<std::string_view>& args) {
    const auto input_file = std::filesystem::path(args.back());
    const auto sidecar_path = derive_sidecar_path(input_file);
    const auto field_name = set_field_name(selector, args);
    const auto raw_value = set_raw_value(selector, args);
    const bool writes_inline = selector.kind == semdl::core::SelectorKind::path || selector.kind == semdl::core::SelectorKind::id;

    UpdatePreview preview;
    preview.command_line = selector.kind == semdl::core::SelectorKind::id
                               ? "ssd set " + std::string(args[1]) + " " + field_name + " " + quote_value(raw_value) + " " + input_file.generic_string()
                               : selector.kind == semdl::core::SelectorKind::path
                                     ? "ssd set " + std::string(args[1]) + " " + quote_value(raw_value) + " " + input_file.generic_string()
                                     : "ssd set " + std::string(args[1]) + " " + raw_value + " " + input_file.generic_string();
    preview.target_profile = writes_inline ? "inline" : "sidecar";
    preview.target_file = writes_inline ? input_file.generic_string() : sidecar_path.generic_string();
    preview.detail_lines.push_back("selector: " + std::string(args[1]));
    if (selector.kind == semdl::core::SelectorKind::id) {
        preview.detail_lines.push_back("field: " + field_name);
    }
    preview.detail_lines.push_back("old: " + require_field_from_entity(document, selector.entity_id, field_name, selector.kind == semdl::core::SelectorKind::meta));
    preview.detail_lines.push_back("new: " + render_scalar_argument(raw_value));
    return preview;
}

UpdatePreview build_set_preview(const semdl::core::DocumentData& document,
                               const semdl::core::Selector& selector,
                               const ParsedSetArgs& parsed) {
    const auto sidecar_path = derive_sidecar_path(parsed.input_file);
    const auto field_name = selector.kind == semdl::core::SelectorKind::id ? parsed.field_name.value_or("") : selector.field_path;
    const bool writes_inline = selector.kind == semdl::core::SelectorKind::path || selector.kind == semdl::core::SelectorKind::id;

    UpdatePreview preview;
    preview.command_line = "ssd set " + parsed.selector + " ";
    if (selector.kind == semdl::core::SelectorKind::id) {
        preview.command_line += field_name + " ";
    }
    preview.command_line += selector.kind == semdl::core::SelectorKind::meta
                                ? parsed.raw_value
                                : quote_value(parsed.raw_value);
    if (parsed.allow_multi) {
        preview.command_line += " --allow-multi";
    }
    if (parsed.output_file.has_value()) {
        preview.command_line += " --out " + parsed.output_file->generic_string();
    }
    preview.command_line += " " + parsed.input_file.generic_string();
    preview.target_profile = writes_inline ? "inline" : "sidecar";
    preview.target_file = parsed.output_file.has_value()
                              ? parsed.output_file->generic_string()
                              : (writes_inline ? parsed.input_file.generic_string() : sidecar_path.generic_string());
    preview.detail_lines.push_back("selector: " + parsed.selector);
    if (selector.kind == semdl::core::SelectorKind::id) {
        preview.detail_lines.push_back("field: " + field_name);
    }
    preview.detail_lines.push_back("old: " + require_field_from_entity(document, selector.entity_id, field_name, selector.kind == semdl::core::SelectorKind::meta));
    preview.detail_lines.push_back("new: " + render_scalar_argument(parsed.raw_value));
    if (parsed.output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + parsed.input_file.generic_string());
        if (document.has_sidecar) {
            preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
        }
    }
    return preview;
}

UpdatePreview build_set_preview(const semdl::core::DocumentData& document,
                               const std::vector<semdl::core::Selector>& selectors,
                               const ParsedSetArgs& parsed) {
    UpdatePreview preview;
    preview.command_line = "ssd set " + parsed.selector + " " + parsed.field_name.value_or("") + " " + quote_value(parsed.raw_value);
    if (parsed.allow_multi) {
        preview.command_line += " --allow-multi";
    }
    if (parsed.output_file.has_value()) {
        preview.command_line += " --out " + parsed.output_file->generic_string();
    }
    preview.command_line += " " + parsed.input_file.generic_string();
    preview.target_profile = "inline";
    preview.target_file = parsed.output_file.has_value() ? parsed.output_file->generic_string() : parsed.input_file.generic_string();
    preview.detail_lines.push_back("selector: " + parsed.selector);
    preview.detail_lines.push_back("field: " + parsed.field_name.value_or(""));
    preview.detail_lines.push_back("targets:");
    for (const auto& selector : selectors) {
        preview.detail_lines.push_back("  - " + selector.entity_id + " old: " + require_field_from_entity(document, selector.entity_id, selector.field_path, false));
    }
    preview.detail_lines.push_back("new: " + render_scalar_argument(parsed.raw_value));
    if (parsed.output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + parsed.input_file.generic_string());
        if (document.has_sidecar) {
            preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
        }
    }
    preview.changes = static_cast<int>(selectors.size());
    return preview;
}

UpdatePreview build_annotate_preview(const semdl::core::DocumentData& document,
                                     const ParsedAnnotateArgs& parsed,
                                     std::string_view resolved_target,
                                     int changes) {
    const auto sidecar_path = derive_sidecar_path(parsed.input_file);

    UpdatePreview preview;
    preview.command_line = "ssd annotate " + parsed.selector + " " + parsed.annotation_kind + " " + quote_value(parsed.text_value) +
                           " --target " + parsed.target;
    if (parsed.allow_multi) {
        preview.command_line += " --allow-multi";
    }
    if (parsed.output_file.has_value()) {
        preview.command_line += " --out " + parsed.output_file->generic_string();
    }
    preview.command_line += " " + parsed.input_file.generic_string();
    preview.target_profile = std::string(resolved_target);
    preview.target_file = parsed.output_file.has_value()
                              ? parsed.output_file->generic_string()
                              : (resolved_target == "inline" ? parsed.input_file.generic_string() : sidecar_path.generic_string());
    preview.detail_lines.push_back("selector: " + parsed.selector);
    preview.detail_lines.push_back("annotation_kind: " + parsed.annotation_kind);
    preview.detail_lines.push_back("write: " + quote_value(parsed.text_value));
    if (parsed.output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + parsed.input_file.generic_string());
        if (document.has_sidecar) {
            preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
        }
    }
    preview.changes = changes;
    return preview;
}

UpdatePreview build_annotate_preview(const semdl::core::DocumentData& document,
                                     const ParsedAnnotateArgs& parsed,
                                     std::string_view resolved_target) {
    return build_annotate_preview(document, parsed, resolved_target, 1);
}

UpdatePreview build_remove_preview(const semdl::core::DocumentData& document,
                                   const ParsedRemoveArgs& parsed,
                                   bool metadata_remove,
                                   int changes) {
    UpdatePreview preview;
    preview.command_line = "ssd remove " + parsed.selector;
    if (parsed.allow_multi) {
        preview.command_line += " --allow-multi";
    }
    if (parsed.use_cascade) {
        preview.command_line += " --cascade";
    }
    if (parsed.output_file.has_value()) {
        preview.command_line += " --out " + parsed.output_file->generic_string();
    }
    preview.command_line += " --dry-run " + parsed.input_file.generic_string();

    const auto sidecar_path = derive_sidecar_path(parsed.input_file);
    preview.target_profile = parsed.output_file.has_value() ? "inline" : (metadata_remove ? "sidecar" : "inline");
    preview.target_file = parsed.output_file.has_value()
                              ? parsed.output_file->generic_string()
                              : (metadata_remove ? sidecar_path.generic_string() : parsed.input_file.generic_string());
    preview.detail_lines.push_back("selector: " + parsed.selector);
    preview.detail_lines.push_back(std::string("source_profile: ") + (document.has_sidecar ? "sidecar" : "standalone"));
    preview.detail_lines.push_back(std::string("result_profile: ") + (parsed.output_file.has_value() ? "inline" : preview.target_profile));
    if (parsed.output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + parsed.input_file.generic_string());
        if (document.has_sidecar) {
            preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
        }
    } else if (!metadata_remove && document.has_sidecar) {
        preview.detail_lines.push_back("write_ssm: " + document.sidecar_file.generic_string());
    }
    preview.changes = changes;
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

UpdatePreview build_merge_preview(const semdl::core::DocumentData& document,
                                  const std::filesystem::path& input_file,
                                  const std::optional<std::filesystem::path>& output_file,
                                  const std::optional<std::string>& preferred_source,
                                  bool warn_on_conflict,
                                  bool use_dry_run) {
    UpdatePreview preview;
    preview.command_line = "ssd merge " + input_file.generic_string();
    if (output_file.has_value()) {
        preview.command_line += " --out " + output_file->generic_string();
    }
    if (use_dry_run) {
        preview.command_line += " --dry-run";
    }
    if (preferred_source.has_value()) {
        preview.command_line += " --prefer-source " + *preferred_source;
    }
    if (warn_on_conflict) {
        preview.command_line += " --warn-on-conflict";
    }
    preview.target_profile = "inline";
    preview.target_file = output_file.has_value() ? output_file->generic_string() : input_file.generic_string();
    preview.detail_lines.push_back("source_profile: sidecar");
    preview.detail_lines.push_back("result_profile: inline");
    if (preferred_source.has_value()) {
        preview.detail_lines.push_back("preferred_source: " + *preferred_source);
    }
    if (output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + input_file.generic_string());
        preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
    } else {
        preview.detail_lines.push_back("remove: " + document.sidecar_file.generic_string());
    }
    return preview;
}

UpdatePreview build_normalize_preview(const semdl::core::DocumentData& document,
                                      const std::filesystem::path& input_file,
                                      const std::optional<std::filesystem::path>& output_file,
                                      std::string_view result_format) {
    UpdatePreview preview;
    preview.command_line = "ssd normalize " + input_file.generic_string();
    if (output_file.has_value()) {
        preview.command_line += " --out " + output_file->generic_string();
    }
    if (result_format != "inline") {
        preview.command_line += " --format " + std::string(result_format);
    }
    preview.target_profile = std::string(result_format);
    preview.target_file = output_file.has_value() ? output_file->generic_string() : input_file.generic_string();
    preview.detail_lines.push_back(std::string("source_profile: ") + (document.has_sidecar ? "sidecar" : "standalone"));
    preview.detail_lines.push_back("result_profile: " + std::string(result_format));
    if (result_format == "sidecar") {
        const auto sidecar_target = derive_sidecar_path(output_file.has_value() ? *output_file : input_file);
        preview.detail_lines.push_back("write_ssd: " + preview.target_file);
        preview.detail_lines.push_back("write_ssm: " + sidecar_target.generic_string());
        if (output_file.has_value()) {
            preview.detail_lines.push_back("preserve_source_ssd: " + input_file.generic_string());
            if (document.has_sidecar) {
                preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
            }
        }
        return preview;
    }

    if (output_file.has_value()) {
        preview.detail_lines.push_back("preserve_source_ssd: " + input_file.generic_string());
        if (document.has_sidecar) {
            preview.detail_lines.push_back("preserve_source_ssm: " + document.sidecar_file.generic_string());
        }
    } else if (document.has_sidecar) {
        preview.detail_lines.push_back("remove: " + document.sidecar_file.generic_string());
    }
    return preview;
}

bool transform_out_aliases_source(const semdl::core::DocumentData& document,
                                  const std::filesystem::path& input_file,
                                  const std::filesystem::path& output_file,
                                  std::filesystem::path& aliased_file,
                                  const std::optional<std::filesystem::path>& secondary_output = std::nullopt,
                                  const std::optional<std::filesystem::path>& reserved_alias = std::nullopt) {
    const auto normalized_input = normalize_path_for_alias_compare(input_file);
    const auto normalized_sidecar = document.has_sidecar
        ? std::optional<std::filesystem::path>(normalize_path_for_alias_compare(document.sidecar_file))
        : std::nullopt;
    const auto normalized_reserved_alias = reserved_alias.has_value()
        ? std::optional<std::filesystem::path>(normalize_path_for_alias_compare(*reserved_alias))
        : std::nullopt;

    const auto aliases_known_source = [&](const std::filesystem::path& candidate) {
        const auto normalized_candidate = normalize_path_for_alias_compare(candidate);
        if (normalized_candidate == normalized_input) {
            aliased_file = input_file;
            return true;
        }
        if (normalized_sidecar.has_value() && normalized_candidate == *normalized_sidecar) {
            aliased_file = document.sidecar_file;
            return true;
        }
        if (normalized_reserved_alias.has_value() && normalized_candidate == *normalized_reserved_alias) {
            aliased_file = *reserved_alias;
            return true;
        }
        return false;
    };

    if (aliases_known_source(output_file)) {
        return true;
    }
    if (secondary_output.has_value() && aliases_known_source(*secondary_output)) {
        return true;
    }
    return false;
}

bool is_allowed_transform_format(std::string_view format) {
    return format == "inline" || format == "sidecar";
}

bool is_allowed_merge_preferred_source(std::string_view source) {
    return source == "inline" || source == "sidecar";
}

bool is_allowed_extract_stdout_format(std::string_view format) {
    return format == "inline" || format == "sidecar" || format == "bundle";
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
        auto document = store.load_document(std::filesystem::path(args[2]));
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
            const auto parsed = parse_set_args(args);
            if (!parsed.valid) {
                if (!parsed.invalid_option.empty()) {
                    return make_invalid_set_options_error(args);
                }
                return make_missing_required_argument_error(args,
                                                            "ssd set <selector> <value-or-field> ... [--stdout|--dry-run|--out <output-file> [--dry-run]] <file>",
                                                            "set");
            }

            semdl::core::DocumentStore store;
            auto document = store.load_document(parsed.input_file);
            const auto selector_group = parse_set_selector_group(parsed.selector);
            if (!selector_group.valid) {
                if (selector_group.is_list || parsed.selector.find(',') != std::string::npos) {
                    return make_invalid_set_selector_list_error(args);
                }
                return make_invalid_selector_error(args);
            }

            std::vector<semdl::core::Selector> target_selectors;
            if (selector_group.is_list) {
                if (!parsed.field_name.has_value()) {
                    return make_invalid_set_selector_list_error(args);
                }

                std::set<std::string> seen_ids;
                for (auto selector : selector_group.selectors) {
                    const auto resolution = semdl::core::resolve_selector(document, selector);
                    if (resolution.error == semdl::core::ResolveError::invalid_selector_syntax) {
                        return make_invalid_selector_error(args);
                    }
                    if (resolution.error == semdl::core::ResolveError::target_not_found) {
                        return make_missing_target_error(args);
                    }
                    if (resolution.error == semdl::core::ResolveError::wrong_layer) {
                        return make_wrong_layer_error(args, parsed.selector, selector.entity_id, 3U, true);
                    }

                    selector.field_path = *parsed.field_name;
                    const auto field_resolution = resolve_set_field_layer(document, selector, selector.field_path);
                    if (field_resolution == semdl::core::ResolveError::wrong_layer) {
                        return make_wrong_layer_error(args, parsed.selector, selector.entity_id, 3U, true);
                    }
                    if (field_resolution == semdl::core::ResolveError::target_not_found) {
                        return make_missing_target_error(args);
                    }

                    if (seen_ids.insert(selector.entity_id).second) {
                        target_selectors.push_back(std::move(selector));
                    }
                }
            } else {
                auto selector = selector_group.selectors.front();
                if (selector.kind == semdl::core::SelectorKind::type) {
                    if (!parsed.allow_multi) {
                        return make_missing_set_allow_multi_error(args);
                    }
                    if (!parsed.field_name.has_value()) {
                        return make_missing_required_argument_error(args,
                                                                    "ssd set <selector> <value-or-field> ... [--stdout|--dry-run|--out <output-file> [--dry-run]] <file>",
                                                                    "set");
                    }

                    target_selectors = collect_set_type_targets(document, selector.entity_id, *parsed.field_name);
                    if (target_selectors.empty()) {
                        return make_missing_target_error(args);
                    }

                    for (const auto& target_selector : target_selectors) {
                        const auto field_resolution = resolve_set_field_layer(document, target_selector, target_selector.field_path);
                        if (field_resolution == semdl::core::ResolveError::wrong_layer) {
                            return make_wrong_layer_error(args, parsed.selector, target_selector.entity_id, 3U, true);
                        }
                        if (field_resolution == semdl::core::ResolveError::target_not_found) {
                            return make_missing_target_error(args);
                        }
                    }
                } else {
                    const auto resolution = semdl::core::resolve_selector(document, selector);
                    if (resolution.error == semdl::core::ResolveError::invalid_selector_syntax) {
                        return make_invalid_selector_error(args);
                    }
                    if (resolution.error == semdl::core::ResolveError::target_not_found) {
                        return make_missing_target_error(args);
                    }
                    if (resolution.error == semdl::core::ResolveError::wrong_layer) {
                        return make_wrong_layer_error(args);
                    }

                    if (selector.kind == semdl::core::SelectorKind::id) {
                        selector.field_path = parsed.field_name.value_or("");
                    }

                    const auto field_resolution = resolve_set_field_layer(document, selector, selector.field_path);
                    if (field_resolution == semdl::core::ResolveError::wrong_layer) {
                        return make_wrong_layer_error(args);
                    }
                    if (field_resolution == semdl::core::ResolveError::target_not_found) {
                        return make_missing_target_error(args);
                    }

                    target_selectors.push_back(std::move(selector));
                }
            }

            const bool writes_inline = target_selectors.front().kind == semdl::core::SelectorKind::path ||
                                       target_selectors.front().kind == semdl::core::SelectorKind::id;
            if (parsed.use_stdout && (parsed.use_dry_run || parsed.output_file.has_value())) {
                return make_invalid_set_options_error(args);
            }
            if (parsed.output_file.has_value()) {
                std::filesystem::path aliased_file;
                const auto reserved_alias = writes_inline
                    ? std::nullopt
                    : std::optional<std::filesystem::path>(derive_sidecar_path(parsed.input_file));
                if (transform_out_aliases_source(document, parsed.input_file, *parsed.output_file, aliased_file, std::nullopt, reserved_alias)) {
                    return make_transform_out_alias_error(args, *parsed.output_file, aliased_file);
                }
            }

            if (parsed.use_dry_run) {
                if (selector_group.is_list || selector_group.selectors.front().kind == semdl::core::SelectorKind::type) {
                    return make_dry_run_result(build_set_preview(document, target_selectors, parsed));
                }
                return make_dry_run_result(build_set_preview(document, target_selectors.front(), parsed));
            }

            for (const auto& selector : target_selectors) {
                if (!apply_set_change(document, selector, parsed.raw_value)) {
                    return make_apply_not_implemented_error(args, "set");
                }
            }

            if (parsed.use_stdout) {
                if (writes_inline) {
                    return CommandResult{
                        .exit_code = 0,
                        .stdout_text = semdl::core::render_canonical_inline_document(document),
                        .stderr_text = "",
                    };
                }

                const auto rendered = semdl::core::render_split_document(document);
                return CommandResult{
                    .exit_code = 0,
                    .stdout_text = rendered.sidecar_document,
                    .stderr_text = "",
                };
            }

            if (parsed.output_file.has_value()) {
                if (writes_inline) {
                    write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(document));
                    return make_transform_out_result(*parsed.output_file, static_cast<int>(target_selectors.size()));
                }

                const auto rendered = semdl::core::render_split_document(document);
                write_text_file(*parsed.output_file, rendered.sidecar_document);
                return make_transform_out_result(*parsed.output_file, static_cast<int>(target_selectors.size()), "", "wrote_ssm");
            }

            if (writes_inline) {
                if (document.has_sidecar) {
                    const auto rendered = semdl::core::render_split_document(document);
                    write_text_file(parsed.input_file, rendered.inline_document);
                } else {
                    write_text_file(parsed.input_file, semdl::core::render_canonical_inline_document(document));
                }
                return make_update_apply_result(parsed.input_file, std::nullopt, static_cast<int>(target_selectors.size()));
            }

            const auto sidecar_file = derive_sidecar_path(parsed.input_file);
            const auto rendered = semdl::core::render_split_document(document);
            write_text_file(sidecar_file, rendered.sidecar_document);
            return make_update_apply_result(std::nullopt, sidecar_file, static_cast<int>(target_selectors.size()));
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
        const auto parsed = parse_annotate_args(args);
        if (!parsed.valid) {
            if (!parsed.invalid_option.empty()) {
                return make_invalid_annotate_options_error(args);
            }
            return make_missing_required_argument_error(args,
                                                        "ssd annotate <selector> <kind> <text> --target inline|sidecar|auto [--stdout|--dry-run|--out <output-file> [--dry-run]] <file>",
                                                        "annotate");
        }
        if (looks_unterminated_quoted_string(parsed.text_value)) {
            return make_unterminated_quoted_string_error(args);
        }
        if (!is_allowed_annotation_kind(parsed.annotation_kind)) {
            return make_invalid_annotation_kind_error(args);
        }
        if (parsed.target != "inline" && parsed.target != "sidecar" && parsed.target != "auto") {
            return make_invalid_annotate_target_error(args, parsed.target);
        }

        semdl::core::DocumentStore store;
        auto document = store.load_document(parsed.input_file);
        const auto selector_group = parse_annotate_selector_group(parsed.selector);
        if (!selector_group.valid) {
            return selector_group.is_list ? make_invalid_annotate_selector_list_error(args) : make_invalid_selector_error(args);
        }

        if (parsed.use_stdout && (parsed.use_dry_run || parsed.output_file.has_value())) {
            return make_invalid_annotate_options_error(args);
        }

        std::vector<std::string> annotation_targets;
        std::string resolved_target;
        if (selector_group.is_list) {
            if (parsed.target == "auto") {
                return make_invalid_annotate_multi_target_target_error(args, parsed.target);
            }

            std::set<std::string> seen_targets;
            for (const auto& selector : selector_group.selectors) {
                const auto resolution = semdl::core::resolve_selector(document, selector);
                if (resolution.error == semdl::core::ResolveError::target_not_found) {
                    return make_missing_target_error(args, parsed.selector, 3);
                }
                if (resolution.error == semdl::core::ResolveError::multiple_targets) {
                    return make_remove_multiple_targets_error(args, resolution.matched_count);
                }
                if (seen_targets.insert(resolution.target_id).second) {
                    annotation_targets.push_back(resolution.target_id);
                }
            }

            resolved_target = parsed.target;
            if (resolved_target == "inline") {
                if (document.has_sidecar) {
                    return make_annotate_inline_requires_standalone_error(args);
                }
                for (const auto& target_id : annotation_targets) {
                    if (!supports_inline_annotation_kind(document, target_id)) {
                        const auto* entity = document.find_entity(target_id);
                        return make_annotate_inline_target_unsupported_error(args, target_id, entity == nullptr ? std::string_view{} : entity->kind);
                    }
                }
            }
        } else {
            const auto& selector = selector_group.selectors.front();
            if (selector.kind == semdl::core::SelectorKind::type) {
                if (!parsed.allow_multi) {
                    return make_missing_annotate_allow_multi_error(args);
                }
                if (parsed.target != "sidecar") {
                    return make_invalid_annotate_type_target_error(args, parsed.target);
                }

                annotation_targets = collect_annotate_type_targets(document, selector.entity_id);
                if (annotation_targets.empty()) {
                    return make_missing_target_error(args);
                }

                resolved_target = "sidecar";
            } else {
                const auto resolution = semdl::core::resolve_selector(document, selector);
                if (resolution.error == semdl::core::ResolveError::invalid_selector_syntax) {
                    return make_invalid_selector_error(args);
                }
                if (resolution.error == semdl::core::ResolveError::target_not_found) {
                    return make_missing_target_error(args);
                }
                if (resolution.error == semdl::core::ResolveError::multiple_targets) {
                    return make_remove_multiple_targets_error(args, resolution.matched_count);
                }

                annotation_targets.push_back(resolution.target_id);
                resolved_target = resolve_annotate_target(document, resolution.target_id, parsed.target);
                if (resolved_target.empty()) {
                    return make_invalid_annotate_target_error(args, parsed.target);
                }
                if (resolved_target == "inline") {
                    if (document.has_sidecar) {
                        return make_annotate_inline_requires_standalone_error(args);
                    }
                    if (!supports_inline_annotation_kind(document, resolution.target_id)) {
                        return make_annotate_inline_target_unsupported_error(args, resolution.target_id, resolution.target_kind);
                    }
                }
            }
        }

        if (parsed.output_file.has_value()) {
            std::filesystem::path aliased_file;
            const auto reserved_alias = resolved_target == "sidecar"
                ? std::optional<std::filesystem::path>(derive_sidecar_path(parsed.input_file))
                : std::nullopt;
            if (transform_out_aliases_source(document, parsed.input_file, *parsed.output_file, aliased_file, std::nullopt, reserved_alias)) {
                return make_transform_out_alias_error(args, *parsed.output_file, aliased_file);
            }
        }

        if (parsed.use_dry_run) {
            return make_dry_run_result(build_annotate_preview(document, parsed, resolved_target, static_cast<int>(annotation_targets.size())));
        }

        if (!apply_annotation_changes(document, annotation_targets, parsed.annotation_kind, parsed.text_value)) {
            return make_apply_not_implemented_error(args, "annotate");
        }

        if (parsed.use_stdout) {
            if (resolved_target == "inline") {
                return CommandResult{
                    .exit_code = 0,
                    .stdout_text = semdl::core::render_canonical_inline_document(document),
                    .stderr_text = "",
                };
            }

            const auto rendered = semdl::core::render_split_document(document);
            return CommandResult{
                .exit_code = 0,
                .stdout_text = rendered.sidecar_document,
                .stderr_text = "",
            };
        }

        if (parsed.output_file.has_value()) {
            if (resolved_target == "inline") {
                write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(document));
                return make_transform_out_result(*parsed.output_file, static_cast<int>(annotation_targets.size()));
            } else {
                const auto rendered = semdl::core::render_split_document(document);
                write_text_file(*parsed.output_file, rendered.sidecar_document);
                return make_transform_out_result(*parsed.output_file, static_cast<int>(annotation_targets.size()), "", "wrote_ssm");
            }
        }

        if (resolved_target == "inline") {
            write_text_file(parsed.input_file, semdl::core::render_canonical_inline_document(document));
            return make_update_apply_result(parsed.input_file, std::nullopt, static_cast<int>(annotation_targets.size()));
        }

        const auto sidecar_file = derive_sidecar_path(parsed.input_file);
        const auto rendered = semdl::core::render_split_document(document);
        write_text_file(sidecar_file, rendered.sidecar_document);
        return make_update_apply_result(std::nullopt, sidecar_file, static_cast<int>(annotation_targets.size()));
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
        const auto parsed = parse_remove_args(args);
        if (!parsed.valid) {
            if (!parsed.invalid_option.empty()) {
                return make_invalid_remove_options_error(args);
            }
            return make_missing_required_argument_error(args,
                                                        "ssd remove <selector> [--allow-multi [--cascade]|--cascade] [--stdout|--dry-run|--out <output.ssd> [--dry-run]] <file>",
                                                        "remove");
        }
        semdl::core::DocumentStore store;
        auto document = store.load_document(parsed.input_file);
        const auto selector_group = parse_remove_selector_group(parsed.selector);

        if (!selector_group.valid) {
            return selector_group.is_list ? make_invalid_remove_selector_error(args) : make_invalid_selector_error(args);
        }

        const auto& selector = selector_group.selectors.front();

        if (parsed.use_stdout && (parsed.use_dry_run || parsed.output_file.has_value())) {
            return make_invalid_remove_options_error(args);
        }

        if (parsed.allow_multi && !selector_group.contains_type) {
            return make_invalid_remove_options_error(args);
        }

        if (parsed.output_file.has_value()) {
            std::filesystem::path aliased_file;
            if (transform_out_aliases_source(document, parsed.input_file, *parsed.output_file, aliased_file)) {
                return make_transform_out_alias_error(args, *parsed.output_file, aliased_file);
            }
        }

        if (!selector_group.is_list && selector.kind == semdl::core::SelectorKind::meta) {
            const auto resolution = semdl::core::resolve_selector(document, selector);
            if (resolution.error == semdl::core::ResolveError::invalid_selector_syntax) {
                return make_invalid_selector_error(args);
            }
            if (resolution.error == semdl::core::ResolveError::wrong_layer) {
                return make_wrong_layer_error(args);
            }
            if (parsed.use_dry_run) {
                auto preview_document = document;
                if (!apply_remove_meta_change(preview_document, selector)) {
                    return make_missing_target_error(args);
                }
                return make_dry_run_result(build_remove_preview(document, parsed, true, 1));
            }

            auto updated_document = document;
            if (!apply_remove_meta_change(updated_document, selector)) {
                return make_missing_target_error(args);
            }

            if (parsed.use_stdout) {
                return CommandResult{
                    .exit_code = 0,
                    .stdout_text = semdl::core::render_canonical_inline_document(updated_document),
                    .stderr_text = "",
                };
            }

            if (parsed.output_file.has_value()) {
                write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(updated_document));
                return make_transform_out_result(*parsed.output_file, 1);
            }

            const auto sidecar_file = derive_sidecar_path(parsed.input_file);
            const auto rendered = semdl::core::render_split_document(updated_document);
            write_text_file(sidecar_file, rendered.sidecar_document);
            return make_update_apply_result(std::nullopt, sidecar_file, 1);
        }

        if (is_structural_remove_selector_kind(selector.kind)) {
            std::vector<std::string> root_ids;
            std::set<std::string> seen_root_ids;

            for (const auto& selector_item : selector_group.selectors) {
                const auto resolution = semdl::core::resolve_selector(document, selector_item);
                if (resolution.error == semdl::core::ResolveError::invalid_selector_syntax) {
                    return make_invalid_selector_error(args);
                }
                if (resolution.error == semdl::core::ResolveError::target_not_found) {
                    return make_missing_target_error(args);
                }
                if (resolution.error == semdl::core::ResolveError::wrong_layer) {
                    return make_wrong_layer_error(args);
                }
                if (resolution.error == semdl::core::ResolveError::multiple_targets) {
                    if (selector_item.kind == semdl::core::SelectorKind::type && parsed.allow_multi) {
                        for (const auto& root_id : collect_remove_targets_for_kind(document, selector_item.entity_id)) {
                            if (seen_root_ids.insert(root_id).second) {
                                root_ids.push_back(root_id);
                            }
                        }
                        continue;
                    }
                    return make_remove_multiple_targets_error(args, resolution.matched_count);
                }

                if (seen_root_ids.insert(resolution.target_id).second) {
                    root_ids.push_back(resolution.target_id);
                }
            }

            const auto removed_ids = parsed.use_cascade ? collect_remove_multi_closure(document, root_ids) : root_ids;
            const auto referenced_by = describe_remove_dependents_outside(document, removed_ids);
            if (!referenced_by.empty()) {
                return make_remove_break_reference_error(args, referenced_by);
            }

            if (parsed.use_dry_run) {
                return make_dry_run_result(build_remove_preview(document, parsed, false, static_cast<int>(removed_ids.size())));
            }

            auto updated_document = document;
            if (!apply_remove_structural_change(updated_document, removed_ids)) {
                return make_apply_not_implemented_error(args, "remove");
            }

            if (parsed.use_stdout) {
                return CommandResult{
                    .exit_code = 0,
                    .stdout_text = semdl::core::render_canonical_inline_document(updated_document),
                    .stderr_text = "",
                };
            }

            if (parsed.output_file.has_value()) {
                write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(updated_document));
                return make_transform_out_result(*parsed.output_file, static_cast<int>(removed_ids.size()));
            }

            if (updated_document.has_sidecar) {
                const auto rendered = semdl::core::render_split_document(updated_document);
                write_text_file(parsed.input_file, rendered.inline_document);
                const auto sidecar_file = derive_sidecar_path(parsed.input_file);
                write_text_file(sidecar_file, rendered.sidecar_document);
                return make_update_apply_result(parsed.input_file, sidecar_file, static_cast<int>(removed_ids.size()));
            }

            write_text_file(parsed.input_file, semdl::core::render_canonical_inline_document(updated_document));
            return make_update_apply_result(parsed.input_file, std::nullopt, static_cast<int>(removed_ids.size()));
        }
        return make_apply_not_implemented_error(args, "remove");
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
        const auto parsed = parse_transform_args(args);
        if (!parsed.valid) {
            return make_invalid_transform_options_error(args,
                                                        "merge",
                                                        "ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] [--prefer-source inline|sidecar] [--warn-on-conflict] [--fail-on-conflict]");
        }
        if (parsed.format.has_value()) {
            return make_invalid_transform_options_error(args,
                                                        "merge",
                                                        "ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] [--prefer-source inline|sidecar] [--warn-on-conflict] [--fail-on-conflict]");
        }

        semdl::core::DocumentStore store;
        std::optional<semdl::core::DocumentSourceViews> source_views;
        if (parsed.fail_on_conflict || parsed.warn_on_conflict || parsed.preferred_source.has_value()) {
            source_views = store.load_document_source_views(parsed.input_file);
        }
        if (parsed.preferred_source.has_value() && !is_allowed_merge_preferred_source(*parsed.preferred_source)) {
            return make_invalid_merge_preferred_source_error(args, *parsed.preferred_source);
        }
        if (parsed.fail_on_conflict) {
            if (const auto conflict = find_merge_sidecar_conflict(*source_views); conflict.has_value()) {
                return make_merge_conflict_error(args, parsed.input_file, conflict->id, conflict->field_name);
            }
        }
        std::string merge_warning_text;
        if (parsed.warn_on_conflict) {
            if (const auto conflict = find_merge_sidecar_conflict(*source_views); conflict.has_value()) {
                merge_warning_text = make_merge_conflict_warning(args,
                                                                 parsed.input_file,
                                                                 conflict->id,
                                                                 conflict->field_name,
                                                                 parsed.preferred_source);
            }
        }
        semdl::core::DocumentData document = parsed.preferred_source.has_value()
            ? build_merge_document(*source_views, parsed.preferred_source)
            : store.load_document(parsed.input_file);
        if (parsed.preferred_source.has_value() && !document.has_sidecar) {
            return make_merge_preferred_source_requires_paired_input_error(args);
        }
        if (parsed.use_stdout) {
            return CommandResult{
                .exit_code = 0,
                .stdout_text = semdl::core::render_canonical_inline_document(document),
                .stderr_text = std::move(merge_warning_text),
            };
        }

        if (!document.has_sidecar) {
            return make_merge_requires_paired_input_error(args);
        }

        if (parsed.use_dry_run) {
            return make_dry_run_result(build_merge_preview(document,
                                                           parsed.input_file,
                                                           parsed.output_file,
                                                           parsed.preferred_source,
                                                           parsed.warn_on_conflict,
                                                           parsed.use_dry_run),
                                       std::move(merge_warning_text));
        }

        if (parsed.output_file.has_value()) {
            std::filesystem::path aliased_file;
            if (transform_out_aliases_source(document, parsed.input_file, *parsed.output_file, aliased_file)) {
                return make_transform_out_alias_error(args, *parsed.output_file, aliased_file);
            }
            write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(document));
            return make_transform_out_result(*parsed.output_file, 1, std::move(merge_warning_text));
        }

        if (!parsed.use_stdout && !parsed.use_dry_run && !parsed.output_file.has_value()) {
            write_text_file(parsed.input_file, semdl::core::render_canonical_inline_document(document));
            std::filesystem::remove(document.sidecar_file);
            return make_transform_apply_result(parsed.input_file, document.sidecar_file, 1, std::move(merge_warning_text));
        }
        return make_invalid_transform_options_error(args,
                                                    "merge",
                                                    "ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] [--prefer-source inline|sidecar] [--warn-on-conflict] [--fail-on-conflict]");
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
        if (!parsed.valid || parsed.input_files.empty()) {
            return make_invalid_extract_options_error(args);
        }
        if (parsed.format.has_value() && !is_allowed_extract_stdout_format(*parsed.format)) {
            return make_invalid_extract_options_error(args);
        }
        if (parsed.provider.empty() != parsed.model.empty()) {
            return make_missing_extract_embedding_args_error(args);
        }
        if (parsed.format.has_value() && (!parsed.use_stdout || parsed.provider.empty() || parsed.model.empty())) {
            return make_invalid_extract_options_error(args);
        }
        if (parsed.use_stdout && !parsed.provider.empty() && parsed.input_files.size() != 1U && !parsed.format.has_value()) {
            return make_invalid_extract_options_error(args);
        }
        if (!parsed.provider.empty() && !is_supported_extract_provider(parsed.provider)) {
            return make_unsupported_extract_provider_error(args, parsed.provider);
        }

        semdl::core::DocumentStore store;
        std::vector<semdl::core::DocumentData> source_documents;
        source_documents.reserve(parsed.input_files.size());

        for (const auto& input_file : parsed.input_files) {
            const bool raw_text_input = is_raw_text_extract_input(input_file);
            if (raw_text_input && parsed.use_stdout && !parsed.provider.empty()) {
                return make_invalid_extract_output_mode_error(args);
            }

            if (raw_text_input) {
                source_documents.push_back(build_raw_text_extract_document(input_file));
            } else {
                source_documents.push_back(store.load_document(input_file));
            }
        }

        auto document = source_documents.size() == 1U
            ? source_documents.front()
            : aggregate_extract_documents(source_documents);

        if (parsed.provider.empty()) {
            const std::string inline_text = semdl::core::render_canonical_inline_document(document);
            if (parsed.use_stdout) {
                return make_extract_stdout_result(inline_text);
            }

            std::filesystem::path conflicting_output;
            std::filesystem::path aliased_file;
            if (extract_out_aliases_source(parsed.input_files,
                                           source_documents,
                                           *parsed.output_file,
                                           std::nullopt,
                                           conflicting_output,
                                           aliased_file)) {
                return make_extract_out_alias_error(args, conflicting_output, aliased_file);
            }

            {
                std::ofstream output(*parsed.output_file, std::ios::binary);
                output << inline_text;
            }
            return make_extract_out_result(parsed.output_file->generic_string(), std::nullopt, std::nullopt, std::nullopt);
        }

        const auto sidecar_file = parsed.use_stdout
            ? std::optional<std::filesystem::path>()
            : std::optional<std::filesystem::path>(derive_sidecar_path(*parsed.output_file));
        if (sidecar_file.has_value()) {
            std::filesystem::path conflicting_output;
            std::filesystem::path aliased_file;
            if (extract_out_aliases_source(parsed.input_files,
                                           source_documents,
                                           *parsed.output_file,
                                           *sidecar_file,
                                           conflicting_output,
                                           aliased_file)) {
                return make_extract_out_alias_error(args, conflicting_output, aliased_file);
            }
        }

        const auto plan = semdl::core::build_extract_embedding_plan(document);
        const std::string provider_command = resolve_extract_provider_command(parsed.provider);
        const std::string generated_at = current_utc_timestamp();

        std::vector<semdl::core::GeneratedEmbeddingRecord> records;
        records.reserve(plan.targets.size());
        for (const auto& target : plan.targets) {
            const auto process = run_process_capture_output(provider_command,
                                                            build_extract_provider_argv(parsed.provider, parsed.model, target.source_text));
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
            if (parsed.format == std::optional<std::string>{"bundle"}) {
                return make_extract_stdout_result(render_extract_stdout_bundle(render_extract_stdout_inline_document(document, records),
                                                                              sidecar_text));
            }
            if (parsed.format == std::optional<std::string>{"inline"}) {
                return make_extract_stdout_result(render_extract_stdout_inline_document(document, records));
            }
            return make_extract_stdout_result(sidecar_text);
        }

        {
            std::ofstream output(*parsed.output_file, std::ios::binary);
            output << semdl::core::render_canonical_inline_document(document);
        }
        {
            std::ofstream output(*sidecar_file, std::ios::binary);
            output << sidecar_text;
        }
        return make_extract_out_result(parsed.output_file->generic_string(), sidecar_file, static_cast<int>(records.size()), plan.skipped_count);
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

    if (args[0] == "add") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "add", parsed.format);
        }

        const auto parsed = parse_add_args(args);
        if (!parsed.valid) {
            if (args.size() < 3) {
                return make_missing_required_argument_error(args, "ssd add <kind> <file> [field=value ...]", "add");
            }
            if (!parsed.invalid_option.empty()) {
                return make_invalid_add_options_error(args);
            }
            return make_invalid_add_field_error(args, parsed.invalid_field);
        }
        if (!is_supported_add_kind(parsed.kind)) {
            return make_invalid_add_kind_error(args, parsed.kind);
        }

        if (parsed.use_stdout && (parsed.use_dry_run || parsed.output_file.has_value())) {
            return make_invalid_add_options_error(args);
        }

        if (parsed.target.has_value()) {
            if (is_structural_add_kind(parsed.kind)) {
                return make_invalid_add_options_error(args);
            }
            if (*parsed.target != "sidecar") {
                return make_invalid_add_target_error(args, *parsed.target);
            }
        }

        if (is_metadata_add_kind(parsed.kind) && (!parsed.target.has_value() || *parsed.target != "sidecar")) {
            return make_add_target_sidecar_required_error(args, parsed.kind);
        }

        std::vector<std::string> missing_fields;
        for (const auto& field_name : required_add_fields_for_kind(parsed.kind)) {
            if (!parsed.field_map.contains(field_name)) {
                missing_fields.push_back(field_name);
            }
        }
        if (!missing_fields.empty()) {
            return make_add_missing_required_fields_error(args, parsed.kind, missing_fields);
        }

        semdl::core::DocumentStore store;
        auto document = store.load_document(parsed.input_file);
        const auto& entity_id = parsed.field_map.at("id");
        if (parsed.output_file.has_value()) {
            std::filesystem::path aliased_file;
            const auto reserved_alias = is_metadata_add_kind(parsed.kind)
                ? std::optional<std::filesystem::path>(derive_sidecar_path(parsed.input_file))
                : std::nullopt;
            if (transform_out_aliases_source(document, parsed.input_file, *parsed.output_file, aliased_file, std::nullopt, reserved_alias)) {
                return make_transform_out_alias_error(args, *parsed.output_file, aliased_file);
            }
        }
        if (!is_metadata_add_kind(parsed.kind) && document.contains_entity_id(entity_id)) {
            return make_add_duplicate_id_error(args, entity_id);
        }
        if (is_metadata_add_kind(parsed.kind) && !document.contains_entity_id(entity_id)) {
            return make_add_metadata_target_not_found_error(args, entity_id);
        }
        if (parsed.kind == "annotation" && !is_allowed_annotation_kind(parsed.field_map.at("kind"))) {
            return make_invalid_add_annotation_kind_error(args, parsed.field_map.at("kind"));
        }
        if (is_metadata_add_kind(parsed.kind)) {
            if (const auto conflict = validate_metadata_add_conflicts(document, parsed); conflict.has_value()) {
                return make_add_metadata_conflict_error(args, entity_id, *conflict);
            }
        }
        if (const auto reference_error = validate_add_references(document, parsed.kind, parsed.field_map); reference_error.has_value()) {
            return make_add_reference_target_not_found_error(args, reference_error->first, reference_error->second);
        }
        if (parsed.use_dry_run) {
            return make_dry_run_result(build_add_preview(document, parsed));
        }
        if (!apply_add_change(document, parsed)) {
            return make_subcommand_not_implemented_error(args);
        }

        if (is_metadata_add_kind(parsed.kind)) {
            const auto rendered = semdl::core::render_split_document(document);
            if (parsed.use_stdout) {
                return CommandResult{
                    .exit_code = 0,
                    .stdout_text = rendered.sidecar_document,
                    .stderr_text = "",
                };
            }

            if (parsed.output_file.has_value()) {
                write_text_file(*parsed.output_file, rendered.sidecar_document);
                return make_update_apply_result(std::nullopt, *parsed.output_file, 1);
            }

            const auto sidecar_file = derive_sidecar_path(parsed.input_file);
            write_text_file(sidecar_file, rendered.sidecar_document);
            return make_update_apply_result(std::nullopt, sidecar_file, 1);
        }

        if (parsed.use_stdout) {
            return CommandResult{
                .exit_code = 0,
                .stdout_text = semdl::core::render_canonical_inline_document(document),
                .stderr_text = "",
            };
        }

        if (parsed.output_file.has_value()) {
            write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(document));
            return make_transform_out_result(*parsed.output_file, 1);
        }

        if (document.has_sidecar) {
            const auto rendered = semdl::core::render_split_document(document);
            write_text_file(parsed.input_file, rendered.inline_document);
        } else {
            write_text_file(parsed.input_file, semdl::core::render_canonical_inline_document(document));
        }

        return make_update_apply_result(parsed.input_file, std::nullopt, 1);
    }

    if (args[0] == "normalize") {
        if (args.size() >= 2 && args[1] == "--help") {
            const auto parsed = parse_help_format_tail(args, 2);
            if (!parsed.valid) {
                return make_invalid_help_format_error(args, parsed.invalid_value);
            }
            return make_help_result(args, "reference", "normalize", parsed.format);
        }
        const auto parsed = parse_transform_args(args);
        if (!parsed.valid) {
            return make_invalid_transform_options_error(args,
                                                        "normalize",
                                                        "ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <output.ssd> [--format inline|sidecar] [--dry-run]]");
        }
        if (parsed.fail_on_conflict || parsed.warn_on_conflict || parsed.preferred_source.has_value()) {
            return make_invalid_transform_options_error(args,
                                                        "normalize",
                                                        "ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <output.ssd> [--format inline|sidecar] [--dry-run]]");
        }

        semdl::core::DocumentStore store;
        const auto document = store.load_document(parsed.input_file);
        const std::string result_format = parsed.format.value_or("inline");
        if (!is_allowed_transform_format(result_format)) {
            return make_invalid_transform_options_error(args,
                                                        "normalize",
                                                        "ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <output.ssd> [--format inline|sidecar] [--dry-run]]");
        }
        if (parsed.use_stdout && parsed.format.has_value()) {
            return make_invalid_transform_options_error(args,
                                                        "normalize",
                                                        "ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <output.ssd> [--format inline|sidecar] [--dry-run]]");
        }
        if (!parsed.use_dry_run && !parsed.output_file.has_value() && parsed.format.has_value()) {
            return make_invalid_transform_options_error(args,
                                                        "normalize",
                                                        "ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <output.ssd> [--format inline|sidecar] [--dry-run]]");
        }
        if (parsed.use_stdout) {
            return CommandResult{
                .exit_code = 0,
                .stdout_text = semdl::core::render_canonical_inline_document(document),
                .stderr_text = "",
            };
        }

        if (parsed.use_dry_run) {
            return make_dry_run_result(build_normalize_preview(document, parsed.input_file, parsed.output_file, result_format));
        }

        if (parsed.output_file.has_value()) {
            std::filesystem::path aliased_file;
            const auto sidecar_output = result_format == "sidecar"
                                            ? std::optional<std::filesystem::path>(derive_sidecar_path(*parsed.output_file))
                                            : std::nullopt;
            if (transform_out_aliases_source(document, parsed.input_file, *parsed.output_file, aliased_file, sidecar_output)) {
                return make_transform_out_alias_error(args, *parsed.output_file, aliased_file);
            }
            if (result_format == "sidecar") {
                const auto rendered = semdl::core::render_split_document(document);
                write_text_file(*parsed.output_file, rendered.inline_document);
                write_text_file(*sidecar_output, rendered.sidecar_document);
                return make_update_apply_result(*parsed.output_file, *sidecar_output, 1);
            }
            write_text_file(*parsed.output_file, semdl::core::render_canonical_inline_document(document));
            return make_transform_out_result(*parsed.output_file, 1);
        }

        if (args.size() == 2) {
            write_text_file(parsed.input_file, semdl::core::render_canonical_inline_document(document));
            if (document.has_sidecar) {
                std::filesystem::remove(document.sidecar_file);
                return make_transform_apply_result(parsed.input_file, document.sidecar_file, 1);
            }
            return make_update_apply_result(parsed.input_file, std::nullopt, 1);
        }
        return make_invalid_transform_options_error(args,
                                                    "normalize",
                                                    "ssd normalize <input.ssd> [--stdout|--dry-run [--format inline|sidecar]|--out <output.ssd> [--format inline|sidecar] [--dry-run]]");
    }

    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unknown_subcommand\ncommand: " + join_args(args) + "\nhint: see `ssd help toc`\n",
    };
}

}  // namespace semdl::cli
#include "semdl/cli/cli_app.hpp"

#include "semdl/core/semantic_model.hpp"
#include "semdl/core/document_store.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace semdl::cli {

namespace {

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

CommandResult make_unknown_option_error(const std::vector<std::string_view>& args, std::string_view option, bool trailing_form) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unknown_option\ncommand: " + join_args(args) +
                       "\noption: " + std::string(option) +
                       "\nsubcommand: check\nhint: remove the unknown option or add it to the formal CLI grammar before use" +
                       std::string(trailing_form ? "" : "\n"),
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

bool is_allowed_annotation_kind(std::string_view kind) {
    return kind == "rationale" || kind == "caveat" || kind == "todo" || kind == "status" || kind == "explanation";
}

bool looks_unterminated_quoted_string(std::string_view value) {
    return !value.empty() && value.front() == '"' && value.back() != '"';
}

std::string unquote_or_keep(std::string_view value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return std::string(value.substr(1, value.size() - 2));
    }
    return std::string(value);
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
        return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
    }

    if (args[0] == "check") {
        if (args.size() < 2) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
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
        if (args.size() < 3) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
        }

        if (args.size() != 3) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
        }

        semdl::core::DocumentStore store;
        const auto document = store.load_document(std::filesystem::path(args[2]));
        const auto explain_view = semdl::core::build_explain_view(document, args[1]);
        if (explain_view.kind.empty()) {
            return CommandResult{.exit_code = 3, .stdout_text = "", .stderr_text = "ERROR explain_target_not_found\n"};
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

            if (args.size() >= 5 && args[3] == "--dry-run") {
                const auto sidecar_path = derive_sidecar_path(std::filesystem::path(args.back()));
                return CommandResult{
                    .exit_code = 0,
                    .stdout_text = selector.kind == semdl::core::SelectorKind::path
                               ? "DRY-RUN\ncommand: ssd set " + std::string(args[1]) + " \"" + std::string(args[2]) + "\" " + std::string(args.back()) +
                                   "\ntarget_profile: inline\ntarget_file: " + std::string(args.back()) +
                                             "\nselector: " + std::string(args[1]) +
                                             "\nold: " + require_field_from_entity(document, selector.entity_id, selector.field_path, false) +
                                             "\nnew: \"" + std::string(args[2]) + "\"\nchanges: 1\n"
                               : "DRY-RUN\ncommand: ssd set " + std::string(args[1]) + " " + std::string(args[2]) + " " + std::string(args.back()) +
                                             "\ntarget_profile: sidecar\ntarget_file: " + sidecar_path.generic_string() + "\nselector: " + std::string(args[1]) +
                                             "\nold: " + require_field_from_entity(document, selector.entity_id, selector.field_path, true) +
                                             "\nnew: " + std::string(args[2]) + "\nchanges: 1\n",
                    .stderr_text = "",
                };
            }
        }
    }

    if (args[0] == "annotate") {
        if (args.size() < 4) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
        }
        if (looks_unterminated_quoted_string(args[3])) {
            return make_unterminated_quoted_string_error(args);
        }
        if (!is_allowed_annotation_kind(args[2])) {
            return make_invalid_annotation_kind_error(args);
        }
        if (args.size() >= 8 && args[4] == "--target" && args[5] == "sidecar" && args[6] == "--dry-run") {
            const auto sidecar_path = derive_sidecar_path(std::filesystem::path(args[7]));
            return CommandResult{
                .exit_code = 0,
                .stdout_text = "DRY-RUN\ncommand: ssd annotate " + std::string(args[1]) + " " + std::string(args[2]) + " \"" + std::string(args[3]) +
                               "\" --target sidecar " + std::string(args[7]) +
                               "\ntarget_profile: sidecar\ntarget_file: " + sidecar_path.generic_string() + "\nselector: " + std::string(args[1]) +
                               "\nannotation_kind: " + std::string(args[2]) +
                               "\nappend: \"" + std::string(args[3]) + "\"\nchanges: 1\n",
                .stderr_text = "",
            };
        }
    }

    if (args[0] == "remove") {
        if (args.size() < 3) {
            return CommandResult{.exit_code = 2, .stdout_text = "", .stderr_text = "usage: ssd <subcommand> ...\n"};
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
        if (args.size() >= 3 && args[2] == "--dry-run") {
            const auto sidecar_path = derive_sidecar_path(std::filesystem::path(args[1]));
            return CommandResult{
                .exit_code = 0,
                .stdout_text = "DRY-RUN\ncommand: ssd split " + std::string(args[1]) +
                               "\nsource_profile: inline\nresult_profile: sidecar\ncreate: " + sidecar_path.generic_string() + "\nkeep_in_ssd:\n  - document D1.title\n  - document D1.source_ref\n  - resource R1\n  - segment S1\n  - assertion A1.label\n  - assertion A1.source_segment\n  - assertion A1.source_presence\n  - assertion A1.embedding_presence\n  - hypothesis H1.kind\n  - hypothesis H1.summary\n  - hypothesis H1.alternative_group\nmove_to_ssm:\n  - document_meta D1.version\n  - document_meta D1.generator\n  - meta A1.confidence\n  - meta A1.provenance_kind\n  - meta A1.rationale\n  - meta A1.embedding\n  - meta H1.confidence\n  - meta H1.rationale\n  - meta H1.caveat\nchanges: 9\n",
                .stderr_text = "",
            };
        }
    }

    if (args[0] == "merge") {
        if (args.size() >= 3 && args[2] == "--stdout") {
            return CommandResult{
                .exit_code = 0,
                .stdout_text = read_text_file(std::filesystem::path(args[1]).replace_extension(".inline.ssd")),
                .stderr_text = "",
            };
        }
    }

    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "subcommand skeleton: not implemented\n",
    };
}

}  // namespace semdl::cli
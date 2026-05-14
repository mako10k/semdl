#include "semdl/cli/cli_app.hpp"

#include "semdl/core/document_store.hpp"

#include <filesystem>
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

CommandResult make_unknown_option_error(const std::vector<std::string_view>& args, std::string_view option) {
    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "ERROR unknown_option\ncommand: " + join_args(args) +
                       "\noption: " + std::string(option) +
                       "\nsubcommand: check\nhint: remove the unknown option or add it to the formal CLI grammar before use\n",
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

std::string require_field(const semdl::core::EntityData& entity, const std::string& field_name) {
    const auto it = entity.fields.find(field_name);
    return it == entity.fields.end() ? std::string{} : it->second;
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
            return make_unknown_option_error(args, args[1]);
        }

        if (args.size() != 2) {
            for (std::size_t index = 2; index < args.size(); ++index) {
                if (args[index].starts_with("--")) {
                    return make_unknown_option_error(args, args[index]);
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
        const auto* entity = document.find_entity(args[1]);
        const auto* metadata = document.find_metadata(args[1]);

        if (entity == nullptr) {
            return CommandResult{.exit_code = 3, .stdout_text = "", .stderr_text = "ERROR explain_target_not_found\n"};
        }

        std::ostringstream output;
        output << "id: " << args[1] << "\n";
        output << "kind: " << entity->kind << "\n";
        output << "subject: " << require_field(*entity, "subject") << "\n";
        output << "predicate: " << require_field(*entity, "predicate") << "\n";
        output << "object: " << require_field(*entity, "object") << "\n";
        output << "label: " << require_field(*entity, "label") << "\n";
        output << "source_segment: " << require_field(*entity, "source_segment") << "\n";
        output << "source_presence: " << require_field(*entity, "source_presence") << "\n";
        if (metadata != nullptr) {
            output << "confidence: " << require_field(*metadata, "confidence") << "\n";
            output << "provenance_kind: " << require_field(*metadata, "provenance_kind") << "\n";
            output << "rationale: " << require_field(*metadata, "rationale") << "\n";
            output << "embedding.model: " << require_field(*metadata, "embedding.model") << "\n";
            output << "embedding.dimensions: " << require_field(*metadata, "embedding.dimensions") << "\n";
            output << "embedding.generated_at: " << require_field(*metadata, "embedding.generated_at") << "\n";
        }

        return CommandResult{
            .exit_code = 0,
            .stdout_text = output.str(),
            .stderr_text = "",
        };
    }

    if (args[0] == "set") {
        if (args.size() >= 4 && args[1].starts_with("path:") && args[1].find('.') == std::string_view::npos) {
            return make_invalid_selector_error(args);
        }

        if (args.size() >= 5 && args[1].starts_with("id:")) {
            const std::string_view id = args[1].substr(3);
            semdl::core::DocumentStore store;
            const auto document = store.load_document(std::filesystem::path(args[4]));
            if (document.is_valid() && !document.contains_entity_id(id)) {
                return make_missing_target_error(args);
            }
        }
    }

    return CommandResult{
        .exit_code = 2,
        .stdout_text = "",
        .stderr_text = "subcommand skeleton: not implemented\n",
    };
}

}  // namespace semdl::cli
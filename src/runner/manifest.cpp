#include "semdl/runner/manifest.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace semdl::runner {
namespace {

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    TestManifest parse_manifest(const TestCaseManifest& manifest, const std::filesystem::path& repo_root) {
        TestManifest loaded{.name = manifest.name, .manifest_path = manifest.manifest_path};
        skip_ws();
        expect('{');
        skip_ws();

        while (!peek('}')) {
            const std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();

            if (key == "cases") {
                loaded.cases = parse_cases(repo_root);
            } else {
                skip_value();
            }

            skip_ws();
            if (peek(',')) {
                ++pos_;
                skip_ws();
            }
        }

        expect('}');
        return loaded;
    }

private:
    std::vector<TestCaseSpec> parse_cases(const std::filesystem::path& repo_root) {
        std::vector<TestCaseSpec> cases;
        expect('[');
        skip_ws();

        while (!peek(']')) {
            cases.push_back(parse_case(repo_root));
            skip_ws();
            if (peek(',')) {
                ++pos_;
                skip_ws();
            }
        }

        expect(']');
        return cases;
    }

    TestCaseSpec parse_case(const std::filesystem::path& repo_root) {
        TestCaseSpec test_case;
        bool has_id = false;
        bool has_command = false;
        bool has_argv = false;
        bool has_cwd = false;
        bool has_stdin = false;
        bool has_environment = false;
        bool has_expected_exit = false;
        bool has_expected_stdout = false;
        bool has_expected_stderr = false;
        bool has_expected_output_kind = false;
        bool has_notes = false;
        expect('{');
        skip_ws();

        while (!peek('}')) {
            const std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();

            if (key == "id") {
                test_case.id = parse_string();
                has_id = true;
            } else if (key == "command") {
                test_case.command = parse_string();
                has_command = true;
            } else if (key == "argv") {
                test_case.argv = parse_string_array();
                has_argv = true;
            } else if (key == "cwd") {
                test_case.cwd = repo_root / parse_string();
                has_cwd = true;
            } else if (key == "stdin") {
                test_case.stdin_text = parse_string();
                has_stdin = true;
            } else if (key == "environment") {
                test_case.environment = parse_string_map();
                has_environment = true;
            } else if (key == "expected_exit") {
                test_case.expected_exit = parse_number();
                has_expected_exit = true;
            } else if (key == "expected_stdout") {
                const std::string value = parse_string();
                test_case.expected_stdout = value.empty() ? std::filesystem::path{} : repo_root / value;
                has_expected_stdout = true;
            } else if (key == "expected_stderr") {
                const std::string value = parse_string();
                test_case.expected_stderr = value.empty() ? std::filesystem::path{} : repo_root / value;
                has_expected_stderr = true;
            } else if (key == "expected_output_kind") {
                test_case.expected_output_kind = parse_string();
                has_expected_output_kind = true;
            } else if (key == "notes") {
                test_case.notes = parse_string();
                has_notes = true;
            } else {
                skip_value();
            }

            skip_ws();
            if (peek(',')) {
                ++pos_;
                skip_ws();
            }
        }

        expect('}');

        if (!(has_id && has_command && has_argv && has_cwd && has_stdin && has_environment && has_expected_exit &&
              has_expected_stdout && has_expected_stderr && has_expected_output_kind && has_notes)) {
            throw std::runtime_error("manifest case missing required keys");
        }

        return test_case;
    }

    std::vector<std::string> parse_string_array() {
        std::vector<std::string> values;
        expect('[');
        skip_ws();

        while (!peek(']')) {
            values.push_back(parse_string());
            skip_ws();
            if (peek(',')) {
                ++pos_;
                skip_ws();
            }
        }

        expect(']');
        return values;
    }

    std::map<std::string, std::string> parse_string_map() {
        std::map<std::string, std::string> values;
        expect('{');
        skip_ws();
        while (!peek('}')) {
            const auto key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            values.emplace(key, parse_string());
            skip_ws();
            if (peek(',')) {
                ++pos_;
                skip_ws();
            }
        }
        expect('}');
        return values;
    }

    int parse_number() {
        skip_ws();
        std::size_t consumed = 0;
        const int value = std::stoi(text_.substr(pos_), &consumed);
        pos_ += consumed;
        return value;
    }

    std::string parse_string() {
        expect('"');
        std::string value;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                break;
            }
            if (ch == '\\' && pos_ < text_.size()) {
                const char escaped = text_[pos_++];
                switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case 'n': value.push_back('\n'); break;
                default: value.push_back(escaped); break;
                }
            } else {
                value.push_back(ch);
            }
        }
        return value;
    }

    void skip_value() {
        skip_ws();
        if (peek('"')) {
            (void)parse_string();
            return;
        }
        if (peek('{')) {
            expect('{');
            skip_ws();
            while (!peek('}')) {
                (void)parse_string();
                skip_ws();
                expect(':');
                skip_ws();
                skip_value();
                skip_ws();
                if (peek(',')) {
                    ++pos_;
                    skip_ws();
                }
            }
            expect('}');
            return;
        }
        if (peek('[')) {
            expect('[');
            skip_ws();
            while (!peek(']')) {
                skip_value();
                skip_ws();
                if (peek(',')) {
                    ++pos_;
                    skip_ws();
                }
            }
            expect(']');
            return;
        }
        while (pos_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos_])) && text_[pos_] != ',' && text_[pos_] != '}' && text_[pos_] != ']') {
            ++pos_;
        }
    }

    void expect(char expected) {
        if (pos_ >= text_.size() || text_[pos_] != expected) {
            throw std::runtime_error("invalid manifest json");
        }
        ++pos_;
    }

    [[nodiscard]] bool peek(char expected) const {
        return pos_ < text_.size() && text_[pos_] == expected;
    }

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    std::string text_;
    std::size_t pos_ = 0;
};

}  // namespace

TestManifest load_manifest(const TestCaseManifest& manifest, const std::filesystem::path& repo_root) {
    std::ifstream input(manifest.manifest_path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open manifest: " + manifest.manifest_path.generic_string());
    }

    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    JsonParser parser(std::move(text));
    return parser.parse_manifest(manifest, repo_root);
}

}  // namespace semdl::runner
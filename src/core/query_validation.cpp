#include "semdl/core/query_validation.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>

namespace semdl::core {

namespace {

std::string read_text_file(const std::filesystem::path& file_path) {
    std::ifstream input(file_path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string trim_space_copy(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && value[start] == ' ') {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\r')) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string ltrim_any_whitespace_copy(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    return std::string(value.substr(start));
}

bool is_identifier_token(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    const auto is_alpha = [](unsigned char ch) {
        return std::isalpha(ch) != 0;
    };
    const auto is_digit = [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    };

    if (!is_alpha(static_cast<unsigned char>(value.front()))) {
        return false;
    }

    for (const char character : value) {
        const auto ch = static_cast<unsigned char>(character);
        if (!is_alpha(ch) && !is_digit(ch) && character != '_' && character != '-') {
            return false;
        }
    }

    return true;
}

bool is_quoted_string_token(std::string_view value) {
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
        return false;
    }

    for (std::size_t index = 1; index + 1 < value.size(); ++index) {
        if (value[index] == '"') {
            return false;
        }
    }

    return true;
}

bool is_number_token(std::string_view value) {
    if (value.empty()) {
        return false;
    }

    std::size_t index = 0;
    if (value.front() == '-') {
        if (value.size() == 1) {
            return false;
        }
        index = 1;
    }

    if (index >= value.size() || std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
        return false;
    }

    while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index])) != 0) {
        ++index;
    }

    if (index == value.size()) {
        return true;
    }

    if (value[index] != '.') {
        return false;
    }
    ++index;

    if (index >= value.size() || std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
        return false;
    }

    while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index])) != 0) {
        ++index;
    }

    return index == value.size();
}

bool contains_unquoted_sequence(std::string_view text, std::string_view needle) {
    bool in_quotes = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && index + needle.size() <= text.size() && text.substr(index, needle.size()) == needle) {
            return true;
        }
    }
    return false;
}

std::size_t find_unquoted_char(std::string_view text, char target, std::size_t start_index = 0) {
    bool in_quotes = false;
    for (std::size_t index = start_index; index < text.size(); ++index) {
        if (text[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && text[index] == target) {
            return index;
        }
    }
    return std::string_view::npos;
}

QueryValidationIssue validate_where_expression(std::string_view expression) {
    QueryValidationIssue issue;
    issue.expression = trim_space_copy(expression);
    if (issue.expression.empty()) {
        issue.valid = false;
        issue.reason = "where requires a filter expression";
        return issue;
    }

    if (find_unquoted_char(issue.expression, '\t') != std::string::npos) {
        issue.valid = false;
        issue.reason = "tab characters are not supported in the current .ssq filter profile";
        return issue;
    }

    if (contains_unquoted_sequence(issue.expression, " and ") || contains_unquoted_sequence(issue.expression, " or ")) {
        issue.valid = false;
        issue.reason = "logical composition is not supported in the current .ssq filter profile";
        return issue;
    }

    if (find_unquoted_char(issue.expression, '>') != std::string::npos ||
        find_unquoted_char(issue.expression, '<') != std::string::npos ||
        contains_unquoted_sequence(issue.expression, "!=")) {
        issue.valid = false;
        issue.reason = "comparison operators other than '=' are not supported in the current .ssq filter profile";
        return issue;
    }

    const auto equal_index = find_unquoted_char(issue.expression, '=');
    if (equal_index == std::string::npos) {
        if (!is_identifier_token(issue.expression)) {
            issue.valid = false;
            issue.reason = "presence checks must be a single field name";
        }
        return issue;
    }

    if (find_unquoted_char(issue.expression, '=', equal_index + 1) != std::string::npos) {
        issue.valid = false;
        issue.reason = "only one equality check is supported in the current .ssq filter profile";
        return issue;
    }

    const std::string left = trim_space_copy(std::string_view(issue.expression).substr(0, equal_index));
    const std::string right = trim_space_copy(std::string_view(issue.expression).substr(equal_index + 1));
    if (!is_identifier_token(left)) {
        issue.valid = false;
        issue.reason = "equality checks must use a field name on the left-hand side";
        return issue;
    }

    if (!(is_quoted_string_token(right) || is_number_token(right) || right == "true" || right == "false")) {
        issue.valid = false;
        issue.reason = "equality checks must use a quoted string, number, or boolean on the right-hand side";
    }
    return issue;
}

}  // namespace

QueryValidationIssue validate_initial_ssq_profile(const std::filesystem::path& query_file) {
    std::istringstream input(read_text_file(query_file));
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = ltrim_any_whitespace_copy(line);
        if (!trimmed.starts_with("where:")) {
            continue;
        }
        return validate_where_expression(std::string_view(trimmed).substr(6));
    }
    return QueryValidationIssue{};
}

}  // namespace semdl::core
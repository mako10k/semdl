#include "semdl/core/query_validation.hpp"

#include "semdl/core/document_store.hpp"
#include "semdl/core/similarity.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string_view>
#include <unordered_set>

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

bool contains_unquoted_logical_keyword(std::string_view text, std::string_view keyword) {
    bool in_quotes = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (in_quotes || index == 0 || index + keyword.size() >= text.size()) {
            continue;
        }
        if (text.substr(index, keyword.size()) != keyword) {
            continue;
        }
        if (text[index - 1] != ' ' || text[index + keyword.size()] != ' ') {
            continue;
        }
        return true;
    }
    return false;
}

std::size_t find_unquoted_char(std::string_view text, char target, std::size_t start_index = 0);

std::vector<std::string> split_unquoted_sequence(std::string_view text, std::string_view needle) {
    std::vector<std::string> parts;
    bool in_quotes = false;
    std::size_t segment_start = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && index + needle.size() <= text.size() && text.substr(index, needle.size()) == needle) {
            parts.push_back(trim_space_copy(text.substr(segment_start, index - segment_start)));
            index += needle.size() - 1;
            segment_start = index + 1;
        }
    }
    parts.push_back(trim_space_copy(text.substr(segment_start)));
    return parts;
}

std::vector<std::string> split_unquoted_logical_keyword(std::string_view text, std::string_view keyword) {
    std::vector<std::string> parts;
    bool in_quotes = false;
    std::size_t segment_start = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (in_quotes || index == 0 || index + keyword.size() >= text.size()) {
            continue;
        }
        if (text.substr(index, keyword.size()) != keyword) {
            continue;
        }
        if (text[index - 1] != ' ' || text[index + keyword.size()] != ' ') {
            continue;
        }

        std::size_t left_boundary = index;
        while (left_boundary > segment_start && text[left_boundary - 1] == ' ') {
            --left_boundary;
        }
        parts.push_back(trim_space_copy(text.substr(segment_start, left_boundary - segment_start)));

        std::size_t next_segment = index + keyword.size();
        while (next_segment < text.size() && text[next_segment] == ' ') {
            ++next_segment;
        }
        segment_start = next_segment;
        index = next_segment > 0 ? next_segment - 1 : next_segment;
    }
    parts.push_back(trim_space_copy(text.substr(segment_start)));
    return parts;
}

enum class FilterOperator {
    presence,
    equal,
    greater,
    greater_equal,
    less,
    less_equal,
};

struct FilterClause {
    std::string field;
    FilterOperator filter_operator = FilterOperator::presence;
    std::string raw_value;
};

struct WhereExpressionNode {
    enum class Kind {
        clause,
        negation,
        conjunction,
        disjunction,
    };

    Kind kind = Kind::clause;
    FilterClause clause;
    std::vector<WhereExpressionNode> children;
};

struct ParsedWhereExpression {
    QueryValidationIssue issue;
    std::optional<WhereExpressionNode> root;
};

std::optional<double> parse_number_value(std::string_view value) {
    if (!is_number_token(value)) {
        return std::nullopt;
    }

    try {
        return std::stod(std::string(value));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::pair<std::size_t, FilterOperator>> find_comparison_operator(std::string_view expression) {
    bool in_quotes = false;
    for (std::size_t index = 0; index < expression.size(); ++index) {
        if (expression[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (expression[index] == '>') {
            if (index + 1 < expression.size() && expression[index + 1] == '=') {
                return std::pair<std::size_t, FilterOperator>{index, FilterOperator::greater_equal};
            }
            return std::pair<std::size_t, FilterOperator>{index, FilterOperator::greater};
        }
        if (expression[index] == '<') {
            if (index + 1 < expression.size() && expression[index + 1] == '=') {
                return std::pair<std::size_t, FilterOperator>{index, FilterOperator::less_equal};
            }
            return std::pair<std::size_t, FilterOperator>{index, FilterOperator::less};
        }
        if (expression[index] == '=') {
            return std::pair<std::size_t, FilterOperator>{index, FilterOperator::equal};
        }
        if (expression[index] == '!') {
            return std::pair<std::size_t, FilterOperator>{index, FilterOperator::presence};
        }
    }
    return std::nullopt;
}

bool contains_additional_comparison_operator(std::string_view expression, std::size_t start_index) {
    bool in_quotes = false;
    for (std::size_t index = start_index; index < expression.size(); ++index) {
        if (expression[index] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && (expression[index] == '>' || expression[index] == '<' || expression[index] == '=' || expression[index] == '!')) {
            return true;
        }
    }
    return false;
}

std::optional<FilterClause> parse_filter_clause(std::string_view expression, QueryValidationIssue& issue) {
    const std::string trimmed = trim_space_copy(expression);
    if (trimmed.empty()) {
        issue.valid = false;
        issue.reason = "logical chains require a filter term on both sides of the operator";
        return std::nullopt;
    }

    const auto comparison = find_comparison_operator(trimmed);
    if (!comparison.has_value()) {
        if (!is_identifier_token(trimmed)) {
            issue.valid = false;
            issue.reason = "presence checks must be a single field name";
            return std::nullopt;
        }
        return FilterClause{
            .field = trimmed,
            .filter_operator = FilterOperator::presence,
        };
    }

    const auto [operator_index, filter_operator] = *comparison;
    if (trimmed[operator_index] == '!') {
        issue.valid = false;
        issue.reason = "comparison operators other than '=', '>', '>=', '<', '<=' are not supported in the current .ssq filter profile";
        return std::nullopt;
    }

    const std::size_t operator_width = (filter_operator == FilterOperator::greater_equal || filter_operator == FilterOperator::less_equal) ? 2 : 1;
    if (contains_additional_comparison_operator(trimmed, operator_index + operator_width)) {
        issue.valid = false;
        issue.reason = "only one comparison check is supported per filter term";
        return std::nullopt;
    }

    const std::string left = trim_space_copy(std::string_view(trimmed).substr(0, operator_index));
    const std::string right = trim_space_copy(std::string_view(trimmed).substr(operator_index + operator_width));
    if (!is_identifier_token(left)) {
        issue.valid = false;
        issue.reason = "comparison checks must use a field name on the left-hand side";
        return std::nullopt;
    }

    if (filter_operator == FilterOperator::equal) {
        if (!(is_quoted_string_token(right) || is_number_token(right) || right == "true" || right == "false")) {
            issue.valid = false;
            issue.reason = "equality checks must use a quoted string, number, or boolean on the right-hand side";
            return std::nullopt;
        }
    } else if (!is_number_token(right)) {
        issue.valid = false;
        issue.reason = "range comparisons must use a number on the right-hand side";
        return std::nullopt;
    }

    return FilterClause{
        .field = left,
        .filter_operator = filter_operator,
        .raw_value = right,
    };
}

ParsedWhereExpression parse_where_expression(std::string_view expression) {
    ParsedWhereExpression parsed;
    parsed.issue.clause = "where";
    parsed.issue.expression = trim_space_copy(expression);
    if (parsed.issue.expression.empty()) {
        parsed.issue.valid = false;
        parsed.issue.reason = "where requires a filter expression";
        return parsed;
    }

    if (find_unquoted_char(parsed.issue.expression, '\t') != std::string::npos) {
        parsed.issue.valid = false;
        parsed.issue.reason = "tab characters are not supported in the current .ssq filter profile";
        return parsed;
    }

    struct WhereParser {
        std::string_view text;
        std::size_t position = 0;
        QueryValidationIssue& issue;

        void skip_spaces() {
            while (position < text.size() && text[position] == ' ') {
                ++position;
            }
        }

        static bool is_logical_keyword_at(std::string_view value, std::size_t index, std::string_view keyword) {
            if (index + keyword.size() > value.size()) {
                return false;
            }
            if (value.substr(index, keyword.size()) != keyword) {
                return false;
            }
            return index + keyword.size() == value.size() || value[index + keyword.size()] == ' ';
        }

        std::size_t find_filter_term_end() const {
            bool in_quotes = false;
            for (std::size_t index = position; index < text.size(); ++index) {
                if (text[index] == '"') {
                    in_quotes = !in_quotes;
                    continue;
                }
                if (in_quotes) {
                    continue;
                }
                if (text[index] == ')' || text[index] == '(') {
                    return index;
                }
                if (index > position && text[index - 1] == ' ' &&
                    (is_logical_keyword_at(text, index, "and") || is_logical_keyword_at(text, index, "or"))) {
                    return index;
                }
            }
            return text.size();
        }

        bool consume_keyword(std::string_view keyword) {
            skip_spaces();
            if (!is_logical_keyword_at(text, position, keyword)) {
                return false;
            }
            position += keyword.size();
            skip_spaces();
            return true;
        }

        std::optional<WhereExpressionNode> parse_primary() {
            skip_spaces();
            if (position >= text.size()) {
                issue.valid = false;
                issue.reason = "logical chains require a filter term on both sides of the operator";
                return std::nullopt;
            }

            if (text[position] == ')') {
                issue.valid = false;
                issue.reason = "unexpected ')' without a matching '(' in the current .ssq filter profile";
                return std::nullopt;
            }

            if (text[position] == '(') {
                ++position;
                skip_spaces();
                if (position < text.size() && text[position] == ')') {
                    issue.valid = false;
                    issue.reason = "parenthesized groups must contain a filter expression";
                    return std::nullopt;
                }

                auto grouped = parse_or_expression();
                if (!grouped.has_value()) {
                    return std::nullopt;
                }

                skip_spaces();
                if (position >= text.size() || text[position] != ')') {
                    issue.valid = false;
                    issue.reason = "closing ')' is required for each opening '(' in the current .ssq filter profile";
                    return std::nullopt;
                }
                ++position;
                return grouped;
            }

            const std::size_t term_end = find_filter_term_end();
            const std::string term = trim_space_copy(text.substr(position, term_end - position));
            if (term.empty()) {
                issue.valid = false;
                issue.reason = "boolean terms and groups must be separated by `and` or `or`";
                return std::nullopt;
            }

            auto clause = parse_filter_clause(term, issue);
            if (!clause.has_value()) {
                return std::nullopt;
            }

            position = term_end;
            return WhereExpressionNode{
                .kind = WhereExpressionNode::Kind::clause,
                .clause = std::move(*clause),
            };
        }

        std::optional<WhereExpressionNode> parse_unary_expression() {
            skip_spaces();
            if (!is_logical_keyword_at(text, position, "not")) {
                return parse_primary();
            }

            position += 3;
            skip_spaces();
            if (position >= text.size() || text[position] == ')') {
                issue.valid = false;
                issue.reason = "unary `not` must be followed by a filter term or parenthesized group";
                return std::nullopt;
            }
            if (text[position] == '=' || text[position] == '>' || text[position] == '<') {
                issue.valid = false;
                issue.reason = "reserved keyword `not` cannot be used as a field name in the current .ssq filter profile";
                return std::nullopt;
            }

            auto operand = parse_unary_expression();
            if (!operand.has_value()) {
                return std::nullopt;
            }

            WhereExpressionNode node;
            node.kind = WhereExpressionNode::Kind::negation;
            node.children.push_back(std::move(*operand));
            return node;
        }

        std::optional<WhereExpressionNode> parse_and_expression() {
            auto first = parse_unary_expression();
            if (!first.has_value()) {
                return std::nullopt;
            }

            std::vector<WhereExpressionNode> children;
            children.push_back(std::move(*first));

            while (consume_keyword("and")) {
                auto next = parse_unary_expression();
                if (!next.has_value()) {
                    return std::nullopt;
                }
                children.push_back(std::move(*next));
            }

            if (children.size() == 1) {
                return children.front();
            }
            return WhereExpressionNode{
                .kind = WhereExpressionNode::Kind::conjunction,
                .children = std::move(children),
            };
        }

        std::optional<WhereExpressionNode> parse_or_expression() {
            auto first = parse_and_expression();
            if (!first.has_value()) {
                return std::nullopt;
            }

            std::vector<WhereExpressionNode> children;
            children.push_back(std::move(*first));

            while (consume_keyword("or")) {
                auto next = parse_and_expression();
                if (!next.has_value()) {
                    return std::nullopt;
                }
                children.push_back(std::move(*next));
            }

            if (children.size() == 1) {
                return children.front();
            }
            return WhereExpressionNode{
                .kind = WhereExpressionNode::Kind::disjunction,
                .children = std::move(children),
            };
        }
    };

    WhereParser parser{.text = parsed.issue.expression, .issue = parsed.issue};
    parsed.root = parser.parse_or_expression();
    if (!parsed.issue.valid) {
        return parsed;
    }

    parser.skip_spaces();
    if (parser.position != parsed.issue.expression.size()) {
        parsed.issue.valid = false;
        parsed.issue.reason = parsed.issue.expression[parser.position] == ')'
                                  ? "unexpected ')' without a matching '(' in the current .ssq filter profile"
                                  : "boolean terms and groups must be separated by `and` or `or`";
        return parsed;
    }

    return parsed;
}

std::size_t find_unquoted_char(std::string_view text, char target, std::size_t start_index) {
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
    return parse_where_expression(expression).issue;
}

QueryValidationIssue validate_selector_ref_expression(std::string_view expression, std::string_view label) {
    QueryValidationIssue issue;
    issue.clause = std::string(label);
    issue.expression = trim_space_copy(expression);
    if (issue.expression.empty()) {
        issue.valid = false;
        issue.reason = std::string(label) + " requires a selector reference";
        return issue;
    }

    if (find_unquoted_char(issue.expression, '\t') != std::string::npos) {
        issue.valid = false;
        issue.reason = "tab characters are not supported in the current .ssq filter profile";
        return issue;
    }

    if (label == "similar") {
        if (!is_identifier_token(issue.expression)) {
            issue.valid = false;
            issue.reason = "similar must use an existing target identifier";
        }
        return issue;
    }

    if (!(is_identifier_token(issue.expression) || is_quoted_string_token(issue.expression))) {
        issue.valid = false;
        issue.reason = std::string(label) + " must use an identifier or quoted string selector";
    }
    return issue;
}

QueryValidationIssue validate_return_expression(std::string_view expression) {
    QueryValidationIssue issue;
    issue.clause = "return";
    issue.expression = trim_space_copy(expression);
    if (issue.expression.empty()) {
        issue.valid = false;
        issue.reason = "return requires a result mode";
        return issue;
    }

    if (find_unquoted_char(issue.expression, '\t') != std::string::npos) {
        issue.valid = false;
        issue.reason = "tab characters are not supported in the current .ssq filter profile";
        return issue;
    }

    if (issue.expression != "matches" && issue.expression != "subgraph") {
        issue.valid = false;
        issue.reason = "return must be matches or subgraph";
    }
    return issue;
}

std::optional<std::string> read_clause_value(const std::filesystem::path& query_file, std::string_view clause) {
    std::istringstream input(read_text_file(query_file));
    std::string line;
    const std::string prefix = std::string(clause) + ":";
    while (std::getline(input, line)) {
        const std::string trimmed = ltrim_any_whitespace_copy(line);
        if (trimmed.starts_with(prefix)) {
            return trim_space_copy(std::string_view(trimmed).substr(prefix.size()));
        }
    }
    return std::nullopt;
}

std::map<std::string, std::string> merged_fields_for(const DocumentData& document, const std::string& entity_id) {
    std::map<std::string, std::string> merged;
    if (const auto* entity = document.find_entity(entity_id); entity != nullptr) {
        merged.insert(entity->fields.begin(), entity->fields.end());
    }
    if (const auto* metadata = document.find_metadata(entity_id); metadata != nullptr) {
        merged.insert(metadata->fields.begin(), metadata->fields.end());
    }
    return merged;
}

std::vector<SearchMatch> build_structural_context_nodes(const DocumentData& document,
                                                        const std::filesystem::path& input_file,
                                                        const std::string& entity_id,
                                                        std::string_view entity_kind) {
    std::vector<std::string> references;
    if (const auto* entity = document.find_entity(entity_id); entity != nullptr) {
        const auto push_if_present = [&](std::string_view field_name) {
            const auto it = entity->fields.find(std::string(field_name));
            if (it != entity->fields.end()) {
                references.push_back(it->second);
            }
        };

        if (entity_kind == "segment") {
            push_if_present("source");
        } else if (entity_kind == "assertion") {
            push_if_present("subject");
            push_if_present("source_segment");
        } else if (entity_kind == "hypothesis") {
            push_if_present("about");
        }
    }

    std::vector<SearchMatch> context_nodes;
    std::unordered_set<std::string> seen_ids;
    for (const auto& reference_id : references) {
        if (!seen_ids.insert(reference_id).second) {
            continue;
        }
        if (const auto* target = document.find_entity(reference_id); target != nullptr) {
            context_nodes.push_back(SearchMatch{
                .file = input_file.generic_string(),
                .id = reference_id,
                .kind = target->kind,
            });
        }
    }

    return context_nodes;
}

bool matches_where_expression(const std::map<std::string, std::string>& fields, std::string_view expression) {
    const auto parsed = parse_where_expression(expression);
    if (!parsed.issue.valid) {
        return false;
    }

    const auto matches_clause = [&](const FilterClause& clause) {
        if (clause.filter_operator == FilterOperator::presence) {
            return fields.contains(clause.field);
        }

        const auto it = fields.find(clause.field);
        if (it == fields.end()) {
            return false;
        }

        if (clause.filter_operator == FilterOperator::equal) {
            return it->second == clause.raw_value;
        }

        const auto left_number = parse_number_value(it->second);
        const auto right_number = parse_number_value(clause.raw_value);
        if (!left_number.has_value() || !right_number.has_value()) {
            return false;
        }

        switch (clause.filter_operator) {
            case FilterOperator::greater:
                return *left_number > *right_number;
            case FilterOperator::greater_equal:
                return *left_number >= *right_number;
            case FilterOperator::less:
                return *left_number < *right_number;
            case FilterOperator::less_equal:
                return *left_number <= *right_number;
            case FilterOperator::presence:
            case FilterOperator::equal:
                return false;
        }

        return false;
    };

    const auto matches_node = [&](const auto& self, const WhereExpressionNode& node) -> bool {
        if (node.kind == WhereExpressionNode::Kind::clause) {
            return matches_clause(node.clause);
        }
        if (node.kind == WhereExpressionNode::Kind::negation) {
            return node.children.size() == 1U ? !self(self, node.children.front()) : false;
        }
        if (node.kind == WhereExpressionNode::Kind::disjunction) {
            for (const auto& child : node.children) {
                if (self(self, child)) {
                    return true;
                }
            }
            return false;
        }
        for (const auto& child : node.children) {
            if (!self(self, child)) {
                return false;
            }
        }
        return true;
    };

    return parsed.root.has_value() && matches_node(matches_node, *parsed.root);
}

QueryValidationIssue validate_select_expression(std::string_view expression) {
    const std::string trimmed = trim_space_copy(expression);

    if (trimmed.empty()) {
        return QueryValidationIssue{
            .valid = false,
            .clause = "select",
            .expression = "<missing>",
            .reason = "select is required",
        };
    }

    if (!(is_identifier_token(trimmed) || is_quoted_string_token(trimmed))) {
        return QueryValidationIssue{
            .valid = false,
            .clause = "select",
            .expression = trimmed,
            .reason = "select must use an identifier or quoted string selector",
        };
    }

    return QueryValidationIssue{};
}

std::string normalize_selector_value(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

}  // namespace

QueryValidationIssue validate_initial_ssq_profile(const std::filesystem::path& query_file) {
    std::istringstream input(read_text_file(query_file));
    std::string line;
    bool seen_select = false;
    while (std::getline(input, line)) {
        const std::string trimmed = ltrim_any_whitespace_copy(line);
        if (trimmed.starts_with("select:")) {
            seen_select = true;
            const auto issue = validate_select_expression(std::string_view(trimmed).substr(7));
            if (!issue.valid) {
                return issue;
            }
            continue;
        }
        if (trimmed.starts_with("where:")) {
            const auto issue = validate_where_expression(std::string_view(trimmed).substr(6));
            if (!issue.valid) {
                return issue;
            }
            continue;
        }
        if (trimmed.starts_with("similar:")) {
            const auto issue = validate_selector_ref_expression(std::string_view(trimmed).substr(8), "similar");
            if (!issue.valid) {
                return issue;
            }
            continue;
        }
        if (trimmed.starts_with("return:")) {
            const auto issue = validate_return_expression(std::string_view(trimmed).substr(7));
            if (!issue.valid) {
                return issue;
            }
        }
    }

    if (!seen_select) {
        return QueryValidationIssue{
            .valid = false,
            .clause = "select",
            .expression = "<missing>",
            .reason = "select is required",
        };
    }

    return QueryValidationIssue{};
}

SearchQuery parse_initial_search_query(const std::filesystem::path& query_file) {
    SearchQuery query;
    if (const auto select = read_clause_value(query_file, "select"); select.has_value()) {
        query.select = normalize_selector_value(*select);
    }
    query.where_expression = read_clause_value(query_file, "where");
    query.similar_expression = read_clause_value(query_file, "similar");
    if (const auto return_mode = read_clause_value(query_file, "return"); return_mode.has_value()) {
        query.result_mode = *return_mode;
    }
    return query;
}

SearchResult execute_initial_search_query(const SearchQuery& query,
                                         const std::vector<std::filesystem::path>& input_files,
                                         const DocumentStore& store) {
    SearchResult result;
    result.query = query;
    if (query.similar_expression.has_value()) {
        result.anchor_id = *query.similar_expression;
        result.metric = "cosine";
    }

    struct LoadedDocument {
        std::filesystem::path input_file;
        DocumentData document;
    };

    std::vector<LoadedDocument> loaded_documents;
    loaded_documents.reserve(input_files.size());
    for (const auto& input_file : input_files) {
        loaded_documents.push_back(LoadedDocument{
            .input_file = input_file,
            .document = store.load_document(input_file),
        });
    }

    const LoadedDocument* anchor_document = nullptr;
    if (query.similar_expression.has_value()) {
        for (const auto& loaded : loaded_documents) {
            if (loaded.document.find_entity(*query.similar_expression) != nullptr) {
                anchor_document = &loaded;
                break;
            }
        }
        if (anchor_document == nullptr) {
            result.similarity_failure.error = SimilarityError::target_not_found;
            result.similarity_failure.target = *query.similar_expression;
            return result;
        }
    }

    for (const auto& loaded : loaded_documents) {
        for (const auto& [entity_id, entity] : loaded.document.entities) {
            if (entity.kind != query.select) {
                continue;
            }

            const auto merged_fields = merged_fields_for(loaded.document, entity_id);
            if (query.where_expression.has_value() && !matches_where_expression(merged_fields, *query.where_expression)) {
                continue;
            }

            if (query.similar_expression.has_value()) {
                if (entity_id == *query.similar_expression) {
                    continue;
                }

                const auto similarity = compare_pairwise_similarity(anchor_document->document,
                                                                    anchor_document->input_file,
                                                                    *query.similar_expression,
                                                                    loaded.document,
                                                                    loaded.input_file,
                                                                    entity_id);
                if (similarity.error != SimilarityError::none) {
                    result.similarity_failure = similarity;
                    return result;
                }
                if (result.model.empty()) {
                    result.model = similarity.model;
                    result.dimensions = similarity.dimensions;
                }

                SearchMatch match{
                    .file = loaded.input_file.generic_string(),
                    .id = entity_id,
                    .kind = entity.kind,
                    .score = similarity.score,
                };
                if (query.result_mode == "subgraph") {
                    result.subgraphs.push_back(SearchSubgraph{
                        .match = std::move(match),
                        .context_nodes = build_structural_context_nodes(loaded.document, loaded.input_file, entity_id, entity.kind),
                    });
                } else {
                    result.matches.push_back(std::move(match));
                }
                continue;
            }

            if (query.result_mode == "subgraph") {
                result.subgraphs.push_back(SearchSubgraph{
                    .match = SearchMatch{
                        .file = loaded.input_file.generic_string(),
                        .id = entity_id,
                        .kind = entity.kind,
                    },
                    .context_nodes = build_structural_context_nodes(loaded.document, loaded.input_file, entity_id, entity.kind),
                });
                continue;
            }

            result.matches.push_back(SearchMatch{
                .file = loaded.input_file.generic_string(),
                .id = entity_id,
                .kind = entity.kind,
            });
        }
    }

    if (query.similar_expression.has_value()) {
        std::sort(result.matches.begin(), result.matches.end(), [](const SearchMatch& left, const SearchMatch& right) {
            const double left_score = left.score.value_or(0.0);
            const double right_score = right.score.value_or(0.0);
            if (left_score != right_score) {
                return left_score > right_score;
            }
            if (left.file != right.file) {
                return left.file < right.file;
            }
            return left.id < right.id;
        });
        std::sort(result.subgraphs.begin(), result.subgraphs.end(), [](const SearchSubgraph& left, const SearchSubgraph& right) {
            const double left_score = left.match.score.value_or(0.0);
            const double right_score = right.match.score.value_or(0.0);
            if (left_score != right_score) {
                return left_score > right_score;
            }
            if (left.match.file != right.match.file) {
                return left.match.file < right.match.file;
            }
            return left.match.id < right.match.id;
        });
    }

    return result;
}

}  // namespace semdl::core
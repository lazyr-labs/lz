#include <iostream>
#include <string>
#include <vector>

#include "filter_tree.h"

template<typename Filters>
auto create_or_node(Filters& beg, Filters end, filtertree::TreeInfo& tree_info, bool negate) -> std::unique_ptr<filtertree::FilterNode>;
template<typename Filters>
auto create_and_node(Filters& beg, Filters end, filtertree::TreeInfo& tree_info) -> std::unique_ptr<filtertree::FilterNode>;
template<typename Filters>
auto create_variable_node(Filters& beg, Filters end, filtertree::TreeInfo& tree_info) -> std::unique_ptr<filtertree::FilterNode>;

auto filtertree::print_tree(const std::unique_ptr<filtertree::FilterNode>& node, int depth) -> void {
    // Prepend a number of spaces depending on the depth so to see
    // the structure more clearly.
    for (int j = 0; j < (2 * depth); ++j) {
        std::cout << " ";
    }

    node->print();
    for (const auto& child : node->children) {
        filtertree::print_tree(child, depth + 1);
    }
}

auto filtertree::OrNode::is_match(const std::string& haystack) const -> bool {
    for (auto& child : children) {
        bool matched = child->is_match(haystack);
        matched = negate ? !matched : matched;
        if (matched) { // short-circuit.
            return true;
        }
    }
    return false;
}

auto filtertree::AndNode::is_match(const std::string& haystack) const -> bool {
    for (auto& child : children) {
        if (!child->is_match(haystack)) { // short-circuit.
            return false;
        }
    }
    return true;
}

auto filtertree::VariableNode::is_match(const std::string& haystack) const -> bool {
    return (*filter)(haystack);
}

auto filtertree::FlatNode::is_match(const std::string& haystack) const -> bool {
    for (const auto& and_factors : or_of_ands) {
        bool term_output = true;
        for (const auto& and_factor : and_factors) {
            if (!and_factor->is_match(haystack)) {
                term_output = false;
                break;
            }
        }
        if (term_output) {
            return true;
        }
    }
    return false;
}

auto filtertree::FlatNode::print() const -> void {
    std::cout << "OR" << std::endl;
    for (const auto& and_factors : or_of_ands) {
        std::cout << "  AND" << std::endl;
        for (const auto& and_factor : and_factors) {
            std::cout << "    ";
            and_factor->print();
        }
    }
}

/**
 * Advance `beg` to `end` or the first instance of `delimiter`.
 *
 * Specifically, `(*beg)->get_type()` is compared to `delimeter`.
*/
template<typename Filters>
auto find_delimiter(Filters& beg, Filters end, const filtertree::FilterType delimiter) -> void {
    while ((beg < end) && ((*beg)->get_type() != delimiter)) {
        ++beg;
    }
}

/**
 * Implements the create_variable_node block (see `FilterTree::set` docs).
*/
template<typename Filters>
auto create_variable_node(Filters& beg, Filters end, filtertree::TreeInfo& tree_info) -> std::unique_ptr<filtertree::FilterNode> {
    std::unique_ptr<filtertree::FilterNode> variable_node;
    if (((*beg)->get_type() == filtertree::FilterType::GRP_BEGIN) || ((*beg)->get_type() == filtertree::FilterType::NOT_GRP_BEGIN)) {
        bool negate_grp = (*beg)->get_type() == filtertree::FilterType::NOT_GRP_BEGIN;
        beg += 1; // Move to next variable.
        auto grp_end = beg;
        find_delimiter(grp_end, end, filtertree::FilterType::GRP_END);
        variable_node = create_or_node(beg, grp_end, tree_info, negate_grp);
        tree_info.depth += 1;
    }
    else {
        variable_node = std::make_unique<filtertree::VariableNode>(std::move(*beg));
        beg += 1;
        tree_info.n_nodes += 1;
    }
    return variable_node;
}

/**
 * Implements the create_and_node block (see `FilterTree::set` docs).
 *
 * There is no special type for AND.  Consecutive variables are AND'd
 * together by putting them in the same AND node.
*/
template<typename Filters>
auto create_and_node(Filters& beg, Filters end, filtertree::TreeInfo& tree_info) -> std::unique_ptr<filtertree::FilterNode> {
    auto and_node = std::make_unique<filtertree::AndNode>();
    while ((beg < end) && ((*beg)->get_type() != filtertree::FilterType::OR)) {
        auto variable_node = create_variable_node(beg, end, tree_info);
        and_node->add_child(std::move(variable_node));
    }

    // Move `beg` to the next token.
    if (beg < end) {
        beg = ((*beg)->get_type() == filtertree::FilterType::GRP_END) || ((*beg)->get_type() == filtertree::FilterType::OR) ? beg + 1 : beg;
    }

    return and_node;
}

/**
 * Implements the create_or_node block (see `FilterTree::set` docs).
*/
template<typename Filters>
auto create_or_node(Filters& beg, Filters end, filtertree::TreeInfo& tree_info, bool negate) -> std::unique_ptr<filtertree::FilterNode> {
    auto or_node = std::make_unique<filtertree::OrNode>(negate);
    while (beg < end) {
        auto and_node = create_and_node(beg, end, tree_info);
        or_node->add_child(std::move(and_node));
    }
    return or_node;
}

/**
 * Let z = `a1 OR a2 OR ... OR am` be a boolean expression, where each `aj`
 * is a product of boolean expressions `x`.  Then the following recursion
 * builds the expression tree for `z` (`A -> B` means `B` is a child of `A`).
 *
 * *** create_or_node ***
 * tree(0, z) = empty OrNode
 * for each aj in z:
 *   tree(n, z) = tree(n - 1, z) -> AndNode(0, aj)
 *
 * *** create_and_node ***
 * AndNode(0, aj) = empty AndNode
 * for each x in aj:
 *   AndNode(n, aj) = AndNode(n - 1, aj) -> VariableNode
 *
 * *** create_variable_node ***
 * VariableNode = tree(0, x) if x is in parentheses, VariableNode(x) otherwise.
*/
auto filtertree::FilterTree::set(std::vector<std::unique_ptr<Filter>>& filters) -> void {
    if (filters.empty()) {
        return;
    }

    auto beg = std::begin(filters);
    auto tree_info = filtertree::TreeInfo();
    root = create_or_node(beg, std::end(filters), tree_info, false);
    //std::cout << "depth: " << tree_info.depth << ", n_nodes: " << tree_info.n_nodes << std::endl;

    // No subexpressions, so the tree can be flattened.
    if (tree_info.depth == 0) {
        flatten();
    }
}

auto filtertree::FilterTree::is_match(const std::string& haystack) const -> bool {
    if (flat_node) {
        return flat_node->is_match(haystack);
    }
    else if (root) {
        return root->is_match(haystack);
    }
    return true;
}

auto filtertree::FilterTree::print() const -> void {
    if (flat_node) {
        flat_node->print();
    }
    else if (root) {
        filtertree::print_tree(root, 0);
    }
}


// In the case when the tree has no subexpressions (no
// parentheses), it can evaluated faster by converting it to
// a list of lists ("lists" is used loosely here), where each
// inner list contains variables that are AND'd together, and
auto filtertree::FilterTree::flatten() -> void {
    if (!root) {
        return;
    }

    auto ops = std::vector<std::vector<std::unique_ptr<filtertree::FilterNode>>>();
    for (auto& and_node : root->children) {
        auto variable_nodes = std::vector<std::unique_ptr<filtertree::FilterNode>>();
        for (auto& variable_node : and_node->children) {
            variable_nodes.emplace_back(std::move(variable_node));
        }
        ops.emplace_back(std::move(variable_nodes));
    }
    flat_node = std::make_unique<filtertree::FlatNode>(std::move(ops));
}

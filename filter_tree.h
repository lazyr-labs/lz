#ifndef SUBSEQSEARCH_FILTER_TREE_H
#define SUBSEQSEARCH_FILTER_TREE_H

#include <iostream>
#include <functional>
#include <vector>
#include <string>
#include <memory>

#include "querydata.h"

namespace qdata = qrydata;

namespace filtertree {

/**
 * Types of Filter objects.
 *
 * There is no AND because consecutive variables are AND'd by
 * default.
 *
 *  NOT_GRP_BEGIN: !(
 *  GRP_BEGIN: (
 *  GRP_END: )
 *  OR: |
 *  VARIABLE: string
*/
enum class FilterType {
    NOT_GRP_BEGIN,
    GRP_BEGIN,
    GRP_END,
    OR,
    VARIABLE,
};

/**
 * Function object that wraps a filter function.
 *
 * A filter function is a function of a character sequence and a
 * QueryData object, and returns the position of the first match
 * of the query in the sequence.
 *
 * This class wraps a filter function to extend it by enabling it
 * to be negated and parametrized by a specific query.  This class
 * acts as a function of a string, parametrized by a query given
 * in the constructor, that determines if the query is found in the
 * string.
*/
class Filter {
    public:
        // TODO: no public.
        qdata::QueryData qdata;
        bool negate;
        std::function<const char*(const char*, int seq_len, const qdata::QueryData&)> filter;
        FilterType filter_type;

        /**
         * @param qdata QueryData containing information about the query
         * @param negate whether to negate the output of `filter`
         * @param filter function of char* and QueryData& that searches
         *      for the query in the char* and returns a char* to the
         *      first match, or 0 if the query was not found.
         * @param filter_type the type of this filter
        */
        Filter(qdata::QueryData qdata, bool negate, std::function<const char*(const char*, int seq_len, const qdata::QueryData&)> filter, FilterType filter_type) {
            this->qdata = qdata;
            this->negate = negate;
            this->filter = filter;
            this->filter_type = filter_type;
        }

        /**
         * @param haystack string in which to search for the query.
         *
         * @return true if the query was found, false otherwise.  If
         *      `negate` is true, then the return value is inverted.
        */
        auto operator()(const std::string& haystack) const -> bool {
            // TODO: cache size and data.
            bool res = filter(haystack.data(), haystack.size(), qdata) != 0;
            res = negate ? !res : res;
            return res;
        }

        /**
         * @return the type of this filter
        */
        auto get_type() const -> FilterType { return filter_type; }
};

/**
 * Information about the properties of a FilterTree object.
*/
struct TreeInfo {
    int depth;
    int n_nodes;
};

/**
 * A FilterTree node.
 *
 * The primary function is to determine if any children match a
 * given string.
*/
class FilterNode {
    friend void print_tree(const std::unique_ptr<FilterNode>& node, int depth);
    friend class FilterTree;

    public:
        FilterNode() : children() {}
        /**
         * Add a node as a child.
         */
        auto add_child(std::unique_ptr<FilterNode> child) -> void { children.emplace_back(std::move(child)); }
        /**
         * Check if the node is found in the haystack.
         *
         * This should be overriden by subclasses.
         *
         * @return false
         */
        virtual auto is_match(const std::string& haystack) const -> bool { return false; };
        /**
         * Print information of this node.
         *
         * This should be overriden by subclasses.
         */
        virtual auto print() const -> void { std::cout << typeid(*this).name() << std::endl; };

    protected:
        std::vector<std::unique_ptr<FilterNode>> children;
};

/**
 * A FilterNode representing logical or.
 *
 * This is an inner node.
*/
class OrNode : public FilterNode {

    public:
        /**
         * @param negate whether or not to negate the value returned
         * by `is_match`.
        */
        OrNode(bool negate) : FilterNode(), negate(negate) {  }
        /**
         * Check if at least one child matches the haystack.
         *
         * This short-circuits, meaning the iteration over children
         * stops at the first one that returns true.
         *
         * @param haystack the string in which the query is searched
         *      for.
         *
         * @return true if `is_match` returns true for at least one
         *      child node, false otherwise.  If `negate` is true,
         *      then the result is inverted.
        */
        virtual auto is_match(const std::string& haystack) const -> bool;
        virtual auto print() const -> void {
            std::cout << "OR " << (negate ? "NOT" : "") << std::endl;
        };

    private:
        bool negate;
};

/**
 * A FilterNode representing logical and.
 *
 * This is an inner node.
*/
class AndNode : public FilterNode {

    public:
        AndNode() : FilterNode() {  }
        /**
         * Check if all children match the haystack.
         *
         * This short-circuits, meaning the iteration over children
         * stops at the first one that returns false.
         *
         * @param haystack the string in which the query is searched
         *      for.
         *
         * @return true if `is_match` returns true for all children
         *      nodes, false otherwise.
        */
        virtual auto is_match(const std::string& haystack) const -> bool;
        virtual auto print() const -> void { std::cout << "AND" << std::endl; };
};

/**
 * A FilterNode representing a logical variable.
 *
 * This is a leaf node.
 *
 * This variable can take the values true or false according to
 * the filter function given in the constructor.
*/
class VariableNode : public FilterNode {

    public:
        /**
         * @param filter a Filter that, when given the haystack,
         *      returns true if the filter's query is found, otherwise
         *      false.
        */
        VariableNode(std::unique_ptr<Filter> filter) : FilterNode(), filter(std::move(filter)) {  }
        /**
         * Apply the filter to the haystack.
         *
         * This essentially does a `return filter(haystack)`.
         *
         * @param haystack the string in which the query is searched
         *      for.
         *
         * @return the value of the filter applied to the haystack.
        */
        virtual auto is_match(const std::string& haystack) const -> bool;
        virtual auto print() const -> void {
            std::cout << (filter->negate ? "NOT " : "") << filter->qdata.q << std::endl;
        };

    private:
        std::unique_ptr<Filter> filter;
};

/**
 * A node representing a flat or-of-ands expression with no subexpressions
 * (parentheses).
 *
 * This provides a more efficient way to evaluate an or-of-ands
 * expression that eliminates overhead from traversing the hierarchy
 * of children (when the nodes are VariableNodes).
*/
class FlatNode : public FilterNode {

    public:
        /**
         * @param or_of_ands 2d vector, where the rows represent terms
         *      to be OR'd, and the columns represent terms to be AND'd.
        */
        FlatNode(std::vector<std::vector<std::unique_ptr<FilterNode>>> or_of_ands) : FilterNode(), or_of_ands(std::move(or_of_ands)) {  }
        /**
         * Check if at least one row matches the haystack.
         *
         * This short-circuits, meaning the iteration over rows
         * stops at the first one that returns true.
         *
         * @param haystack the string in which the query is searched
         *      for.
         *
         * @return true if `is_match` returns true for all columns
         *      at least one row of nodes.
        */
        virtual auto is_match(const std::string& haystack) const -> bool;
        virtual auto print() const -> void;

    private:
        std::vector<std::vector<std::unique_ptr<FilterNode>>> or_of_ands;
};

/**
 * A boolean expression tree.
 *
 * This is a tree that evaluates the boolean expression given
 * by a sequence of Filter objects.
*/
class FilterTree {

    public:
        FilterTree() : root(), flat_node() {}
        /**
         * Build the tree for the given expression.
         *
         * The types of each Filter determine the structure of the
         * tree (see docs for FilterType).
         *
         * The sequence is assumed to be a valid boolean expression.
         * This method does not do any validation.
         *
         * @param filters nonemptys equence of `Filter`s that represent
         *      a valid boolean expression
        */
        auto set(std::vector<std::unique_ptr<Filter>>& filters) -> void;
        /**
         * Evaluate this tree's expression on the haystack.
         *
         * @return the result of the expression.
        */
        auto is_match(const std::string& haystack) const -> bool;
        /**
         * Print the tree.
        */
        auto print() const -> void;

    private:
        /**
         * Flatten the tree.
        */
        auto flatten() -> void;

        std::unique_ptr<FilterNode> root;
        std::unique_ptr<FilterNode> flat_node;
};

auto print_tree(const std::unique_ptr<FilterNode>& node, int depth) -> void;

} // namespace filtertree

#endif

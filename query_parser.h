#ifndef SUBSEQSEARCH_QUERY_PARSER_H
#define SUBSEQSEARCH_QUERY_PARSER_H

#include <vector>
#include <string>

#include "filter_tree.h"
#include "fuzzy.h"
#include "querydata.h"

namespace qdata = qrydata;

namespace qryparser {

auto is_delim(const char c, const std::string& delims) -> bool;

/**
*/
template<typename Scorer>
struct Query {
    std::unique_ptr<fuzzy::Fuzzy<Scorer>> fuzzy;
    std::unique_ptr<filtertree::FilterTree> filter_tree;
};

/**
 * Advance `beg` until it points to a character in `delims` or `end`.
 *
 * An error is thrown if `beg >= end`.
 *
 * Backslashes are skipped, and the character after one is added to
 * the return string, even if it is in `delims`.
 *
 * @return the string from `beg` to either `end` or the first instance
 *      of a non-escaped character in `delims` (exclusive).  `beg`
 *      is positioned at the stopping point.
*/
auto parse_exact(std::string::const_iterator& beg, std::string::const_iterator end, const std::string& delims) -> std::string;
/**
 * Advance `beg` until the end of the phrase or `end`.
 *
 * A phrase ends in non-escaped `"`.  It can include any character other than
 * `"`, even spaces.  The only characters that can come after
 * it are ` `, `)`, and `|`.  Anything else, and an error is thrown
 * because it is not clear if the `"` is meant to end the phrase
 * or be escaped.
 *
 * An error is thrown if no `"` is found.
 *
 * An error is thrown if the phrase is empty.
 *
 * Backslashes are skipped, and the character after one is added to
 * the return string, even if it is `"`.
 *
 * @return the string from `beg` to the first instance of a
 *      non-escaped `"` (exclusive).  `beg` is positioned at
 *      the first character after the `"`.
*/
auto parse_phrase(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string;
/**
 * Advance `beg` until the end of the phrase/word or `end`.
 *
 * If `*beg == "`, then the rest of the string will be treated as
 * a phrase.  Otherwise, the string is treated as word terminated
 * by a space.
 *
 * An error is thrown if the string is empty.
 *
 * Backslashes are skipped, and the character after one is added to
 * the return string.
 *
 * @return the string from `beg` to the first instance of a
 *      non-escaped `"` (for phrases), ` ` (for non-phrases), or
 *      until `end`.  `beg` is positioned at the first character
 *      after one of the aformentioned terminal characters.
*/
auto parse_meta(std::string::const_iterator& beg, std::string::const_iterator end, std::string&& meta) -> std::string;
/**
 * Convenience function for readability when parsing a fuzzy string that calls `parse_meta`.
*/
auto parse_fuzzy(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string;
/**
 * Convenience function for readability when parsing a prefix string that calls `parse_meta`.
*/
auto parse_prefix(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string;
/**
 * Convenience function for readability when parsing a suffix string that calls `parse_meta`.
*/
auto parse_suffix(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string;
/**
 * Negate the filter created by the string that follows a `!` symbol.
 *
 * For example, for the string `!=asdf`, a Filter for `asdf` is
 * created and set to negate.
 *
 * @return the negated filter.
*/
auto parse_neg(std::string::const_iterator& beg, std::string::const_iterator end, bool ignore_neg, const qdata::SearchArgs& search_args) -> std::unique_ptr<filtertree::Filter>;
/**
 * Advance `beg` until the first character that is not `delim`, or `end`,
 * whichever comes first.
*/
auto skip_delim(std::string::const_iterator& beg, std::string::const_iterator end, char delim) -> void;
/**
 * Start parsing a string.
 *
 * The string consists of substrings with different interpretations
 * depending on what character they start with.  The interpretation
 * determines how to create its Filter and how to build the substring.
 *
 * <First characters>: <interpretation>, <consists of>
 *   ": phrase, everthing within enclosing non-escaped double quotes.
 *   =: exact, everthing after until the first non-escaped space.
 *   ~: fuzzy, everthing after until the first non-escaped space.
 *   ^: prefix, everthing after until the first non-escaped space.
 *   $: suffix, everthing after until the first non-escaped space.
 *   !: not, everthing after until the first non-escaped space.
 *   (: group begin, only `(`.
 *   !(: not group begin, only `(`.
 *   ): group end, only `)`.
 *   |: or, only `|`.
 *
 * The Filter for the last four will have the respective FilterType.
 * The Filter for the others will have type `VARIABLE`.  For these,
 * the first character can be followed by `"` to denote an exact
 * phrase, a fuzzy phrase, a prefix phrase, a suffix phrase, or not
 * the phrase.  Also, `!` can be followed by one of the characters
 * above it.  For exampe, `!$` is not suffix, `!^"` is not prefix
 * phrase.
 *
 * @param beg beginning of string to parse
 * @param end end of string to parse
 * @param ignore_case whether to set the Filter to ignore case
 * @param topk number of results to display (this will be removed later)
 *
 * @return vector of Filter unique pointers.  Each Filter contains
 *      a string and type interpreted as above and the appropriate
 *      filter function.  The order of the Filter objects is the
 *      same as how they appear in the string.
*/
auto parse(std::string::const_iterator& beg, std::string::const_iterator end, const qdata::SearchArgs& search_args) -> std::vector<std::unique_ptr<filtertree::Filter>>;
/**
 * Inner loop of `parse` that selects which parsing function to use
 * for the current position.
*/
auto select_parse(std::string::const_iterator& beg, std::string::const_iterator end, bool ignore_neg, const qdata::SearchArgs& search_args) -> std::unique_ptr<filtertree::Filter>;
/**
 * Parse a string for its fuzzy and boolean parts.
 *
 * The string should be formatted as `z ; b`, where `z` is any
 * number of space-separated fuzzy strings, and `b` is a string
 * denoting a boolean expression of filters.  Only the `z` is
 * required, but the ` ; ` is necessary if `b` is present, otherwise
 * it will be interpreted as a fuzzy string.
 *
 * Strings in `z` can be phrases (surrounded by `"`).  Otherwise,
 * they are sequences of non-spaces.
 *
 * Strings in `b` can be phrases or preceeded by special characters:
 * `^` for exact prefix matching, `$` for exact suffix matching,
 * `=` for exact matching, `"` for exact phrase matching, and `~`
 * for fuzzy matching.  Prefix, suffix, and exact strings can be
 * phrases, for example `^"as df"` for matching `as df` as a prefix.
 * Any of these special characters can be preceeded by `!` to denote
 * negation, so `!^"as df"` matches anything that does not start with
 * `as df`.
 *
 * Strings in `b` separated by a space are AND'd together.  Strings
 * separated by ` | ` are OR'd.  Expressions in parentheses are
 * evaluated with higher precedence.  These expressions can be
 * negated (for example, `!(a | b)c` means match c and neither a
 * nor b).
 *
 * @return Query object for the fuzzy expression and boolean expression.
*/
template<typename Scorer>
auto getparse(const qdata::SearchArgs& search_args) -> Query<Scorer>;

/**
 * Parse string to locate fuzzy strings.
 *
 * This functions finds all space-separated strings and puts them
 * in a Fuzzy object.  Strings can also be phrases, in which case
 * they are dilimited by `"`.  Parsing stops until either the end
 * of the string is reached, or a ` ;` is encountered (note the
 * preceeding space).  Parsing also stops if `;` is the first
 * character.
 *
 * An error is thrown if `beg >= end` or the string is just spaces.
 *
 * @return a Fuzzy unique pointer for all the fuzzy queries in the
 *      string.  If a `;` was found, `beg` is positioned to the
 *      character after it.  Otherwise, it is `end`.
*/
template<typename Scorer>
auto parse_fuzzies(std::string::const_iterator& beg, std::string::const_iterator end, const qdata::SearchArgs& search_args) -> std::unique_ptr<fuzzy::Fuzzy<Scorer>> {
    if (beg >= end) {
        throw std::runtime_error("Query can't be empty.");
    }

    skip_delim(beg, end, ' ');
    if (beg >= end) {
        throw std::runtime_error("Query can't be empty.");
    }

    auto fuzzy_queries = std::vector<qdata::QueryData>();
    static const std::string delims = " ";

    while (beg < end) {
        std::string q = "";

        if (*beg == '"') {
            ++beg;
            q = parse_phrase(beg, end);
            if ((beg < end) && (*beg != ' ')) {
                throw std::runtime_error("Extra symbols after closing \".");
            }
        }
        else if (*beg == ';') {
            ++beg;
            break;
        }
        else {
            q = parse_exact(beg, end, delims);
        }

        qdata::SearchArgs sa = search_args;
        sa.q = q;
        auto qdata = qdata::QueryData(sa);
        fuzzy_queries.emplace_back(std::move(qdata));
        skip_delim(beg, end, ' ');
    }

    auto fuzzy = std::make_unique<fuzzy::Fuzzy<Scorer>>(fuzzy_queries);
    return fuzzy;
}

template<typename Scorer>
auto getparse(const qdata::SearchArgs& search_args) -> Query<Scorer> {
    auto beg = std::cbegin(search_args.q);
    auto end = std::cend(search_args.q);
    auto fuzzy = parse_fuzzies<Scorer>(beg, end, search_args);
    auto tst = parse(beg, end, search_args);
    auto filter_tree = std::make_unique<filtertree::FilterTree>();
    filter_tree->set(tst);
    auto query = Query<Scorer>{.fuzzy = std::move(fuzzy), .filter_tree = std::move(filter_tree)};
    return query;
}

/**
*/
template<typename Scorer>
auto f() -> int {
    // TODO: query and haystack must be nonempty.
    // TODO: find doesn't work.
    const auto search_args = qdata::SearchArgs{
        .q = ";qy qw | (!qy !qw)",
        .ignore_case=false,
        .smart_case=true,
        .topk=10,
        .filenames=std::vector<std::string>{""},
        .parallel=false,
        .preserve_order=false,
        .batch_size=10000,
        .max_symbol_dist=10,
    };
    //std::string s1 = "qy qw !(qy qw)";
    //std::string s1 = "qy qw | \"!(qy qw)|()\" asd ; lol";
    //std::string s1 = "qy qw qy qw";
    std::string haystack = "qwerty";
    auto beg = std::cbegin(search_args.q);
    auto end = std::cend(search_args.q);

    auto fuzzy = parse_fuzzies<Scorer>(beg, end, search_args);
    auto tst = parse(beg, end, search_args);
    auto filter_tree = std::make_unique<filtertree::FilterTree>();
    filter_tree->set(tst);
    auto query = Query<Scorer>{.fuzzy = std::move(fuzzy), .filter_tree = std::move(filter_tree)};
    std::cout << query.filter_tree->is_match(haystack) << std::endl;
    std::cout << query.fuzzy->is_match(haystack.data(), haystack.size()) << std::endl;
    //std::cout << query.fuzzy->calc_score(haystack) << std::endl;

    query.fuzzy->print();
    query.filter_tree->print();
    return 1;
}
} // namespace qryparser

#endif

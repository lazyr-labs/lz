#ifndef SUBSEQSEARCH_QUERYDATA_H
#define SUBSEQSEARCH_QUERYDATA_H

#include <vector>
#include <string>

namespace qrydata {

/**
 * Arguments, mainly from the command line, for how to search.
 * These are shared among all threads.
*/
struct SearchArgs {
    std::string q;
    bool ignore_case;
    bool smart_case;
    int topk;
    std::vector<std::string> filenames;
    bool parallel;
    bool preserve_order;
    int batch_size;
    int max_symbol_dist; // TODO: rename to max_symbol_gap.
    std::string gap_penalty;
    std::string word_delims;
    bool show_color;
};

/**
 * Information about and parameters for a fuzzy query.
 *
 * In particular, `qq` and `include_str` contain precomputed case
 * conversions mainly for use with `strpbrk`.  The characters
 * contained in these variables appear in the same order as in `q`.
 *
 *   ignore_case: should case be ignored when searching.
 *   topk: number of results to show.
 *   q_len: length of `q`.
 *   qq: vector where `qq[j]` contains upper and lower case versions
 *          of `q[j]` if `ignore_case == true`.  Otherwise, `qq[j]`
 *          contains `q[j]`.  All elements end in the null byte.
 *   include_str: concatenation of all strings in `qq` into one
 *          string.  This ends in the null byte.
 *   q: the fuzzy query.
*/
struct QueryData {
    bool ignore_case;
    bool preserve_order;
    int topk; // TODO: remove.
    int max_symbol_dist;
    int q_len;
    std::vector<const char*> qq; // TODO: rename.
    char* include_str; // TODO: rename.
    std::string q;
    std::string word_delims;

    explicit QueryData(const SearchArgs& sa);
    QueryData(const SearchArgs& sa, const std::string& s);
    QueryData() {};
};

} // namespace qrydata

#endif

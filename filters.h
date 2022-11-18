#ifndef SUBSEQSEARCH_FILTERS_H
#define SUBSEQSEARCH_FILTERS_H

#include "querydata.h"

namespace qdata = qrydata;

namespace filters {

/**
 * Check if a char is in a string of chars.
 *
 * @return true if `c` is in `valid_chars`, false otherwise.
*/
auto is_match(char c, const char* valid_chars) -> bool;
/**
 * Check if the prefix of a string exactly matches the query.
 *
 * @return `seq` if its prefix matches the query, otherwise 0.
*/
auto find_prefix(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char*;
/**
 * Check if the suffix of a string exactly matches the query.
 *
 * @return pointer to the start of the suffix in `seq` if it matches
 *      the query, otherwise 0.
*/
auto find_suffix(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char*;
/**
 * Check if a substring of a string exactly matches the query.
 *
 * @return pointer to the start of the first substring in `seq`
 *      if it matches the query, otherwise 0.
*/
auto find(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char*;
/**
 * Check if a subsequence of a string exactly matches the query.
 *
 * The subsequence need not be contiguous.
 *
 * @return pointer to the start of the subsequence in `seq` if it
 *      matches the query, otherwise 0.
*/
auto find_subseq(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char*;

auto find_subseq_range(const char* seq, int seq_len, const qdata::QueryData& qdata) -> std::pair<const char*, const char*>;
} // namespace filters

#endif

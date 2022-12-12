#include <vector>
#include <string>
#include <cctype>

#include "querydata.h"

namespace qdata = qrydata;

/**
 * Precompute all cases for all characters in string.
 *
 * For example, if `qry` is `asdf` and `ignore_case` is `true`,
 * then `cases` will be `{aA, sS, dD, fF}`.  When `ignore_case`
 * is `false` and `qry` is `asDf`, then `cases` is `{a, s, D, f}`.
 * All strings in `cases` are terminated by the null byte.
 *
 * @param qry string to compute cases for.  Assumed to be lowercase
 *      if `ignore_case == true` (to avoid lower casing twice).
 * @param q_len length of `qry`.
 * @param ignore_case whether or not to ignore case.
 * @param cases container in which to store results.
 *
 * @return combined length of all created strings.
 */
auto precompute_cases(const std::string& qry, int q_len, bool ignore_case, std::vector<const char*>& cases) -> int {
    int tot_len = 0;
    for (int j = 0; j < q_len; ++j) {
        char* all_cases;

        // TODO: ignore diacritics.
        if (ignore_case) {
            all_cases = new char[3]; // TODO: delete.
            all_cases[0] = qry[j]; // Already lower case in this case.
            all_cases[1] = std::toupper(qry[j]);
            all_cases[2] = '\0';
            tot_len += 2;
        }
        else {
            all_cases = new char[2]; // TODO: delete.
            all_cases[0] = qry[j];
            all_cases[1] = '\0';
            tot_len += 1;
        }

        cases[j] = all_cases;
    }

    return tot_len;
}

/**
 * Concatenate multiple strings into one string.
 *
 * Order of the characters is the same as in `strings`.
 * The combined length of all null byte terminated strings in
 * `strings` is `tot_len`, excluding the null bytes.  The returned
 * string is null byte terminated and has length `tot_len + 1`,
 * including the null byte.
 */
auto concatenate(const std::vector<const char*> strings, int tot_len) -> char* {
    auto result = new char[tot_len + 1]; // TODO: delete
    int j = 0;
    for (const auto& string : strings) {
        for (int k = 0; string[k] != '\0'; ++k) {
            result[j] = string[k];
            ++j;
        }
    }
    result[tot_len] = '\0';
    return result;
}

// TODO: unicode.
auto lower(std::string& s) -> void {
    auto end = std::end(s);
    for (auto it = std::begin(s); it < end; ++it) {
        *it = std::tolower(*it);
    }
}

// TODO: unicode.
auto upper(std::string& s) -> void {
    auto end = std::end(s);
    for (auto it = std::begin(s); it < end; ++it) {
        *it = std::toupper(*it);
    }
}

qdata::QueryData::QueryData(const qdata::SearchArgs& search_args)
    : qdata::QueryData(search_args, search_args.q) {
}

qdata::QueryData::QueryData(const qdata::SearchArgs& search_args, const std::string& q) {
    this->ignore_case = search_args.ignore_case;
    this->topk = search_args.topk;
    this->preserve_order = search_args.preserve_order;
    this->max_symbol_dist = search_args.max_symbol_dist;
    this->word_delims = search_args.word_delims;
    this->q = q;

    auto q_len = this->q.size();
    auto q_cases = std::vector<const char*>(q_len);

    if (search_args.ignore_case) {
        lower(this->q);
    }

    auto include_str_len = precompute_cases(this->q, q_len, search_args.ignore_case, q_cases);

    this->q_len = q_len;
    this->qq = q_cases;
    this->include_str = concatenate(q_cases, include_str_len);
}

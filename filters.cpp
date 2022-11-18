#include <string.h>
#include <iostream>

#include "querydata.h"
#include "filters.h"

namespace qdata = qrydata;

auto filters::is_match(char c, const char* valid_chars) -> bool {
    for (int k = 0; valid_chars[k] != '\0'; ++k) {
        if (c == valid_chars[k]) {
            return true;
        }
    }
    return false;
}

auto filters::find_prefix(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char* {
    int j = 0;
    for (; j < qdata.q_len; ++j) {
        if (!filters::is_match(seq[j], qdata.qq[j])) {
            return 0;
        }
    }
    return seq;
}

auto filters::find_suffix(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char* {
    for (int j = seq_len - qdata.q_len, k = 0; j < seq_len; ++j, ++k) {
        if (!filters::is_match(seq[j], qdata.qq[k])) {
            return 0;
        }
    }
    return seq - qdata.q_len;
}

// TODO: this is slow (20ms slower than grep -F when searching
// 800k lines; needs to be faster than grep).
auto filters::find(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char* {
    int last_idx = qdata.q_len - 1;
    auto& q_first = qdata.qq[0];
    auto& q_last = qdata.qq[last_idx];
    auto seq_end = seq + seq_len;
    const char* ret = 0;
    while (seq) {
        auto candidate_beg = strpbrk(seq, q_first);
        if ((candidate_beg == 0) || ((seq_end - candidate_beg) < qdata.q_len)) {
            break;
        }

        seq = candidate_beg + 1;
        if (!filters::is_match(candidate_beg[last_idx], q_last)) {
            continue;
        }

        // TODO: already know 0 and last_idx match -- search only
        // in (0,last_idx).  Though this likely wouldn't noticably
        // affect performance.
        candidate_beg = filters::find_prefix(candidate_beg, seq_len, qdata);
        if (candidate_beg) {
            ret = candidate_beg;
            break;
        }
    }
    return ret;
}

auto filters::find_subseq(const char* seq, int seq_len, const qdata::QueryData& qdata) -> const char* {
    return filters::find_subseq_range(seq, seq_len, qdata).first;
}

auto filters::find_subseq_range(const char* seq, int seq_len, const qdata::QueryData& qdata) -> std::pair<const char*, const char*> {
    const char* subseq_beg = 0;
    --seq; // Search starts at `seq + 1`.
    for (const auto& qj : qdata.qq) {
        if ((seq = strpbrk(seq + 1, qj)) == 0) {
            return {0, 0};
        }
        if (subseq_beg == 0) { // Remember the start of the subsequence.
            subseq_beg = seq;
        }
    }
    return {subseq_beg, seq};
}

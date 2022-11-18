#ifndef SUBSEQSEARCH_SCORES_H
#define SUBSEQSEARCH_SCORES_H

#include <array>
#include <vector>

namespace scores {

/**
*/
struct LinearScorer {

    LinearScorer() {}

    /**
     * number of words between child and parent delims.
    */
    auto word_dist(int end1, int end2, bool same_word) const -> float {
        return same_word ? 0.0f : end1 - end2;
    }

    /**
     * distance between delims of new word.
    */
    auto word_len(int delim_idx, bool same_word, const std::vector<int>& word_ends) const -> float {
        if (same_word) {
            return 0.0f;
        }
        return word_ends[delim_idx] - word_ends[delim_idx - 1];
    }

    /**
     * new word.
    */
    auto is_new_word(bool same_word) const -> float {
        return same_word ? 0.0f : 1.0f;
    }

    /**
     * beginning of new word.
    */
    auto is_not_beg(int idx, int word_beg, const std::vector<bool>& idx_to_islower, const std::vector<int>& delim_indices) const -> float {
        return idx_to_islower[idx] && (idx != (1 + delim_indices[word_beg - 1])) ? 1.0f : 0.0f;
    }

    /**
     * contiguous.
    */
    auto is_noncontiguous(int idx1, int idx2) const -> float {
        return idx1 != (idx2 + 1) ? 1.0f : 0.0f;
    }
};

/**
*/
struct LogScorer {

    std::array<float, 128> y;

    LogScorer() {
        y = std::array<float, 128>();
        for (int j = 0; j < 128; ++j) {
            y[j] = log2(j, false);
        }
    }

    /**
    */
    auto log2(int x, bool use_cache=true) const -> float {
        if (x < 2) return 0.0f;

        auto len = use_cache ? y.size() : 1;
        if (x < len) return y[x];

        float q = x;
        int c = y[len - 1];
        while (x > len) {
            x = x >> 1;
            ++c;
        }
        return c + q / (1 << (c + 1));
        //return c + q / ((c + 1) << 1);
        //return c;
    }
    
    /**
     * number of words between child and parent delims.
    */
    auto word_dist(int end1, int end2, bool same_word) const -> float {
        return same_word ? 0.0f : log2(end1 - end2);
    }
    
    /**
     * distance between delims of new word.
    */
    auto word_len(int delim_idx, bool same_word, const std::vector<int>& word_ends) const -> float {
        if (same_word) {
            return 0.0f;
        }
        float len = word_ends[delim_idx] - word_ends[delim_idx - 1];
        len = log2(len);
        return len;
    }

    /**
     * new word.
    */
    auto is_new_word(bool same_word) const -> float {
        return same_word ? 0.0f : 0.0f;
    }
    
    /**
     * beginning of new word.
    */
    auto is_not_beg(int idx, int word_beg, const std::vector<bool>& idx_to_islower, const std::vector<int>& delim_indices) const -> float {
        if (idx == (1 + delim_indices[word_beg - 1])) {
            return -1.0f;
        }
        return idx_to_islower[idx] ? 1.0f : 0.0f;
    }
    
    /**
     * contiguous.
    */
    auto is_noncontiguous(int idx1, int idx2) const -> float {
        //return idx1 != (idx2 + 1) ? 1.0f : 0.0f;
        return log2(idx1 - idx2 - 1.0f);
    }
};
} // namespace scores

#endif

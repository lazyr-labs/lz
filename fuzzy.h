#ifndef SUBSEQSEARCH_FUZZY_H
#define SUBSEQSEARCH_FUZZY_H

#include <algorithm>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>
#include <string.h>

#include "filters.h"
#include "querydata.h"
#include "scores.h"
#include "subseq.h"

namespace qdata = qrydata;

namespace fuzzy {

/**
*/
struct ScoreResults {
    float score;
    std::vector<int> path;
};

template<typename Container>
auto resize(Container& x, int size) -> void {
    if (x.size() < size) {
        x = Container(size * 4);
    }
}

/**
*/
auto remove_outofbounds(std::vector<std::vector<int>>& graph, int max_depth) -> void;

/**
*/
auto remove_max_nodes(std::vector<std::vector<int>>& graph, int max_depth) -> void;

/**
*/
auto remove_min_nodes(std::vector<std::vector<int>>& graph, int max_depth) -> void;

/**
*/
template<typename Scorer>
auto create_score_graph(subseq::HaystackData<Scorer>& hd, int n_layers) -> void {
    for (int j = 0; j < n_layers; ++j) {
        if (hd.graph[j].size() > hd.score_graph[j].size()) {
            hd.score_graph[j] = std::vector<float>(hd.graph[j].size(), 20000000.0f);
        }
        else {
            auto beg = std::begin(hd.score_graph[j]);
            std::fill(beg, beg + hd.graph[j].size(), 20000000.0f);
        }
    }
}

/**
*/
// TODO: `char_to_indices` should be const.
template<typename Scorer>
auto create_graphs(subseq::HaystackData<Scorer>& hd, const qdata::QueryData& qdata, std::vector<std::vector<int>>& char_to_indices) -> void {
    for (int k = 0; k < qdata.q_len; ++k) {
        hd.graph[k] = char_to_indices[qdata.q[k]];
    }
    remove_outofbounds(hd.graph, qdata.q_len - 1);
    remove_max_nodes(hd.graph, qdata.q_len - 1);
    remove_min_nodes(hd.graph, qdata.q_len - 1);
    create_score_graph(hd, qdata.q_len);
}

/**
*/
template<typename Scorer>
auto create_other(subseq::HaystackData<Scorer>& hd, int n_layers, const std::string& haystack) -> void {
    auto beg = std::cbegin(hd.delim_indices);
    auto end = std::cend(hd.delim_indices);
    resize(hd.idx_to_right_delim, haystack.size());
    resize(hd.idx_to_islower, haystack.size());
    for (int j = 0; j < n_layers; ++j) {
        auto first_greater = beg;
        for (const auto& cur_layer = hd.graph[j]; const auto idx : cur_layer) {
            first_greater = std::find_if(
                    first_greater, end,
                    [idx](const auto& delim_idx) { return delim_idx > idx; });
            hd.idx_to_right_delim[idx] = first_greater - beg;
            hd.idx_to_islower[idx] = std::islower(haystack[idx]);
        }
    }
}

/**
*/
auto find_delims(const std::string& haystack, const char* word_delims, std::vector<int>& delim_indices) -> void;


/**
 * Class for handling fuzzy searching and scoring of one of more
 * fuzzy terms.
*/
template<typename Scorer>
class Fuzzy {

    public:
        /**
         * @param queries fuzzy queries to search for
        */
        Fuzzy(const std::vector<qdata::QueryData>& queries);

        /**
         * Search all fuzzy queries in the haystack.
         *
         * @param haystack string to search in a fuzzy way
         *
         * @return true if all queries are found in the haystack,
         *      false otherwise
        */
        auto is_match(const char* haystack, int haystack_len) -> bool;
        /**
         * Compute how well the haystack matches the fuzzy queries.
         *
         * This functions assumes `is_match` returns true for the
         * same haystack.
         *
         * @param haystack the string that is known to match all
         *      fuzzy queries given in the constructor.
         *
         * @return the score.
        */
        auto calc_score(const std::string& haystack) -> ScoreResults;
        /**
         * Print all queries given in the constructor.
        */
        auto print() const -> void;

    private:
        std::vector<qdata::QueryData> queries;
        std::vector<const char*> haystack_offsets;
        std::vector<std::vector<int>> char_to_indices;
        std::string word_delims;
        subseq::Stack stack;
        subseq::HaystackData<Scorer> haystack_data;
        int tot_query_len;
};

template<typename Scorer>
Fuzzy<Scorer>::Fuzzy(const std::vector<qdata::QueryData>& queries) : queries(queries) {
    int max_len = 0;
    int tot_query_len = 0;
    for (const auto& query : queries) {
        tot_query_len += query.q_len;
        if (query.q_len > max_len) {
            max_len = query.q_len;
        }
    }
    this->tot_query_len = tot_query_len;
    max_len *= 4;

    this->word_delims = queries[0].word_delims;
    this->haystack_offsets = std::vector<const char*>(queries.size(), 0);
    this->char_to_indices = std::vector<std::vector<int>>(256);
    this->stack = subseq::Stack(max_len);
    this->haystack_data = subseq::HaystackData<Scorer>(max_len);
    for (const auto& indices : this->char_to_indices) {
        this->char_to_indices.reserve(256);
    }
}

// TODO: `haystack` should be const.
template<typename Scorer>
auto Fuzzy<Scorer>::is_match(const char* haystack, int haystack_len) -> bool {
    const char* prev_end = haystack;
    int dummy_var = 0;
    for (int j = 0; j < queries.size(); ++j) {
        const auto& [offset, match_end] = filters::find_subseq_range(prev_end, dummy_var, queries[j]);

        if (offset == 0) { // Short-circuit exit on first failure.
            return false;
        }
        if ((j > 0) && queries[j].preserve_order && (offset < haystack_offsets[j - 1])) {
            return false;
        }

        haystack_offsets[j] = offset;
        prev_end = queries[j].preserve_order ? match_end + 1 : haystack;
    }
    return true;
}

// TODO: is_match must be true, otherwise segfault.
template<typename Scorer>
auto Fuzzy<Scorer>::calc_score(const std::string& haystack) -> ScoreResults {
    find_delims(haystack, word_delims.data(), haystack_data.delim_indices);
    float score = 0.0f;
    const auto haystack_c = haystack.data();
    const auto n_queries = queries.size();
    auto path = std::vector<int>(this->tot_query_len, 0);
    auto best_path_beg = std::begin(path);

    for (int j = 0; j < n_queries; ++j) {
        const auto& qdata = queries[j];

        for (int j = 0; j < qdata.q_len; ++j) {
            char_to_indices[qdata.q[j]].clear();
        }

        int dd = haystack_offsets[j] - haystack_c;
        int size = subseq::map_indices(haystack_c, dd, qdata.include_str, char_to_indices, qdata.ignore_case);
        create_graphs(haystack_data, qdata, char_to_indices);
        create_other(haystack_data, qdata.q_len, haystack);
        resize(stack, size);

        // TODO: minimize distance to previous query when preserving
        // order.
        score += subseq::get_score(qdata, stack, haystack_data);
        float ceil = (int)score + 1.0f;
        score += (ceil - score)*(1.0f - 1.0f/haystack.size());

        auto beg = std::begin(haystack_data.best_path);
        std::copy(beg, beg + qdata.q_len, best_path_beg);
        best_path_beg += qdata.q_len;
    }
    if (n_queries > 1) {
        std::ranges::sort(path);
    }

    //std::cout << score << std::endl;
    return ScoreResults{score, path};
}

template<typename Scorer>
auto Fuzzy<Scorer>::print() const -> void {
    for (const auto& query : queries) {
        std::cout << query.q << std::endl;
    }
}
} // namespace fuzzy

#endif

#ifndef SUBSEQSEARCH_SUBSEQ_H
#define SUBSEQSEARCH_SUBSEQ_H

#include <cmath>
#include <array>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

#include "querydata.h"

namespace qdata = qrydata;

namespace subseq {

/**
*/
struct GraphNode {
    int idx;
    int depth;
    float score;
    int branch;
    int right_delim_idx;
    int parent_idx;
};

/**
*/
template<typename Scorer>
struct HaystackData {
    std::vector<int> idx_to_right_delim;
    std::vector<int> delim_indices;
    std::vector<bool> idx_to_islower;
    std::vector<std::vector<int>> graph;
    std::vector<std::vector<float>> score_graph;
    std::vector<int> path_branches;
    std::vector<float> path_scores;
    std::vector<int> path;
    std::vector<int> best_path;
    Scorer scorer;

    HaystackData() {}

    explicit HaystackData(int max_len) : idx_to_right_delim(1024), delim_indices(1024), idx_to_islower(1024), graph(max_len), score_graph(max_len), path_branches(max_len), path_scores(max_len), path(max_len, 0), best_path(max_len, 0), scorer() {
    }
    
    /**
    */
    auto operator()(const GraphNode& parent, const GraphNode& child) const -> float {
        auto child_delim_idx = idx_to_right_delim[child.idx];
        bool in_same_word = child_delim_idx == parent.right_delim_idx;
        float score = parent.score;
        score += scorer.word_len(child_delim_idx, in_same_word, delim_indices);
        score += scorer.word_dist(child_delim_idx, parent.right_delim_idx, in_same_word);
        score += scorer.is_new_word(in_same_word);
        score += scorer.is_not_beg(child.idx, child_delim_idx, idx_to_islower, delim_indices);
        score += scorer.is_noncontiguous(child.idx, parent.idx);
        return score;
    }

    /**
    */
    auto operator()(int idx) const -> float {
        auto delim_idx = idx_to_right_delim[idx];
        float score = 0.0f;
        score += scorer.word_len(delim_idx, false, delim_indices);
        score += scorer.word_dist(idx, idx, true);
        score += scorer.is_new_word(false);
        score += scorer.is_not_beg(idx, delim_idx, idx_to_islower, delim_indices);
        score += scorer.is_noncontiguous(idx, idx);
        return score;
    }
};

/**
*/
class Stack {

    public:
        Stack() {}

        /**
        */
        Stack(int init_size) : cur_idx(0) {
            auto node = GraphNode{-1, -1, 20000000.0f, -1, -1, -1};
            this->stack = std::vector<GraphNode>(init_size, node);
        }

        /**
        */
        auto push(int idx, int depth, float score, int branch, int right_delim_idx, int parent_idx) -> void {
            stack[cur_idx].depth = depth;
            stack[cur_idx].idx = idx;
            stack[cur_idx].score = score;
            stack[cur_idx].branch = branch;
            stack[cur_idx].right_delim_idx = right_delim_idx;
            stack[cur_idx].parent_idx = parent_idx;
            ++cur_idx;
        }

        /**
        */
        auto pop() -> GraphNode {
            --cur_idx;
            return stack[cur_idx];
        }

        auto peek() -> GraphNode& {
            return stack[cur_idx]; // out of bounds.
        }

        /**
        */
        auto size() const -> int {
            return stack.size();
        }

        /**
        */
        auto empty() const -> bool {
            return cur_idx == 0;
        }

        /**
        */
        auto clear() -> void {
            cur_idx = 0;
        }

    private:
        std::vector<GraphNode> stack;
        int cur_idx;
};

/**
 * Create a map from characters to their indices in a sequence.
 *
 * @param seq sequence being mapped (haystack)
 * @param offset index of `seq` from where to start
 * @param include_set set of characters to map.  Any character in
 *      `seq` that is not in `include_str` is ignored.
 * @param char_to_indices the map to store results in
 * @param ignore_case whether case of letters in `seq` should be
 *      ignored
 *
 * @return total size of all the containers in `char_to_indices`.
*/
template<typename Map>
auto map_indices(const char* seq, int offset, const char* include_set, Map& char_to_indices, bool ignore_case) -> int {
    int size = 0;
    const auto og_seq = seq;
    seq += offset - 1;
    while (seq) {
        seq = strpbrk(seq + 1, include_set);
        if (!seq) {
            break;
        }
        auto ch = ignore_case ? std::tolower(*seq) : *seq;
        char_to_indices[ch].push_back(seq - og_seq);
        ++size;
    }
    return size;
}

/**
 * Score how well the haystack matches the query.
 *
 * This is a more exhaustive version of `int get_score(const QueryData&, std::unordered_map<char, std::vector<int>>&, std::string&)`.
 * This can give more intuitive results, but is slightly slower.
 *
 * @param qdata query
 * @param char_to_indices map from char in `qdata.q[j]` to indices
 *      they occur in the haystack.
 * @param seq sequence (haystack) being searched for the query
 * @param path container in which to store intermediate paths
 * @param stack container in which to store intermediate tree
 *      traversals
 *
 * @return the score (lower is better).
*/
template<typename Scorer>
auto get_score(const qdata::QueryData& qdata, Stack& stack, HaystackData<Scorer>& hd) -> float;

template<typename Scorer>
auto init_stack(Stack& stack, const HaystackData<Scorer>& hd) -> void {
    for (int branch = 0; const auto& idx : hd.graph[0]) {
        float score = hd(idx);
        stack.push(idx, 0, score, branch, hd.idx_to_right_delim[idx], -1);
        ++branch;
    }
}

template<typename Scorer>
auto maybe_visit(Stack& stack, const GraphNode& parent, const GraphNode& child, const HaystackData<Scorer>& hd, int best_score) -> void {
    if (float score = hd(parent, child); score < best_score) {
        stack.push(child.idx, parent.depth + 1, score, child.branch, hd.idx_to_right_delim[child.idx], parent.idx);
    }
}

/**
*/
template<typename Scorer>
auto update_score_graph(HaystackData<Scorer>& hd, int n_layers) -> void {
    for (int j = 0; j < n_layers; ++j) {
        hd.score_graph[j][hd.path_branches[j]] = hd.path_scores[j];
        hd.best_path[j] = hd.path[j];
    }
}

/**
*/
template<typename Scorer>
auto update_paths(HaystackData<Scorer>& hd, const GraphNode& node) -> void {
    hd.path[node.depth] = node.idx;
    hd.path_scores[node.depth] = node.score;
    hd.path_branches[node.depth] = node.branch;
}

template<typename Scorer>
auto get_score(const qdata::QueryData& qdata, Stack& stack, HaystackData<Scorer>& hd) -> float {
    GraphNode child;
    float best_score = 20000000.0f;
    init_stack(stack, hd);
    while (!stack.empty()) {
        const auto& parent = stack.pop();

        if (parent.score >= hd.score_graph[parent.depth][parent.branch]) {
            continue;
        }

        update_paths(hd, parent);

        // Update best.
        if ((parent.depth + 1) == qdata.q_len) {
            if (parent.score < best_score) {
                best_score = parent.score;
                update_score_graph(hd, qdata.q_len);
            }
            continue;
        }

        // Add children.
        int dist = parent.idx - parent.parent_idx;
        for (int branch = 0; const auto child_idx : hd.graph[parent.depth + 1]) {
            if (((dist < qdata.max_symbol_dist) || ((child_idx - parent.idx) < qdata.max_symbol_dist)) && (child_idx > parent.idx)) {
                child.branch = branch;
                child.idx = child_idx;
                maybe_visit(stack, parent, child, hd, best_score);
            }
            ++branch;
        }
    }

    return best_score;
}

} // namespace subseq

#endif

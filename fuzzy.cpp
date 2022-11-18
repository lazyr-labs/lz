#include <algorithm>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>
#include <string.h>

#include "fuzzy.h"

/**
*/
auto fuzzy::remove_outofbounds(std::vector<std::vector<int>>& graph, int max_depth) -> void {
    const int max_idx = graph[max_depth].back();
    for (int cur_depth = 0; cur_depth < max_depth; ++cur_depth) {
        const int hops_to_bottom = max_depth - cur_depth;
        while ((graph[cur_depth].back() + hops_to_bottom) > max_idx) {
            graph[cur_depth].pop_back();
        }
    }
}

/**
*/
auto fuzzy::remove_max_nodes(std::vector<std::vector<int>>& graph, int max_depth) -> void {
    for (int cur_depth = max_depth; cur_depth > 0; --cur_depth) {
        const int cur_max = graph[cur_depth].back();
        const int prev_depth = cur_depth - 1;
        while (graph[prev_depth].back() > cur_max) {
            graph[prev_depth].pop_back();
        }
    }
}

/**
*/
auto fuzzy::remove_min_nodes(std::vector<std::vector<int>>& graph, int max_depth) -> void {
    for (int cur_depth = 0; cur_depth < max_depth; ++cur_depth) {
        const int cur_min = graph[cur_depth].front();
        int first_greater = 0;
        while (graph[cur_depth + 1][first_greater] < cur_min) {
            ++first_greater;
        }
        auto beg = std::begin(graph[cur_depth + 1]);
        graph[cur_depth + 1].erase(beg, beg + first_greater);
    }
}

/**
*/
auto fuzzy::find_delims(const std::string& haystack, const char* word_delims, std::vector<int>& delim_indices) -> void {
    delim_indices.clear();
    auto seq = haystack.data();
    const auto beg = seq;
    --seq;
    while (seq) {
        seq = strpbrk(seq + 1, word_delims);
        if (seq) {
            delim_indices.push_back(seq - beg);
        }
    }
    delim_indices.push_back(haystack.size());
}

#include "lzapi.h"

namespace qparse = qryparser;
namespace qdata = qrydata;

namespace lz {

/**
*/
auto _fill_batch(std::vector<std::vector<MatchInfo>>& strings, const std::vector<std::string>& items, int batch_size, int offset, const std::string& filename) -> int {
    auto n_items = std::size(items);
    auto n_threads = std::size(strings);
    std::ranges::for_each(strings, [&](auto& v) { v.clear(); });
    for (int count = 0; count < batch_size; ++count) {
        for (int j = 0; j < n_threads; ++j) {
            if (offset >= n_items) {
                // TODO: return -1.
                return offset + 1;
            }
            strings[j].push_back(MatchInfo{.text=items[offset], .filename=filename, .lineno=offset});
            ++offset;
        }
    }
    return offset;
}

/**
*/
auto _fill_batch(std::vector<std::vector<MatchInfo>>& strings, std::istream& is, int batch_size, int offset, const std::string& filename) -> int {
    auto n_threads = strings.size();
    for (int j = 0; j < n_threads; ++j) {
        strings[j].clear();
    }
    std::string line;
    for (int count = 0; count < batch_size; ++count) {
        for (int j = 0; j < n_threads; ++j) {
            if (!std::getline(is, line)) {
                return -1;
            }
            strings[j].push_back(MatchInfo{.text=line, .filename=filename, .lineno=offset});
            ++offset;
        }
    }
    return offset;
}

/**
 * Initialize `n` score vectors.
*/
auto _create_scores(int n, int topk) -> std::vector<std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>> {
    auto scores = std::vector<std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>>(n);
    for (int j = 0; j < n; ++j) {
        scores[j].reserve(topk);
    }
    return scores;
}

/**
 * Add score to heap if it is less than any element in the heap.
 *
 * The heap (`scores`) is limited in size to `topk` elements.  If the
 * element in the heap with the largest score has a greater score than
 * `score`, then it is replaced with a new element with the given score.
 *
 * @param scores the heap
 * @param topk maximum size of the heap
 * @param score score to potentially add to the heap
 * @param line value associated with `score` that is inserted into the
 *      heap along with `score` as a pair.
*/
auto _add_score(std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, int topk, fuzzy::ScoreResults&& score, const MatchInfo& match_info) -> void {
    if (scores.size() < topk) {
        auto mi = MatchInfo(match_info);
        auto p = std::pair<fuzzy::ScoreResults, MatchInfo>(std::move(score), mi);
        scores.emplace_back(std::move(p));
        std::ranges::push_heap(scores, lz::_comparator);
    }
    else if (score.score < scores[0].first.score) {
        std::ranges::pop_heap(scores, lz::_comparator);

        auto& p = scores.back();
        p.first = std::move(score);
        p.second = MatchInfo(match_info);

        std::ranges::push_heap(scores, lz::_comparator);
    }
}

auto set_case_if_smart(qdata::SearchArgs& search_args) -> void {
    if (!search_args.smart_case) {
        return;
    }

    bool has_upper = std::ranges::any_of(
            search_args.q,
            [](const auto& a) { return std::isupper(a); });
    search_args.ignore_case = !has_upper;
}

} // namespace lz

#ifndef LAZYAPI_H
#define LAZYAPI_H

#include <algorithm>
#include <cctype>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "querydata.h"
#include "query_parser.h"
#include "fuzzy.h"
#include "scores.h"

namespace lz {

namespace qparse = qryparser;
namespace qdata = qrydata;

/**
*/
struct _Comparator {

    /**
    */
    template<typename T>
    auto operator()(const T& a, const T& b) -> bool {
        return a.first.score < b.first.score;
    }
};

constexpr auto _comparator = _Comparator();

/**
*/
struct MatchInfo {
    std::string text;
    std::string filename;
    int lineno;
};

auto _create_scores(int n, int topk) -> std::vector<std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>>;
auto _fill_batch(std::vector<std::vector<MatchInfo>>& strings, const std::vector<std::string>& items, int batch_size, int offset, const std::string& filename = "") -> int;
auto _fill_batch(std::vector<std::vector<MatchInfo>>& strings, std::istream& is, int batch_size, int offset, const std::string& filename = "") -> int;

/**
 * Set case if using smart case.
*/
auto set_case_if_smart(qdata::SearchArgs& search_args) -> void;
auto _add_score(std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, int topk, fuzzy::ScoreResults&& score, const MatchInfo& match_info) -> void;

/**
*/
template<typename Scorer>
auto _find_match(const MatchInfo& line, const qparse::Query<Scorer>& query, std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, int topk) -> bool {
    const auto seq = line.text.data();

    if (!query.fuzzy->is_match(seq, line.text.size()) || !query.filter_tree->is_match(line.text)) {
        return false;
    }

    auto score_results = query.fuzzy->calc_score(line.text);
    _add_score(scores, topk, std::move(score_results), line);
    return true;
}

/**
 * Search a vector for query matches.
*/
template<typename Scorer>
auto _search(const qdata::SearchArgs& search_args, std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, const std::vector<std::string>& lines) -> void {
    // TODO: do this once and copy to each thread.
    const auto query = qparse::getparse<Scorer>(search_args);
    int n_matches = 0;
    auto match_info = MatchInfo{"", "", 0};
    for (const auto& line : lines) {
        match_info.lineno += 1;
        match_info.text = line;
        n_matches += _find_match(match_info, query, scores, search_args.topk);
    }

    //std::cout << n_matches << std::endl;
}

/**
 * Search a vector for query matches.
*/
template<typename Scorer>
auto _search(const qdata::SearchArgs& search_args, std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, const std::vector<MatchInfo>& lines) -> void {
    // TODO: do this once and copy to each thread.
    const auto query = qparse::getparse<Scorer>(search_args);
    int n_matches = 0;
    for (const auto& line : lines) {
        n_matches += _find_match(line, query, scores, search_args.topk);
    }

    //std::cout << n_matches << std::endl;
}

/**
 * Search an input stream query matches.
 *
 * @param search_args search args.
 * @param scores container in which to add scores of matches.
 * @param is input stream of newline-separated lines to search over.
 * @param end the byte of the stream to stop reading at.  If this is
 *      less than 1, then reading will continue until the end of file.
*/
template<typename Scorer>
auto _search(const qdata::SearchArgs& search_args, std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, std::basic_istream<char>& is, const std::string& filename) -> void {
    const auto query = qparse::getparse<Scorer>(search_args);
    std::string line;
    int n_matches = 0;
    auto match_info = MatchInfo{"", filename, 0};
    while (std::getline(is, line)) {
        match_info.text = line;
        match_info.lineno += 1;
        n_matches += _find_match(match_info, query, scores, search_args.topk);
    }
    //std::cout << n_matches << std::endl;
}

/**
 * Entry for search.
 *
 * This prepares the data necessary for searching.
 *
 * @param search_args search args.  If `filename == ""`,
 *     input will be read from stdin.
 * @param scores container to hold scores in
*/
template<typename Scorer>
auto _start_search(const qdata::SearchArgs& search_args, std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>& scores, const std::string& filename) -> void {
    bool using_cin = filename.empty();
    std::ifstream fis = std::ifstream(filename);
    auto& is = using_cin ? std::cin : fis;

    // This makes reading from stdin fast.
    if (using_cin) {
        std::ios::sync_with_stdio(false);
    }

    _search<Scorer>(search_args, scores, is, filename);

    fis.close();
}

/**
 * Search using one thread only.
*/
template<typename Scorer>
auto single_threaded_search(const qdata::SearchArgs& search_args) -> std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>> {
    auto scores = _create_scores(1, search_args.topk)[0];
    for (const auto& filename : search_args.filenames) {
        _start_search<Scorer>(search_args, scores, filename);
    }
    std::ranges::sort(scores, _comparator);
    return scores;
}

/**
 * Search using one thread only.
*/
template<typename Scorer>
auto single_threaded_search(const qdata::SearchArgs& search_args, const std::vector<std::string>& lines) -> std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>> {
    auto scores = _create_scores(1, search_args.topk)[0];
    _search<Scorer>(search_args, scores, lines);
    std::ranges::sort(scores, _comparator);
    return scores;
}

template<typename Scorer>
auto multi_threaded_search(const qdata::SearchArgs& search_args) -> std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>> {
    unsigned int n_threads = std::thread::hardware_concurrency();
    auto thread_scores = _create_scores(n_threads, search_args.topk);

    auto range = std::vector<int>(n_threads, 0);
    std::iota(std::begin(range), std::end(range), 0);
    auto batch = std::vector<std::vector<MatchInfo>>(n_threads);
    for (auto& sj : batch) {
        sj.reserve(search_args.batch_size);
    }

    for (const auto& filename : search_args.filenames) {
        // TODO: this is a duplicate of `start_search`.
        bool using_cin = filename.empty();
        std::ifstream fis = std::ifstream(filename);
        auto& is = using_cin ? std::cin : fis;
        // This makes reading from stdin fast.
        if (using_cin) {
            std::ios::sync_with_stdio(false);
        }

        for (int n_lines_read = 1; n_lines_read > -1;) {
            n_lines_read = _fill_batch(batch, is, search_args.batch_size, n_lines_read, filename);
            std::for_each(
                    std::execution::par,
                    std::cbegin(range), std::cend(range),
                    [&](const auto k) {
                        _search<Scorer>(search_args, thread_scores[k], batch[k]);
                    });
        }
        fis.close();
    }

    // Aggregate thread-specific scores into a single vector, sort, and show.
    auto best_scores = std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>();
    best_scores.reserve(search_args.topk * n_threads);
    for (const auto& scores : thread_scores) {
        for (const auto& score : scores) {
            best_scores.push_back(score);
        }
    }
    std::sort(
            std::execution::par,
            std::begin(best_scores), std::end(best_scores),
            _comparator);
    return best_scores;
}

template<typename Scorer>
auto multi_threaded_search(const qdata::SearchArgs& search_args, const std::vector<std::string>& strings) -> std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>> {
    unsigned int n_threads = std::thread::hardware_concurrency();
    auto thread_scores = _create_scores(n_threads, search_args.topk);

    auto range = std::vector<int>(n_threads, 0);
    std::iota(std::begin(range), std::end(range), 0);
    auto batch = std::vector<std::vector<MatchInfo>>(n_threads);
    for (auto& sj : batch) {
        sj.reserve(search_args.batch_size);
    }

    const auto n_strings = std::size(strings);
    for (int n_strings_read = 1; n_strings_read <= n_strings;) {
        n_strings_read = _fill_batch(batch, strings, search_args.batch_size, n_strings_read);
        std::for_each(
                std::execution::par,
                std::cbegin(range), std::cend(range),
                [&](const auto k) {
                    _search<Scorer>(search_args, thread_scores[k], batch[k]);
                });
    }

    // Aggregate thread-specific scores into a single vector, sort, and show.
    auto best_scores = std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>>();
    best_scores.reserve(search_args.topk * n_threads);
    for (const auto& scores : thread_scores) {
        for (const auto& score : scores) {
            best_scores.push_back(score);
        }
    }
    std::sort(
            std::execution::par,
            std::begin(best_scores), std::end(best_scores),
            _comparator);
    return best_scores;
}

template<typename Scorer>
auto search(const qdata::SearchArgs& search_args, const std::vector<std::string>* strings = nullptr) -> std::vector<std::pair<fuzzy::ScoreResults, MatchInfo>> {
    if (strings != nullptr) {
        if (search_args.parallel) {
            return multi_threaded_search<Scorer>(search_args, *strings);
        }
        return single_threaded_search<Scorer>(search_args, *strings);
    }
    else {
        if (search_args.parallel) {
            return multi_threaded_search<Scorer>(search_args);
        }
        return single_threaded_search<Scorer>(search_args);
    }
}

} // namespace lz

#endif

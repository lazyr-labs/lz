// gpp -o lz -std=c++20 -ffast-math -O3 -march=native lazy.cpp filter_tree.cpp query_parser.cpp scores.cpp fuzzy.cpp subseq.cpp filters.cpp querydata.cpp -ltbb; /usr/bin/time ./lz -k10 'vypybn' < data.txt; /usr/bin/time ./lz -o -k100 'arlst' < data.txt
// TODO:
//   o case
//   o prefix
//   o suffix
//   o exact
//   o fuzzy
//   o negate
//   o phrase
//   o or
//   o pipe io
//   o parentheses
//   o parallel
//   o multiple files
//   o cmdline args
//   o score if all in same word
//   o epsilon-gram
//   o start next query after the end of previous one
//   o add length correction to linearscorer
//   o expose delims as cmdline args
//   o color output
//   o try to preserve order of fuzzy ANDs
//   * guarantee to preserve order of fuzzy ANDs
//   * index
//   * custom score
//   * normalize score
//   * weight scores
//   * ngram fuzzy search
//   * find all optimal paths
//   * rerank results that don't use log
//   * coarse score to fine
//   * minimize distance to previous query when preserving order
//   * ignore case except ones that are upper
//   * interactive menu
#include <algorithm>
#include <cctype>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <getopt.h>

#include "lzapi.h"
#include "querydata.h"
#include "query_parser.h"
#include "fuzzy.h"
#include "scores.h"

namespace qdata = qrydata;

/**
*/
auto get_cmdline_args(int argc, char* argv[]) -> qdata::SearchArgs {
    auto search_args = qdata::SearchArgs{
        .q="",
        .ignore_case=true,
        .smart_case=true,
        .topk=100,
        .filenames=std::vector<std::string>{""},
        .parallel=false,
        .preserve_order=false,
        .batch_size=10000,
        .max_symbol_dist=10,
        .gap_penalty="linear",
        .word_delims=":;,./-_ \t",
        .show_color=true,
        .show_count=true,
        .show_score=true,
        .show_line=true,
        .trim_empty=false,
    };

    const auto shortopts = "opIClciStk:s:g:d:";
    const int BATCH_SIZE=1,
              NO_COLOR='C',
              GAP_PENALTY='g',
              MAX_SYMBOL_GAP='s',
              PRESERVE_ORDER='o',
              PARALLEL='p',
              TOPK='k',
              NO_IGNORE_CASE='I',
              IGNORE_CASE='i',
              WORD_DELIMS='d',
              NO_COUNT='c',
              NO_SCORE='S',
              NO_LINE='l',
              TRIM_EMPTY='t';

    int opt_idx;
    option longopts[] = {
        option{.name="ignore-case", .has_arg=no_argument, .flag=0, .val=IGNORE_CASE},
        option{.name="no-ignore-case", .has_arg=no_argument, .flag=0, .val=NO_IGNORE_CASE},
        option{.name="topk", .has_arg=required_argument, .flag=0, .val=TOPK},
        option{.name="parallel", .has_arg=no_argument, .flag=0, .val=PARALLEL},
        option{.name="preserve-order", .has_arg=no_argument, .flag=0, .val=PRESERVE_ORDER},
        option{.name="batch-size", .has_arg=required_argument, .flag=0, .val=BATCH_SIZE},
        option{.name="max-symbol-gap", .has_arg=required_argument, .flag=0, .val=MAX_SYMBOL_GAP},
        option{.name="gap-penalty", .has_arg=required_argument, .flag=0, .val=GAP_PENALTY},
        option{.name="word-delims", .has_arg=required_argument, .flag=0, .val=WORD_DELIMS},
        option{.name="no-color", .has_arg=required_argument, .flag=0, .val=NO_COLOR},
        option{.name="no-score", .has_arg=required_argument, .flag=0, .val=NO_SCORE},
        option{.name="no-count", .has_arg=required_argument, .flag=0, .val=NO_COUNT},
        option{.name="no-line", .has_arg=required_argument, .flag=0, .val=NO_LINE},
        option{.name="trim-empty-filenames", .has_arg=required_argument, .flag=0, .val=TRIM_EMPTY},
        option{.name=0, .has_arg=0, .flag=0, .val=0},
    };

    while (true) {
        int c = getopt_long(argc, argv, shortopts, longopts, &opt_idx);

        if (c == -1) {
            break;
        }

        switch (c) {
            case TOPK:
                search_args.topk = std::atoi(optarg);
                break;
            case IGNORE_CASE:
                search_args.ignore_case = true;
                search_args.smart_case = false;
                break;
            case NO_IGNORE_CASE:
                search_args.ignore_case = false;
                search_args.smart_case = false;
                break;
            case PARALLEL:
                search_args.parallel = true;
                break;
            case PRESERVE_ORDER:
                search_args.preserve_order = true;
                break;
            case BATCH_SIZE:
                search_args.batch_size = std::atoi(optarg);
                break;
            case MAX_SYMBOL_GAP:
                search_args.max_symbol_dist = std::atoi(optarg);
                if (search_args.max_symbol_dist < 1) {
                    search_args.max_symbol_dist = std::numeric_limits<int>::max();
                }
                break;
            case GAP_PENALTY:
                search_args.gap_penalty = optarg;
                break;
            case WORD_DELIMS:
                search_args.word_delims = optarg;
                break;
            case NO_COLOR:
                search_args.show_color = false;
                break;
            case NO_SCORE:
                search_args.show_score = false;
                break;
            case NO_COUNT:
                search_args.show_count = false;
                break;
            case NO_LINE:
                search_args.show_line = false;
                break;
            case TRIM_EMPTY:
                search_args.trim_empty = true;
                break;
        }
    }

    if (optind < argc) {
        search_args.q = std::string(argv[optind]);
        lz::set_case_if_smart(search_args);

        ++optind;
        if (optind < argc) {
            search_args.filenames.clear();
        }
        for (; optind < argc; ++optind) {
            search_args.filenames.emplace_back(std::string(argv[optind]));
        }
    }
    else {
        throw std::runtime_error("Query not given.");
    }

    return search_args;
}

/**
 * Show the first `topk` pairs of `scores`.
*/
template<typename ContainerOfPairs>
auto print_scores(const ContainerOfPairs& scores, auto search_args) -> void {
    auto beg = std::cbegin(scores);
    auto end = scores.size() < search_args.topk ? std::cend(scores) : beg + search_args.topk;

    const auto red = "\033[31m";
    const auto reset = "\033[0m";
    for (; beg != end; ++beg) {
        if (!search_args.show_color) {
            if (search_args.show_count)
                std::cout << beg->first.score << " ";
            if (!search_args.trim_empty || beg->second.filename.length() > 0)
                std::cout << beg->second.filename << " ";
            if (search_args.show_line)
                std::cout << beg->second.lineno << " ";
            std::cout << beg->second.text << std::endl;
            continue;
        }
        //std::cout << beg->first.score << " " << beg->second << std::endl;
        //std::cout << red << beg->first.score << " " << beg->second << reset << std::endl;
        if (search_args.show_score)
            std::cout << beg->first.score << " ";
        if (!search_args.trim_empty || beg->second.filename.length() > 0)
            std::cout<<beg->second.filename << " " ;
        if (search_args.show_line)
            std::cout << beg->second.lineno << " ";
        int prev = 0;
        auto strbeg = std::cbegin(beg->second.text);
        for (const auto& aj : beg->first.path) {
            const std::string_view tmp(strbeg + prev, strbeg + aj);
            std::cout << tmp;
            std::cout << red << beg->second.text[aj] << reset;
            prev = aj + 1;
        }
        const std::string_view tmp(strbeg + prev, std::cend(beg->second.text));
        std::cout << tmp << std::endl;
    }
    if (search_args.show_count)
        std::cout << scores.size() << std::endl;
}

auto main(int argc, char* argv[]) -> int {
    auto search_args = get_cmdline_args(argc, argv);
    //std::ifstream fis = std::ifstream("data.txt");
    //std::string line;
    //auto lines = std::vector<std::string>();
    //while (std::getline(fis, line)) {
        //lines.emplace_back(std::move(line));
    //}

    auto scores = ([&]() -> auto {
            if (search_args.gap_penalty == "log") {
                return lz::search<scores::LogScorer>(search_args);
            }
            else {
                return lz::search<scores::LinearScorer>(search_args);
            }
    })();

    print_scores(scores, search_args);

    return 0;
}

#include <memory>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

#include "query_parser.h"
#include "querydata.h"
#include "filters.h"
#include "filter_tree.h"
#include "fuzzy.h"

namespace qparse = qryparser;

/**
 * @return true if `c` is any of the characters in `delims`, false
 *      otherwise.
*/
auto qparse::is_delim(const char c, const std::string& delims) -> bool {
    for (const auto& delim : delims) {
        if (c == delim) {
            return true;
        }
    }
    return false;
}

auto qparse::parse_exact(std::string::const_iterator& beg, std::string::const_iterator end, const std::string& delims) -> std::string {
    if (beg >= end) {
        throw std::runtime_error("No string given.  Maybe you forget to escape a meta character or close a phrase.");
    }

    std::string s;
    for (; beg < end; ++beg) {
        const auto c = *beg;
        if ((*(beg - 1) != '\\') && is_delim(c, delims)) {
            break;
        }
        if (c == '\\') {
            continue;
        }
        s += c;
    }

    return s;
}

auto qparse::parse_phrase(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string {
    static const std::string delims = "\"";
    static const std::string valid_end_chars = " )|";
    std::string s = qparse::parse_exact(beg, end, delims);

    if (s.size() < 1) {
        throw std::runtime_error("Phrase can't be empty.");
    }
    if (beg >= end) {
        throw std::runtime_error("Closing \" not found.");
    }

    if (((beg + 1) < end) && !qparse::is_delim(*(beg + 1), valid_end_chars)) {
        throw std::runtime_error("Extra symbols after closing \".");
    }

    ++beg;
    return s;
}

auto qparse::parse_meta(std::string::const_iterator& beg, std::string::const_iterator end, std::string&& meta) -> std::string {
    static const std::string delims = " )|";

    if (*beg == '"') {
        ++beg;
        return qparse::parse_phrase(beg, end);
    }
    if (*beg == ' ') {
        auto msg = "Empty " + meta + ".  Use \\ to escape the space, or wrap in \" to match a space, or \\" + meta + " to match a literal " + meta + ".";
        throw std::runtime_error(msg);
    }

    return qparse::parse_exact(beg, end, delims);
}

auto qparse::parse_fuzzy(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string {
    return qparse::parse_meta(beg, end, "~");
}

auto qparse::parse_prefix(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string {
    return qparse::parse_meta(beg, end, "^");
}

auto qparse::parse_suffix(std::string::const_iterator& beg, std::string::const_iterator end) -> std::string {
    return qparse::parse_meta(beg, end, "$");
}

auto qparse::parse_neg(std::string::const_iterator& beg, std::string::const_iterator end, bool ignore_neg, const qdata::SearchArgs& search_args) -> std::unique_ptr<filtertree::Filter> {
    return qparse::select_parse(beg, end, true, search_args);
}

auto qparse::skip_delim(std::string::const_iterator& beg, std::string::const_iterator end, char delim) -> void {
    while ((beg < end) && (*beg == delim)) {
        ++beg;
    }
}

auto qparse::parse(std::string::const_iterator& beg, std::string::const_iterator end, const qdata::SearchArgs& search_args) -> std::vector<std::unique_ptr<filtertree::Filter>> {
    auto v = std::vector<std::unique_ptr<filtertree::Filter>>();
    v.reserve(16);
    int n_beg = 0;
    int n_end = 0;

    qparse::skip_delim(beg, end, ' ');
    while (beg < end) {
        auto s = qparse::select_parse(beg, end, false, search_args);

        // validation.
        if (!v.empty()) {
            const auto& last = v.back();
            if (s->get_type() == filtertree::FilterType::OR) {
                if (last->get_type() == filtertree::FilterType::OR) {
                    throw std::runtime_error("Missing text after `|`.");
                }
                else if ((last->get_type() == filtertree::FilterType::GRP_BEGIN) || (last->get_type() == filtertree::FilterType::NOT_GRP_BEGIN)) {
                    throw std::runtime_error("Missing text before `|`.");
                }
            }
            else if (s->get_type() == filtertree::FilterType::GRP_END) {
                if (last->get_type() == filtertree::FilterType::OR) {
                    throw std::runtime_error("Missing text after `|`.");
                }
                ++n_end;
            }
        }
        if ((s->get_type() == filtertree::FilterType::GRP_BEGIN) || (s->get_type() == filtertree::FilterType::NOT_GRP_BEGIN)) {
            ++n_beg;
        }

        v.emplace_back(std::move(s));
        qparse::skip_delim(beg, end, ' ');
    }

    if (v.empty()) {
        return v;
    }
    const auto& last = v.back();
    if ((last->get_type() == filtertree::FilterType::GRP_BEGIN) || (last->get_type() == filtertree::FilterType::OR)) {
        throw std::runtime_error("Can't end in `|` or `(`.");
    }
    if ((v.front()->get_type() == filtertree::FilterType::GRP_END) || (v.front()->get_type() == filtertree::FilterType::OR)) {
        throw std::runtime_error("Can't begin in `|` or `)`.");
    }
    if (n_beg != n_end) {
        throw std::runtime_error("Unbalanced parentheses.");
    }

    return v;
}

auto qparse::select_parse(std::string::const_iterator& beg, std::string::const_iterator end, bool ignore_neg, const qdata::SearchArgs& search_args) -> std::unique_ptr<filtertree::Filter> {
    static const std::string exact_delims = " )|";
    const auto& ch = *beg;
    auto s = std::string();
    std::unique_ptr<filtertree::Filter> qp;

    if (ch == '^') {
        ++beg;
        s = qparse::parse_prefix(beg, end);
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find_prefix, filtertree::FilterType::VARIABLE);
    }
    else if (ch == '$') {
        ++beg;
        s = qparse::parse_suffix(beg, end);
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find_suffix, filtertree::FilterType::VARIABLE);
    }
    else if (ch == '"') {
        ++beg;
        s = qparse::parse_phrase(beg, end);
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find_subseq, filtertree::FilterType::VARIABLE);
    }
    else if (ch == '=') {
        ++beg;
        s = qparse::parse_exact(beg, end, exact_delims);
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find, filtertree::FilterType::VARIABLE);
    }
    else if ((ch == '!') && !ignore_neg) {
        ++beg;
        qp = qparse::parse_neg(beg, end, ignore_neg, search_args);
        qp->negate = true;
    }
    else if (ch == '~') {
        ++beg;
        s = qparse::parse_fuzzy(beg, end);
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find_subseq, filtertree::FilterType::VARIABLE);
    }
    else if (ch == '(') {
        ++beg;
        if (ignore_neg) {
            qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find, filtertree::FilterType::NOT_GRP_BEGIN);
        }
        else {
            qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find, filtertree::FilterType::GRP_BEGIN);
        }
    }
    else if (ch == ')') {
        ++beg;
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find, filtertree::FilterType::GRP_END);
    }
    else if (ch == '|') {
        ++beg;
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find, filtertree::FilterType::OR);
    }
    else {
        s = qparse::parse_exact(beg, end, exact_delims);
        qp = std::make_unique<filtertree::Filter>(std::move(qdata::QueryData(search_args, s)), false, filters::find, filtertree::FilterType::VARIABLE);
    }
    return qp;
}

//auto main() -> int {
    //int j = f();
//
    //int topk = 10;
    //bool ignore_case = true;
//
    //std::string q = "abc xyz";
    //std::string delims = "2";
    //auto b = q.begin();
    //auto e = parse_exact(b, q.end(), delims);
    //assert(e == q);
    //assert(b == q.end());
//
    //b = q.begin();
    //delims = " ";
    //e = parse_exact(b, q.end(), delims);
    //assert(e == "abc");
    //assert(*b == ' ');
//
    //q = "abc\\ xyz";
    //b = q.begin();
    //e = parse_exact(b, q.end(), delims);
    //assert(e == "abc xyz");
//
    //q = "\\!";
    //b = q.begin();
    //e = parse_exact(b, q.end(), delims);
    //assert(e == "!");
//
    //q = "\\\"";
    //b = q.begin();
    //e = parse_exact(b, q.end(), delims);
    //assert(e == "\"");
//
    //q = "\\~";
    //b = q.begin();
    //e = parse_exact(b, q.end(), delims);
    //assert(e == "~");
//
    //q = "a\\ l\\sdls";
    //b = q.begin();
    //delims = " s";
    //e = parse_exact(b, q.end(), delims);
    //assert(e == "a lsdl");
    //assert(*b == 's');
//
    //q = "";
    //b = q.begin();
    //bool errored = false;
    //try {
        //e = parse_exact(b, q.end(), delims);
    //}
    //catch (std::exception& err) {
        ////std::cout << err.what() << std::endl;
        //errored = true;
    //}
    //assert(errored);
//
    //q = "abc \\\"x'yz\" 23e";
    //b = q.begin();
    //e = parse_phrase(b, q.end());
    //assert(e == "abc \"x'yz");
    //assert(*b == ' ');
//
    //q = "abc \\\"x'yz)\"";
    //b = q.begin();
    //e = parse_phrase(b, q.end());
    //assert(e == "abc \"x'yz)");
    //assert(b == q.end());
//
    //q = "123\")";
    //b = q.begin();
    //e = parse_phrase(b, q.end());
    //assert(e == "123");
    //assert(*b == ')');
//
    //q = "123\"|";
    //b = q.begin();
    //e = parse_phrase(b, q.end());
    //assert(e == "123");
    //assert(*b == '|');
//
    //q = "123\")|";
    //b = q.begin();
    //e = parse_phrase(b, q.end());
    //assert(e == "123");
    //assert(*b == ')');
//
    //q = "123\"|)";
    //b = q.begin();
    //e = parse_phrase(b, q.end());
    //assert(e == "123");
    //assert(*b == '|');
//
    //q = "\"";
    //b = q.begin();
    //errored = false;
    //try {
        //e = parse_phrase(b, q.end());
    //}
    //catch (std::exception& err) {
        ////std::cout << err.what() << std::endl;
        //errored = true;
    //}
    //assert(errored);
//
    //q = "abc|";
    //b = q.begin();
    //errored = false;
    //try {
        //e = parse_phrase(b, q.end());
    //}
    //catch (std::exception& err) {
        ////std::cout << err.what() << std::endl;
        //errored = true;
    //}
    //assert(errored);
//
    //q = "abc)";
    //b = q.begin();
    //errored = false;
    //try {
        //e = parse_phrase(b, q.end());
    //}
    //catch (std::exception& err) {
        ////std::cout << err.what() << std::endl;
        //errored = true;
    //}
    //assert(errored);
//
    //q = "abc \\\"x'yz\"123\"";
    //b = q.begin();
    //errored = false;
    //try {
        //e = parse_phrase(b, q.end());
    //}
    //catch (std::exception& err) {
        ////std::cout << err.what() << std::endl;
        //errored = true;
    //}
    //assert(errored);
//
    //q = "abc \\\"x'yz\"";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == "abc");
    //assert(*b == ' ');
//
    //q = "\"abc \\\"x'yz\"";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == "abc \"x'yz");
    //assert(b == q.end());
//
    //q = "\\ a";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == " a");
    //assert(b == q.end());
//
    //q = "$) a";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == "$");
    //assert(*b == ')');
//
    //q = "$| a";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == "$");
    //assert(*b == '|');
//
    //q = "$|) a";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == "$");
    //assert(*b == '|');
//
    //q = "$)| a";
    //b = q.begin();
    //e = parse_meta(b, q.end(), "$");
    //assert(e == "$");
    //assert(*b == ')');
//
    //q = " a";
    //b = q.begin();
    //errored = false;
    //try {
        //e = parse_meta(b, q.end(), "$");
    //}
    //catch (std::exception& err) {
        //errored = true;
        ////std::cout << err.what() << std::endl;
    //}
    //assert(errored);
//
    //q = "a";
    //b = q.begin();
    //e = parse_fuzzy(b, q.end());
    //assert(e == "a");
    //assert(b == q.end());
//
    //q = "a";
    //b = q.begin();
    //e = parse_suffix(b, q.end());
    //assert(e == "a");
    //assert(b == q.end());
//
    //q = "\"a\"";
    //b = q.begin();
    //e = parse_prefix(b, q.end());
    //assert(e == "a");
    //assert(b == q.end());
//
    //q = "a b";
    //b = q.begin();
    //e = parse_default(b, q.end());
    //assert(e == "a");
    //assert(*b == ' ');
//
    //q = "23sdf ";
    //b = q.begin();
    //auto ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "23sdf");
    //assert(*b == ' ');
//
    //q = "$23ed ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "23ed");
    //assert(*b == ' ');
//
    //q = "^98di ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "98di");
    //assert(*b == ' ');
//
    //q = "!ldkd ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "!ldkd");
    //assert(*b == ' ');
//
    //q = "\"sd sdf324\" ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "sd sdf324");
    //assert(*b == ' ');
//
    //q = "~eoi";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(b == q.end());
//
    //q = "!(eoi)";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "");
    //assert(ee->get_type() == FilterType::NOT_GRP_BEGIN);
    //assert(*b == 'e');
//
    //q = "(eoi)";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "");
    //assert(ee->get_type() == FilterType::GRP_BEGIN);
    //assert(*b == 'e');
//
    //q = "eoi)";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->get_type() == FilterType::GRP_END);
    //assert(b == q.end());
//
    //q = "a|eoi)";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(*b == ')');
//
    //q = "eoi|a";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->get_type() == FilterType::OR);
    //assert(*b == 'a');
//
    //q = "a|eoi|b";
    //b = q.begin();
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //ee = select_parse(b, q.end(), true, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(*b == '|');
//
    //q = "23sdf ";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "23sdf");
    //assert(*b == ' ');
//
    //q = "$23ed ";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "23ed");
    //assert(*b == ' ');
//
    //q = "^98di ";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "98di");
    //assert(*b == ' ');
//
    //q = "!ldkd ";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "!ldkd");
    //assert(*b == ' ');
//
    //q = "\"sd sdf324\" ";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "sd sdf324");
    //assert(*b == ' ');
//
    //q = "~eoi";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(b == q.end());
//
    //q = "!!!$^~\"!\"!";
    //b = q.begin();
    //ee = parse_neg(b, q.end(), ignore_case, topk);
    //assert(ee->qdata.q == "!!!$^~\"!\"!");
    //assert(b == q.end());
//
    //q = "!eoi ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(*b == ' ');
//
    //q = "!$eoi ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(*b == ' ');
//
    //q = "!!eoi ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "!eoi");
    //assert(*b == ' ');
//
    //q = "!^eoi ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(*b == ' ');
//
    //q = "!~eoi ";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(*b == ' ');
//
    //q = "!\"eoi\"";
    //b = q.begin();
    //ee = select_parse(b, q.end(), false, ignore_case, topk);
    //assert(ee->qdata.q == "eoi");
    //assert(b == q.end());
//
    //q = "23sdf)(asdf)";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "(23sdf)(asdf";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "23sdf|)";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "(|23sdf";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "(23sdf";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "23sdf )";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "23sdf (";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = ")23sdf";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "|23sdf";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //q = "23sdf|";
    //b = q.begin();
    //errored = false;
    //try {
        //parse(b, q.end(), ignore_case, topk);
    //}
    //catch (std::exception& e) {
        //errored = true;
    //}
    //assert(errored);
//
    //std::vector<std::string> outputs = {
        //"23sdf", "23ed", "98di", "ldkd", "sd sdf324", "eoi",
//
        //"(", "23sdf", ")", "(", "23ed", ")", "(", "98di", ")",
        //"(", "ldkd", ")", "(", "sd sdf324", ")", "(", "eoi", ")",
//
        //"(", "23sdf", "23ed", "98di", "ldkd", "sd sdf324", "eoi", ")",
//
        //"(", "23sdf", "23ed", "98di", ")", "(", "ldkd", "sd s()df324", ")", "ee()oi",
//
        //"23sdf", "|", "(", "23ed", "98di", ")", "|", "(", "ldkd", "sd s()df324", ")", "|", "ee()oi",
//
        //"23sdf", "|", "23ed", "|", "98di", "|", "ldkd", "|", "sd sdf324", "eoi",
//
        //"23sdf", "|", "23ed", "(", "98di", "|", "ldkd", ")", "|", "(", "sd sdf324", "eoi", ")",
    //};
    //q = "23sdf $23ed ^98di !ldkd \"sd sdf324\" ~eoi";
    //q += " (23sdf) ($23ed) (^98di) (!ldkd) (\"sd sdf324\") (~eoi)";
    //q += " (23sdf $23ed ^98di !ldkd \"sd sdf324\" ~eoi)";
    //q += " (23sdf $23ed ^98di ) ( !ldkd \"sd s()df324\"   ) ~ee\\(\\)oi";
    //q += " 23sdf | ($23ed ^98di )| ( !ldkd \"sd s()df324\"   )    |  ~ee\\(\\)oi";
    //q += " 23sdf|$23ed |^98di| !ldkd | \"sd sdf324\" ~eoi";
    //q += " 23sdf|$23ed (^98di|!ldkd)|(\"sd sdf324\" ~eoi)";
    //b = q.begin();
    //auto v = parse(b, q.end(), ignore_case, topk);
    //assert(v.size() == outputs.size());
    //for (int j = 0; j < v.size(); ++j) {
        ////std::cout << v[j] << " ... " << outputs[j] << std::endl;
        //if (outputs[j] == "(") {
            //assert(v[j]->get_type() == FilterType::GRP_BEGIN);
        //}
        //else if (outputs[j] == ")") {
            //assert(v[j]->get_type() == FilterType::GRP_END);
        //}
        //else if (outputs[j] == "|") {
            //assert(v[j]->get_type() == FilterType::OR);
        //}
        //else {
            //assert(v[j]->qdata.q == outputs[j]);
        //}
    //}
//
    //return 0;
//}

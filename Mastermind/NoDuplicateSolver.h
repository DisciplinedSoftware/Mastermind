#pragma once

#include <bitset>
#include <generator>
#include <map>
#include <tuple>
#include <vector>


#include "Code.h"
#include "Feedback.h"


namespace no_duplicate {

static constexpr size_t bitset_size = 32;
using FrequencyMap = std::bitset<bitset_size>;
static inline unsigned int compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs) {
    static_assert(sizeof(unsigned long) >= (bitset_size / 8), "Unsigned long cannot hold bitset size, change to a larger type");
    return std::popcount((lhs & rhs).to_ulong());
}

using History = std::tuple<Code, FrequencyMap>;



static inline unsigned int count_black_pegs(const Code& code, const Code& old_guess, size_t position) {
    unsigned int black = 0;
    for (size_t i = 0; i <= position; ++i) {
        if (code[i] == old_guess[i]) {
            ++black;
        }
    }
    return black;
}


static inline unsigned int count_white_pegs(const FrequencyMap& code_frequency_map,
    const FrequencyMap& old_guess_frequency_map,
    unsigned int black) {
    const unsigned int white = compare_and_count(code_frequency_map, old_guess_frequency_map);
    return white - black;
}


// FeedbackCalculator: encapsulates feedback logic and reuses count vectors
class FeedbackCalculator {
    unsigned int pegs;
    FrequencyMap secret_frequency_map;
public:
    FeedbackCalculator(unsigned int pegs);

    FeedbackCalculator(unsigned int pegs, Code secret);

    void set_secret(const Code& secret);

    Feedback get_feedback(const Code& guess, const Code& secret, const FrequencyMap& guess_frequency_map);
};



class Solver {
    struct NewValue {};

    const unsigned int pegs;
    unsigned int colors;
    std::map<Feedback, History, std::greater<>> history;
    FrequencyMap code_frequency_map;
    Code code;
    FrequencyMap converted_code_frequency_map;
    Code converted_code;
    size_t position;
    const size_t last_position;
    bool all_colors_known_mode;
    std::vector<Color> color_map;
    std::generator<NewValue> code_gen;
    decltype(code_gen.begin()) code_it;

public:
    using FeedbackCalculator = no_duplicate::FeedbackCalculator;

    Solver(unsigned int pegs, unsigned int colors)
        : pegs(pegs)
        , colors(colors)
        , code(pegs, 0)
        , converted_code(pegs, 0)
        , position(0)
        , last_position(pegs - 1)
        , all_colors_known_mode(false)
        , color_map(colors)
        , code_gen(backtrack())
        , code_it(code_gen.begin()) {
    }

    std::tuple<const Code&, const FrequencyMap&> next_guess() {
        if (all_colors_known_mode) {
            converted_code_frequency_map.reset();
            for (size_t i = 0; i < pegs; ++i) {
                const Color converted_color = color_map[code[i]];
                converted_code[i] = converted_color;
                converted_code_frequency_map.flip(converted_color);
            }

            return { converted_code, converted_code_frequency_map };
        }
        else {
            return { code, code_frequency_map };
        }
    }

    void apply_feedback(const Feedback& feedback) {
        history.emplace(feedback, History{ code, code_frequency_map });

        // Check if we should switch to permutation mode
        if (!all_colors_known_mode && feedback.black() + feedback.white() == pegs) {
            all_colors_known_mode = true;
            code_gen = backtrack_using_only_code_colors();
            code_it = code_gen.begin();
            return;
        }

        ++code_it;
    }

    bool can_continue() const {
        return code_it != code_gen.end();
    }

private:
    template<typename Pred>
        requires std::predicate<Pred, unsigned int, unsigned int>
    inline bool compare_feedback(const Code& old_guess,
        const Feedback& old_guess_feedback,
        const FrequencyMap& old_guess_frequency_map,
        Pred pred) {
        // Black pegs
        const unsigned int black = count_black_pegs(code, old_guess, position);
        if (!pred(black, old_guess_feedback.black())) {
            return false;
        }

        // White pegs
        const unsigned int white = count_white_pegs(code_frequency_map, old_guess_frequency_map, black);
        return pred(white, old_guess_feedback.white());
    }

    inline bool is_same_feedback(const Code& old_guess, const Feedback& old_guess_feedback, const FrequencyMap& old_guess_frequency_map) {
        return compare_feedback(old_guess, old_guess_feedback, old_guess_frequency_map, std::equal_to<unsigned int>{});
    }

    inline bool is_similar_feedback(const Code& old_guess, const Feedback& old_guess_feedback, const FrequencyMap& old_guess_frequency_map) {
        return compare_feedback(old_guess, old_guess_feedback, old_guess_frequency_map, std::less_equal<unsigned int>{});
    }


    std::generator<NewValue> backtrack();
    std::generator<NewValue> backtrack_using_only_code_colors();

    void create_color_map();
    void convert_code_and_history();
};

}

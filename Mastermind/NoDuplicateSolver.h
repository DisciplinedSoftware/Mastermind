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

static inline std::uint8_t compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs) {
    static_assert(sizeof(unsigned long) >= (bitset_size / 8), "Unsigned long cannot hold bitset size, change to a larger type");
    return std::popcount((lhs & rhs).to_ulong());
}

using History = std::tuple<Code, FrequencyMap>;



static inline std::uint8_t count_black_pegs(const Code& code, const Code& old_guess, size_t position) {
    std::uint8_t black = 0;
    for (size_t i = 0; i <= position; ++i) {
        if (code[i] == old_guess[i]) {
            ++black;
        }
    }
    return black;
}


static inline std::uint8_t count_white_pegs(const FrequencyMap& code_frequency_map,
    const FrequencyMap& old_guess_frequency_map,
    std::uint8_t black) {
    const std::uint8_t white = compare_and_count(code_frequency_map, old_guess_frequency_map);
    return white - black;
}


// FeedbackCalculator: encapsulates feedback logic and reuses count vectors
class FeedbackCalculator {
    std::uint8_t pegs;
    FrequencyMap secret_frequency_map;
    Code secret;
public:
    FeedbackCalculator(std::uint8_t pegs);

    FeedbackCalculator(std::uint8_t pegs, const Code& secret);

    void set_secret(const Code& secret);

    Feedback get_feedback(const Code& guess, const FrequencyMap& guess_frequency_map);
};



class Solver {
    struct NewValue {};

    const std::uint8_t pegs;
    std::uint8_t colors;
    std::multimap<Feedback, History, std::greater<>> history;
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
    FeedbackCalculator feedback_calculator;

public:
    Solver(std::uint8_t pegs, std::uint8_t colors);

    FeedbackCalculator& get_feedback_calculator();

    std::tuple<const Code&, const FrequencyMap&> next_guess();

    void apply_feedback(const Feedback& feedback);

    bool can_continue() const;

private:
    template<typename Pred>
        requires std::predicate<Pred, std::uint8_t, std::uint8_t>
    inline bool compare_feedback(const Code& old_guess,
        const Feedback& old_guess_feedback,
        const FrequencyMap& old_guess_frequency_map,
        Pred pred) {
        // Black pegs
        const std::uint8_t black = count_black_pegs(code, old_guess, position);
        if (!pred(black, old_guess_feedback.black())) {
            return false;
        }

        // White pegs
        const std::uint8_t white = count_white_pegs(code_frequency_map, old_guess_frequency_map, black);
        return pred(white, old_guess_feedback.white());
    }

    inline bool is_same_feedback(const Code& old_guess, const Feedback& old_guess_feedback, const FrequencyMap& old_guess_frequency_map) {
        return compare_feedback(old_guess, old_guess_feedback, old_guess_frequency_map, std::equal_to<std::uint8_t>{});
    }

    inline bool is_similar_feedback(const Code& old_guess, const Feedback& old_guess_feedback, const FrequencyMap& old_guess_frequency_map) {
        return compare_feedback(old_guess, old_guess_feedback, old_guess_frequency_map, std::less_equal<std::uint8_t>{});
    }


    std::generator<NewValue> backtrack();
    std::generator<NewValue> backtrack_using_only_code_colors();

    void create_color_map();
    void convert_code_and_history();
};

}

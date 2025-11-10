#pragma once

#include <generator>
#include <map>
#include <numeric>
#include <tuple>
#include <unordered_map>
#include <vector>


#include "Code.h"
#include "Feedback.h"


namespace duplicate {

using FrequencyMap = std::vector<unsigned int>;

static inline unsigned int compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs, unsigned int nb_colors) {
    unsigned int count = 0;
    for (unsigned int color = 0; color < nb_colors; ++color) {
        count += std::min(lhs[color], rhs[color]);
    }

    return count;
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
    unsigned int nb_colors,
    unsigned int black) {
    const unsigned int white = compare_and_count(code_frequency_map, old_guess_frequency_map, nb_colors);
    return white - black;
}


// FeedbackCalculator: encapsulates feedback logic and reuses count vectors
class FeedbackCalculator {
    unsigned int pegs;
    unsigned int colors;
    FrequencyMap secret_frequency_map;
public:
    FeedbackCalculator(unsigned int pegs, unsigned int colors);
    FeedbackCalculator(unsigned int pegs, unsigned int colors, Code secret);

    void set_secret(const Code& secret);

    Feedback get_feedback(const Code& guess, const Code& secret, const FrequencyMap& guess_frequency_map);
};



class Solver {
    struct NewValue {};

    const unsigned int pegs;
    unsigned int colors;
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
    Solver(unsigned int pegs, unsigned int colors);

    FeedbackCalculator& get_feedback_calculator();

    std::tuple<const Code&, const FrequencyMap&> next_guess();

    void apply_feedback(const Feedback& feedback);

    bool can_continue() const;

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
        const unsigned int white = count_white_pegs(code_frequency_map, old_guess_frequency_map, colors, black);
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

#pragma once

#include <cassert>
#include <generator>
#include <map>
#include <new>
#include <tuple>
#include <vector>

#include <immintrin.h>


#include "Code.h"
#include "Feedback.h"


namespace duplicate {

class FrequencyMap {
    alignas(std::hardware_destructive_interference_size) std::vector<std::uint8_t> frequencyMap;
    std::uint8_t nb_bins;
public:
    FrequencyMap(std::uint8_t nb_bins)
        : nb_bins(nb_bins)
        , frequencyMap(((nb_bins - 1) / 16 + 1) * 16, 0)
    {}

    inline auto begin() { return frequencyMap.begin(); }
    inline auto begin() const { return frequencyMap.begin(); }
    inline auto end() { return frequencyMap.begin() + nb_bins; }
    inline auto end() const { return frequencyMap.begin() + nb_bins; }

    inline auto& operator[](std::uint8_t index) { return frequencyMap[index]; }
    inline const auto& operator[](std::uint8_t index) const { return frequencyMap[index]; }

    static inline std::uint8_t compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs, std::uint8_t nb_colors) {
        std::uint8_t count = 0;
        for (std::uint8_t i = 0; i < nb_colors; i+=16) {
            __m128i data_lhs = *reinterpret_cast<const __m128i*>(&lhs.frequencyMap[i]);
            __m128i data_rhs = *reinterpret_cast<const __m128i*>(&rhs.frequencyMap[i]);

            __m128i min_vals = _mm_min_epu8(data_lhs, data_rhs);
            __m128i sad = _mm_sad_epu8(min_vals, _mm_setzero_si128());
            __m128i sum = _mm_add_epi64(sad, _mm_srli_si128(sad, 8));
            count += static_cast<uint8_t>(_mm_cvtsi128_si64(sum));
        }

        return count;
    }
};

static inline std::uint8_t compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs, std::uint8_t nb_colors) {
    return FrequencyMap::compare_and_count(lhs, rhs, nb_colors);
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
    std::uint8_t nb_colors,
    std::uint8_t black) {
    const std::uint8_t white = compare_and_count(code_frequency_map, old_guess_frequency_map, nb_colors);
    return white - black;
}


// FeedbackCalculator: encapsulates feedback logic and reuses count vectors
class FeedbackCalculator {
    std::uint8_t pegs;
    std::uint8_t colors;
    FrequencyMap secret_frequency_map;
public:
    FeedbackCalculator(std::uint8_t pegs, std::uint8_t colors);
    FeedbackCalculator(std::uint8_t pegs, std::uint8_t colors, Code secret);

    void set_secret(const Code& secret);

    Feedback get_feedback(const Code& guess, const Code& secret, const FrequencyMap& guess_frequency_map);
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
        const std::uint8_t white = count_white_pegs(code_frequency_map, old_guess_frequency_map, colors, black);
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

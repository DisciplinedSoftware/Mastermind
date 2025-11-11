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
        for (std::uint8_t i = 0; i < nb_colors; i += 16) {
            __m128i data_lhs = _mm_load_si128(reinterpret_cast<const __m128i*>(&lhs.frequencyMap[i]));
            __m128i data_rhs = _mm_load_si128(reinterpret_cast<const __m128i*>(&rhs.frequencyMap[i]));

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

class History {
    alignas(std::hardware_destructive_interference_size) Code code;
    alignas(std::hardware_destructive_interference_size) FrequencyMap frequencyMap;

public:
    History(const Code& code, const FrequencyMap& frequencyMap)
        : code(code)
        , frequencyMap(frequencyMap)
    {}
    inline const Code& get_code() const { return code; }
    inline Code& get_code() { return code; }

    inline const FrequencyMap& get_frequency_map() const { return frequencyMap; }
    inline FrequencyMap& get_frequency_map() { return frequencyMap; }
};

template <size_t I>
inline const auto& get(const History& h) {
    if constexpr (I == 0) {
        return h.get_code();
    }
    else {
        return h.get_frequency_map();
    }
}

template <size_t I>
inline auto& get(History& h) {
    if constexpr (I == 0) {
        return h.get_code();
    }
    else {
        return h.get_frequency_map();
    }
}


} // namespace duplicate

namespace std {
template <> struct tuple_size<duplicate::History> : integral_constant<size_t, 2> {};
template <> struct tuple_element<0, duplicate::History> { using type = ::Code; };
template <> struct tuple_element<1, duplicate::History> { using type = duplicate::FrequencyMap; };
}

namespace duplicate {

static inline std::uint8_t count_black_pegs(const Code& code, const Code& old_guess, size_t position) {
    std::uint8_t count = 0;
    std::uint8_t i = 0;
    for (; i + 16 <= position; i += 16) {
        __m128i c = _mm_load_si128(reinterpret_cast<const __m128i*>(&code[i]));
        __m128i g = _mm_load_si128(reinterpret_cast<const __m128i*>(&old_guess[i]));
        __m128i cmp = _mm_cmpeq_epi8(c, g);
        std::uint32_t mask = _mm_movemask_epi8(cmp);
        count += static_cast<std::uint8_t>(std::popcount(mask));
    }

    if (i < position) {
        // _mm_load_si128()
        __m128i c = _mm_load_si128(reinterpret_cast<const __m128i*>(&code[i]));
        __m128i g = _mm_load_si128(reinterpret_cast<const __m128i*>(&old_guess[i]));
        __m128i cmp = _mm_cmpeq_epi8(c, g);
        std::uint32_t mask = _mm_movemask_epi8(cmp);
        std::uint32_t relevant_mask = mask & ((1U << (position + 1)) - 1);
        count += static_cast<std::uint8_t>(std::popcount(relevant_mask));
    }

    return count;
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
    alignas(std::hardware_destructive_interference_size) Code code;
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

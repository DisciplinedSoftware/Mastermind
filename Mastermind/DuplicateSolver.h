#pragma once

#include <array>
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
    FrequencyMap(std::uint8_t nb_bins);

    inline auto begin() { return frequencyMap.begin(); }
    inline auto begin() const { return frequencyMap.begin(); }
    inline auto end() { return frequencyMap.begin() + nb_bins; }
    inline auto end() const { return frequencyMap.begin() + nb_bins; }

    inline auto& operator[](std::uint8_t index) { return frequencyMap[index]; }
    inline const auto& operator[](std::uint8_t index) const { return frequencyMap[index]; }

    static inline std::uint8_t compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs, std::uint8_t nb_colors) {
        std::uint64_t count = 0;
        for (std::uint8_t i = 0; i < nb_colors; i += 16) {
            const __m128i data_lhs = _mm_load_si128(reinterpret_cast<const __m128i*>(&lhs.frequencyMap[i]));
            const __m128i data_rhs = _mm_load_si128(reinterpret_cast<const __m128i*>(&rhs.frequencyMap[i]));

            const __m128i min_vals = _mm_min_epu8(data_lhs, data_rhs);
            const __m128i sad = _mm_sad_epu8(min_vals, _mm_setzero_si128());
            const __m128i sum = _mm_add_epi64(sad, _mm_srli_si128(sad, 8));
            count += _mm_cvtsi128_si64(sum);
        }

        return static_cast<uint8_t>(count);
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

static const std::array<__mmask16, 16> masks{
    (1U << 1) - 1,
    (1U << 2) - 1,
    (1U << 3) - 1,
    (1U << 4) - 1,
    (1U << 5) - 1,
    (1U << 6) - 1,
    (1U << 7) - 1,
    (1U << 8) - 1,
    (1U << 9) - 1,
    (1U << 10) - 1,
    (1U << 11) - 1,
    (1U << 12) - 1,
    (1U << 13) - 1,
    (1U << 14) - 1,
    (1U << 15) - 1,
    (1U << 16) - 1,
};


static inline __mmask16 compare(const Code& code, std::uint8_t i, const Code& old_guess)
{
    const __m128i c = _mm_load_si128(reinterpret_cast<const __m128i*>(&code[i]));
    const __m128i g = _mm_load_si128(reinterpret_cast<const __m128i*>(&old_guess[i]));
    const __m128i cmp = _mm_cmpeq_epi8(c, g);
    return _mm_movemask_epi8(cmp);
}

static inline std::uint8_t count_black_pegs(const Code& code, const Code& old_guess, size_t position) {
    int count = 0;
    std::uint8_t i = 0;
    for (; i + 16 <= position; i += 16) {
        const __mmask16 res = compare(code, i, old_guess);
        count += std::popcount(res);
    }

    if (i < position) {
        const __mmask16 res = compare(code, i, old_guess);
        const __mmask16 mask = masks[position - i];
        const __mmask16 relevant_res = res & mask;  // Keep only the relevant values
        count += std::popcount(relevant_res);
    }

    return static_cast<std::uint8_t>(count);
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
    const std::uint8_t pegs;
    const std::uint8_t colors;
    FrequencyMap secret_frequency_map;
    Code secret;
public:
    FeedbackCalculator(std::uint8_t pegs, std::uint8_t colors);
    FeedbackCalculator(std::uint8_t pegs, std::uint8_t colors, const Code& secret);

    void set_secret(const Code& secret);

    Feedback get_feedback(const Code& guess, const FrequencyMap& guess_frequency_map);
};



class Solver {
    struct NewValue {};

    const std::uint8_t pegs;
    std::uint8_t colors;
    std::multimap<Feedback, History, std::greater<>> history;
    FrequencyMap code_frequency_map;
    alignas(std::hardware_destructive_interference_size) Code code;
    FrequencyMap converted_code_frequency_map;
    alignas(std::hardware_destructive_interference_size) Code converted_code;
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

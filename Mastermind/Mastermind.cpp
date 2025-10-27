// Mastermind.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <coroutine>
#include <optional>
#include <generator>
#include <memory>
#include <ranges>
#include <limits>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <numeric>
#include <mdspan>
#include <set>
#include <stack>
#include <iomanip>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdlib>
#include <bitset>
#include <map>
#include <immintrin.h>



using Color = std::uint8_t; // Color represented as a single byte
using Colors = std::vector<Color>;
using Code = std::vector<Color>;

static constexpr size_t bitset_size = 32;
using FrequencyMap = std::bitset<bitset_size>;
inline unsigned int compare_and_count(const FrequencyMap& lhs, const FrequencyMap& rhs) {
    static_assert(sizeof(unsigned long) <= bitset_size / 8, "Unsigned long cannot hold bitset size, change to a larger type");
    return std::popcount((lhs & rhs).to_ulong());
}


// Feedback: encapsulates black and white peg counts
class Feedback {
    unsigned int black_;
    unsigned int white_;
public:
    Feedback(unsigned int black, unsigned int white) : black_(black), white_(white) {}
    unsigned int black() const { return black_; }
    unsigned int white() const { return white_; }
    bool operator==(const Feedback& other) const = default;
};


// Print a code using letters
std::ostream& operator<<(std::ostream& stream, const Code& code) {
    for (auto peg : code | std::views::transform([](auto c) -> char { return c + 'A'; })) stream << peg;
    return stream;
}


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
    unsigned int colors;
    FrequencyMap guess_frequency_map;
    FrequencyMap secret_frequency_map;
public:
    FeedbackCalculator(unsigned int pegs, unsigned int colors)
        : pegs(pegs)
        , colors(colors)
        , guess_frequency_map(colors)
        , secret_frequency_map(colors) {
    }

    Feedback get_feedback(const Code& guess, const Code& secret) {
        //guess_frequency_map ^= guess_frequency_map;
        //secret_frequency_map ^= secret_frequency_map;
        guess_frequency_map = FrequencyMap();
        secret_frequency_map = FrequencyMap();

        for (size_t i = 0; i < pegs; ++i) {
            guess_frequency_map[guess[i]] = true;
            secret_frequency_map[secret[i]] = true;
        }

        const unsigned int black = count_black_pegs(guess, secret, pegs - 1);
        const unsigned int white = count_white_pegs(guess_frequency_map, secret_frequency_map, black);
        return { black, white };
    }
};

bool operator>(const Feedback& fb_a, const Feedback& fb_b) {
    if (fb_a.black() != fb_b.black())
        return fb_a.black() > fb_b.black();
    if (fb_a.white() != fb_b.white())
        return fb_a.white() > fb_b.white();
    return false;
}


using History = std::tuple<Code, FrequencyMap>;

// Add above CodeBreakerSolver
struct HistoryFeedbackOrder {
    bool operator()(const Feedback& fb_a, const Feedback& fb_b) const {
        return fb_a > fb_b;
    }
};

template<std::integral Ta, std::integral Tb>
Ta ceil_to_multiple_of(Ta a, Tb b) {
    return static_cast<Ta>(std::ceil(static_cast<long double>(a)/ static_cast<long double>(a)) * b);
}


// --- CodeBreakerSolver class ---
class CodeBreakerSolver {
    struct NewValue {};

    const unsigned int pegs;
    unsigned int colors;
    std::map<Feedback, History, std::greater<>> history;
    FrequencyMap code_frequency_map;
    Code code;
    size_t position;
    const size_t last_position;
    bool all_colors_known_mode;
    std::vector<Color> valid_color_converter;
    std::function<Code(const Code&)> convert_code_color;
    std::vector<Color> color_map;
    std::generator<NewValue> code_gen;
    decltype(code_gen.begin()) code_it;

public:
    CodeBreakerSolver(unsigned int pegs, unsigned int colors)
        : pegs(pegs)
        , colors(colors)
        , code(pegs, 0)
        , position(0)
        , last_position(pegs - 1)
        , all_colors_known_mode(false)
        , valid_color_converter(colors + 1)
        , color_map(colors)
        , convert_code_color([this](const Code& code) { return code; })
        , code_gen(backtrack())
        , code_it(code_gen.begin()) {
    }

    Code next_guess() {
        return convert_code_color(code);
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


    std::generator<NewValue> backtrack() {
        while (true) {
            const Color color = code[position];
            if (static_cast<unsigned int>(color) >= colors) {
                if (position == 0u) {
                    break;
                }

                code_frequency_map[code[--position]] = false;
            }
            else {
                if (!code_frequency_map[color]) {
                    code_frequency_map[color] = true;

                    if (position == last_position) {
                        if (std::ranges::all_of(history, [&](const auto& h) {
                            const auto& [old_guess_feedback, data] = h;
                            const auto& [old_guess, old_guess_frequency_map] = data;
                            return is_same_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);
                            })) {

                            co_yield{};
                        }

                        code_frequency_map[color] = false;
                    }
                    else {
                        // Partial code pruning
                        if (std::ranges::all_of(history, [&](const auto& h) {
                            const auto& [old_guess_feedback, data] = h;
                            const auto& [old_guess, old_guess_frequency_map] = data;
                            return is_similar_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);

                            })) {
                            code[++position] = 0;
                            continue;
                        }
                        else {
                            code_frequency_map[color] = false;
                        }
                    }
                }
            }

            ++code[position];
        }
    }

    std::generator<NewValue> backtrack_using_only_code_colors() {
        create_color_map();
        convert_code_and_history();

        colors = pegs;
        convert_code_color = [this](const Code& code) { return code
            | std::views::transform([this](Color color) { return color_map[color]; })
            | std::ranges::to<Code>(); };

        // Free last color
        code_frequency_map[code[position]] = false;
        return backtrack();
    }

    void create_color_map() {
        std::ranges::copy(code.begin(), code.begin() + pegs, color_map.begin());
        std::ranges::sort(color_map.begin(), color_map.begin() + pegs);

        Color c = 0;
        size_t j = 0;
        for (size_t i = pegs; i < colors; ++i) {
            // Find next missing color
            while (j < pegs && color_map[j] <= c) {
                ++c;
                ++j;
            }

            color_map[i] = c++;
        }
    }

    void convert_code_and_history()
    {
        Colors reverse_color_map(colors, 0);
        for (const auto [i, c] : std::views::enumerate(color_map)) {
            reverse_color_map[c] = static_cast<Color>(i);
        }

        convert_inplace_code_and_frequency_map(code, code_frequency_map, reverse_color_map);
        for (auto& [old_guess_feedback, data] : history) {
            auto& [old_guess, old_guess_frequency_map] = data;
            convert_inplace_code_and_frequency_map(old_guess, old_guess_frequency_map, reverse_color_map);
        }
    }

    inline void convert_inplace_code_and_frequency_map(Code& code, FrequencyMap& code_frequency_map, const Colors& reverse_color_map) {
        // This is faster than setting all colors in code to false since the values are compacted in a bitset
        //code_frequency_map ^= code_frequency_map;
        code_frequency_map = FrequencyMap();

        for (Color& color : code) {
            color = reverse_color_map[color];
            code_frequency_map[color] = true;
        }
    }
};


Colors randomize_colors(unsigned int colors, unsigned int seed) {
    std::mt19937 rng(seed);
    auto color_chars = std::views::iota(0u, colors) | std::ranges::to<std::vector<Color>>();
    std::ranges::shuffle(color_chars, rng);
    return color_chars;
}

Code generate_secret(unsigned int pegs, unsigned int colors, unsigned int seed) {
    auto color_chars = randomize_colors(colors, seed);
    return { color_chars.begin(), color_chars.begin() + pegs };
}


// Timer class for measuring elapsed time
class Timer {
    std::chrono::high_resolution_clock::time_point start_;
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    auto elapsed_seconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    }
};

int main() {
    const auto pegs = 5;
    const auto colors = 8;
    FeedbackCalculator feedback_calculator(pegs, colors);

    constexpr unsigned int nb_tries = 100;
    constexpr unsigned int count = 200;
    std::array<std::array<std::chrono::microseconds, count>, nb_tries> times{};
    for (auto i : std::views::iota(0u, nb_tries)) {
        for (auto j : std::views::iota(0u, count)) {
            const Code secret = generate_secret(pegs, colors, 42 + j);    // Pseudo-random secret

            //unsigned long nb_guess = 0;
            Code guess;
            Timer timer;
            CodeBreakerSolver code_breaker(pegs, colors);
            while (code_breaker.can_continue()) {
                //++nb_guess;
                guess = code_breaker.next_guess();
                Feedback feedback = feedback_calculator.get_feedback(guess, secret);
                if (feedback.black() == pegs) {
                    break;
                }
                code_breaker.apply_feedback(feedback);
            }
            const auto elapsed_time = timer.elapsed_seconds();
            times[i][j] = elapsed_time;
            if ((guess | std::views::take(pegs) | std::ranges::to<Code>()) != secret) {
                std::cout << "Error for secret: " << secret << std::endl;
                return 0;
            }
            //if (i == 0) {
            //    std::cout << nb_guess << '\n';
            //}
        }
    }

    auto total = std::chrono::microseconds::zero();
    auto min = std::chrono::microseconds::max();
    auto max = std::chrono::microseconds::min();
    for (const auto& t : times) {
        const auto current_run_time = std::accumulate(t.begin(), t.end(), std::chrono::microseconds::zero());
        total += current_run_time;
        min = std::min(min, current_run_time);
        max = std::max(max, current_run_time);
        std::cout << std::setprecision(std::numeric_limits<double>::digits10) << current_run_time.count() << '\n';
    }

    std::cout << std::setprecision(std::numeric_limits<double>::digits10)
        << "Total: " << total << ' '
        << "Min: " << min << ' '
        << "Max: " << max << '\n';

    return 0;
}

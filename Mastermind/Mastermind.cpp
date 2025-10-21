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
#include <immintrin.h>
#include <cassert>


using Color = unsigned char; // Color represented as a single character
using Colors = std::vector<Color>;
using Code = std::vector<Color>;

using FrequencyMap = std::vector<bool>;

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
        std::ranges::fill(guess_frequency_map, false);
        std::ranges::fill(secret_frequency_map, false);
        // Black pegs
        unsigned int black = 0;
        for (size_t i = 0; i < pegs; ++i) {
            guess_frequency_map[guess[i]] = true;
            secret_frequency_map[secret[i]] = true;
            if (guess[i] == secret[i]) {
                ++black;
            }
        }
        // White pegs
        unsigned int white = 0;
        for (auto color : guess | std::views::take(pegs)) {
            white += guess_frequency_map[color] && secret_frequency_map[color];
        }
        return { black, white - black };
    }
};


using History = std::tuple<Code, Feedback, FrequencyMap>;

// Add above CodeBreakerSolver
struct HistoryFeedbackOrder {
    bool operator()(const History& a, const History& b) const {
        const auto& fb_a = std::get<1>(a);
        const auto& fb_b = std::get<1>(b);
        if (fb_a.black() != fb_b.black())
            return fb_a.black() > fb_b.black();
        if (fb_a.white() != fb_b.white())
            return fb_a.white() > fb_b.white();
        return false;
    }
};

template<std::integral Ta, std::integral Tb>
Ta ceil(Ta a, Tb b) {
    return static_cast<Ta>(std::ceil(static_cast<long double>(a)/ static_cast<long double>(a)) * b);
}


// --- CodeBreakerSolver class ---
class CodeBreakerSolver {
    struct NewValue {};

    unsigned int pegs;
    unsigned int colors;
    std::set<History, HistoryFeedbackOrder> history;
    FrequencyMap code_frequency_map;
    Code code;
    std::vector<Color> stack;
    size_t position;
    bool all_colors_known_mode;
    std::vector<Color> valid_color_converter;
    std::generator<NewValue> code_gen;
    decltype(code_gen.begin()) code_it;

public:
    CodeBreakerSolver(unsigned int pegs, unsigned int colors)
        : pegs(pegs)
        , colors(colors)
        , code_frequency_map(colors, false)
        , code(ceil(pegs, 32u), 0)
        , stack({ 0 })
        , position(0)
        , all_colors_known_mode(false)
        , valid_color_converter(colors + 1)
        , code_gen(backtrack())
        , code_it(code_gen.begin()) {
    }

    Code next_guess() {
        return code;
    }

    void apply_feedback(const Feedback& feedback) {
        history.emplace(code, feedback, code_frequency_map);

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
    inline unsigned int count_equal_avx2(const Code& old_guess) {
        assert(code.size() == old_guess.size());
        const size_t size = code.size();
        unsigned int sum = 0;
        for (size_t i = 0; i < size; i += 32 / sizeof(Code::value_type)) {
            // Load 32 bytes from each array
            __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(code.data()));
            __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(old_guess.data()));
            // Compare for equality
            __m256i cmp = _mm256_cmpeq_epi8(va, vb);
            // Create a 32-bit mask: each bit is 1 if corresponding bytes are equal
            unsigned int mask = _mm256_movemask_epi8(cmp);
            // Count the number of set bits (equal bytes)
            sum += std::popcount(mask) - static_cast<unsigned int>(size - position);
        }
        return sum;
    }

    inline bool is_same_feedback(const Code& old_guess, const Feedback& old_guess_feedback, const FrequencyMap& old_guess_frequency_map)
    {
        // Black pegs
        const unsigned int black = count_equal_avx2(old_guess);
        if (black != old_guess_feedback.black()) {
            return false;
        }

        // White pegs
        unsigned int white = 0;
        for (size_t i = 0; i < position; ++i) {
            const auto code_color = code[i];
            white += code_frequency_map[code_color] && old_guess_frequency_map[code_color];
        }
        return (white - black) == old_guess_feedback.white();
    }

    inline bool is_similar_feedback(const Code& old_guess, const Feedback& old_guess_feedback, const FrequencyMap& old_guess_frequency_map)
    {
        // Black pegs
        unsigned int black = 0;
        for (size_t i = 0; i <= position; ++i) {
            if (code[i] == old_guess[i]) {
                ++black;
            }
        }
        if (black > old_guess_feedback.black()) {
            return false;
        }

        // White pegs
        unsigned int white = 0;
        for (size_t i = 0; i <= position; ++i) {
            const auto code_color = code[i];
            white += code_frequency_map[code_color] && old_guess_frequency_map[code_color];
        }
        return (white - black) <= old_guess_feedback.white();
    }

    std::generator<NewValue> backtrack() {
        while (!stack.empty()) {
            Color& color = stack.back();
            if (position == pegs) {
                if (std::ranges::all_of(history, [&](const auto& h) {
                    const auto& [old_guess, old_guess_feedback, old_guess_frequency_map] = h;
                    return is_same_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);
                    })) {

                    co_yield {};
                }
                stack.pop_back();
                code_frequency_map[code[--position]] = false;
            }
            else if (static_cast<unsigned int>(color) >= colors) {
                if (position == 0u) {
                    break;
                }

                stack.pop_back();
                code_frequency_map[code[--position]] = false;
            }
            else {
                code[position] = color;

                if (!code_frequency_map[color]) {
                    code_frequency_map[color] = true;

                    // Partial code pruning
                    if (std::ranges::all_of(history, [&](const auto& h) {
                        const auto& [old_guess, old_guess_feedback, old_guess_frequency_map] = h;

                        return is_similar_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);

                        })) {
                        ++position;
                        stack.push_back(0);
                    }
                    else {
                        code_frequency_map[color] = false;
                    }
                }

                ++color;
            }
        }
    }

    std::generator<NewValue> backtrack_using_only_code_colors() {
        stack.pop_back();
        code_frequency_map[code[--position]] = false;

        Code sorted_code(code.begin(), code.begin() + pegs);
        std::ranges::sort(sorted_code);
        sorted_code.emplace_back(colors);

        size_t c = 0;
        for (size_t i = 0; i <= colors; ++i) {
            if (i > sorted_code[c]) {
                ++c;
            }
            valid_color_converter[i] = sorted_code[c];
        }

        for (auto& color : stack) {
            color = valid_color_converter[color];
        }

        while (!stack.empty()) {
            Color& color = stack.back();
            if (position == pegs) {
                if (std::ranges::all_of(history, [&](const auto& h) {
                    const auto& [old_guess, old_guess_feedback, old_guess_frequency_map] = h;
                    return is_same_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);
                    })) {

                    co_yield {};
                }
                stack.pop_back();
                code_frequency_map[code[--position]] = false;
            }
            else if (static_cast<unsigned int>(color) >= colors) {
                if (position == 0u) {
                    break;
                }

                stack.pop_back();
                code_frequency_map[code[--position]] = false;
            }
            else {
                code[position] = color;

                if (!code_frequency_map[color]) {
                    code_frequency_map[color] = true;

                    // Partial code pruning
                    if (std::ranges::all_of(history, [&](const auto& h) {
                        const auto& [old_guess, old_guess_feedback, old_guess_frequency_map] = h;
                        return is_similar_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);
                        })) {
                        ++position;
                        stack.push_back(valid_color_converter[0]);
                    }
                    else {
                        code_frequency_map[color] = false;
                    }
                }

                color = valid_color_converter[color+1];
            }
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

// Print a code using letters
std::ostream& operator<<(std::ostream& stream, const Code& code) {
    for (auto peg : code | std::views::transform([](auto c) -> char { return c + 'A'; })) stream << peg;
    return stream;
}

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

            Code guess;
            Timer timer;
            CodeBreakerSolver code_breaker(pegs, colors);
            while (code_breaker.can_continue()) {
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

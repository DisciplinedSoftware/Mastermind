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
#include <iomanip>


using Color = char; // Color represented as a single character
using Colors = std::vector<Color>;
using Code = std::vector<Color>;

// Generate all possible codes (no duplicate colors, supports any pegs/colors)
std::vector<Code> generate_all_codes(unsigned int pegs, unsigned int colors) {
    const auto color_chars = std::views::iota(0u, colors) | std::ranges::to<Colors>();
    std::vector<Code> codes;
    Code code;
    std::function<void(size_t, std::vector<bool>&)> backtrack = [&](size_t pos, std::vector<bool>& used) {
        if (pos == pegs) {
            codes.push_back(code);
            return;
        }
        for (size_t c = 0; c < colors; ++c) {
            if (!used[c]) {
                code.push_back(color_chars[c]);
                used[c] = true;
                backtrack(pos + 1, used);
                used[c] = false;
                code.pop_back();
            }
        }
        };
    std::vector<bool> used(colors, false);
    backtrack(0, used);
    return codes;
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

// FeedbackCalculator: encapsulates feedback logic and reuses count vectors
class FeedbackCalculator {
    unsigned int pegs;
    unsigned int colors;
    std::vector<bool> guess_frequency_map;
    std::vector<bool> secret_frequency_map;
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
            if (guess[i] == secret[i]) {
                ++black;
            }
            else {
                guess_frequency_map[guess[i]] = true;
                secret_frequency_map[secret[i]] = true;
            }
        }
        // White pegs
        unsigned int white = 0;
        for (unsigned int i = 0; i < colors; ++i) {
            white += guess_frequency_map[i] && secret_frequency_map[i];
        }
        return { black, white };
    }
};


// --- CodeBreakerSolver class ---
class CodeBreakerSolver {
    std::vector<Code> possibilities;
    Code last_guess;
    FeedbackCalculator feedback_calculator;
public:
    CodeBreakerSolver(unsigned int pegs, unsigned int colors)
        : possibilities(generate_all_codes(pegs, colors))
        , feedback_calculator(pegs, colors)
    {
    }

    Code next_guess() {
        last_guess = possibilities[0];
        return last_guess;
    }
    void feedback(const Feedback& fb) {
        possibilities = possibilities
            | std::views::filter([&](const Code& code) { return feedback_calculator.get_feedback(last_guess, code) == fb; })
            | std::ranges::to<std::vector>();
    }
    bool can_continue() const {
        return !possibilities.empty();
    }
};

std::vector<Color> randomize_colors(unsigned int colors, unsigned int seed) {
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
            const auto secret = generate_secret(pegs, colors, 42 + i);    // Pseudo-random secret

            Code guess;
            Timer timer;
            CodeBreakerSolver code_breaker(pegs, colors);
            while (code_breaker.can_continue()) {
                guess = code_breaker.next_guess();
                Feedback fb = feedback_calculator.get_feedback(guess, secret);
                if (fb.black() == pegs) {
                    break;
                }
                code_breaker.feedback(fb);
            }
            const auto elapsed_time = timer.elapsed_seconds();
            times[i][j] = elapsed_time;
            if (guess != secret) {
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
        min = std::min(min, *std::ranges::min_element(t));
        max = std::max(max, *std::ranges::max_element(t));
        std::cout << std::setprecision(std::numeric_limits<double>::digits10) << current_run_time.count() << '\n';
    }

    std::cout << std::setprecision(std::numeric_limits<double>::digits10)
        << "Total: " << total << ' '
        << "Min: " << min << ' '
        << "Max: " << max << '\n';

    return 0;
}

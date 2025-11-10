// Mastermind.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>

#include "Code.h"
#include "Feedback.h"
#include "DuplicateSolver.h"
#include "NoDuplicateSolver.h"





std::vector<Color> randomize_colors(unsigned int colors, unsigned int seed) {
    std::mt19937 rng(seed);
    auto color_chars = std::views::iota(0u, colors) | std::ranges::to<std::vector<Color>>();
    std::ranges::shuffle(color_chars, rng);
    return color_chars;
}

Code generate_secret_no_duplicate(unsigned int pegs, unsigned int colors, unsigned int seed) {
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


class TimeStatistics {
    std::vector<std::chrono::microseconds> mean_times;
    std::chrono::microseconds total;
    std::chrono::microseconds min;
    std::chrono::microseconds max;

public:
    TimeStatistics(std::vector<std::chrono::microseconds>&& mean_times,
        std::chrono::microseconds total,
        std::chrono::microseconds min,
        std::chrono::microseconds max)
    : mean_times(mean_times), total(total), min(min), max(max)
    {}

    const std::vector<std::chrono::microseconds>& get_mean_times() const { return mean_times; }
    std::chrono::microseconds get_total() const { return total; }
    std::chrono::microseconds get_min() const { return min; }
    std::chrono::microseconds get_max() const { return max; }
};

std::ostream& operator<<(std::ostream& stream, const TimeStatistics& statistics) {
    //for (auto time : statistics.get_mean_times()) {
    //    stream << std::setprecision(std::numeric_limits<double>::digits10) << time.count() << '\n';
    //}

    stream << std::setprecision(std::numeric_limits<double>::digits10)
        << "Time: "
        << "Total: " << statistics.get_total() << ' '
        << "Min: " << statistics.get_min() << ' '
        << "Max: " << statistics.get_max();

    return stream;
}


TimeStatistics compute_time_statistics(const std::vector<std::vector<std::chrono::microseconds>>& times)
{
    std::vector<std::chrono::microseconds> mean_times;
    mean_times.reserve(times.size());

    auto total = std::chrono::microseconds::zero();
    auto min = std::chrono::microseconds::max();
    auto max = std::chrono::microseconds::min();

    for (const auto& t : times) {
        const auto current_run_time = std::accumulate(t.begin(), t.end(), std::chrono::microseconds::zero());
        mean_times.emplace_back(current_run_time);
        total += current_run_time;
        min = std::min(min, current_run_time);
        max = std::max(max, current_run_time);
    }

    return { std::move(mean_times), total, min, max };
}


class NbGuessStatistics {
    unsigned int total;
    double mean;
public:
    NbGuessStatistics(unsigned int total, double mean)
        : total(total), mean(mean)
    {}

    unsigned int get_total() const { return total; }
    double get_mean() const { return mean; }
};

std::ostream& operator<<(std::ostream& stream, const NbGuessStatistics& statistics) {
    stream << std::setprecision(std::numeric_limits<double>::digits10)
        << "Nb Guesses: "
        << "Total: " << statistics.get_total() << ' '
        << "Mean: " << statistics.get_mean();

    return stream;
}

NbGuessStatistics compute_nb_guesses_statistics(const std::vector<unsigned int>& nb_guesses) {
    const auto total = std::accumulate(nb_guesses.begin(), nb_guesses.end(), 0u);
    const auto mean = static_cast<double>(total) / nb_guesses.size();
    return { total, mean };
}

template<class Solver> inline std::tuple<Code, unsigned int> solve(std::uint8_t pegs, std::uint8_t colors, const Code& secret)
{
    unsigned int nb_guesses = 0;
    Code final_guess;
    Solver solver(pegs, colors);
    auto feedback_calculator = solver.get_feedback_calculator();
    feedback_calculator.set_secret(secret);
    while (solver.can_continue()) {
        ++nb_guesses;
        const auto& [guess, guess_frequency_map] = solver.next_guess();
        Feedback feedback = feedback_calculator.get_feedback(guess, secret, guess_frequency_map);
        if (feedback.black() == pegs) {
            final_guess = guess;
            break;
        }
        solver.apply_feedback(feedback);
    }

    return { final_guess, nb_guesses };
}

int main() {
    const std::uint8_t pegs = 5;
    const std::uint8_t colors = 8;

    constexpr unsigned int nb_tries = 100;
    constexpr unsigned int count = 200;

    std::vector<std::vector<std::chrono::microseconds>> all_times;
    std::vector<unsigned int> all_nb_guesses;

    all_times.reserve(count);
    all_nb_guesses.reserve(count);

    for (auto i : std::views::iota(0u, nb_tries)) {
        all_times.emplace_back();
        all_times.back().reserve(count);
        for (auto j : std::views::iota(0u, count)) {
            const Code secret = generate_secret_no_duplicate(pegs, colors, 42 + j);    // Pseudo-random secret

            //using Solver = no_duplicate::Solver;
            using Solver = duplicate::Solver;

            Timer timer;
            auto [final_guess, nb_guesses] = solve<Solver>(pegs, colors, secret);
            const auto elapsed_time = timer.elapsed_seconds();

            all_times.back().emplace_back(elapsed_time);
            if ((final_guess | std::views::take(pegs) | std::ranges::to<Code>()) != secret) {
                std::cout << "Error for secret: " << secret << std::endl;
                return 0;
            }
            if (i == 0) {
                all_nb_guesses.emplace_back(nb_guesses);
            }
        }
    }

    const auto time_statistics = compute_time_statistics(all_times);
    const auto guesses_statistics = compute_nb_guesses_statistics(all_nb_guesses);

    std::cout << time_statistics << '\n';
    std::cout << guesses_statistics << '\n';

    return 0;
}

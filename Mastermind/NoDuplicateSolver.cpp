#include "NoDuplicateSolver.h"
#include <algorithm>
#include <ranges>


namespace no_duplicate {

FeedbackCalculator::FeedbackCalculator(std::uint8_t pegs)
    : pegs(pegs)
{}

void FeedbackCalculator::set_secret(const Code& secret) {
    this->secret = secret;

    // Reset secret frequency map and compute it
    secret_frequency_map.reset();

    for (size_t i = 0; i < pegs; ++i) {
        secret_frequency_map.flip(secret[i]);
    }
}

Feedback FeedbackCalculator::get_feedback(const Code& guess, const FrequencyMap& guess_frequency_map) {
    const std::uint8_t black = count_black_pegs(guess, secret, pegs - 1);
    const std::uint8_t white = count_white_pegs(guess_frequency_map, secret_frequency_map, black);
    return { black, white };
}

FeedbackCalculator::FeedbackCalculator(std::uint8_t pegs, const Code& secret) : FeedbackCalculator(pegs) {
    set_secret(secret);
}

Solver::Solver(std::uint8_t pegs, std::uint8_t colors)
    : pegs(pegs)
    , colors(colors)
    , code(pegs, 0)
    , converted_code(pegs, 0)
    , position(0)
    , last_position(pegs - 1)
    , all_colors_known_mode(false)
    , color_map(colors)
    , code_gen(backtrack())
    , code_it(code_gen.begin())
    , feedback_calculator(pegs)
{}

FeedbackCalculator& Solver::get_feedback_calculator() {
    return feedback_calculator;
}

std::tuple<const Code&, const FrequencyMap&> Solver::next_guess() {
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

void Solver::apply_feedback(const Feedback& feedback) {
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

bool Solver::can_continue() const {
    return code_it != code_gen.end();
}

std::generator<Solver::NewValue> Solver::backtrack() {
    while (true) {
        const Color color = code[position];
        if (color >= colors) {
            if (position == 0u) {
                break;
            }

            code_frequency_map.flip(code[--position]);
        }
        else {
            if (!code_frequency_map.test(color)) {
                code_frequency_map.flip(color);

                if (position == last_position) {
                    if (std::ranges::all_of(history, [&](const auto& h) {
                        const auto& [old_guess_feedback, data] = h;
                        const auto& [old_guess, old_guess_frequency_map] = data;
                        return is_same_feedback(old_guess, old_guess_feedback, old_guess_frequency_map);
                        })) {

                        co_yield{};
                    }

                    code_frequency_map.flip(color);
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
                        code_frequency_map.flip(color);
                    }
                }
            }
        }

        ++code[position];
    }
}

std::generator<Solver::NewValue> Solver::backtrack_using_only_code_colors() {
    create_color_map();
    convert_code_and_history();

    colors = pegs;

    // Free last color
    code_frequency_map.flip(code[position]);

    return backtrack();
}

void Solver::create_color_map() {
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

void convert_inplace_code_and_frequency_map(Code& code, FrequencyMap& code_frequency_map, const std::vector<Color>& reverse_color_map) {
    // This is faster than setting all colors in code to false since the values are compacted in a bitset
    code_frequency_map.reset();

    for (Color& color : code) {
        color = reverse_color_map[color];
        code_frequency_map.flip(color);
    }
}

void Solver::convert_code_and_history() {
    std::vector<Color> reverse_color_map(colors, 0);
    for (const auto [i, c] : std::views::enumerate(color_map)) {
        reverse_color_map[c] = static_cast<Color>(i);
    }

    convert_inplace_code_and_frequency_map(code, code_frequency_map, reverse_color_map);
    for (auto& [old_guess_feedback, data] : history) {
        auto& [old_guess, old_guess_frequency_map] = data;
        convert_inplace_code_and_frequency_map(old_guess, old_guess_frequency_map, reverse_color_map);
    }
}

}
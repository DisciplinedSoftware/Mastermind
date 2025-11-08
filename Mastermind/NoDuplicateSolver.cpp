#include "NoDuplicateSolver.h"
#include <algorithm>
#include <ranges>


namespace no_duplicate {

no_duplicate::FeedbackCalculator::FeedbackCalculator(unsigned int pegs) : pegs(pegs)
{

}

void no_duplicate::FeedbackCalculator::set_secret(const Code& secret)
{
    // Reset secret frequency map and compute it
    secret_frequency_map.reset();

    for (size_t i = 0; i < pegs; ++i) {
        secret_frequency_map.flip(secret[i]);
    }
}

Feedback no_duplicate::FeedbackCalculator::get_feedback(const Code& guess, const Code& secret, const FrequencyMap& guess_frequency_map)
{
    const unsigned int black = count_black_pegs(guess, secret, pegs - 1);
    const unsigned int white = count_white_pegs(guess_frequency_map, secret_frequency_map, black);
    return { black, white };
}

no_duplicate::FeedbackCalculator::FeedbackCalculator(unsigned int pegs, Code secret) : FeedbackCalculator(pegs)
{
    set_secret(secret);
}

std::generator<no_duplicate::Solver::NewValue> Solver::backtrack()
{
    while (true) {
        const Color color = code[position];
        if (static_cast<unsigned int>(color) >= colors) {
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

std::generator<no_duplicate::Solver::NewValue> Solver::backtrack_using_only_code_colors()
{
    create_color_map();
    convert_code_and_history();

    colors = pegs;

    // Free last color
    code_frequency_map.flip(code[position]);
    return backtrack();
}

void Solver::create_color_map()
{
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

void convert_inplace_code_and_frequency_map(Code& code, FrequencyMap& code_frequency_map, const std::vector<Color>& reverse_color_map)
{
    // This is faster than setting all colors in code to false since the values are compacted in a bitset
    code_frequency_map.reset();

    for (Color& color : code) {
        color = reverse_color_map[color];
        code_frequency_map.flip(color);
    }
}

void Solver::convert_code_and_history()
{
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
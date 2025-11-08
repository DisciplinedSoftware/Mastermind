#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

using Color = std::uint8_t; // Color represented as a single byte
using Code = std::vector<Color>;


std::ostream& operator<<(std::ostream& stream, const Code& code);
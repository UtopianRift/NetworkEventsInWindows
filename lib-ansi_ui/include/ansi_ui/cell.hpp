// MIT License
// ansi_ui/include/ansi_ui/cell.hpp
#pragma once
#include <cstdint>

#include "color.hpp"

namespace ansi_ui
{

    enum Style : uint8_t
    {
        StyleNone = 0,
        StyleBold = 1 << 0,
        StyleUnderline = 1 << 1,
        StyleInverse = 1 << 2
    };

    struct Cell
    {
        char32_t ch{ U' ' };
        Color fg{ 255,255,255 };
        Color bg{ 0,0,0 };
        uint8_t style{ StyleNone };
    };

} // namespace ansi_ui

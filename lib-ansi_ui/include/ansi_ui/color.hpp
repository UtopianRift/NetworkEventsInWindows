// MIT License
// ansi_ui/include/ansi_ui/color.hpp
#pragma once
#include <cstdint>

namespace ansi_ui
{

    struct Color
    {
        uint8_t r{}, g{}, b{};
        constexpr Color() = default;
        constexpr Color(uint8_t R, uint8_t G, uint8_t B) : r(R), g(G), b(B) {}
        static constexpr Color rgb(uint8_t R, uint8_t G, uint8_t B) { return Color(R, G, B); }
    };

    namespace colors
    {
        // 16 basic ANSI colors as sRGB approximations
        static constexpr Color Black{ 0,0,0 };
        static constexpr Color Red{ 205,49,49 };
        static constexpr Color Green{ 13,188,121 };
        static constexpr Color Yellow{ 229,229,16 };
        static constexpr Color Blue{ 36,114,200 };
        static constexpr Color Magenta{ 188,63,188 };
        static constexpr Color Cyan{ 17,168,205 };
        static constexpr Color White{ 229,229,229 };

        static constexpr Color BrightBlack{ 102,102,102 };
        static constexpr Color BrightRed{ 241,76,76 };
        static constexpr Color BrightGreen{ 35,209,139 };
        static constexpr Color BrightYellow{ 245,245,67 };
        static constexpr Color BrightBlue{ 59,142,234 };
        static constexpr Color BrightMagenta{ 214,112,214 };
        static constexpr Color BrightCyan{ 41,184,219 };
        static constexpr Color BrightWhite{ 255,255,255 };
    } // namespace colors

} // namespace ansi_ui

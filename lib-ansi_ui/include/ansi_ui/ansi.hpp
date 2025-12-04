#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace ansi_ui::ansi
{

    inline constexpr const char* CSI = "\x1b[";
    inline constexpr const char* OSC = "\x1b]";
    inline constexpr const char* ST = "\x1b\\";

    inline std::string sgr_reset() { return std::string(CSI) + "0m"; }
    inline std::string hide_cursor() { return std::string(CSI) + "?25l"; }
    inline std::string show_cursor() { return std::string(CSI) + "?25h"; }

    inline std::string move_abs(int x, int y)
    { // 1-based
        if (x < 1)
            x = 1;
        if (y < 1)
            y = 1;
        return std::string(CSI) + std::to_string(y) + ";" + std::to_string(x) + "H";
    }

    inline std::string set_fg_rgb(uint8_t r, uint8_t g, uint8_t b)
    {
        return std::string(CSI) + "38;2;" + std::to_string((int)r) + ";" + std::to_string((int)g) + ";" + std::to_string((int)b) + "m";
    }
    inline std::string set_bg_rgb(uint8_t r, uint8_t g, uint8_t b)
    {
        return std::string(CSI) + "48;2;" + std::to_string((int)r) + ";" + std::to_string((int)g) + ";" + std::to_string((int)b) + "m";
    }

    inline std::string clear_screen() { return std::string(CSI) + "2J"; }
    inline std::string clear_eol() { return std::string(CSI) + "K"; }

} // namespace ansi_ui::ansi

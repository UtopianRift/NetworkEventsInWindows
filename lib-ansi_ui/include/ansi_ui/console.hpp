#pragma once
#include <string>
#include <string_view>
#include <cstdio>
#include <vector>
#include <utility>
#include <algorithm>

#include "ansi.hpp"
#include "buffer.hpp"
#include "color.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#endif
#include "cell.hpp"

namespace ansi_ui
{

    inline void append_utf8(std::string& out, uint32_t ch) {
        if (ch <= 0x7F) {
            out.push_back(static_cast<char>(ch));
        } else if (ch <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((ch >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else if (ch <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | ((ch >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else if (ch <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | ((ch >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((ch >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            append_utf8(out, 0xFFFD);
        }
    }

    class Console
    {
    public:
        Console() { init(); }
        ~Console() { write(ansi::show_cursor()); }

        // Delete copy constructor and copy assignment operator to prevent accidental copying
        Console(const Console&) = delete;
        Console& operator=(const Console&) = delete;

        // Allow move constructor and move assignment operator
        Console(Console&& other) noexcept
#if defined(_WIN32)
            : hOut_(other.hOut_)
            , shadow_(std::move(other.shadow_))
#else
            : shadow_(std::move(other.shadow_))
#endif
        {
#if defined(_WIN32)
            other.hOut_ = nullptr;
#endif
        }

        Console& operator=(Console&& other) noexcept
        {
            if (this != &other)
            {
#if defined(_WIN32)
                hOut_ = other.hOut_;
                other.hOut_ = nullptr;
#endif
                shadow_ = std::move(other.shadow_);
            }
            return *this;
        }

        std::pair<int, int> size_chars() const
        {
#if defined(_WIN32)
            CONSOLE_SCREEN_BUFFER_INFOEX info{};
            info.cbSize = sizeof(info);
            GetConsoleScreenBufferInfoEx(hOut_, &info);
            int w = info.srWindow.Right - info.srWindow.Left + 1;
            int h = info.srWindow.Bottom - info.srWindow.Top + 1;
            return { w,h };
#else
            int w = 120, h = 40; // fallback
            const char* cols = std::getenv("COLUMNS");
            const char* lines = std::getenv("LINES");
            if (cols) w = std::max(1, std::atoi(cols));
            if (lines) h = std::max(1, std::atoi(lines));
            return { w,h };
#endif
        }

        void write(std::string_view s) const
        {
            std::fwrite(s.data(), 1, s.size(), stdout);
            std::fflush(stdout);
        }

        void move_abs(int x, int y) const { write(ansi::move_abs(x, y)); }

        void put_text(int x, int y, std::string_view s) const
        {
            move_abs(x, y);
            write(s);
        }

        // Present a Buffer at absolute position
        void present(int x, int y, const Buffer& buf)
        {
            for (int row = 0; row < buf.height(); ++row)
            {
                move_abs(x, y + row);
                // stream one row
                std::string line;
                line.reserve(size_t(buf.width()) * 10);
                Color curFg{ 255,255,255 };
                Color curBg{ 0,0,0 };
                bool first = true;
                for (int col = 0; col < buf.width(); ++col)
                {
                    const Cell& c = buf.at(col, row);
                    if (first || c.fg.r != curFg.r || c.fg.g != curFg.g || c.fg.b != curFg.b)
                    {
                        line += ansi::set_fg_rgb(c.fg.r, c.fg.g, c.fg.b);
                        curFg = c.fg;
                    }
                    if (first || c.bg.r != curBg.r || c.bg.g != curBg.g || c.bg.b != curBg.b)
                    {
                        line += ansi::set_bg_rgb(c.bg.r, c.bg.g, c.bg.b);
                        curBg = c.bg;
                    }
                    append_utf8(line, c.ch);
                    first = false;
                }
                line += ansi::sgr_reset();
                write(line);
            }
            // Fix for E0040 and C2589: Add missing parenthesis in std::max arguments
            shadow_.resize(
                std::max(shadow_.width(), x - 1 + buf.width()),
                std::max(shadow_.height(), y - 1 + buf.height())
            );
            // store into shadow buffer (1-based x,y to 0-based indexes)
            for (int row = 0; row < buf.height(); ++row)
            {
                for (int col = 0; col < buf.width(); ++col)
                {
                    ensure_shadow();
                    shadow_.at((x - 1) + col, (y - 1) + row) = buf.at(col, row);
                }
            }
        }

        // Attempt to capture a region from the real console. On Windows we can do it; else use shadow_.
        Buffer capture(int x, int y, int w, int h)
        {
            Buffer out(w, h);
#if defined(_WIN32)
            SMALL_RECT rect;
            rect.Left = SHORT(x - 1);
            rect.Top = SHORT(y - 1);
            rect.Right = SHORT(x - 1 + w - 1);
            rect.Bottom = SHORT(y - 1 + h - 1);
            std::vector<CHAR_INFO> chi(size_t(w) * size_t(h));
            COORD bufSize{ SHORT(w), SHORT(h) };
            COORD bufCoord{ 0,0 };
            if (ReadConsoleOutputW(hOut_, chi.data(), bufSize, bufCoord, &rect))
            {
                for (int yy = 0; yy < h; ++yy)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        auto& dst = out.at(xx, yy);
                        const CHAR_INFO& s = chi[size_t(yy) * size_t(w) + size_t(xx)];
                        dst.ch = s.Char.UnicodeChar ? char32_t(s.Char.UnicodeChar) : U' ';
                        // crude mapping of attributes to RGB; prefer using shadow if available
                        dst.fg = { 255,255,255 };
                        dst.bg = { 0,0,0 };
                    }
                }
                return out;
            }
#endif
            // Fallback to shadow buffer if available
            if (shadow_.width() > 0 && shadow_.height() > 0)
            {
                for (int yy = 0; yy < h; ++yy)
                {
                    for (int xx = 0; xx < w; ++xx)
                    {
                        int sx = ( x - 1 ) + xx;
                        int sy = ( y - 1 ) + yy;
                        if (sx >= 0 && sy >= 0 && sx < shadow_.width() && sy < shadow_.height())
                        {
                            out.at(xx, yy) = shadow_.at(sx, sy);
                        }
                    }
                }
            }
            else
            {
                out.fill(U' ', { 255,255,255 }, { 0,0,0 });
            }
            return out;
        }

        void clear()
        {
            write(ansi::clear_screen());
            move_abs(1, 1);
            shadow_.resize(0, 0);
        }

    private:
        void ensure_shadow()
        {
            if (shadow_.width() == 0 || shadow_.height() == 0)
            {
                auto [w, h] = size_chars();
                shadow_.resize(w, h);
                shadow_.fill(U' ', { 255,255,255 }, { 0,0,0 });
            }
        }
        void init()
        {
            write(ansi::hide_cursor());
#if defined(_WIN32)
            hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            if (GetConsoleMode(hOut_, &mode))
            {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut_, mode);
            }
#endif
        }
#if defined(_WIN32)
        HANDLE hOut_{ GetStdHandle(STD_OUTPUT_HANDLE) };
#endif
        mutable Buffer shadow_;

        Console(const HANDLE& hOut_, const Buffer& shadow_)
            : hOut_(hOut_), shadow_(shadow_)
        {
        }

        bool operator==(const Console& other) const = default;
};

} // namespace ansi_ui

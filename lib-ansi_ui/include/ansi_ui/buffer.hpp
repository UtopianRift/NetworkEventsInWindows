// ansi_ui/include/ansi_ui/buffer.hpp
#pragma once
#include "ansi.hpp"
#include "cell.hpp"
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace ansi_ui
{

    class Buffer
    {
    public:
        Buffer() = default;

        Buffer(int w, int h) { resize(w, h); }

        void resize(int w, int h)
        {
            width_ = w;
            height_ = h;
            cells_.assign(size_t(w) * size_t(h), {});
        }

        int width() const { return width_; }

        int height() const { return height_; }

        Cell& at(int x, int y)
        {
            assert(x >= 0 && x < width_ && y >= 0 && y < height_);
            return cells_[size_t(y) * size_t(width_) + size_t(x)];
        }

        const Cell& at(int x, int y) const
        {
            assert(x >= 0 && x < width_ && y >= 0 && y < height_);
            return cells_[size_t(y) * size_t(width_) + size_t(x)];
        }

        void fill(char32_t ch, Color fg, Color bg, uint8_t style = StyleNone)
        {
            for (auto& c : cells_)
            {
                c.ch = ch;
                c.fg = fg;
                c.bg = bg;
                c.style = style;
            }
        }

        void blit_from(const Buffer& src, int sx, int sy, int sw, int sh, int dx, int dy)
        {
            for (int y = 0; y < sh; ++y)
            {
                for (int x = 0; x < sw; ++x)
                {
                    int tx = dx + x;
                    int ty = dy + y;
                    if (tx < 0 || tx >= width_ || ty < 0 || ty >= height_)
                        continue;
                    at(tx, ty) = src.at(sx + x, sy + y);
                }
            }
        }

        void copy_to(Buffer& dst, int sx, int sy, int sw, int sh, int dx, int dy) const
        {
            for (int y = 0; y < sh; ++y)
            {
                for (int x = 0; x < sw; ++x)
                {
                    int tx = dx + x;
                    int ty = dy + y;
                    if (tx < 0 || tx >= dst.width_ || ty < 0 || ty >= dst.height_)
                        continue;
                    dst.at(tx, ty) = at(sx + x, sy + y);
                }
            }
        }

    private:
        int width_{ 0 }, height_{ 0 };
        std::vector<Cell> cells_;
    };

} // namespace ansi_ui

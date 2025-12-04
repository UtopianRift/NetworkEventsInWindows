// MIT License
// ansi_ui/include/ansi_ui/background_provider.hpp
#pragma once
#include "buffer.hpp"
#include "console.hpp"
#include <memory>

namespace ansi_ui
{

    // Abstract service that can capture existing console content for later restore.
    class BackgroundProvider
    {
    public:
        virtual ~BackgroundProvider() = default;
        // Capture a region at absolute console coordinates (1-based x,y)
        virtual Buffer capture(int x, int y, int w, int h) = 0;
    };

    // An opt-in provider that reads back from the Console (real console if possible, else shadow).
    class ConsoleReadbackBackgroundProvider : public BackgroundProvider
    {
    public:
        explicit ConsoleReadbackBackgroundProvider(Console& con) : con_(con) {}
        Buffer capture(int x, int y, int w, int h) override { return con_.capture(x, y, w, h); }
    private:
        Console& con_;
    };

    class ShadowReadbackBackgroundProvider : public BackgroundProvider
    {
    public:
        explicit ShadowReadbackBackgroundProvider(Buffer& buffer) : buffer_(buffer) {}
        Buffer capture(int x, int y, int w, int h) override
        {
            Buffer out(w, h);
            for (int yy = 0; yy < h; ++yy)
            {
                for (int xx = 0; xx < w; ++xx)
                {
                    int sx = (x - 1) + xx, sy = (y - 1) + yy;
                    if (sx >= 0 && sy >= 0 && sx < buffer_.width() && sy < buffer_.height())
                    {
                        out.at(xx, yy) = buffer_.at(sx, sy);
                    }
                    else
                    {
                        out.at(xx, yy).ch = U' ';
                        out.at(xx, yy).fg = {255,255,255};
                        out.at(xx, yy).bg = {0,0,0};
                    }
                }
            }
            return out;
        }
    private:
        Buffer& buffer_;
    };

} // namespace ansi_ui

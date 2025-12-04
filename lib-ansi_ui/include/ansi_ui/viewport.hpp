// MIT License
// ansi_ui/include/ansi_ui/rect.hpp
#pragma once
#include "buffer.hpp"
#include "color.hpp"
#include "console.hpp"
#include "event.hpp"
#include "background_provider.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ansi_ui
{

    enum class BorderStyle
    {
        None,
        Single,
        Double
    };

    struct RectStyle
    {
        Color fg{ 255, 255, 255 };
        Color bg{ 0, 0, 0 };
        BorderStyle border{ BorderStyle::None };
        Color border_fg{ 255, 255, 255 };
        Color border_bg{ 0, 0, 0 };
    };

    // Layout insets reserved around the client area (SRP: separate from visual style)
    struct BoxInsets
    {
        int thickness{ 0 }; // uniform thickness on all sides
    };

    // Struct containing box-drawing characters
    struct GRAPH
    {
        // Single-line box-drawing characters
        enum class SINGLE : char32_t
        {
            HORIZ = U'\u2500',       // ─
            VERT = U'\u2502',        // │
            UP_LEFT = U'\u250C',     // ┌
            UP_RIGHT = U'\u2510',    // ┐
            DOWN_LEFT = U'\u2514',   // └
            DOWN_RIGHT = U'\u2518'   // ┘
        };

        // Double-line box-drawing characters
        enum class DOUBLE : char32_t
        {
            HORIZ = U'\u2550',       // ═
            VERT = U'\u2551',        // ║
            UP_LEFT = U'\u2554',     // ╔
            UP_RIGHT = U'\u2557',    // ╗
            DOWN_LEFT = U'\u255A',   // ╚
            DOWN_RIGHT = U'\u255D'   // ╝
        };
    };

    // ViewPort is both a container and a drawable region with its own coordinate space.
    class ViewPort : public EventProcessor
    {
    public:
        ViewPort(int x, int y, int w, int h, RectStyle style = {}, BoxInsets insets = {}) :
            x_(x), y_(y), w_(w), h_(h), style_(style), insets_(insets)
        {
            int cw = client_width();
            int ch = client_height();
            canvas_.resize(cw, ch);
            canvas_.fill(U' ', style_.fg, style_.bg);
        }

        // hierarchy
        ViewPort& add_child(std::unique_ptr<ViewPort> child)
        {
            child->parent_ = this;
            children_.push_back(std::move(child));
            return *children_.back();
        }

        // Optional background provider for snapshot/restore integration scenarios
        void set_background_provider(std::shared_ptr<BackgroundProvider> provider) { bg_provider_ = std::move(provider); }
        std::shared_ptr<BackgroundProvider> background_provider() const { return bg_provider_; }

        // Layout insets
        void set_insets(BoxInsets insets) { insets_ = insets; }
        const BoxInsets& insets() const { return insets_; }

        // drawing into client area
        Buffer& canvas() { return canvas_; }
        const Buffer& canvas() const { return canvas_; }

        // draw border + background for insets + client canvas + children
        void present(Console& con)
        {
            draw_border_and_background(con);
            // present client canvas at absolute client origin
            auto [ax, ay] = client_abs_origin();
            con.present(ax, ay, canvas_);
            // present children
            for (auto& c : children_)
                c->present(con);
        }

        // Save what's currently visible under this viewport (border + client) using background provider if set
        void snapshot(Console&/*con*/)
        {
            if (!bg_provider_) return; // opt-in only
            snapshot_border_ = bg_provider_->capture(abs_x(), abs_y(), w_, h_);
            snapshot_client_ = bg_provider_->capture(client_abs_x(), client_abs_y(), client_width(), client_height());
        }

        // Restore previously captured content
        void restore(Console& con)
        {
            if (snapshot_border_.width() && snapshot_border_.height())
                con.present(abs_x(), abs_y(), snapshot_border_);
            if (snapshot_client_.width() && snapshot_client_.height())
                con.present(client_abs_x(), client_abs_y(), snapshot_client_);
        }

        // geometry (x_,y_) are client-relative to parent; absolute are computed
        int x() const { return x_; }
        int y() const { return y_; }
        int w() const { return w_; }
        int h() const { return h_; }
        int client_width() const { return std::max(0, w_ - insets_.thickness * 2); }
        int client_height() const { return std::max(0, h_ - insets_.thickness * 2); }
        int border_thickness() const { return insets_.thickness; }

        // absolute positions in console coordinates (1-based)
        int abs_x() const { return (parent_ ? parent_->client_abs_x() : 0) + x_; }
        int abs_y() const { return (parent_ ? parent_->client_abs_y() : 0) + y_; }

        // absolute position of client origin (1-based console coordinates)
        int client_abs_x() const { return abs_x() + insets_.thickness; }
        int client_abs_y() const { return abs_y() + insets_.thickness; }
        std::pair<int, int> client_abs_origin() const { return { client_abs_x(), client_abs_y() }; }

        RectStyle& style() { return style_; }
        const RectStyle& style() const { return style_; }

    private:
        void draw_border_and_background(Console& con)
        {
            // If neither border nor insets, nothing to draw here.
            if (style_.border == BorderStyle::None && insets_.thickness == 0)
                return;
            Buffer b(w_, h_);
            b.fill(U' ', style_.border_fg, style_.border_bg);
            if (style_.border != BorderStyle::None)
            {
                // Box-drawing characters
                char32_t hch, vch, tl, tr, bl, br;
                if (style_.border == BorderStyle::Single)
                {
                    hch = static_cast<char32_t>(GRAPH::SINGLE::HORIZ);
                    vch = static_cast<char32_t>(GRAPH::SINGLE::VERT);
                    tl = static_cast<char32_t>(GRAPH::SINGLE::UP_LEFT);
                    tr = static_cast<char32_t>(GRAPH::SINGLE::UP_RIGHT);
                    bl = static_cast<char32_t>(GRAPH::SINGLE::DOWN_LEFT);
                    br = static_cast<char32_t>(GRAPH::SINGLE::DOWN_RIGHT);
                }
                else // Double
                {
                    hch = static_cast<char32_t>(GRAPH::DOUBLE::HORIZ);
                    vch = static_cast<char32_t>(GRAPH::DOUBLE::VERT);
                    tl = static_cast<char32_t>(GRAPH::DOUBLE::UP_LEFT);
                    tr = static_cast<char32_t>(GRAPH::DOUBLE::UP_RIGHT);
                    bl = static_cast<char32_t>(GRAPH::DOUBLE::DOWN_LEFT);
                    br = static_cast<char32_t>(GRAPH::DOUBLE::DOWN_RIGHT);
                }
                for (int xx = 0; xx < w_; ++xx)
                {
                    b.at(xx, 0).ch = hch;
                    b.at(xx, h_ - 1).ch = hch;
                }
                for (int yy = 0; yy < h_; ++yy)
                {
                    b.at(0, yy).ch = vch;
                    b.at(w_ - 1, yy).ch = vch;
                }
                b.at(0, 0).ch = tl;
                b.at(w_ - 1, 0).ch = tr;
                b.at(0, h_ - 1).ch = bl;
                b.at(w_ - 1, h_ - 1).ch = br;
            }
            con.present(abs_x(), abs_y(), b);
        }

        int x_, y_, w_, h_;
        RectStyle style_;
        BoxInsets insets_{};
        std::shared_ptr<BackgroundProvider> bg_provider_;
        Buffer canvas_;
        ViewPort* parent_{ nullptr };
        std::vector<std::unique_ptr<ViewPort>> children_;
        Buffer snapshot_border_;
        Buffer snapshot_client_;
        std::shared_ptr<BackgroundProvider> bg_provider{};
    };

    // RootViewPort is an implied viewport covering the entire console.
    class RootViewPort : public ViewPort
    {
    public:
        explicit RootViewPort(Console& con, RectStyle style = {}) :
            ViewPort(1, 1, con.size_chars().first, con.size_chars().second, style)
        {
        }
    };

} // namespace ansi_ui

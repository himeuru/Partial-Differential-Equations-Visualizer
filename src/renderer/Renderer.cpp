#include "Renderer.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>

// sf::Text with std::string treats bytes as Latin-1, so UTF-8 has to be
// converted explicitly or Greek/Cyrillic turns into garbage glyphs.
static sf::String utf8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}
static sf::String utf8(const char* s) {
    return sf::String::fromUtf8(s, s + std::strlen(s));
}

// Tick-label font, loaded once on first use
static sf::Font  s_tickFont;
static bool      s_tickFontLoaded = false;

static bool ensureTickFont() {
    if (s_tickFontLoaded) return true;
    static const char* candidates[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        nullptr
    };
    for (int i = 0; candidates[i]; ++i) {
        if (s_tickFont.loadFromFile(candidates[i])) {
            s_tickFontLoaded = true;
            return true;
        }
    }
    return false;
}

// PlotArea

void PlotArea::setup(unsigned w, unsigned h,
                     float xMin, float xMax,
                     float yMin, float yMax)
{
    if (w_ != w || h_ != h) {
        w_ = w; h_ = h;
        rt_.create(w, h);
    }
    xMin_ = xMin; xMax_ = xMax;
    yMin_ = yMin; yMax_ = yMax;
}

void PlotArea::setRange(float xMin, float xMax, float yMin, float yMax) {
    xMin_ = xMin; xMax_ = xMax;
    yMin_ = yMin; yMax_ = yMax;
}

sf::Vector2f PlotArea::toPixel(float x, float y) const {
    float px = padL + (x - xMin_) / (xMax_ - xMin_) * plotW();
    float py = padT + (1.f - (y - yMin_) / (yMax_ - yMin_)) * plotH();
    return {px, py};
}

void PlotArea::beginFrame(sf::Color bg) {
    rt_.clear(bg);
}

void PlotArea::drawAxes() {
    const sf::Color gridCol  (255, 255, 255, 16);
    const sf::Color axisCol  (255, 255, 255, 55);
    const sf::Color zeroCol  (255, 255, 255, 90);
    const sf::Color borderCol(255, 255, 255, 35);

    // Border rectangle
    sf::Vertex border[5];
    sf::Vector2f tl = {padL, padT};
    sf::Vector2f tr = {padL + plotW(), padT};
    sf::Vector2f br = {padL + plotW(), padT + plotH()};
    sf::Vector2f bl = {padL, padT + plotH()};
    border[0] = border[4] = sf::Vertex(tl, borderCol);
    border[1] = sf::Vertex(tr, borderCol);
    border[2] = sf::Vertex(br, borderCol);
    border[3] = sf::Vertex(bl, borderCol);
    rt_.draw(border, 5, sf::LineStrip);

    // Horizontal grid lines (5 divisions)
    int ny = 5;
    for (int i = 0; i <= ny; ++i) {
        float v = yMin_ + (yMax_ - yMin_) * i / float(ny);
        sf::Color col = (i == 0 || i == ny) ? axisCol : gridCol;
        sf::Vertex ln[] = {
            sf::Vertex(toPixel(xMin_, v), col),
            sf::Vertex(toPixel(xMax_, v), col)
        };
        rt_.draw(ln, 2, sf::Lines);
    }

    // Vertical grid lines (4 divisions)
    int nx = 4;
    for (int i = 0; i <= nx; ++i) {
        float v = xMin_ + (xMax_ - xMin_) * i / float(nx);
        sf::Vertex ln[] = {
            sf::Vertex(toPixel(v, yMin_), gridCol),
            sf::Vertex(toPixel(v, yMax_), gridCol)
        };
        rt_.draw(ln, 2, sf::Lines);
    }

    // Highlighted zero lines
    if (yMin_ < 0.f && yMax_ > 0.f) {
        sf::Vertex zl[] = {
            sf::Vertex(toPixel(xMin_, 0.f), zeroCol),
            sf::Vertex(toPixel(xMax_, 0.f), zeroCol)
        };
        rt_.draw(zl, 2, sf::Lines);
    }
    if (xMin_ < 0.f && xMax_ > 0.f) {
        sf::Vertex zl[] = {
            sf::Vertex(toPixel(0.f, yMin_), zeroCol),
            sf::Vertex(toPixel(0.f, yMax_), zeroCol)
        };
        rt_.draw(zl, 2, sf::Lines);
    }
    // Explicit marker at x=xMin so the left endpoint of the rod stays visible
    {
        const sf::Color leftCol(255, 255, 255, 110);
        sf::Vertex zl[] = {
            sf::Vertex(toPixel(xMin_, yMin_), leftCol),
            sf::Vertex(toPixel(xMin_, yMax_), leftCol)
        };
        rt_.draw(zl, 2, sf::Lines);
    }

    // Tick labels (only if we have a font)
    if (!ensureTickFont()) return;

    const sf::Color tickCol(190, 195, 210, 170);
    const unsigned  tickSz = 9u;

    char buf[32];

    // Y-axis: 5 labels on the left
    for (int i = 0; i <= ny; ++i) {
        float v = yMin_ + (yMax_ - yMin_) * i / float(ny);
        // Use compact format: avoid too many decimals
        float absv = std::fabs(v);
        if      (absv < 1e-9f)                  std::snprintf(buf, sizeof(buf), "0");
        else if (absv >= 100.f || absv < 0.001f) std::snprintf(buf, sizeof(buf), "%.1e", v);
        else if (absv >= 10.f)                  std::snprintf(buf, sizeof(buf), "%.1f", v);
        else if (absv >= 1.f)                   std::snprintf(buf, sizeof(buf), "%.2f", v);
        else                                     std::snprintf(buf, sizeof(buf), "%.3f", v);

        sf::Text txt(buf, s_tickFont, tickSz);
        txt.setFillColor(tickCol);
        sf::FloatRect bounds = txt.getLocalBounds();
        sf::Vector2f  pos    = toPixel(xMin_, v);
        // right-align against the left padding edge
        txt.setPosition(padL - bounds.width - bounds.left - 4.f,
                        pos.y - bounds.height * 0.5f - bounds.top);
        rt_.draw(txt);
    }

    // X-axis: nx+1 labels on the bottom
    for (int i = 0; i <= nx; ++i) {
        float v = xMin_ + (xMax_ - xMin_) * i / float(nx);
        float absv = std::fabs(v);
        if      (absv < 1e-9f)                  std::snprintf(buf, sizeof(buf), "0");
        else if (absv >= 100.f || absv < 0.001f) std::snprintf(buf, sizeof(buf), "%.1e", v);
        else if (absv >= 10.f)                  std::snprintf(buf, sizeof(buf), "%.1f", v);
        else if (absv >= 1.f)                   std::snprintf(buf, sizeof(buf), "%.2f", v);
        else                                     std::snprintf(buf, sizeof(buf), "%.3f", v);

        sf::Text txt(buf, s_tickFont, tickSz);
        txt.setFillColor(tickCol);
        sf::FloatRect bounds = txt.getLocalBounds();
        sf::Vector2f  pos    = toPixel(v, yMin_);
        txt.setPosition(pos.x - bounds.width * 0.5f - bounds.left,
                        pos.y + 3.f);
        rt_.draw(txt);
    }

    // Axis labels: X centered below ticks, Y rotated -90 in the left gutter
    const sf::Color axLabCol(225, 230, 245, 220);
    const unsigned  axLabSz = 13u;
    if (!xLabel.empty()) {
        sf::Text t(utf8(xLabel), s_tickFont, axLabSz);
        t.setStyle(sf::Text::Italic);
        t.setFillColor(axLabCol);
        sf::FloatRect b = t.getLocalBounds();
        t.setPosition(padL + plotW() * 0.5f - b.width * 0.5f - b.left,
                      padT + plotH() + 15.f);
        rt_.draw(t);
    }
    if (!yLabel.empty()) {
        sf::Text t(utf8(yLabel), s_tickFont, axLabSz);
        t.setStyle(sf::Text::Italic);
        t.setFillColor(axLabCol);
        sf::FloatRect b = t.getLocalBounds();
        t.setRotation(-90.f);
        float cx = 12.f;
        float cy = padT + plotH() * 0.5f + b.width * 0.5f;
        t.setPosition(cx, cy);
        rt_.draw(t);
    }
}

void PlotArea::drawCurve(const std::vector<float>& y,
                          float xMin, float xMax,
                          PlotStyle style)
{
    int n = (int)y.size();
    if (n < 2) return;
    float dx = (xMax - xMin) / (n - 1);

    // Pin values to the visible box so locked axes don't leak curves
    auto clampY = [&](float v) { return std::clamp(v, yMin_, yMax_); };

    if (style.filled) {
        std::vector<sf::Vertex> verts;
        verts.reserve(n * 2 + 2);
        float yBase = std::clamp(0.f, yMin_, yMax_);
        for (int i = 0; i < n; ++i) {
            float x = xMin + i * dx;
            verts.push_back({toPixel(x, clampY(y[i])), style.fillColor});
            verts.push_back({toPixel(x, yBase),        style.fillColor});
        }
        rt_.draw(verts.data(), verts.size(), sf::TriangleStrip);
    }

    // Line
    std::vector<sf::Vertex> line;
    line.reserve(n);
    for (int i = 0; i < n; ++i)
        line.push_back({toPixel(xMin + i * dx, clampY(y[i])), style.lineColor});
    rt_.draw(line.data(), line.size(), sf::LineStrip);
}

void PlotArea::drawVLine(float x, sf::Color col) {
    sf::Vertex v[] = {
        sf::Vertex(toPixel(x, yMin_), col),
        sf::Vertex(toPixel(x, yMax_), col)
    };
    rt_.draw(v, 2, sf::Lines);
}

void PlotArea::endFrame() {
    rt_.display();
}

// Heatmap

void Heatmap::setup(unsigned w, unsigned h) {
    w_ = w; h_ = h;
    img_.create(w, h, sf::Color::Black);
    tex_.loadFromImage(img_);
}

void Heatmap::update(const std::vector<float>& data, int cols, int rows,
                     float vmin, float vmax)
{
    img_.create(w_, h_, sf::Color::Black);
    if (cols < 1 || rows < 1 || data.empty()) return;
    for (unsigned py = 0; py < h_; ++py) {
        for (unsigned px = 0; px < w_; ++px) {
            int ci = std::clamp(int(px * (cols-1) / float(w_-1)), 0, cols-1);
            int ri = std::clamp(int((h_-1-py) * (rows-1) / float(h_-1)), 0, rows-1);
            float v = data[ri * cols + ci];
            float t = (vmax > vmin) ? (v - vmin) / (vmax - vmin) : 0.5f;
            img_.setPixel(px, py, colormap(std::clamp(t, 0.f, 1.f)));
        }
    }
    tex_.loadFromImage(img_);
}

sf::Color Heatmap::colormap(float t) {
    struct Stop { float p; uint8_t r,g,b; };
    static const Stop s[] = {
        {0.00f,   8,   8,  45},
        {0.20f,  40,  60, 180},
        {0.45f,  20, 170, 190},
        {0.70f, 140, 210,  50},
        {1.00f, 255, 245, 180},
    };
    for (int i = 0; i < 4; ++i) {
        if (t <= s[i+1].p) {
            float k = (t - s[i].p) / (s[i+1].p - s[i].p);
            auto L = [&](uint8_t a, uint8_t b){ return uint8_t(a + int((b-a)*k)); };
            return {L(s[i].r,s[i+1].r), L(s[i].g,s[i+1].g), L(s[i].b,s[i+1].b)};
        }
    }
    return {s[4].r, s[4].g, s[4].b};
}
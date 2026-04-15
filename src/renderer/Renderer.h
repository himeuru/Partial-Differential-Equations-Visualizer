#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

struct PlotStyle {
    sf::Color lineColor = sf::Color(100, 200, 255);
    float     lineWidth = 1.5f;
    bool      filled    = false;
    sf::Color fillColor = sf::Color(100, 200, 255, 35);
};

// 1D curve plot rendered into an off-screen RenderTexture, with axes,
// tick labels and optional axis labels.
class PlotArea {
public:
    // Padding: left/bottom wider to fit tick + axis labels
    float padL = 70.f, padR = 14.f, padT = 16.f, padB = 36.f;

    std::string xLabel;
    std::string yLabel;

    void setup(unsigned w, unsigned h,
               float xMin, float xMax,
               float yMin, float yMax);

    void setRange(float xMin, float xMax, float yMin, float yMax);

    void beginFrame(sf::Color bg = sf::Color(12, 14, 24));
    void drawAxes();
    void drawCurve(const std::vector<float>& y,
                   float xMin, float xMax,
                   PlotStyle style = {});
    void drawVLine(float x, sf::Color col = sf::Color(255, 200, 80, 200));
    void endFrame();

    const sf::Texture& texture() const { return rt_.getTexture(); }
    unsigned width()  const { return w_; }
    unsigned height() const { return h_; }

private:
    sf::RenderTexture rt_;
    unsigned w_ = 0, h_ = 0;
    float xMin_, xMax_, yMin_, yMax_;

    sf::Vector2f toPixel(float x, float y) const;   // data -> pixel

    float plotW() const { return float(w_) - padL - padR; }
    float plotH() const { return float(h_) - padT - padB; }
};

// 2D scalar field rendered as a colour-mapped image.
class Heatmap {
public:
    void setup(unsigned w, unsigned h);
    void update(const std::vector<float>& data, int cols, int rows,
                float vmin, float vmax);
    const sf::Texture& texture() const { return tex_; }
private:
    sf::Texture tex_;
    sf::Image   img_;
    unsigned    w_ = 0, h_ = 0;
    static sf::Color colormap(float t);
};
#pragma once
#include "IC.h"
#include "ui/PlotState.h"
#include "renderer/Renderer.h"
#include <imgui.h>
#include <SFML/Graphics.hpp>

namespace ui {

// Decorative
sf::Color  harmCol(int k);
void       drawTex(sf::RenderWindow& win, const sf::Texture& tex, float x, float y);

// Pretty-prints a parsed expression underneath disabled label text
void       prettyFormula(const char* src, ImVec4 color = {0.75f, 0.88f, 1.f, 1.f});

// Custom-formula input with template palette and live MathRender preview.
// Returns true on the frame the source string changed.
bool       customExprInput(const char* label, ICCustomExpr& e,
                           const char* vars = "доступны: x, L, xi, pi, e");
bool       boundExprInput(const char* label, ICCustomExpr& e);

// Slider with inline +/- buttons, labelled to the right of the controls
bool       sliderF(const char* label, float* v, float lo, float hi,
                   float step, const char* fmt = "%.3f");
bool       sliderI(const char* label, int*   v, int lo, int hi, int step = 1);

// Checkbox that re-arms AxisLock::needCapture on a 0->1 transition
void       axisLockCheckbox(const char* label, AxisLock& lk);

// Floating overlay with min/max/mean/rms/L1/L2 stats over a curve
void       drawStatsHUD(float px, float py, float pw,
                        const char* title, const Stats& s);

// Append a hit-test record for the just-drawn plot
void       registerPlot(std::vector<PlotHit>& dst,
                        float sx, float sy, const PlotArea& pa, View* v,
                        float xlo, float xhi, float ylo, float yhi);

} // namespace ui

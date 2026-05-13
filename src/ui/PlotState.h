#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

// Per-plot pan/zoom in normalized units of the current axis span
struct View {
    float panX = 0.f, panY = 0.f;
    float zoom = 1.f;

    void reset() { panX = panY = 0.f; zoom = 1.f; }

    void apply(float& xlo, float& xhi, float& ylo, float& yhi) const {
        float xc0 = 0.5f * (xlo + xhi), xs0 = xhi - xlo;
        float yc0 = 0.5f * (ylo + yhi), ys0 = yhi - ylo;
        float xspan = xs0 / zoom;
        float yspan = ys0 / zoom;
        float xc = xc0 + panX * xs0;
        float yc = yc0 + panY * ys0;
        xlo = xc - xspan * 0.5f; xhi = xc + xspan * 0.5f;
        ylo = yc - yspan * 0.5f; yhi = yc + yspan * 0.5f;
    }
};

// Sticky Y-axis: capture the first frame after enabling, then reuse those
// limits so e.g. heat decay isn't auto-rescaled into a flat line.
struct AxisLock {
    bool  yLocked     = true;
    bool  needCapture = true;
    float yLo = 0.f, yHi = 0.f;

    void apply(float& ylo, float& yhi) {
        if (!yLocked) return;
        if (needCapture) { yLo = ylo; yHi = yhi; needCapture = false; }
        else             { ylo = yLo; yhi = yHi; }
    }
};

// One sampled curve attached to a plot for hover readout. Owns its samples
// because the source vector is a local in the per-tab render function and
// goes out of scope before the hover pass runs.
struct PlotCurve {
    std::vector<float> y;
    float       x0, x1;
    sf::Color   color;
    std::string label;
};

// Hit-test record for mouse dispatch over an active plot rect
struct PlotHit {
    sf::FloatRect rect;
    View*         view;
    float         xBase0, xBase1, yBase0, yBase1;
    std::vector<PlotCurve> curves;
};

// Append a curve to the last-registered plot (no-op if none).
inline void registerCurve(std::vector<PlotHit>& plots,
                          const std::vector<float>& y, float x0, float x1,
                          sf::Color color, const char* label)
{
    if (plots.empty()) return;
    plots.back().curves.push_back({y, x0, x1, color, label});
}

// Linear interpolation of a sampled curve at data-x.
inline bool sampleCurveAt(const PlotCurve& c, float dataX, float& out) {
    if (c.y.size() < 2)               return false;
    if (dataX < c.x0 || dataX > c.x1) return false;
    int   n   = int(c.y.size());
    float fx  = (dataX - c.x0) / (c.x1 - c.x0);
    float idx = fx * float(n - 1);
    int   i0  = std::clamp(int(idx), 0, n - 2);
    float t   = idx - float(i0);
    out = (1.f - t) * c.y[i0] + t * c.y[i0 + 1];
    return std::isfinite(out);
}

struct Stats { float mn, mx, mean, rms, l1, l2; };

inline std::pair<float, float> autoRangeSymm(const std::vector<float>& v, float margin = 0.2f) {
    float mx = 0.01f;
    for (float x : v) mx = std::max(mx, std::fabs(x));
    mx *= (1.f + margin);
    return { -mx, mx };
}

inline std::pair<float, float> autoRangePos(const std::vector<float>& v, float margin = 0.15f) {
    float lo = 1e9f, hi = -1e9f;
    for (float x : v) { lo = std::min(lo, x); hi = std::max(hi, x); }
    float span = std::max(hi - lo, 0.01f);
    return { lo - span * margin, hi + span * margin };
}

inline Stats calcStats(const std::vector<float>& v, float dx) {
    Stats s{};
    if (v.empty()) return s;
    s.mn = v[0]; s.mx = v[0];
    double sum = 0, sumAbs = 0, sumSq = 0;
    for (float x : v) {
        s.mn = std::min(s.mn, x);
        s.mx = std::max(s.mx, x);
        sum    += x;
        sumAbs += std::fabs(x);
        sumSq  += double(x) * x;
    }
    s.mean = float(sum / v.size());
    s.rms  = float(std::sqrt(sumSq / v.size()));
    s.l1   = float(sumAbs * dx);
    s.l2   = float(std::sqrt(sumSq * dx));
    return s;
}

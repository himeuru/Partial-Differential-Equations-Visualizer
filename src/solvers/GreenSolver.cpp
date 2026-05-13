#include "GreenSolver.h"
#include <cmath>
#include <algorithm>
#include <limits>

void GreenSolver::compute() {
    switch (mode) {
        case GreenMode::Heat1D:         computeHeat1D();         break;
        case GreenMode::Wave1D:         computeWave1D();         break;
        case GreenMode::SturmLiouville: computeSturmLiouville(); break;
        case GreenMode::Laplace2D:      computeLaplace2D();      break;
        case GreenMode::Custom1D:       computeCustom1D();       break;
    }
    if (!g1d.empty()) {
        gMin = *std::min_element(g1d.begin(), g1d.end());
        gMax = *std::max_element(g1d.begin(), g1d.end());
    }
    if (!g2d.empty()) {
        gMin = *std::min_element(g2d.begin(), g2d.end());
        gMax = *std::max_element(g2d.begin(), g2d.end());
    }
    if (gMax == gMin) gMax = gMin + 1.f;
}

// G(x,xi,t) = 1/sqrt(4 pi a^2 t) * exp(-(x-xi)^2 / (4 a^2 t))
void GreenSolver::computeHeat1D() {
    g2d.clear(); g2dW = g2dH = 0;

    float xMin = 0.f;
    float xMax = L;

    g1d.resize(N + 1);
    float tSafe = std::max(t, 1e-4f);
    float sigma = std::sqrt(2.f * a * a * tSafe);
    float coeff = 1.f / (sigma * float(std::sqrt(2.0 * kPi)));

    float dx = (xMax - xMin) / N;
    for (int i = 0; i <= N; ++i) {
        float x = xMin + i * dx;
        float d = x - xi;
        g1d[i]  = coeff * std::exp(-d * d / (2.f * sigma * sigma));
    }
}

// G(x,xi,t) = 1/(2a) inside the light cone |x-xi| <= a t, zero outside.
void GreenSolver::computeWave1D() {
    g2d.clear(); g2dW = g2dH = 0;
    g1d.resize(N + 1);

    float xMin = xi - 2.f * L;
    float xMax = xi + 2.f * L;
    float dx   = (xMax - xMin) / N;
    float cone = a * t;
    float val  = 1.f / (2.f * a);

    for (int i = 0; i <= N; ++i) {
        float x = xMin + i * dx;
        g1d[i] = (std::fabs(x - xi) <= cone) ? val : 0.f;
    }
}

// -u'' = f on [0,L], u(0)=u(L)=0.
// G(x,xi) = sum_n (2/L) sin(n pi x / L) sin(n pi xi / L) / (n pi / L)^2
void GreenSolver::computeSturmLiouville() {
    g2d.clear(); g2dW = g2dH = 0;
    g1d.resize(N + 1);

    double dx  = L / N;
    double xi_ = xi;

    for (int i = 0; i <= N; ++i) {
        double x   = i * dx;
        double sum = 0.0;
        for (int n = 1; n <= nTerms; ++n) {
            double lambda = n * kPi / L;
            sum += (2.0 / L)
                   * std::sin(lambda * x)
                   * std::sin(lambda * xi_)
                   / (lambda * lambda);
        }
        g1d[i] = float(sum);
    }
}

// User-defined piecewise G(x; xi, t, a, L)
void GreenSolver::computeCustom1D() {
    g2d.clear(); g2dW = g2dH = 0;
    g1d.resize(N + 1);
    float xMin = 0.f, xMax = L;
    float dx   = (xMax - xMin) / N;

    parser::EvalContext ctx;
    ctx.vars["xi"] = double(xi);
    ctx.vars["t"]  = double(t);
    ctx.vars["a"]  = double(a);
    ctx.vars["L"]  = double(L);

    struct Resolved { double lo, hi; bool usable; };
    std::vector<Resolved> bounds(customPieces.size());
    for (size_t pi = 0; pi < customPieces.size(); ++pi) {
        const auto& p = customPieces[pi];
        double lo = p.xLoExpr.ok ? p.xLoExpr.expr.eval(ctx) : 0.0;
        double hi = p.xHiExpr.ok ? p.xHiExpr.expr.eval(ctx) : double(L);
        bool usable = p.expr.ok && std::isfinite(lo) && std::isfinite(hi);
        bounds[pi] = { lo, hi, usable };
    }

    customNanCount = 0;
    customTotalCount = 0;
    customMissCount  = 0;
    for (int i = 0; i <= N; ++i) {
        double x = xMin + i * dx;
        ctx.vars["x"] = x;
        float v = 0.f;
        bool matched = false;
        for (size_t pi = 0; pi < customPieces.size(); ++pi) {
            const auto& b = bounds[pi];
            if (!b.usable)          continue;
            if (x < b.lo || x > b.hi) continue;
            double r = customPieces[pi].expr.expr.eval(ctx);
            // Reject near-resonance overflow. For the default Helmholtz Green
            // on [0, pi], sin(a*pi) hits zero at integer a; just slightly off
            // gives values O(1e6) that visually swamp the real shape. 1e3 is
            // above any physically reasonable Green amplitude on the domains
            // we care about.
            if (std::isfinite(r) && std::fabs(r) < 1.0e3) {
                v = float(r);
            } else {
                v = 0.f;
                ++customNanCount;
            }
            ++customTotalCount;
            matched = true;
            break;
        }
        if (!matched) ++customMissCount;
        g1d[i] = v;
    }
}

// G(r) = (1/(2 pi)) log(1/r), r = |(x,y) - (xi,eta)|. Singular at source.
void GreenSolver::computeLaplace2D() {
    g1d.clear();

    int W = N + 1, H = N + 1;
    g2d.resize(W * H);
    g2dW = W; g2dH = H;

    float dx = L / N, dy = L / N;
    float eps = dx * 0.5f;   

    for (int j = 0; j <= N; ++j) {
        float y = j * dy;
        for (int i = 0; i <= N; ++i) {
            float x = i * dx;
            float r = std::sqrt((x - xi)*(x - xi) + (y - eta)*(y - eta));
            r = std::max(r, eps);
            g2d[j * W + i] = float(1.0 / (2.0 * kPi) * std::log(1.0 / r));
        }
    }
}

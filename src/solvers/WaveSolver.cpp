#include "WaveSolver.h"
#include <cmath>

static const double PI = std::acos(-1.0);

void WaveSolver::init() {
    time = 0.0f;
    fdFirstStep = true;
    computeFourierCoeffs();
    initFDGrid();
}

void WaveSolver::stepFD(float dt) {
    if (dt <= 0.f) return;
    double dx = double(L) / double(N);
    double aa = std::max(1e-6, double(a));

    // Courant 0.7 — some slack above CFL for slider jitter
    const double rTarget = 0.7;
    double dtMaxPerStep  = rTarget * dx / aa;

    // cap per-call advance so the first-frame dt spike doesn't explode nsub
    double dtTotal = std::min(double(dt), 200.0 * dtMaxPerStep);

    // 3-level stencil needs uniform sub-steps
    int nsub = (int)std::ceil(dtTotal / dtMaxPerStep);
    if (nsub < 1) nsub = 1;
    double dtStep = dtTotal / nsub;
    double r      = aa * dtStep / dx;
    double r2     = r * r;

    int n = (int)u1.size();
    if (n < 3) return;
    u2.resize(n);

    for (int k = 0; k < nsub; ++k) {
        if (fdFirstStep) {
            // zero initial velocity: u1 = u0 + (r^2/2) * Laplacian(u0)
            for (int i = 1; i < n - 1; ++i)
                u2[i] = u1[i] + 0.5 * r2 * (u1[i+1] - 2.0*u1[i] + u1[i-1]);
            fdFirstStep = false;
        } else {
            for (int i = 1; i < n - 1; ++i)
                u2[i] = 2.0*(1.0 - r2)*u1[i] + r2*(u1[i+1] + u1[i-1]) - u0[i];
        }
        u2[0] = 0.0; u2[n-1] = 0.0;

        // explicit guard — reset instead of propagating inf to the plot
        double peak = 0.0;
        for (int i = 0; i < n; ++i) {
            double v = std::fabs(u2[i]);
            if (v > peak) peak = v;
        }
        if (!std::isfinite(peak) || peak > 1e6) {
            initFDGrid();
            fdFirstStep = true;
            time = 0.f;
            return;
        }

        // rotate: u0 <- old u1 (prev), u1 <- just computed
        u0.swap(u1);
        u1.swap(u2);
        time += float(dtStep);
    }
}

double WaveSolver::fourierAt(double x, double t) const {
    double sum = 0.0;
    for (int n = 0; n < (int)A.size(); ++n) {
        double lambda = (n + 1) * PI / L;
        sum += A[n] * std::cos(lambda * a * t) * std::sin(lambda * x);
    }
    return sum;
}

void WaveSolver::getFourierProfile(float t, std::vector<float>& out) const {
    int pts = N + 1;
    out.resize(pts);
    double dx = L / N;
    for (int i = 0; i <= N; ++i)
        out[i] = float(fourierAt(i * dx, t));
}

void WaveSolver::getFDProfile(std::vector<float>& out) const {
    int n = (int)u1.size();
    out.resize(n);
    for (int i = 0; i < n; ++i)
        out[i] = float(u1[i]);
}

void WaveSolver::computeFourierCoeffs() {
    // A_n = (2/L) integral_0^L f(x) sin(n pi x/L) dx, trapezoid rule
    int   M  = 2000;
    double dx = L / M;
    A.resize(nTerms);
    for (int n = 1; n <= nTerms; ++n) {
        double sum = 0.0;
        double lambda = n * PI / L;
        for (int i = 0; i <= M; ++i) {
            double x  = i * dx;
            double fx = evalIC(preset, float(x), L, &customIC);
            double w  = (i == 0 || i == M) ? 0.5 : 1.0;
            sum += w * fx * std::sin(lambda * x) * dx;
        }
        A[n-1] = 2.0 / L * sum;
    }
}

void WaveSolver::initFDGrid() {
    int pts = N + 1;
    u0.resize(pts);
    u1.resize(pts);
    u2.resize(pts);

    double dx = L / N;
    // u0 = u1 = f(x); initial velocity is zero
    for (int i = 0; i <= N; ++i) {
        double val = evalIC(preset, float(i * dx), L, &customIC);
        u0[i] = val;
        u1[i] = val;
    }
    u0[0] = u0[N] = 0.0;
    u1[0] = u1[N] = 0.0;
}

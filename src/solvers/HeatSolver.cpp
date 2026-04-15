#include "HeatSolver.h"
#include <cmath>
#include <algorithm>

static const double PI_H = std::acos(-1.0);

void HeatSolver::init() {
    time = 0.0f;
    computeCoeffs();
    initGrid();
}

// Crank-Nicolson: (I - r/2 D^2) u^{n+1} = (I + r/2 D^2) u^n, r = a^2 dt/dx^2.
// Unconditionally stable, but we sub-step to keep per-step r <~ 5 for accuracy.
void HeatSolver::stepCN(float dt) {
    if (dt <= 0.f) return;
    int    M    = (int)u.size();   // = N+1
    double dx   = L / (M - 1);
    double a2   = double(a) * double(a);
    double dtAcc = 5.0 * dx * dx / std::max(1e-9, a2);
    double remain = dt;

    std::vector<double> rhs(M), al(M), bl(M), cl(M);

    while (remain > 1e-9) {
        double dtStep = std::min(dtAcc, remain);
        double r      = a2 * dtStep / (dx * dx);

        rhs[0]     = (double)bcLeft.value;
        rhs[M - 1] = (double)bcRight.value;
        for (int i = 1; i < M - 1; ++i)
            rhs[i] = r/2.0 * u[i-1] + (1.0 - r) * u[i] + r/2.0 * u[i+1];

        std::fill(al.begin(), al.end(), -r/2.0);
        std::fill(bl.begin(), bl.end(),  1.0 + r);
        std::fill(cl.begin(), cl.end(), -r/2.0);
        al[0]   = cl[0]   = 0.0; bl[0]   = 1.0;
        al[M-1] = cl[M-1] = 0.0; bl[M-1] = 1.0;

        solveTridiag(al, bl, cl, rhs, u);
        time   += float(dtStep);
        remain -= dtStep;
    }
}

// Non-zero Dirichlet BCs handled via boundary lift phi(x) = bcL + (bcR-bcL)x/L:
// u(x,t) = phi(x) + sum Bn exp(-a^2 lam_n^2 t) sin(lam_n x),
// with Bn = sine coefficients of f(x) - phi(x).
void HeatSolver::getFourierProfile(float t, std::vector<float>& out) const {
    int pts = N + 1;
    out.resize(pts);
    double dx = L / N;
    double bL = (double)bcLeft.value;
    double bR = (double)bcRight.value;
    for (int i = 0; i <= N; ++i) {
        double x   = i * dx;
        double phi = bL + (bR - bL) * (x / L);
        double val = phi;
        for (int n = 1; n <= (int)B.size(); ++n) {
            double lambda = n * PI_H / L;
            val += B[n-1]
                   * std::exp(-(double)a*a * lambda*lambda * t)
                   * std::sin(lambda * x);
        }
        out[i] = float(val);
    }
}

void HeatSolver::getCNProfile(std::vector<float>& out) const {
    int n = (int)u.size();
    out.resize(n);
    for (int i = 0; i < n; ++i) out[i] = float(u[i]);
}

void HeatSolver::computeCoeffs() {
    // Bn = (2/L) integral_0^L [f(x) - phi(x)] sin(n pi x / L) dx
    int    M  = 2000;
    double dx = L / M;
    double bL = (double)bcLeft.value;
    double bR = (double)bcRight.value;
    B.resize(nTerms);
    for (int n = 1; n <= nTerms; ++n) {
        double sum    = 0.0;
        double lambda = n * PI_H / L;
        for (int i = 0; i <= M; ++i) {
            double x   = i * dx;
            double phi = bL + (bR - bL) * (x / L);
            double fx  = evalIC(preset, float(x), L, &customIC) - phi;
            double w   = (i == 0 || i == M) ? 0.5 : 1.0;
            sum += w * fx * std::sin(lambda * x) * dx;
        }
        B[n-1] = 2.0 / L * sum;
    }
}

void HeatSolver::initGrid() {
    int pts = N + 1;
    u.resize(pts);
    double dx = L / N;
    for (int i = 0; i <= N; ++i)
        u[i] = evalIC(preset, float(i * dx), L, &customIC);
    u[0]   = bcLeft.value;
    u[N]   = bcRight.value;
}

void HeatSolver::solveTridiag(const std::vector<double>& a,
                               const std::vector<double>& b,
                               const std::vector<double>& c,
                               const std::vector<double>& d,
                               std::vector<double>& x)
{
    int n = (int)b.size();
    std::vector<double> c2(n), d2(n);
    x.resize(n);

    c2[0] = c[0] / b[0];
    d2[0] = d[0] / b[0];
    for (int i = 1; i < n; ++i) {
        double m = b[i] - a[i] * c2[i-1];
        c2[i]    = c[i] / m;
        d2[i]    = (d[i] - a[i] * d2[i-1]) / m;
    }
    x[n-1] = d2[n-1];
    for (int i = n - 2; i >= 0; --i)
        x[i] = d2[i] - c2[i] * x[i+1];
}

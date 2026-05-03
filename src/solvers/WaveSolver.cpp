#include "WaveSolver.h"
#include <cmath>

double WaveSolver::evalAlpha(double t) const {
    if (!useNonhomogBC || !alphaExpr.ok) return 0.0;
    parser::EvalContext ctx;
    ctx.vars["t"] = t;
    ctx.vars["L"] = double(L);
    ctx.vars["a"] = double(a);
    double v = alphaExpr.expr.eval(ctx);
    return std::isfinite(v) ? v : 0.0;
}

double WaveSolver::evalBeta(double t) const {
    if (!useNonhomogBC || !betaExpr.ok) return 0.0;
    parser::EvalContext ctx;
    ctx.vars["t"] = t;
    ctx.vars["L"] = double(L);
    ctx.vars["a"] = double(a);
    double v = betaExpr.expr.eval(ctx);
    return std::isfinite(v) ? v : 0.0;
}

double WaveSolver::evalPsi(double x) const {
    if (!useNonzeroPsi || !psiExpr.ok) return 0.0;
    double Ld = std::max(1e-9, double(L));
    parser::EvalContext ctx;
    ctx.vars["x"]  = x;
    ctx.vars["xi"] = x / Ld;
    ctx.vars["L"]  = double(L);
    ctx.vars["a"]  = double(a);
    double v = psiExpr.expr.eval(ctx);
    return std::isfinite(v) ? v : 0.0;
}

double WaveSolver::evalForcing(double x) const {
    if (!useForcing || !forcingExpr.ok) return 0.0;
    double Ld = std::max(1e-9, double(L));
    parser::EvalContext ctx;
    ctx.vars["x"]  = x;
    ctx.vars["xi"] = x / Ld;
    ctx.vars["L"]  = double(L);
    ctx.vars["a"]  = double(a);
    double v = forcingExpr.expr.eval(ctx);
    return std::isfinite(v) ? v : 0.0;
}

// Central-difference second derivative for α(t), β(t).
double WaveSolver::alphaDD(double t) const {
    const double h = 1e-4;
    return (evalAlpha(t + h) - 2.0 * evalAlpha(t) + evalAlpha(t - h)) / (h * h);
}
double WaveSolver::betaDD(double t) const {
    const double h = 1e-4;
    return (evalBeta(t + h) - 2.0 * evalBeta(t) + evalBeta(t - h)) / (h * h);
}

// Boundary lift v(x,t) = α(t)(1 - x/L) + β(t)(x/L)
double WaveSolver::vAt(double x, double t) const {
    if (!useNonhomogBC) return 0.0;
    double xi = x / std::max(1e-9, double(L));
    return evalAlpha(t) * (1.0 - xi) + evalBeta(t) * xi;
}

void WaveSolver::init() {
    time = 0.0f;
    fdFirstStep      = true;
    fourierFirstStep = true;
    fdAccum          = 0.0;
    fourierAccum     = 0.0;

    alphaExpr.reparse();
    betaExpr.reparse();
    psiExpr.reparse();
    forcingExpr.reparse();

    computeFourierCoeffs();
    initFDGrid();
}

void WaveSolver::stepFD(float dtF) {
    if (dtF <= 0.f) return;
    double dx = double(L) / double(N);
    double aa = std::max(1e-6, double(a));

    // Fixed sub-step at Courant 0.7. Leapfrog references u[n-1] from the
    // previous call, so dtStep MUST be constant across calls — varying it
    // breaks the stencil and grids visibly explode when the speed slider
    // is dragged. Real-time fluctuations are absorbed by the accumulator.
    const double rTarget = 0.7;
    const double dtStep  = rTarget * dx / aa;
    const double r       = rTarget;
    const double r2      = r * r;

    fdAccum += double(dtF);
    int nsub = (int)std::floor(fdAccum / dtStep);
    fdAccum -= nsub * dtStep;
    if (nsub > 200) {                  // dt spike: drop the backlog rather than catch up
        nsub = 200;
        fdAccum = 0.0;
    }
    if (nsub < 1) return;

    int n = (int)u1.size();
    if (n < 3) return;
    u2.resize(n);

    for (int k = 0; k < nsub; ++k) {
        if (fdFirstStep) {
            // first step with ψ and f: u¹ = u⁰ + ψ·dt + (dt²/2)[a²·Δu⁰ + f]
            for (int i = 1; i < n - 1; ++i) {
                double xi  = i * dx;
                double lap = (u1[i+1] - 2.0*u1[i] + u1[i-1]);
                u2[i] = u1[i]
                      + evalPsi(xi) * dtStep
                      + 0.5 * r2 * lap
                      + 0.5 * dtStep * dtStep * evalForcing(xi);
            }
            fdFirstStep = false;
        } else {
            for (int i = 1; i < n - 1; ++i) {
                double xi = i * dx;
                u2[i] = 2.0*(1.0 - r2)*u1[i] + r2*(u1[i+1] + u1[i-1]) - u0[i]
                      + dtStep * dtStep * evalForcing(xi);
            }
        }
        double tNext = double(time) + dtStep;
        u2[0]   = evalAlpha(tNext);
        u2[n-1] = evalBeta (tNext);

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

        u0.swap(u1);
        u1.swap(u2);
        time += float(dtStep);
    }
}

void WaveSolver::stepFourier(float dtF) {
    if (dtF <= 0.f) return;
    if (nTerms < 1) return;

    // Homogeneous case: keep closed-form path, just advance simulated time.
    if (!useForcing && !useNonhomogBC && !useNonzeroPsi) {
        time += dtF;
        return;
    }

    if ((int)Tprev.size() != nTerms || (int)Tcur.size() != nTerms) {
        computeFourierCoeffs();
        fourierFirstStep = true;
        fourierAccum     = 0.0;
    }

    // Fixed Verlet sub-step. Same reason as stepFD: variable ds across
    // calls invalidates the previous-step amplitude T[n-1]. The 5 ms cap
    // keeps enough sub-steps per 16 ms frame that animation looks smooth
    // at speed=1 even when the stability bound itself would be large.
    double aa       = std::max(1e-6, double(a));
    double omegaMax = nTerms * kPi * aa / double(L);
    double dtStable = (omegaMax > 1e-9) ? (1.6 / omegaMax) : 5e-3;
    const double ds = std::min(dtStable, 5e-3);

    fourierAccum += double(dtF);
    int nsub = (int)std::floor(fourierAccum / ds);
    fourierAccum -= nsub * ds;
    if (nsub > 200) {
        nsub = 200;
        fourierAccum = 0.0;
    }
    if (nsub < 1) return;

    for (int k = 0; k < nsub; ++k) {
        double t   = double(time);
        double aDD = useNonhomogBC ? alphaDD(t) : 0.0;
        double bDD = useNonhomogBC ? betaDD (t) : 0.0;

        for (int n = 1; n <= nTerms; ++n) {
            double omega  = n * kPi * aa / double(L);
            double omega2 = omega * omega;

            // Fₙ(t) = fₙ − α''(t)·(2/(nπ)) − β''(t)·(2/(nπ))·(−1)^(n+1)
            double liftA = 2.0 / (double(n) * kPi);
            double sign  = (n % 2 == 0) ? -1.0 : 1.0;
            double liftB = liftA * sign;
            double fn    = useForcing ? fHat[n-1] : 0.0;
            double Fn    = fn - aDD * liftA - bDD * liftB;

            double next;
            if (fourierFirstStep) {
                double phi_n = A[n-1];
                double psi_n = B[n-1];
                next = phi_n + psi_n * ds
                     + 0.5 * ds * ds * (-omega2 * phi_n + Fn);
                Tprev[n-1] = phi_n;
                Tcur [n-1] = next;
            } else {
                double Tnow = Tcur [n-1];
                double Told = Tprev[n-1];
                next = 2.0 * Tnow - Told + ds * ds * (-omega2 * Tnow + Fn);
                if (!std::isfinite(next) || std::fabs(next) > 1e6) next = 0.0;
                Tprev[n-1] = Tnow;
                Tcur [n-1] = next;
            }
        }
        fourierFirstStep = false;
        time += float(ds);
    }
}

double WaveSolver::fourierAt(double x, double t) const {
    // Homogeneous: exact closed form (unchanged behavior).
    if (!useForcing && !useNonhomogBC && !useNonzeroPsi) {
        double sum = 0.0;
        for (int n = 0; n < (int)A.size(); ++n) {
            double lambda = (n + 1) * kPi / L;
            sum += A[n] * std::cos(lambda * a * t) * std::sin(lambda * x);
        }
        return sum;
    }
    // Inhomogeneous: reconstruct w(x,t) from Verlet amplitudes + boundary lift.
    double sum = 0.0;
    for (int n = 0; n < (int)Tcur.size(); ++n) {
        double lambda = (n + 1) * kPi / L;
        sum += Tcur[n] * std::sin(lambda * x);
    }
    return sum + vAt(x, t);
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
    // Project onto sin basis, 500-pt trapezoid:
    //   φₙ = (2/L) ∫ [φ(x) - v(x,0)]   sin(nπx/L) dx
    //   ψₙ = (2/L) ∫ [ψ(x) - v_t(x,0)] sin(nπx/L) dx
    //   fₙ = (2/L) ∫  f(x)              sin(nπx/L) dx
    int    M  = 500;
    double dx = L / double(M);

    A.assign(nTerms, 0.0);
    B.assign(nTerms, 0.0);
    fHat.assign(nTerms, 0.0);
    Tprev.assign(nTerms, 0.0);
    Tcur .assign(nTerms, 0.0);

    // v_t(x,0) via forward difference on α/β
    const double hT = 1e-5;
    double alpha0 = evalAlpha(0.0), beta0 = evalBeta(0.0);
    double alphaH = evalAlpha(hT),  betaH = evalBeta(hT);

    for (int n = 1; n <= nTerms; ++n) {
        double sumPhi = 0.0, sumPsi = 0.0, sumF = 0.0;
        double lambda = n * kPi / double(L);
        for (int i = 0; i <= M; ++i) {
            double x    = i * dx;
            double xi   = x / std::max(1e-9, double(L));
            double phiX = evalIC(preset, float(x), L, &customIC);
            double v0   = useNonhomogBC ? (alpha0*(1.0 - xi) + beta0*xi) : 0.0;
            double vH   = useNonhomogBC ? (alphaH*(1.0 - xi) + betaH*xi) : 0.0;
            double vT0  = useNonhomogBC ? (vH - v0) / hT                 : 0.0;
            double psiX = evalPsi    (x);
            double fX   = evalForcing(x);
            double w    = (i == 0 || i == M) ? 0.5 : 1.0;
            double s    = std::sin(lambda * x);
            sumPhi += w * (phiX - v0 ) * s * dx;
            sumPsi += w * (psiX - vT0) * s * dx;
            sumF   += w *  fX          * s * dx;
        }
        A   [n-1] = 2.0 / L * sumPhi;
        B   [n-1] = 2.0 / L * sumPsi;
        fHat[n-1] = 2.0 / L * sumF;
        Tprev[n-1] = A[n-1];   // T(0) = φₙ
        Tcur [n-1] = A[n-1];   // placeholder until the first Verlet step
    }
}

void WaveSolver::initFDGrid() {
    int pts = N + 1;
    u0.resize(pts);
    u1.resize(pts);
    u2.resize(pts);

    double dx = L / double(N);
    for (int i = 0; i <= N; ++i) {
        double val = evalIC(preset, float(i * dx), L, &customIC);
        u0[i] = val;
        u1[i] = val;
    }
    // Bake α(0), β(0) into the endpoints so the first FD step is consistent.
    double a0 = evalAlpha(0.0);
    double b0 = evalBeta (0.0);
    u0[0] = u1[0] = a0;
    u0[N] = u1[N] = b0;
}

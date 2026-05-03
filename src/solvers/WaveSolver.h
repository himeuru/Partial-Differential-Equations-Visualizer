#pragma once
#include "IC.h"

// u_tt = a^2 u_xx + f(x) on [0, L] with Dirichlet BCs u(0,t)=α(t), u(L,t)=β(t)
// and initial conditions u(x,0)=φ(x), u_t(x,0)=ψ(x).
// Two methods in parallel: analytic Fourier series (with boundary lift + Verlet
// integration of mode amplitudes when the problem is inhomogeneous) and
// explicit FD.
class WaveSolver {
public:
    float  L        = 1.0f;
    float  a        = 1.0f;
    int    N        = 200;     // FD grid points
    int    nTerms   = 20;      // Fourier terms
    ICPreset     preset = ICPreset::GaussianPulse;
    ICCustomExpr customIC;

    // Inhomogeneous BVP data
    ICCustomExpr alphaExpr;    // α(t)  — left Dirichlet
    ICCustomExpr betaExpr;     // β(t)  — right Dirichlet
    ICCustomExpr psiExpr;      // ψ(x) — initial velocity
    ICCustomExpr forcingExpr;  // f(x) — forcing term

    bool useNonhomogBC = false;
    bool useForcing    = false;
    bool useNonzeroPsi = false;

    float  time     = 0.0f;

    // Fourier state
    std::vector<double> A;       // φₙ: sin-projection of w(x,0) = φ(x) - v(x,0)
    std::vector<double> B;       // ψₙ: sin-projection of w_t(x,0) = ψ(x) - v_t(x,0)
    std::vector<double> fHat;    // fₙ: sin-projection of forcing f(x)
    std::vector<double> Tprev;   // Tₙ(t-dt)  Verlet history
    std::vector<double> Tcur;    // Tₙ(t)
    bool fourierFirstStep = true;

    // FD state — three time layers for the stencil
    std::vector<double> u0, u1, u2;
    bool fdFirstStep = true;

    // Real-time accumulators. Both stepFD and stepFourier (inhomogeneous
    // branch) integrate with a fixed internal sub-step so leapfrog/Verlet
    // stay self-consistent when the input dt jitters between frames.
    double fdAccum      = 0.0;
    double fourierAccum = 0.0;

    void init();
    void stepFD(float dt);
    void stepFourier(float dt);

    double fourierAt(double x, double t) const;
    void   getFourierProfile(float t, std::vector<float>& out) const;
    void   getFDProfile(std::vector<float>& out) const;

    // expression evaluators (return 0 when the corresponding flag is off)
    double evalAlpha  (double t) const;
    double evalBeta   (double t) const;
    double evalPsi    (double x) const;
    double evalForcing(double x) const;

private:
    void   computeFourierCoeffs();
    void   initFDGrid();
    double alphaDD(double t) const;
    double betaDD (double t) const;
    double vAt    (double x, double t) const;
};

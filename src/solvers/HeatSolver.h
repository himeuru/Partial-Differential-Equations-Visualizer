#pragma once
#include "IC.h"

// u_t = a^2 u_xx on [0, L].
// Analytic Fourier series with boundary lift, plus Crank-Nicolson FD.
class HeatSolver {
public:
    float    L        = 1.0f;
    float    a        = 1.0f;   // diffusivity
    int      N        = 200;
    int      nTerms   = 40;
    ICPreset     preset   = ICPreset::GaussianPulse;
    ICCustomExpr customIC;

    BoundaryCondition bcLeft, bcRight;

    float time = 0.0f;

    std::vector<double> B;   // sine coefficients of (f - phi)
    std::vector<double> u;   // CN grid values

    void init();
    void stepCN(float dt);

    void getFourierProfile(float t, std::vector<float>& out) const;
    void getCNProfile(std::vector<float>& out) const;

private:
    void computeCoeffs();
    void initGrid();

    // Thomas sweep for a tridiagonal system
    void solveTridiag(const std::vector<double>& a,
                      const std::vector<double>& b,
                      const std::vector<double>& c,
                      const std::vector<double>& d,
                      std::vector<double>& x);
};

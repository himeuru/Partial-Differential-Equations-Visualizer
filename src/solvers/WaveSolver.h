#pragma once
#include "Common.h"

// u_tt = a^2 u_xx on [0, L] with Dirichlet BCs.
// Two methods kept in parallel: analytic Fourier series and explicit FD.
class WaveSolver {
public:
    float  L        = 1.0f;
    float  a        = 1.0f;
    int    N        = 200;     // FD grid points
    int    nTerms   = 20;      // Fourier terms
    ICPreset     preset = ICPreset::GaussianPulse;
    ICCustomExpr customIC;

    float  time     = 0.0f;

    std::vector<double> A;           // sine coefficients of IC

    // three time layers for the FD stencil
    std::vector<double> u0, u1, u2;
    bool fdFirstStep = true;         // uses half-coefficient formula once

    void init();
    void stepFD(float dt);

    double fourierAt(double x, double t) const;
    void   getFourierProfile(float t, std::vector<float>& out) const;
    void   getFDProfile(std::vector<float>& out) const;

private:
    void computeFourierCoeffs();
    void initFDGrid();
};

#pragma once
#include "Common.h"

// Fourier sine-series decomposition and partial-sum convergence visualizer.

class FourierViz {
public:
    float    L        = 1.0f;
    int      N        = 200;     // grid points for plotting
    int      nMax     = 30;      // total harmonics computed
    int      nShow    = 1;       // how many terms to include in partial sum
    ICPreset     preset   = ICPreset::StepFunction;
    ICCustomExpr customIC;

    bool showHarmonics  = false;
    bool showError      = true;

    // Outputs
    std::vector<float> exact;          // f(x)
    std::vector<float> partial;        // Sₙ(x)
    std::vector<float> error;          // |f - Sₙ|
    std::vector<double> coeffs;        // bₙ, size = nMax
    std::vector<std::vector<float>> harmonics;  // [n][x]

    void compute();

private:
    void computeCoeffs();
};

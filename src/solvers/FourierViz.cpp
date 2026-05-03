#include "FourierViz.h"
#include <cmath>
#include <algorithm>

void FourierViz::compute() {
    computeCoeffs();

    int pts = N + 1;
    exact.resize(pts);
    partial.resize(pts);
    error.resize(pts);
    harmonics.resize(nMax);
    for (int k = 0; k < nMax; ++k) harmonics[k].resize(pts);

    double dx = L / N;

    for (int i = 0; i <= N; ++i) {
        float x  = float(i * dx);
        float fx = evalIC(preset, x, float(L), &customIC);
        exact[i] = fx;

        float sum = 0.f;
        for (int k = 1; k <= nShow; ++k) {
            double lambda = k * kPi / L;
            float  harm   = float(coeffs[k-1] * std::sin(lambda * x));
            harmonics[k-1][i] = harm;
            sum += harm;
        }
        partial[i] = sum;
        error[i]   = std::fabs(fx - sum);
    }
}

void FourierViz::computeCoeffs() {
    int    M  = 4000;
    double dx = L / M;
    coeffs.resize(nMax);
    for (int n = 1; n <= nMax; ++n) {
        double sum    = 0.0;
        double lambda = n * kPi / L;
        for (int i = 0; i <= M; ++i) {
            double x  = i * dx;
            double fx = evalIC(preset, float(x), float(L), &customIC);
            double w  = (i == 0 || i == M) ? 0.5 : 1.0;
            sum += w * fx * std::sin(lambda * x) * dx;
        }
        coeffs[n-1] = 2.0 / L * sum;
    }
}

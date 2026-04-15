#pragma once
#include "Common.h"

// Green's functions visualizer: heat, wave, Sturm-Liouville, Laplace 2D,
// and user-defined piecewise.
enum class GreenMode {
    Heat1D,
    Wave1D,
    SturmLiouville,
    Laplace2D,
    Custom1D,
};

static const char* GreenModeNames[] = {
    "Heat kernel (1D)",
    "Wave kernel (1D)",
    "Sturm-Liouville (1D, -d^2/dx^2)",
    "Laplace 2D (free space)",
    u8"Custom G(x, \u03be, t) ...",
};

class GreenSolver {
public:
    GreenMode mode    = GreenMode::Heat1D;

    float L           = 1.0f;
    float a           = 1.0f;    // diffusivity or wave speed
    float t           = 0.05f;
    float xi          = 0.5f;    // source x
    float eta         = 0.5f;    // source y (Laplace 2D)
    int   N           = 300;
    int   nTerms      = 60;      // series truncation for SL

    // Piecewise user-defined mode. The first piece whose [xLo, xHi] contains
    // x wins. Bounds are themselves expressions and may depend on t, xi, a, L.
    struct Piece {
        ICCustomExpr xLoExpr;
        ICCustomExpr xHiExpr;
        ICCustomExpr expr;
    };
    std::vector<Piece> customPieces;

    // Diagnostics filled by computeCustom1D for the UI
    int customNanCount   = 0;
    int customTotalCount = 0;
    int customMissCount  = 0;

    // Outputs
    std::vector<float> g1d;
    std::vector<float> g2d;
    int                g2dW = 0, g2dH = 0;

    float gMin = 0.f, gMax = 1.f;

    void compute();

private:
    void computeHeat1D();
    void computeWave1D();
    void computeSturmLiouville();
    void computeLaplace2D();
    void computeCustom1D();
};

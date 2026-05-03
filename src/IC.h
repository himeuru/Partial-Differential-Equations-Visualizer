#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "parser/Expr.hpp"

inline constexpr double kPi = 3.14159265358979323846;

// All boundaries currently are Dirichlet — the struct exists so the
// call site reads as `bcLeft.value` rather than a bare float.
struct BoundaryCondition {
    float value = 0.0f;
};

enum class ICPreset {
    SinePulse,
    TrianglePulse,
    GaussianPulse,
    StepFunction,
    TwoModes,
    Custom,   // user expression, see ICCustomExpr
};

static const char* ICPresetNames[] = {
    "sin(pi*x/L)  [mode 1]",
    "Triangle pulse",
    "Gaussian pulse",
    "Step function",
    "sin + 0.5*sin(2x)  [modes 1,2]",
    "Custom f(x) ...",
};

// User-defined IC: parsed expression + editor-side state for customExprInput.
struct ICCustomExpr {
    parser::Expr expr;
    std::string  source = "sin(pi*x/L)";
    bool         ok     = true;
    std::string  err;

    // editor state
    int          cursorPos           = 0;
    bool         needsFocus          = false;
    std::string  pendingInsert;
    int          pendingCursorOffset = -1;

    bool reparse() {
        ok = expr.parse(source);
        if (!ok) { err = expr.error(); return false; }
        // f(x), g(x), u(x,t), G(x,xi,t) parse as calls so MathRender can
        // typeset them, but the evaluator has no body — reject here so the
        // user gets an error instead of a silent zero.
        std::string displayOnly;
        if (expr.hasDisplayOnlyCalls(displayOnly)) {
            ok  = false;
            err = "функция " + displayOnly + "(...) только для отображения — задайте её явно";
            return false;
        }
        err.clear();
        return true;
    }
};

inline float evalIC(ICPreset p, float x, float L,
                    const ICCustomExpr* custom = nullptr)
{
    float xi = x / L;
    switch (p) {
        case ICPreset::SinePulse:
            return std::sin(float(kPi) * xi);
        case ICPreset::TrianglePulse:
            return (xi < 0.5f) ? 2.0f * xi : 2.0f * (1.0f - xi);
        case ICPreset::GaussianPulse: {
            float cx = xi - 0.5f;
            return std::exp(-40.0f * cx * cx);
        }
        case ICPreset::StepFunction:
            return (xi < 0.5f) ? 1.0f : 0.0f;
        case ICPreset::TwoModes:
            return std::sin(float(kPi)*xi) + 0.5f*std::sin(2.f*float(kPi)*xi);
        case ICPreset::Custom: {
            if (!custom || !custom->ok) return 0.0f;
            parser::EvalContext ctx;
            ctx.vars["x"]  = double(x);
            ctx.vars["L"]  = double(L);
            ctx.vars["xi"] = double(xi);
            double v = custom->expr.eval(ctx);
            if (!std::isfinite(v)) return 0.0f;
            return float(v);
        }
    }
    return 0.0f;
}

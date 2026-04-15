#pragma once
// Lightweight 2D math typesetter over parser::Expr.
// Renders numbers, variables with Greek substitution, subscripts via `_`,
// unary minus, +-*/ with fractions stacked, ^ as raised exponent, function
// calls, and the display-only sum/integral operators with limits.
//
//    mrender::render(expr, pxHeight, colorU32);
#include "parser/Expr.hpp"
#include <imgui.h>
#include <string>
#include <vector>
#include <cstdio>

namespace mrender {

using Node = parser::Expr::Node;

// Laid-out fragment: a text leaf or a group of positioned children.
struct Laid {
    ImVec2             size{0, 0};      // bounding box
    float              axis = 0.f;      // y-offset of main baseline from top
    // Leaf:
    std::string        text;            // non-empty for text leaves
    float              fontScale = 1.f; // scale applied to leaf text
    // Composite:
    std::vector<Laid>  kids;
    std::vector<ImVec2> offsets;        // top-left of each kid, box-local
    bool               drawFracLine = false;  // horizontal bar at y=axis
};

static inline ImVec2 measureText(const char* s, float pxHeight) {
    ImFont* f = ImGui::GetFont();
    return f->CalcTextSizeA(pxHeight, FLT_MAX, 0.f, s);
}

static inline std::string greekOf(const std::string& n) {
    if (n == "pi")     return u8"\u03c0";
    if (n == "xi")     return u8"\u03be";
    if (n == "eta")    return u8"\u03b7";
    if (n == "theta")  return u8"\u03b8";
    if (n == "lambda") return u8"\u03bb";
    if (n == "phi")    return u8"\u03c6";
    if (n == "omega")  return u8"\u03c9";
    if (n == "mu")     return u8"\u03bc";
    if (n == "sigma")  return u8"\u03c3";
    if (n == "alpha")  return u8"\u03b1";
    if (n == "beta")   return u8"\u03b2";
    if (n == "gamma")  return u8"\u03b3";
    return n;
}

static inline std::string numText(double v) {
    char buf[48];
    if (v == (double)(long long)v && std::fabs(v) < 1e15)
        std::snprintf(buf, sizeof(buf), "%lld", (long long)v);
    else
        std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

// Operator precedence (higher binds tighter)
static inline int precedence(const Node* n) {
    if (!n) return 100;
    if (n->kind == Node::BIN) {
        switch (n->op) {
            case '=':           return 0;   // display-only, lowest
            case '+': case '-': return 1;
            case '*': case '/': return 2;
            case '^':           return 3;
        }
    }
    if (n->kind == Node::NEG) return 1;
    return 100;  // atoms
}

// Forward declarations
static Laid layoutNode(const Node* n, float pxHeight, bool withParens = false);

static Laid makeLeaf(const std::string& text, float pxHeight) {
    Laid l;
    l.text = text;
    l.fontScale = pxHeight / ImGui::GetFont()->FontSize;
    ImVec2 sz = measureText(text.c_str(), pxHeight);
    l.size = sz;
    l.axis = sz.y * 0.55f;   // optical center, below geometric midpoint
    return l;
}

// Horizontal stack with axes aligned across children
static Laid hstack(std::initializer_list<Laid> parts, float pad = 0.f) {
    Laid out;
    float axis = 0.f;
    for (const auto& p : parts) axis = std::max(axis, p.axis);
    float totalW = 0.f, totalH = 0.f;
    // First pass: compute height
    for (const auto& p : parts) {
        float top = axis - p.axis;
        totalH    = std::max(totalH, top + p.size.y);
    }
    // Second pass: place
    for (const auto& p : parts) {
        float top = axis - p.axis;
        out.kids.push_back(p);
        out.offsets.push_back({totalW, top});
        totalW += p.size.x + pad;
    }
    if (totalW > 0) totalW -= pad;
    out.size = {totalW, totalH};
    out.axis = axis;
    return out;
}

// Wrap a box in parentheses. The cap on parenScale avoids "bold" strokes
// when a single glyph is stretched too much (no stretchy-bracket font here).
static Laid wrapParen(const Laid& inner, float pxHeight) {
    float parenH    = std::max(pxHeight, inner.size.y);
    float parenScale = std::min(1.35f, parenH / pxHeight);
    Laid lp = makeLeaf("(", pxHeight * parenScale);
    Laid rp = makeLeaf(")", pxHeight * parenScale);
    Laid out;
    out.size = {lp.size.x + inner.size.x + rp.size.x, inner.size.y};
    out.axis = inner.axis;
    out.kids.push_back(lp);
    out.offsets.push_back({0.f, inner.size.y * 0.5f - lp.size.y * 0.5f});
    out.kids.push_back(inner);
    out.offsets.push_back({lp.size.x, 0.f});
    out.kids.push_back(rp);
    out.offsets.push_back({lp.size.x + inner.size.x, inner.size.y * 0.5f - rp.size.y * 0.5f});
    return out;
}

static Laid layoutNode(const Node* n, float pxHeight, bool withParens) {
    if (!n) return makeLeaf("?", pxHeight);

    switch (n->kind) {

    case Node::NUM: return makeLeaf(numText(n->num), pxHeight);
    case Node::VAR: {
        // Underscore splits the name into base and subscript: u_tt, S_N, lambda_n
        const std::string& raw = n->name;
        size_t us = raw.find('_');
        if (us != std::string::npos && us > 0 && us + 1 < raw.size()) {
            Laid base = makeLeaf(greekOf(raw.substr(0, us)), pxHeight);
            Laid sub  = makeLeaf(greekOf(raw.substr(us + 1)), pxHeight * 0.66f);
            float drop = base.size.y * 0.40f;
            Laid out;
            out.size = {base.size.x + sub.size.x + 1.f,
                        std::max(base.size.y, drop + sub.size.y)};
            out.axis = base.axis;
            out.kids.push_back(base);
            out.offsets.push_back({0.f, 0.f});
            out.kids.push_back(sub);
            out.offsets.push_back({base.size.x, drop});
            return out;
        }
        return makeLeaf(greekOf(raw), pxHeight);
    }

    case Node::NEG: {
        Laid inner = layoutNode(n->kids[0].get(), pxHeight,
                                precedence(n->kids[0].get()) < 2);
        Laid minus = makeLeaf(u8"\u2212", pxHeight);
        Laid out   = hstack({minus, inner});
        if (withParens) out = wrapParen(out, pxHeight);
        return out;
    }

    case Node::BIN: {
        // Division renders as stacked numerator-over-denominator fraction
        if (n->op == '/') {
            float subH = pxHeight * 0.92f;
            Laid num = layoutNode(n->kids[0].get(), subH, false);
            Laid den = layoutNode(n->kids[1].get(), subH, false);
            float gap = pxHeight * 0.18f;
            float barPad = pxHeight * 0.15f;
            float w   = std::max(num.size.x, den.size.x) + barPad * 2.f;
            float h   = num.size.y + gap * 2.f + den.size.y;
            float ax  = num.size.y + gap;
            Laid out;
            out.size  = {w, h};
            out.axis  = ax;
            out.drawFracLine = true;
            // Center numerator and denominator horizontally
            out.kids.push_back(num);
            out.offsets.push_back({(w - num.size.x) * 0.5f, 0.f});
            out.kids.push_back(den);
            out.offsets.push_back({(w - den.size.x) * 0.5f, num.size.y + gap * 2.f});
            if (withParens) out = wrapParen(out, pxHeight);
            return out;
        }
        // Power a ^ b : base with raised smaller exponent
        if (n->op == '^') {
            bool baseParen = precedence(n->kids[0].get()) < 3;
            Laid base = layoutNode(n->kids[0].get(), pxHeight, baseParen);
            Laid exp  = layoutNode(n->kids[1].get(), pxHeight * 0.68f, false);
            float raise = base.size.y * 0.45f;
            float totalH = raise + base.size.y;
            float w      = base.size.x + exp.size.x * 0.95f;
            Laid out;
            out.size = {w, totalH};
            out.axis = raise + base.axis;
            out.kids.push_back(base);
            out.offsets.push_back({0.f, raise});
            out.kids.push_back(exp);
            out.offsets.push_back({base.size.x, 0.f});
            if (withParens) out = wrapParen(out, pxHeight);
            return out;
        }
        // Horizontal  a op b  with precedence-driven auto-parens on the rhs
        int myPrec = precedence(n);
        Laid lhs = layoutNode(n->kids[0].get(), pxHeight,
                              precedence(n->kids[0].get()) < myPrec);
        bool rhsParen = precedence(n->kids[1].get()) < myPrec
                        || (precedence(n->kids[1].get()) == myPrec
                            && (n->op == '-' || n->op == '/'));
        Laid rhs = layoutNode(n->kids[1].get(), pxHeight, rhsParen);
        std::string op = (n->op == '*') ? std::string(u8"\u00b7")
                        : (n->op == '-') ? std::string(u8"\u2212")
                        : std::string(1, n->op);
        Laid opLeaf = makeLeaf(op, pxHeight);
        // '=' gets extra breathing room so equations read as equations
        float pad = (n->op == '=') ? pxHeight * 0.55f : pxHeight * 0.14f;
        Laid out = hstack({lhs, opLeaf, rhs}, pad);
        if (withParens) out = wrapParen(out, pxHeight);
        return out;
    }

    case Node::CALL: {
        // sum/integral: big operator with limits, body to the right
        if ((n->name == "sum" || n->name == "integral") && n->kids.size() == 4) {
            bool isSum = (n->name == "sum");
            Laid big = makeLeaf(isSum ? u8"\u03a3" : u8"\u222b",
                                pxHeight * (isSum ? 1.55f : 1.85f));
            float lim = pxHeight * 0.55f;

            Laid hi = layoutNode(n->kids[2].get(), lim, false);
            Laid lo;
            if (isSum) {
                // "var = lo" as subscript

                Laid var = makeLeaf(greekOf(n->kids[0]->name), lim);
                Laid eq  = makeLeaf("=", lim);
                Laid lov = layoutNode(n->kids[1].get(), lim, false);
                lo = hstack({var, eq, lov}, 1.f);
            } else {
                lo = layoutNode(n->kids[1].get(), lim, false);
            }

            // Vertical stack: hi, big, lo. Axis goes through big.
            float colW = std::max({big.size.x, hi.size.x, lo.size.x});
            float gap  = 1.f;
            Laid limited;
            limited.size = {colW,
                            hi.size.y + gap + big.size.y + gap + lo.size.y};
            limited.axis = hi.size.y + gap + big.axis;
            limited.kids.push_back(hi);
            limited.offsets.push_back({(colW - hi.size.x) * 0.5f, 0.f});
            limited.kids.push_back(big);
            limited.offsets.push_back({(colW - big.size.x) * 0.5f,
                                       hi.size.y + gap});
            limited.kids.push_back(lo);
            limited.offsets.push_back({(colW - lo.size.x) * 0.5f,
                                       hi.size.y + gap + big.size.y + gap});

            Laid body = layoutNode(n->kids[3].get(), pxHeight, false);

            Laid out;
            if (isSum) {
                out = hstack({limited, body}, pxHeight * 0.20f);
            } else {
                // integral: trailing "d<var>"
                std::string dv = "d" + greekOf(n->kids[0]->name);
                Laid dLeaf = makeLeaf(dv, pxHeight);
                out = hstack({limited, body, dLeaf}, pxHeight * 0.18f);
            }
            if (withParens) out = wrapParen(out, pxHeight);
            return out;
        }

        Laid name   = makeLeaf(greekOf(n->name), pxHeight);
        Laid lparen = makeLeaf("(", pxHeight);
        Laid rparen = makeLeaf(")", pxHeight);
        std::vector<Laid> argParts;
        argParts.reserve(n->kids.size() * 2);
        for (size_t i = 0; i < n->kids.size(); ++i) {
            if (i > 0) argParts.push_back(makeLeaf(",", pxHeight));
            argParts.push_back(layoutNode(n->kids[i].get(), pxHeight, false));
        }
        // Assemble: name ( arg, arg, ... )
        std::vector<Laid> parts = {name, lparen};
        for (auto& a : argParts) parts.push_back(a);
        parts.push_back(rparen);
        // hstack takes an initializer_list; build by hand here
        Laid out;
        float axis = 0.f;
        for (const auto& p : parts) axis = std::max(axis, p.axis);
        float totalW = 0.f, totalH = 0.f;
        for (const auto& p : parts) {
            float top = axis - p.axis;
            totalH    = std::max(totalH, top + p.size.y);
        }
        for (const auto& p : parts) {
            float top = axis - p.axis;
            out.kids.push_back(p);
            out.offsets.push_back({totalW, top});
            totalW += p.size.x + (p.text == "," ? pxHeight * 0.15f : 0.f);
        }
        out.size = {totalW, totalH};
        out.axis = axis;
        if (withParens) out = wrapParen(out, pxHeight);
        return out;
    }
    }
    return makeLeaf("?", pxHeight);
}

// Walk the laid-out tree and emit draw commands at a top-left anchor
static void drawLaid(ImDrawList* dl, ImVec2 pos, const Laid& l, ImU32 color) {
    if (!l.text.empty()) {
        float px = l.fontScale * ImGui::GetFont()->FontSize;
        dl->AddText(ImGui::GetFont(), px, pos, color, l.text.c_str());
        return;
    }
    for (size_t i = 0; i < l.kids.size(); ++i) {
        ImVec2 o = l.offsets[i];
        drawLaid(dl, {pos.x + o.x, pos.y + o.y}, l.kids[i], color);
    }
    if (l.drawFracLine) {
        float y = pos.y + l.axis;
        dl->AddLine({pos.x + 1.f, y}, {pos.x + l.size.x - 1.f, y}, color, 1.2f);
    }
}


inline ImVec2 measure(const Node* root, float pxHeight) {
    if (!root) return {0, 0};
    Laid l = layoutNode(root, pxHeight, false);
    return l.size;
}

// Draw into the foreground draw list, reserving space with a Dummy so the
// formula behaves like a regular ImGui widget in the layout flow
inline void render(const parser::Expr& expr, float pxHeight, ImU32 color) {
    if (!expr.valid() || !expr.root()) {
        ImGui::TextDisabled(u8"—");
        return;
    }
    Laid l = layoutNode(expr.root(), pxHeight, false);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    drawLaid(dl, p, l, color);
    ImGui::Dummy(l.size);
}

} // namespace mrender

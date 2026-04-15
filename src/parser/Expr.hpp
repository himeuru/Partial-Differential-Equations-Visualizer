#pragma once
// Tiny recursive-descent math parser.
// Grammar: expr = eqn ; eqn = addsub ('=' addsub)? ; addsub = term (+/-)* ;
//          term = factor ((*|/) factor | implicit-mul factor)* ;
//          factor = unary ('^' factor)? ; unary = -? primary ;
//          primary = number | ident ('(' args ')')? | '(' expr ')'.
// Known functions: sin cos tan asin acos atan atan2 sinh cosh tanh
// exp log log10 sqrt abs sign floor ceil pow min max step, plus
// display-only sum(var, lo, hi, body) and integral(var, lo, hi, body).
// Constants: pi, e.
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdlib>
#include <cctype>

namespace parser {

// Parse-time check: `ident(...)` is a call only if ident is known; otherwise
// it becomes an implicit multiplication `ident * (...)` (math-book style).
inline bool isKnownFunction(const std::string& n) {
    static const std::unordered_set<std::string> fns = {
        "sin","cos","tan","asin","acos","atan","atan2",
        "sinh","cosh","tanh",
        "exp","log","log10","sqrt","abs","sign",
        "floor","ceil","pow","min","max","step",
        "sum","integral",
        // generic display names so f(x), g(y), G(x, xi, t) render as calls
        "f","g","h","u","G"
    };
    return fns.count(n) > 0;
}

struct EvalContext {
    std::unordered_map<std::string, double> vars;
    double get(const std::string& name, double fallback = 0.0) const {
        auto it = vars.find(name);
        return it == vars.end() ? fallback : it->second;
    }
};

class Expr {
public:
    bool parse(const std::string& src) {
        root_.reset();
        err_.clear();
        if (src.empty()) { err_ = "пустое выражение"; return false; }
        Parser p{src, 0, ""};
        auto node = p.parseExpr();
        if (!p.err.empty())              { err_ = p.err; return false; }
        p.skipWs();
        if (p.pos != src.size())         { err_ = "лишние символы в позиции " + std::to_string(p.pos); return false; }
        if (!node)                       { err_ = "не удалось разобрать"; return false; }
        root_ = std::move(node);
        return true;
    }

    double eval(const EvalContext& ctx) const {
        if (!root_) return std::nan("");
        return evalNode(root_.get(), ctx);
    }

    bool               valid() const { return (bool)root_ && err_.empty(); }
    const std::string& error() const { return err_; }
    const std::string& source() const { return src_; }

    struct Node {
        enum Kind { NUM, VAR, NEG, BIN, CALL };
        Kind        kind = NUM;
        double      num  = 0.0;
        std::string name;               // VAR name or CALL function name
        char        op   = 0;           // BIN: +-*/^
        std::vector<std::unique_ptr<Node>> kids;
    };
    const Node* root() const { return root_.get(); }

private:
    std::unique_ptr<Node> root_;
    std::string           err_;
    std::string           src_;

    struct Parser {
        const std::string& src;
        size_t             pos;
        std::string        err;

        bool eof()                 { skipWs(); return pos >= src.size(); }
        char peek()                { skipWs(); return pos < src.size() ? src[pos] : '\0'; }
        void skipWs()              { while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos; }
        bool accept(char c)        { skipWs(); if (pos<src.size() && src[pos]==c) { ++pos; return true; } return false; }

        // '=' is display-only: evaluator returns rhs, MathRender shows "="
        std::unique_ptr<Node> parseExpr() {
            auto lhs = parseAddSub();
            if (err.empty() && peek() == '=') {
                ++pos;
                auto rhs = parseAddSub();
                if (!err.empty()) return nullptr;
                auto n = std::make_unique<Node>();
                n->kind = Node::BIN;
                n->op   = '=';
                n->kids.push_back(std::move(lhs));
                n->kids.push_back(std::move(rhs));
                return n;
            }
            return lhs;
        }

        std::unique_ptr<Node> parseAddSub() {
            auto lhs = parseTerm();
            while (err.empty()) {
                char c = peek();
                if (c != '+' && c != '-') break;
                ++pos;
                auto rhs = parseTerm();
                if (!err.empty()) return nullptr;
                auto n = std::make_unique<Node>();
                n->kind = Node::BIN;
                n->op   = c;
                n->kids.push_back(std::move(lhs));
                n->kids.push_back(std::move(rhs));
                lhs = std::move(n);
            }
            return lhs;
        }

        // Implicit multiplication: 2x, a(b+c), (x+1)(x-1), sin(x)cos(x).
        // Detected when the next char starts a primary: '(' or an identifier.
        // Digits are intentionally excluded ("2 3" is not 2*3).
        std::unique_ptr<Node> parseTerm() {
            auto lhs = parseFactor();
            while (err.empty()) {
                char c = peek();
                bool explicitOp = (c == '*' || c == '/');
                bool implicitMul = (c == '(' ||
                                    std::isalpha((unsigned char)c) ||
                                    c == '_');
                if (!explicitOp && !implicitMul) break;
                char op = '*';
                if (explicitOp) { op = c; ++pos; }
                auto rhs = parseFactor();
                if (!err.empty()) return nullptr;
                auto n = std::make_unique<Node>();
                n->kind = Node::BIN;
                n->op   = op;
                n->kids.push_back(std::move(lhs));
                n->kids.push_back(std::move(rhs));
                lhs = std::move(n);
            }
            return lhs;
        }

        // factor := unary ('^' factor)?   — right-assoc
        std::unique_ptr<Node> parseFactor() {
            auto lhs = parseUnary();
            if (!err.empty()) return nullptr;
            if (peek() == '^') {
                ++pos;
                auto rhs = parseFactor();
                if (!err.empty()) return nullptr;
                auto n = std::make_unique<Node>();
                n->kind = Node::BIN;
                n->op   = '^';
                n->kids.push_back(std::move(lhs));
                n->kids.push_back(std::move(rhs));
                return n;
            }
            return lhs;
        }

        // unary := ('+'|'-')? primary
        std::unique_ptr<Node> parseUnary() {
            skipWs();
            if (accept('+')) return parseUnary();
            if (accept('-')) {
                auto inner = parseUnary();
                if (!err.empty()) return nullptr;
                auto n = std::make_unique<Node>();
                n->kind = Node::NEG;
                n->kids.push_back(std::move(inner));
                return n;
            }
            return parsePrimary();
        }

        std::unique_ptr<Node> parsePrimary() {
            skipWs();
            if (pos >= src.size()) { err = "ожидалось выражение"; return nullptr; }
            char c = src[pos];

            if (c == '(') {
                ++pos;
                auto inner = parseExpr();
                if (!err.empty()) return nullptr;
                if (!accept(')')) { err = "ожидалась )"; return nullptr; }
                return inner;
            }
            if (std::isdigit((unsigned char)c) || c == '.') {
                return parseNumber();
            }
            if (std::isalpha((unsigned char)c) || c == '_') {
                return parseIdentOrCall();
            }
            err = "неожиданный символ '";
            err += c;
            err += "' в позиции " + std::to_string(pos);
            return nullptr;
        }

        std::unique_ptr<Node> parseNumber() {
            size_t start = pos;
            bool   sawDot = false, sawExp = false;
            while (pos < src.size()) {
                char c = src[pos];
                if (std::isdigit((unsigned char)c))       { ++pos; }
                else if (c == '.' && !sawDot && !sawExp)  { sawDot = true; ++pos; }
                else if ((c == 'e' || c == 'E') && !sawExp) {
                    sawExp = true; ++pos;
                    if (pos < src.size() && (src[pos]=='+' || src[pos]=='-')) ++pos;
                }
                else break;
            }
            auto n = std::make_unique<Node>();
            n->kind = Node::NUM;
            n->num  = std::strtod(src.c_str() + start, nullptr);
            return n;
        }

        std::unique_ptr<Node> parseIdentOrCall() {
            size_t start = pos;
            while (pos < src.size() &&
                   (std::isalnum((unsigned char)src[pos]) || src[pos] == '_'))
                ++pos;
            std::string name = src.substr(start, pos - start);
            skipWs();
            // Only treat `ident(...)` as a call when ident is known;
            // otherwise parseTerm will pick up the '(' as implicit mul.
            if (pos < src.size() && src[pos] == '(' && isKnownFunction(name)) {
                ++pos;
                auto n = std::make_unique<Node>();
                n->kind = Node::CALL;
                n->name = name;
                if (peek() != ')') {
                    while (true) {
                        auto arg = parseExpr();
                        if (!err.empty()) return nullptr;
                        n->kids.push_back(std::move(arg));
                        if (accept(',')) continue;
                        break;
                    }
                }
                if (!accept(')')) { err = "ожидалась )"; return nullptr; }
                return n;
            }
            auto n = std::make_unique<Node>();
            n->kind = Node::VAR;
            n->name = name;
            return n;
        }
    };

    // Evaluator
    static double evalNode(const Node* n, const EvalContext& ctx) {
        using std::pow; using std::sin; using std::cos;
        switch (n->kind) {
            case Node::NUM: return n->num;
            case Node::VAR: {
                if (n->name == "pi") return 3.14159265358979323846;
                if (n->name == "e")  return 2.71828182845904523536;
                return ctx.get(n->name, 0.0);
            }
            case Node::NEG: return -evalNode(n->kids[0].get(), ctx);
            case Node::BIN: {
                if (n->op == '=') return evalNode(n->kids[1].get(), ctx);  // display only
                double a = evalNode(n->kids[0].get(), ctx);
                double b = evalNode(n->kids[1].get(), ctx);
                switch (n->op) {
                    case '+': return a + b;
                    case '-': return a - b;
                    case '*': return a * b;
                    case '/': return b == 0.0 ? 0.0 : a / b;
                    case '^': return std::pow(a, b);
                }
                return 0.0;
            }
            case Node::CALL: {
                const auto& k = n->kids;
                auto g = [&](int i){ return evalNode(k[i].get(), ctx); };
                const std::string& f = n->name;
                if (f == "sum" && k.size() == 4) {
                    const Node* v = k[0].get();
                    if (!v || v->kind != Node::VAR) return 0.0;
                    double lo = g(1), hi = g(2);
                    if (!std::isfinite(lo) || !std::isfinite(hi)) return 0.0;
                    int ilo = int(std::floor(lo + 1e-9));
                    int ihi = int(std::floor(hi + 1e-9));
                    if (ihi - ilo > 10000) return 0.0;   // safety cap
                    EvalContext local = ctx;
                    double s = 0.0;
                    for (int i = ilo; i <= ihi; ++i) {
                        local.vars[v->name] = double(i);
                        s += evalNode(k[3].get(), local);
                    }
                    return s;
                }
                // 200-step trapezoid integration
                if (f == "integral" && k.size() == 4) {
                    const Node* v = k[0].get();
                    if (!v || v->kind != Node::VAR) return 0.0;
                    double lo = g(1), hi = g(2);
                    if (!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo) return 0.0;
                    const int M = 200;
                    double dx = (hi - lo) / M;
                    EvalContext local = ctx;
                    double s = 0.0;
                    for (int i = 0; i <= M; ++i) {
                        local.vars[v->name] = lo + i * dx;
                        double w = (i == 0 || i == M) ? 0.5 : 1.0;
                        s += w * evalNode(k[3].get(), local) * dx;
                    }
                    return s;
                }
                if (f == "sin"   && k.size()==1) return std::sin(g(0));
                if (f == "cos"   && k.size()==1) return std::cos(g(0));
                if (f == "tan"   && k.size()==1) return std::tan(g(0));
                if (f == "asin"  && k.size()==1) return std::asin(g(0));
                if (f == "acos"  && k.size()==1) return std::acos(g(0));
                if (f == "atan"  && k.size()==1) return std::atan(g(0));
                if (f == "atan2" && k.size()==2) return std::atan2(g(0), g(1));
                if (f == "sinh"  && k.size()==1) return std::sinh(g(0));
                if (f == "cosh"  && k.size()==1) return std::cosh(g(0));
                if (f == "tanh"  && k.size()==1) return std::tanh(g(0));
                if (f == "exp"   && k.size()==1) return std::exp(g(0));
                if (f == "log"   && k.size()==1) return std::log(g(0));
                if (f == "log10" && k.size()==1) return std::log10(g(0));
                if (f == "sqrt"  && k.size()==1) return std::sqrt(g(0));
                if (f == "abs"   && k.size()==1) return std::fabs(g(0));
                if (f == "sign"  && k.size()==1) { double v=g(0); return (v>0)-(v<0); }
                if (f == "floor" && k.size()==1) return std::floor(g(0));
                if (f == "ceil"  && k.size()==1) return std::ceil(g(0));
                if (f == "pow"   && k.size()==2) return std::pow(g(0), g(1));
                if (f == "min"   && k.size()==2) return std::min(g(0), g(1));
                if (f == "max"   && k.size()==2) return std::max(g(0), g(1));
                // Heaviside step: step(x) = 1 if x >= 0 else 0
                if (f == "step"  && k.size()==1) return g(0) >= 0.0 ? 1.0 : 0.0;
                return 0.0;
            }
        }
        return 0.0;
    }
};

} // namespace parser

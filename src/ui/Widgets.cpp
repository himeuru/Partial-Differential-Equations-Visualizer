#include "ui/Widgets.h"
#include "renderer/MathRender.hpp"
#include <unordered_map>
#include <cstring>
#include <cstdio>

namespace ui {

void drawTex(sf::RenderWindow& win, const sf::Texture& tex, float x, float y) {
    sf::Sprite sp(tex); sp.setPosition(x, y); win.draw(sp);
}

sf::Color harmCol(int k) {
    static const sf::Color c[] = {
        {255, 100, 100}, {100, 255, 120}, {100, 180, 255},
        {255, 210,  60}, {200,  80, 255}, { 60, 230, 200}
    };
    return c[k % 6];
}

void prettyFormula(const char* src, ImVec4 color) {
    static std::unordered_map<std::string, parser::Expr> cache;
    auto it = cache.find(src);
    if (it == cache.end()) {
        parser::Expr e;
        e.parse(src);
        it = cache.emplace(src, std::move(e)).first;
    }
    if (it->second.valid()) {
        ImGui::Indent(4.f);
        mrender::render(it->second, 19.f, ImGui::ColorConvertFloat4ToU32(color));
        ImGui::Unindent(4.f);
    } else {
        ImGui::TextColored(color, "%s", src);
    }
}

static void palBtn(const char* label, const char* templ, ICCustomExpr& e, bool sameLine) {
    if (ImGui::SmallButton(label)) {
        std::string t = templ;
        size_t mk = t.find('|');
        int offset = (int)t.size();
        if (mk != std::string::npos) { t.erase(mk, 1); offset = (int)mk; }
        e.pendingInsert        = t;
        e.pendingCursorOffset  = offset;
        e.needsFocus           = true;
    }
    if (sameLine) ImGui::SameLine();
}

bool customExprInput(const char* label, ICCustomExpr& e, const char* vars) {
    bool reparsed = false;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", e.source.c_str());
    ImGui::PushID(label);
    ImGui::TextDisabled("%s", label);

    struct PalItem { const char* label; const char* templ; };
    static const PalItem palette[] = {
        {u8"π",   "pi"},      {u8"ξ",   "xi"},      {u8"·",   "*"},
        {u8"a/b", "(|)/()"},  {u8"aⁿ",  "(|)^()"},  {u8"√",   "sqrt(|)"},
        {u8"( )", "(|)"},     {u8"sin", "sin(|)"},  {u8"cos", "cos(|)"},
        {u8"exp", "exp(|)"},  {u8"log", "log(|)"},  {u8"|x|", "abs(|)"},
        {u8"η",   "eta"},     {u8"λ",   "lambda"},
    };
    float avail   = ImGui::GetContentRegionAvail().x;
    float rowX    = 0.f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    for (int i = 0; i < (int)IM_ARRAYSIZE(palette); ++i) {
        ImVec2 ts = ImGui::CalcTextSize(palette[i].label);
        float  bw = ts.x + ImGui::GetStyle().FramePadding.x * 2.f;
        if (i > 0) {
            if (rowX + spacing + bw > avail) {
                rowX = 0.f;
            } else {
                ImGui::SameLine();
                rowX += spacing;
            }
        }
        palBtn(palette[i].label, palette[i].templ, e, false);
        rowX += bw;
    }

    if (e.needsFocus) { ImGui::SetKeyboardFocusHere(); e.needsFocus = false; }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##src", buf, sizeof(buf),
        ImGuiInputTextFlags_CallbackAlways,
        [](ImGuiInputTextCallbackData* d) -> int {
            auto* ee = (ICCustomExpr*)d->UserData;
            if (!ee->pendingInsert.empty()) {
                int p = std::max(0, std::min(ee->cursorPos, d->BufTextLen));
                d->InsertChars(p, ee->pendingInsert.c_str());
                int off = (ee->pendingCursorOffset >= 0)
                          ? ee->pendingCursorOffset
                          : (int)ee->pendingInsert.size();
                d->CursorPos       = p + off;
                d->SelectionStart  = d->CursorPos;
                d->SelectionEnd    = d->CursorPos;
                ee->cursorPos      = d->CursorPos;
                ee->pendingInsert.clear();
                ee->pendingCursorOffset = -1;
            } else {
                ee->cursorPos = d->CursorPos;
            }
            return 0;
        }, &e);

    if (std::strcmp(buf, e.source.c_str()) != 0) {
        e.source = buf;
        e.reparse();
        reparsed = true;
    }

    if (e.ok)  ImGui::TextColored({0.55f, 0.90f, 0.55f, 1.f}, u8"✓ прочитано");
    else       ImGui::TextColored({1.00f, 0.55f, 0.55f, 1.f}, u8"✗ %s", e.err.c_str());

    if (e.ok && e.expr.root()) {
        ImGui::Indent(6.f);
        mrender::render(e.expr, 19.f,
                        ImGui::ColorConvertFloat4ToU32({0.90f, 0.92f, 1.00f, 1.f}));
        ImGui::Unindent(6.f);
    }
    ImGui::TextDisabled("%s", vars);
    ImGui::PopID();
    return reparsed;
}

bool boundExprInput(const char* label, ICCustomExpr& e) {
    return customExprInput(label, e, u8"x, xi, t, a, L, pi, e");
}

// Slider with flanking +/- buttons: [slider][-][+] label
bool sliderF(const char* label, float* v, float lo, float hi, float step, const char* fmt) {
    ImGui::PushID(label);
    const float btnW   = 22.f;
    const float labelW = ImGui::CalcTextSize(label).x;
    const float full   = ImGui::GetContentRegionAvail().x;
    const float sw     = std::max(60.f, full - (btnW + 4.f) * 2.f - labelW - 12.f);
    ImGui::SetNextItemWidth(sw);
    bool changed = ImGui::SliderFloat("##s", v, lo, hi, fmt);
    ImGui::SameLine(0.f, 3.f);
    if (ImGui::Button("−", {btnW, 0})) { *v = std::max(lo, *v - step); changed = true; }
    ImGui::SameLine(0.f, 2.f);
    if (ImGui::Button("+",      {btnW, 0})) { *v = std::min(hi, *v + step); changed = true; }
    ImGui::SameLine(0.f, 8.f);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

bool sliderI(const char* label, int* v, int lo, int hi, int step) {
    ImGui::PushID(label);
    const float btnW   = 22.f;
    const float labelW = ImGui::CalcTextSize(label).x;
    const float full   = ImGui::GetContentRegionAvail().x;
    const float sw     = std::max(60.f, full - (btnW + 4.f) * 2.f - labelW - 12.f);
    ImGui::SetNextItemWidth(sw);
    bool changed = ImGui::SliderInt("##s", v, lo, hi);
    ImGui::SameLine(0.f, 3.f);
    if (ImGui::Button("−", {btnW, 0})) { *v = std::max(lo, *v - step); changed = true; }
    ImGui::SameLine(0.f, 2.f);
    if (ImGui::Button("+",      {btnW, 0})) { *v = std::min(hi, *v + step); changed = true; }
    ImGui::SameLine(0.f, 8.f);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

void axisLockCheckbox(const char* label, AxisLock& lk) {
    bool prev = lk.yLocked;
    ImGui::Checkbox(label, &lk.yLocked);
    if (lk.yLocked && !prev) lk.needCapture = true;
}

void drawStatsHUD(float px, float py, float pw, const char* title, const Stats& s) {
    ImGui::SetNextWindowPos ({px + pw - 210.f, py + 10.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({200.f, 0.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin(title, nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoTitleBar);
    ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "%s", title);
    ImGui::Separator();
    ImGui::Text(u8"max  : %+.4f", s.mx);
    ImGui::Text(u8"min  : %+.4f", s.mn);
    ImGui::Text(u8"⟨u⟩  : %+.4f", s.mean);
    ImGui::Text(u8"σ    : %.4f",  s.rms);
    ImGui::Text(u8"‖u‖₁ : %.4f",  s.l1);
    ImGui::Text(u8"‖u‖₂ : %.4f",  s.l2);
    ImGui::End();
}

void registerPlot(std::vector<PlotHit>& dst,
                  float sx, float sy, const PlotArea& pa, View* v,
                  float xlo, float xhi, float ylo, float yhi)
{
    sf::FloatRect r;
    r.left   = sx + pa.padL;
    r.top    = sy + pa.padT;
    r.width  = float(pa.width())  - pa.padL - pa.padR;
    r.height = float(pa.height()) - pa.padT - pa.padB;
    dst.push_back({r, v, xlo, xhi, ylo, yhi});
}

} // namespace ui

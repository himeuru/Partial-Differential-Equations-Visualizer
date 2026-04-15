#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include "solvers/WaveSolver.h"
#include "solvers/HeatSolver.h"
#include "solvers/GreenSolver.h"
#include "solvers/FourierViz.h"
#include "renderer/Renderer.h"
#include "renderer/MathRender.hpp"

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#endif

static int gWinW    = 1600;
static int gWinH    = 1000;
static int gPanelW    = 480;
static int gPanelMinW = 460;
static int gPanelMaxW = 1000;

inline unsigned plotW() { return unsigned(std::max(200, gWinW - gPanelW - 16)); }
inline unsigned plotH() { return unsigned(std::max(200, gWinH - 16)); }
inline unsigned halfH() { return plotH() / 2 - 4; }

enum Tab { TAB_WAVE=0, TAB_HEAT, TAB_GREEN, TAB_FOURIER };
static int gTab = TAB_WAVE;

static void drawTex(sf::RenderWindow& win, const sf::Texture& tex, float x, float y) {
    sf::Sprite sp(tex); sp.setPosition(x,y); win.draw(sp);
}

static sf::Color harmCol(int k) {
    static const sf::Color c[] = {
        {255,100,100},{100,255,120},{100,180,255},
        {255,210,60},{200,80,255},{60,230,200}
    };
    return c[k%6];
}

static void prettyFormula(const char* src, ImVec4 color = {0.75f, 0.88f, 1.f, 1.f})
{
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

static void palBtn(const char* label, const char* templ, ICCustomExpr& e, bool sameLine = true)
{
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

static bool customExprInput(const char* label, ICCustomExpr& e, const char* vars = "доступны: x, L, xi, pi, e")
{
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
    float avail    = ImGui::GetContentRegionAvail().x;
    float rowX     = 0.f;
    float spacing  = ImGui::GetStyle().ItemSpacing.x;
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

    if (e.ok)  ImGui::TextColored({0.55f,0.90f,0.55f,1.f}, u8"\u2713 прочитано");
    else       ImGui::TextColored({1.00f,0.55f,0.55f,1.f}, u8"\u2717 %s", e.err.c_str());

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

static bool boundExprInput(const char* label, ICCustomExpr& e) {
    return customExprInput(label, e, u8"x, xi, t, a, L, pi, e");
}

// Slider with flanking +/- buttons: [slider][-][+] label
static bool sliderF(const char* label, float* v, float lo, float hi, float step, const char* fmt = "%.3f")
{
    ImGui::PushID(label);
    const float btnW   = 22.f;
    const float labelW = ImGui::CalcTextSize(label).x;
    const float full   = ImGui::GetContentRegionAvail().x;
    const float sw     = std::max(60.f, full - (btnW + 4.f)*2.f - labelW - 12.f);
    ImGui::SetNextItemWidth(sw);
    bool changed = ImGui::SliderFloat("##s", v, lo, hi, fmt);
    ImGui::SameLine(0.f, 3.f);
    if (ImGui::Button("\u2212", {btnW, 0})) { *v = std::max(lo, *v - step); changed = true; }
    ImGui::SameLine(0.f, 2.f);
    if (ImGui::Button("+",      {btnW, 0})) { *v = std::min(hi, *v + step); changed = true; }
    ImGui::SameLine(0.f, 8.f);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

static bool sliderI(const char* label, int* v, int lo, int hi, int step = 1) {
    ImGui::PushID(label);
    const float btnW   = 22.f;
    const float labelW = ImGui::CalcTextSize(label).x;
    const float full   = ImGui::GetContentRegionAvail().x;
    const float sw     = std::max(60.f, full - (btnW + 4.f)*2.f - labelW - 12.f);
    ImGui::SetNextItemWidth(sw);
    bool changed = ImGui::SliderInt("##s", v, lo, hi);
    ImGui::SameLine(0.f, 3.f);
    if (ImGui::Button("\u2212", {btnW, 0})) { *v = std::max(lo, *v - step); changed = true; }
    ImGui::SameLine(0.f, 2.f);
    if (ImGui::Button("+",      {btnW, 0})) { *v = std::min(hi, *v + step); changed = true; }
    ImGui::SameLine(0.f, 8.f);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

struct AxisLock {
    bool  yLocked     = true;
    bool  needCapture = true;    
    float yLo = 0.f, yHi = 0.f;
    void apply(float& ylo, float& yhi) {
        if (!yLocked) return;
        if (needCapture) { yLo = ylo; yHi = yhi; needCapture = false; }
        else             { ylo = yLo; yhi = yHi; }
    }
};

static void axisLockCheckbox(const char* label, AxisLock& lk) {
    bool prev = lk.yLocked;
    ImGui::Checkbox(label, &lk.yLocked);
    if (lk.yLocked && !prev) lk.needCapture = true;
}

struct View {
    float panX = 0.f, panY = 0.f;   // in units of base span
    float zoom = 1.f;
    void reset() { panX = panY = 0.f; zoom = 1.f; }
    void apply(float& xlo, float& xhi, float& ylo, float& yhi) const {
        float xc0 = 0.5f*(xlo+xhi), xs0 = xhi-xlo;
        float yc0 = 0.5f*(ylo+yhi), ys0 = yhi-ylo;
        float xspan = xs0 / zoom;
        float yspan = ys0 / zoom;
        float xc = xc0 + panX * xs0;
        float yc = yc0 + panY * ys0;
        xlo = xc - xspan*0.5f; xhi = xc + xspan*0.5f;
        ylo = yc - yspan*0.5f; yhi = yc + yspan*0.5f;
    }
};

// Hovered-plot dispatch
struct PlotHit {
    sf::FloatRect rect;     
    View*         view;
    float         xBase0, xBase1, yBase0, yBase1; 
};

static std::vector<PlotHit> gPlots;

static void registerPlot(float sx, float sy, const PlotArea& pa, View* v, float xlo, float xhi, float ylo, float yhi)
{
    sf::FloatRect r;
    r.left   = sx + pa.padL;
    r.top    = sy + pa.padT;
    r.width  = float(pa.width())  - pa.padL - pa.padR;
    r.height = float(pa.height()) - pa.padT - pa.padB;
    gPlots.push_back({r, v, xlo, xhi, ylo, yhi});
}

// Compute symmetric Y range with margin for a curve
static std::pair<float,float> autoRangeSymm(const std::vector<float>& v, float margin=0.2f) {
    float mx = 0.01f;
    for (float x : v) mx = std::max(mx, std::fabs(x));
    mx *= (1.f + margin);
    return {-mx, mx};
}

static std::pair<float,float> autoRangePos(const std::vector<float>& v, float margin=0.15f) {
    float lo=1e9f, hi=-1e9f;
    for (float x : v) { lo=std::min(lo,x); hi=std::max(hi,x); }
    float span = std::max(hi-lo, 0.01f);
    return {lo - span*margin, hi + span*margin};
}

// Simple descriptive stats
struct Stats { float mn, mx, mean, rms, l1, l2; };
static Stats calcStats(const std::vector<float>& v, float dx) {
    Stats s{};
    if (v.empty()) return s;
    s.mn = v[0]; s.mx = v[0];
    double sum=0, sumAbs=0, sumSq=0;
    for (float x : v) {
        s.mn = std::min(s.mn, x);
        s.mx = std::max(s.mx, x);
        sum    += x;
        sumAbs += std::fabs(x);
        sumSq  += double(x)*x;
    }
    s.mean = float(sum / v.size());
    s.rms  = float(std::sqrt(sumSq / v.size()));
    s.l1   = float(sumAbs * dx);           // ∫|u|dx ≈
    s.l2   = float(std::sqrt(sumSq * dx)); // sqrt(∫u²dx)
    return s;
}

static void drawStatsHUD(float px, float py, float pw,
                         const char* title, const Stats& s)
{
    ImGui::SetNextWindowPos({px + pw - 210.f, py + 10.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({200.f, 0.f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin(title, nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoTitleBar);
    ImGui::TextColored({0.55f,0.85f,1.f,1.f}, "%s", title);
    ImGui::Separator();
    // ⟨u⟩ = average,  σ = RMS,  ‖u‖₁ / ‖u‖₂ = L¹ / L² norms
    ImGui::Text(u8"max  : %+.4f", s.mx);
    ImGui::Text(u8"min  : %+.4f", s.mn);
    ImGui::Text(u8"\u27e8u\u27e9  : %+.4f", s.mean);   // ⟨u⟩
    ImGui::Text(u8"\u03c3    : %.4f",  s.rms);         // σ
    ImGui::Text(u8"\u2016u\u2016\u2081 : %.4f",  s.l1);
    ImGui::Text(u8"\u2016u\u2016\u2082 : %.4f",  s.l2);
    ImGui::End();
}

// main: window, event loop, all four tabs
int main() {
    // SFML takes Latin-1 by default; wrap through fromUtf8 for Cyrillic
    const char* titleUtf8 = u8"УМФ — Визуализатор уравнений математической физики";
    sf::String  titleStr  = sf::String::fromUtf8(titleUtf8, titleUtf8 + std::strlen(titleUtf8));

    sf::VideoMode dm = sf::VideoMode::getDesktopMode();
    gWinW = std::max(1200, int(dm.width)  - 200);
    gWinH = std::max( 800, int(dm.height) - 200);
    sf::RenderWindow window(sf::VideoMode(gWinW, gWinH), titleStr, sf::Style::Default);
#ifdef _WIN32
    // Open maximized. SFML 2.5 has no portable way to do this.
    ::ShowWindow((HWND)window.getSystemHandle(), SW_MAXIMIZE);
    sf::Vector2u sz = window.getSize();
    gWinW = int(sz.x);
    gWinH = int(sz.y);
    window.setView(sf::View(sf::FloatRect(0.f, 0.f, float(gWinW), float(gWinH))));
#endif
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Everything we end up using: Latin, Cyrillic, Greek, math, arrows,
    // sub/superscripts, a few shapes and symbols.
    static const ImWchar fontRanges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin Supplement
        0x0300, 0x036F,  // Combining diacritics
        0x0370, 0x03FF,  // Greek
        0x0400, 0x04FF,  // Cyrillic
        0x2000, 0x206F,  // General punctuation (‖ — –)
        0x2070, 0x209F,  // Super/Subscripts
        0x2100, 0x214F,  // Letterlike
        0x2190, 0x21FF,  // Arrows
        0x2200, 0x22FF,  // Math operators (∫ ∞ ≈ ≤ ≥ ∂)
        0x2300, 0x23FF,  // Misc technical (⏸ ⏵)
        0x25A0, 0x25FF,  // Geometric shapes (▶ ▮ ■ ●)
        0x2600, 0x26FF,  // Misc symbols (⚠)
        0x2700, 0x27BF,  // Dingbats (✓ ✗)
        0x27C0, 0x27EF,  // Misc math A (⟨ ⟩)
        0,
    };
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;

    const char* fontCandidates[] = {
        // Windows
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        // macOS
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        nullptr
    };
    bool fontOk = false;
    for (int i = 0; fontCandidates[i]; ++i) {
        if (io.Fonts->AddFontFromFileTTF(fontCandidates[i], 17.f, &cfg, fontRanges)) {
            fontOk = true;
            // Try to merge a fallback symbol font for any missing glyphs
            ImFontConfig mcfg = cfg;
            mcfg.MergeMode = true;
            const char* symCandidates[] = {
                "C:/Windows/Fonts/seguisym.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/System/Library/Fonts/Apple Symbols.ttf",
                nullptr
            };
            for (int j = 0; symCandidates[j]; ++j)
                if (io.Fonts->AddFontFromFileTTF(symCandidates[j], 17.f, &mcfg, fontRanges))
                    break;
            break;
        }
    }
    if (!fontOk) io.Fonts->AddFontDefault();
    ImGui::SFML::UpdateFontTexture();

    // Slate-ish theme: mid-grey, blue accent
    ImGui::StyleColorsDark();
    ImGuiStyle& sty = ImGui::GetStyle();
    sty.WindowRounding    = 4.f;
    sty.FrameRounding     = 4.f;
    sty.GrabRounding      = 4.f;
    sty.TabRounding       = 4.f;
    sty.PopupRounding     = 4.f;
    sty.ScrollbarRounding = 4.f;
    sty.ScrollbarSize     = 12.f;
    sty.FramePadding      = {8, 5};
    sty.ItemSpacing       = {9, 6};
    sty.ItemInnerSpacing  = {6, 4};
    sty.WindowPadding     = {14, 12};
    sty.IndentSpacing     = 14.f;
    sty.GrabMinSize       = 12.f;
    sty.FrameBorderSize   = 0.f;
    sty.WindowBorderSize  = 0.f;
    sty.SeparatorTextPadding = {20, 3};

    ImVec4* C = sty.Colors;
    C[ImGuiCol_WindowBg]           = {0.16f, 0.17f, 0.20f, 1.00f};
    C[ImGuiCol_ChildBg]            = {0.18f, 0.20f, 0.23f, 1.00f};
    C[ImGuiCol_PopupBg]            = {0.20f, 0.22f, 0.26f, 0.98f};
    C[ImGuiCol_Border]             = {0.32f, 0.36f, 0.42f, 0.50f};
    C[ImGuiCol_FrameBg]            = {0.24f, 0.27f, 0.31f, 1.00f};
    C[ImGuiCol_FrameBgHovered]     = {0.32f, 0.36f, 0.42f, 1.00f};
    C[ImGuiCol_FrameBgActive]      = {0.38f, 0.44f, 0.52f, 1.00f};
    C[ImGuiCol_TitleBg]            = {0.18f, 0.20f, 0.24f, 1.00f};
    C[ImGuiCol_TitleBgActive]      = {0.26f, 0.32f, 0.40f, 1.00f};
    C[ImGuiCol_MenuBarBg]          = {0.20f, 0.22f, 0.26f, 1.00f};
    C[ImGuiCol_ScrollbarBg]        = {0.14f, 0.16f, 0.19f, 1.00f};
    C[ImGuiCol_ScrollbarGrab]      = {0.35f, 0.40f, 0.48f, 1.00f};
    C[ImGuiCol_ScrollbarGrabHovered]={0.45f, 0.52f, 0.62f, 1.00f};
    C[ImGuiCol_SliderGrab]         = {0.55f, 0.75f, 1.00f, 1.00f};
    C[ImGuiCol_SliderGrabActive]   = {0.70f, 0.85f, 1.00f, 1.00f};
    C[ImGuiCol_Button]             = {0.30f, 0.42f, 0.58f, 1.00f};
    C[ImGuiCol_ButtonHovered]      = {0.40f, 0.55f, 0.75f, 1.00f};
    C[ImGuiCol_ButtonActive]       = {0.50f, 0.68f, 0.90f, 1.00f};
    C[ImGuiCol_Tab]                = {0.22f, 0.26f, 0.32f, 1.00f};
    C[ImGuiCol_TabHovered]         = {0.42f, 0.55f, 0.75f, 1.00f};
    C[ImGuiCol_TabActive]          = {0.34f, 0.48f, 0.68f, 1.00f};
    C[ImGuiCol_TabUnfocused]       = {0.20f, 0.23f, 0.28f, 1.00f};
    C[ImGuiCol_TabUnfocusedActive] = {0.28f, 0.36f, 0.48f, 1.00f};
    C[ImGuiCol_Header]             = {0.30f, 0.40f, 0.55f, 0.70f};
    C[ImGuiCol_HeaderHovered]      = {0.40f, 0.55f, 0.75f, 0.80f};
    C[ImGuiCol_HeaderActive]       = {0.50f, 0.68f, 0.90f, 0.80f};
    C[ImGuiCol_Separator]          = {0.35f, 0.40f, 0.48f, 0.55f};
    C[ImGuiCol_SeparatorHovered]   = {0.50f, 0.68f, 0.90f, 0.80f};
    C[ImGuiCol_SeparatorActive]    = {0.60f, 0.78f, 1.00f, 1.00f};
    C[ImGuiCol_CheckMark]          = {0.55f, 0.85f, 1.00f, 1.00f};
    C[ImGuiCol_Text]               = {0.95f, 0.96f, 0.97f, 1.00f};
    C[ImGuiCol_TextDisabled]       = {0.62f, 0.67f, 0.74f, 1.00f};
    C[ImGuiCol_PlotHistogram]      = {0.95f, 0.75f, 0.30f, 1.00f};
    C[ImGuiCol_PlotHistogramHovered]={1.00f, 0.85f, 0.45f, 1.00f};

    // Solvers + default custom expressions
    WaveSolver  wave;
    HeatSolver  heat;
    GreenSolver green;
    FourierViz  fourier;

    wave.customIC.source    = "exp(-40*(x/L - 0.5)^2)";
    heat.customIC.source    = "exp(-40*(x/L - 0.5)^2)";
    fourier.customIC.source = "sin(pi*x/L) + 0.3*sin(3*pi*x/L)";
    wave.customIC.reparse();
    heat.customIC.reparse();
    fourier.customIC.reparse();

    // default piecewise Green: one piece [0, L] = heat kernel
    {
        GreenSolver::Piece p;
        p.xLoExpr.source = "0";
        p.xHiExpr.source = "L";
        p.expr.source    = "exp(-(x - xi)^2 / (4*a*a*t)) / sqrt(4*pi*a*a*t)";
        p.xLoExpr.reparse();
        p.xHiExpr.reparse();
        p.expr.reparse();
        green.customPieces.push_back(std::move(p));
    }

    wave.init();
    heat.init();
    green.compute();
    fourier.compute();

    PlotArea pWave, pHeat, pGreen, pFTop, pFBot;
    Heatmap  hmGreen;

    bool  wPlay=false, hPlay=false, gPlay=false;
    float wSpeed=1.f, hSpeed=0.3f, gSpeed=0.05f;
    float gTMax = 5.f;
    int   wMode = 0;   // 0 = Fourier, 1 = FD
    int   hMode = 0;   // 0 = Fourier, 1 = CN

    int   wNprev=wave.nTerms,  wIprev=(int)wave.preset;
    float wLprev=wave.L,       wAprev=wave.a;
    int   hNprev=heat.nTerms,  hIprev=(int)heat.preset;
    float hLprev=heat.L,       hAprev=heat.a;
    float hBLprev=heat.bcLeft.value, hBRprev=heat.bcRight.value;
    int   fIprev=(int)fourier.preset; float fLprev=fourier.L;

    // per-plot view state and axis locks
    View     vWave, vHeat, vGreen, vFTop, vFBot;
    AxisLock lWave, lHeat, lGreen, lFTop, lFBot;
    auto viewForTab = [&](int tab, int sub = 0) -> View* {
        switch (tab) {
            case TAB_WAVE:    return &vWave;
            case TAB_HEAT:    return &vHeat;
            case TAB_GREEN:   return &vGreen;
            case TAB_FOURIER: return sub == 0 ? &vFTop : &vFBot;
        }
        return nullptr;
    };

    bool        dragging   = false;
    sf::Vector2i dragStart;
    View*       dragView   = nullptr;
    PlotHit     dragHit{};

    bool        splitterDrag = false;

    sf::Clock clk;

    while (window.isOpen()) {
        sf::Event ev;
        while (window.pollEvent(ev)) {
            ImGui::SFML::ProcessEvent(window, ev);
            if (ev.type == sf::Event::Closed) window.close();
            if (ev.type == sf::Event::Resized) {
                gWinW = int(ev.size.width);
                gWinH = int(ev.size.height);
                // Keep view in sync with new framebuffer
                sf::FloatRect vis(0.f, 0.f, float(gWinW), float(gWinH));
                window.setView(sf::View(vis));
            }

            const ImGuiIO& io = ImGui::GetIO();

            // Panel splitter: 10-px strip on right edge
            auto splitterRect = [&]() {
                return sf::FloatRect(float(gPanelW) - 2.f, 0.f, 10.f, float(gWinH));
            };
            if (ev.type == sf::Event::MouseButtonPressed &&
                ev.mouseButton.button == sf::Mouse::Left)
            {
                sf::Vector2f mp(ev.mouseButton.x, ev.mouseButton.y);
                if (splitterRect().contains(mp)) {
                    splitterDrag = true;
                    continue;  // don't let other handlers see this click
                }
            }
            if (ev.type == sf::Event::MouseButtonReleased &&
                ev.mouseButton.button == sf::Mouse::Left)
            {
                splitterDrag = false;
            }
            if (ev.type == sf::Event::MouseMoved && splitterDrag) {
                int w = std::clamp(ev.mouseMove.x, gPanelMinW, gPanelMaxW);
                w = std::min(w, gWinW - 220);   // keep at least 220 px for plots
                gPanelW = w;
                continue;
            }

            if (io.WantCaptureMouse) continue;   // ImGui owns the panel

            // Mouse wheel zoom (focused on cursor)
            if (ev.type == sf::Event::MouseWheelScrolled) {
                sf::Vector2f mp(ev.mouseWheelScroll.x, ev.mouseWheelScroll.y);
                for (auto& h : gPlots) {
                    if (!h.rect.contains(mp)) continue;
                    float xlo=h.xBase0, xhi=h.xBase1, ylo=h.yBase0, yhi=h.yBase1;
                    h.view->apply(xlo, xhi, ylo, yhi);
                    float fx = (mp.x - h.rect.left) / h.rect.width;
                    float fy = 1.f - (mp.y - h.rect.top) / h.rect.height;
                    float dataX = xlo + fx * (xhi - xlo);
                    float dataY = ylo + fy * (yhi - ylo);

                    float factor = (ev.mouseWheelScroll.delta > 0) ? 1.25f : 1.f/1.25f;
                    float newZoom = std::clamp(h.view->zoom * factor, 1e-4f, 1e8f);

                    // keep the point under the cursor anchored
                    float xs0  = h.xBase1 - h.xBase0;
                    float ys0  = h.yBase1 - h.yBase0;
                    float nspx = xs0 / newZoom;
                    float nspy = ys0 / newZoom;
                    float ncx  = dataX - (fx - 0.5f) * nspx;
                    float ncy  = dataY - (fy - 0.5f) * nspy;
                    h.view->zoom = newZoom;
                    h.view->panX = (ncx - 0.5f*(h.xBase0+h.xBase1)) / xs0;
                    h.view->panY = (ncy - 0.5f*(h.yBase0+h.yBase1)) / ys0;
                    break;
                }
            }
            // Right-drag pan
            if (ev.type == sf::Event::MouseButtonPressed &&
                ev.mouseButton.button == sf::Mouse::Right)
            {
                sf::Vector2f mp(ev.mouseButton.x, ev.mouseButton.y);
                for (auto& h : gPlots) {
                    if (h.rect.contains(mp)) {
                        dragging  = true;
                        dragStart = {ev.mouseButton.x, ev.mouseButton.y};
                        dragView  = h.view;
                        dragHit   = h;
                        break;
                    }
                }
            }
            if (ev.type == sf::Event::MouseButtonReleased &&
                ev.mouseButton.button == sf::Mouse::Right)
            {
                dragging = false; dragView = nullptr;
            }
            if (ev.type == sf::Event::MouseMoved && dragging && dragView) {
                float dx = float(ev.mouseMove.x - dragStart.x);
                float dy = float(ev.mouseMove.y - dragStart.y);
                dragStart = {ev.mouseMove.x, ev.mouseMove.y};
                dragView->panX -= dx / dragHit.rect.width  / dragView->zoom;
                dragView->panY += dy / dragHit.rect.height / dragView->zoom;
            }
            // Double-click resets view
            if (ev.type == sf::Event::MouseButtonPressed &&
                ev.mouseButton.button == sf::Mouse::Left)
            {
                static sf::Clock dblClk;
                static sf::Vector2i lastPos{-999,-999};
                sf::Vector2i p{ev.mouseButton.x, ev.mouseButton.y};
                if (dblClk.getElapsedTime().asMilliseconds() < 350 &&
                    std::abs(p.x - lastPos.x) < 5 && std::abs(p.y - lastPos.y) < 5)
                {
                    sf::Vector2f mp(float(p.x), float(p.y));
                    for (auto& h : gPlots)
                        if (h.rect.contains(mp)) { h.view->reset(); break; }
                }
                dblClk.restart();
                lastPos = p;
            }
        }
        // Rebuild plot registry each frame (after rendering)
        gPlots.clear();
        sf::Time dt_sf = clk.restart();
        float dt = dt_sf.asSeconds();
        ImGui::SFML::Update(window, dt_sf);

        // Reinit FD grids whenever params change (CFL depends on dx/a)
        if (wNprev != wave.nTerms || wIprev != (int)wave.preset ||
            wLprev != wave.L      || wAprev != wave.a)
        {
            wave.init();
            wNprev = wave.nTerms; wIprev = (int)wave.preset;
            wLprev = wave.L;      wAprev = wave.a;
        }
        if (hNprev  != heat.nTerms         || hIprev != (int)heat.preset ||
            hLprev  != heat.L              || hAprev != heat.a           ||
            hBLprev != heat.bcLeft.value   || hBRprev != heat.bcRight.value)
        {
            heat.init();
            hNprev  = heat.nTerms;        hIprev  = (int)heat.preset;
            hLprev  = heat.L;             hAprev  = heat.a;
            hBLprev = heat.bcLeft.value;  hBRprev = heat.bcRight.value;
        }
        if (fIprev != (int)fourier.preset || fLprev != fourier.L)
            { fourier.compute(); fIprev = (int)fourier.preset; fLprev = fourier.L; }

        // Mode switch = clean slate: new coefficients, fresh grid, t=0
        static int wModePrev = 0, hModePrev = 0;
        if (wMode != wModePrev) {
            wave.init(); wPlay = false;
            vWave.reset();
            lWave.yLocked = true; lWave.needCapture = true;
        }
        if (hMode != hModePrev) {
            heat.init(); hPlay = false;
            vHeat.reset();
            lHeat.yLocked = true; lHeat.needCapture = true;
        }
        wModePrev = wMode; hModePrev = hMode;

        // Pause auto-playback on tab switch — otherwise t drifts in the background
        static int prevTab = -1;
        if (gTab != prevTab) {
            wPlay = hPlay = gPlay = false;
            prevTab = gTab;
        }

        // Advance simulations only while their tab is visible
        if (wPlay && gTab == TAB_WAVE) {
            float wDt = dt * wSpeed;
            if (wMode == 1) wave.stepFD(wDt);
            else            wave.time += wDt;
        }
        if (hPlay && gTab == TAB_HEAT) {
            float hDt = dt * hSpeed;
            if (hMode == 1) heat.stepCN(hDt);
            else            heat.time += hDt;
        }
        // Green mode change resets t and pauses playback
        static GreenMode gModePrev = green.mode;
        if (green.mode != gModePrev) {
            gPlay   = false;
            green.t = 0.01f;
            vGreen.reset();
            lGreen.yLocked = true; lGreen.needCapture = true;
            gModePrev = green.mode;
        }

        if (gPlay && gTab == TAB_GREEN) {
            green.t += dt * gSpeed;
            if (green.t > gTMax)  green.t = 0.001f;
            if (green.t < 0.001f) green.t = 0.001f;
        }
        // Recompute every frame so xi/t/a sliders feel live
        if (gTab == TAB_GREEN) green.compute();

        // Left panel
        float px0 = float(gPanelW) + 8.f;
        float py0 = 8.f;

        ImGui::SetNextWindowPos ({0.f, 0.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({float(gPanelW), float(gWinH)}, ImGuiCond_Always);
        ImGui::Begin("##ctrl", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {

            // Wave tab
            if (ImGui::BeginTabItem(u8"Волновое")) {
                gTab = TAB_WAVE;
                ImGui::Spacing();
                ImGui::TextColored({0.6f,0.85f,1.f,1.f}, u8"Волновое уравнение:");
                prettyFormula("u_tt = a^2 * u_xx", {0.6f,0.85f,1.f,1.f});
                ImGui::TextDisabled(u8"Аналитическое решение (ряд по собственным модам):");
                prettyFormula(
                    "u(x,t) = sum(n, 1, N, A_n * cos(n*pi*a*t/L) * sin(n*pi*x/L))");
                ImGui::Separator(); ImGui::Spacing();

                sliderF(u8"Скорость a",    &wave.a,      0.1f, 5.f, 0.1f,  "%.2f");
                sliderF(u8"Длина     L",   &wave.L,      0.5f, 3.f, 0.1f,  "%.2f");
                sliderI(u8"Членов ряда N", &wave.nTerms, 1,    60,  1);
                ImGui::Spacing();
                { int p=(int)wave.preset;
                  ImGui::SetNextItemWidth(-1);
                  if (ImGui::Combo("##ic_w",&p,ICPresetNames,IM_ARRAYSIZE(ICPresetNames)))
                      wave.preset=(ICPreset)p;
                  ImGui::TextDisabled(u8"  Начальное условие f(x)"); }
                if (wave.preset == ICPreset::Custom) {
                    if (customExprInput(u8"Ваше f(x):", wave.customIC))
                        wave.init();
                }
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::TextDisabled(u8"Метод решения:");
                ImGui::RadioButton(u8"Ряд Фурье (аналит.)", &wMode, 0);
                ImGui::RadioButton(u8"МКР — явная схема",   &wMode, 1);
                ImGui::Spacing();

                sliderF(u8"Скорость воспр.", &wSpeed, 0.05f, 5.f, 0.1f, "x%.2f");
                ImGui::Spacing();
                if (ImGui::Button(wPlay ? u8"\u23f8 Пауза " : u8"\u25b6 Пуск  ", {120,0}))
                    wPlay = !wPlay;
                ImGui::SameLine();
                if (ImGui::Button(u8"\u21ba Сброс", {80,0})) { wave.init(); wPlay=false; }
                ImGui::SameLine();
                if (ImGui::Button(u8"Сброс вида", {110,0})) { vWave.reset(); lWave.yLocked=false; }
                axisLockCheckbox(u8"Зафиксировать ось Y", lWave);
                ImGui::Spacing();
                ImGui::Text(u8"t = %.4f с    zoom x%.2f", wave.time, vWave.zoom);
                ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                ImGui::TextWrapped(
                    u8"An = (2/L) \u222b f(x) sin(n\u03c0x/L) dx\n\n"
                    u8"МКР — явная центральная схема 2-го порядка;\n"
                    u8"условие устойчивости Куранта: r = a\u00b7\u0394t/\u0394x \u2264 1.\n"
                    u8"Под-шаг выбирается автоматически (r = 0.9).");
                ImGui::EndTabItem();
            }

            // Heat tab
            if (ImGui::BeginTabItem(u8"Теплопр.")) {
                gTab = TAB_HEAT;
                ImGui::Spacing();
                ImGui::TextColored({0.6f,1.f,0.75f,1.f}, u8"Уравнение теплопроводности:");
                prettyFormula("u_t = a^2 * u_xx", {0.6f,1.f,0.75f,1.f});
                ImGui::TextDisabled(u8"Аналитическое решение (собственные моды затухают):");
                prettyFormula(
                    "u(x,t) = sum(n, 1, N, B_n * exp(-a^2*(n*pi/L)^2*t) * sin(n*pi*x/L))");
                ImGui::Separator(); ImGui::Spacing();

                sliderF(u8"Диффузия  a", &heat.a,      0.1f, 3.f, 0.1f, "%.2f");
                sliderF(u8"Длина     L", &heat.L,      0.5f, 3.f, 0.1f, "%.2f");
                sliderI(u8"Членов ряда N", &heat.nTerms, 1,   80, 1);
                ImGui::Spacing();
                { int p=(int)heat.preset;
                  ImGui::SetNextItemWidth(-1);
                  if (ImGui::Combo("##ic_h",&p,ICPresetNames,IM_ARRAYSIZE(ICPresetNames)))
                      heat.preset=(ICPreset)p;
                  ImGui::TextDisabled(u8"  Начальное условие f(x)"); }
                if (heat.preset == ICPreset::Custom) {
                    if (customExprInput(u8"Ваше f(x):", heat.customIC))
                        heat.init();
                }
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::TextDisabled(u8"Граничные условия Дирихле:");
                sliderF("u(0)", &heat.bcLeft.value,  -1.f, 1.f, 0.05f, "%.3f");
                sliderF("u(L)", &heat.bcRight.value, -1.f, 1.f, 0.05f, "%.3f");
                ImGui::Spacing();

                ImGui::TextDisabled(u8"Метод решения:");
                ImGui::RadioButton(u8"Ряд Фурье (аналит.)", &hMode, 0);
                ImGui::RadioButton(u8"Кранк – Николсон",   &hMode, 1);
                ImGui::Spacing();

                sliderF(u8"Скорость сим.", &hSpeed, 0.001f, 2.f, 0.05f, "%.3f");
                ImGui::Spacing();
                if (ImGui::Button(hPlay ? u8"\u23f8 Пауза " : u8"\u25b6 Пуск  ", {120,0}))
                    hPlay = !hPlay;
                ImGui::SameLine();
                if (ImGui::Button(u8"\u21ba Сброс", {80,0})) { heat.init(); hPlay=false; }
                ImGui::SameLine();
                if (ImGui::Button(u8"Сброс вида", {110,0})) { vHeat.reset(); lHeat.yLocked=false; }
                axisLockCheckbox(u8"Зафиксировать ось Y", lHeat);
                ImGui::Spacing();
                ImGui::Text(u8"t = %.4f с    zoom x%.2f", heat.time, vHeat.zoom);
                ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                ImGui::TextWrapped(
                    u8"Мода n затухает как exp(-a\u00b2\u03bb\u00b2n t).\n"
                    u8"Высокие гармоники исчезают первыми — сглаживание сигнала.\n"
                    u8"Схема Кранка–Николсона безусловно устойчива.");
                ImGui::EndTabItem();
            }

            // Green tab
            if (ImGui::BeginTabItem(u8"Грина")) {
                gTab = TAB_GREEN;
                ImGui::Spacing();
                ImGui::TextColored({0.85f,0.7f,1.f,1.f}, u8"Функции Грина");
                ImGui::Separator(); ImGui::Spacing();

                { int gm=(int)green.mode;
                  ImGui::SetNextItemWidth(-1);
                  if (ImGui::Combo("##gm",&gm,GreenModeNames,IM_ARRAYSIZE(GreenModeNames)))
                      green.mode=(GreenMode)gm;
                }
                ImGui::Spacing();

                switch (green.mode) {
                case GreenMode::Heat1D:
                    prettyFormula(
                        "G(x, xi, t) = 1/sqrt(4*pi*a^2*t) * exp(-(x-xi)^2/(4*a^2*t))");
                    ImGui::Spacing();
                    sliderF(u8"Длина    L",  &green.L,  0.5f, 2.f, 0.1f, "%.2f");
                    if (green.xi > green.L) green.xi = green.L;
                    sliderF(u8"Источник \u03be", &green.xi, 0.f, green.L, 0.02f, "%.3f");
                    sliderF(u8"Время    t",  &green.t,  0.001f, gTMax, 0.005f, "%.4f");
                    sliderF(u8"Скорость a",  &green.a,  0.1f, 2.f, 0.1f, "%.2f");
                    ImGui::Spacing();
                    if (ImGui::Button(gPlay ? u8"\u23f8 Стоп авто-t" : u8"\u25b6 Авто-t")) gPlay=!gPlay;
                    sliderF(u8"Скорость t", &gSpeed, 0.001f, 2.f, 0.05f, "%.3f");
                    sliderF(u8"t_max",      &gTMax,  0.1f, 50.f, 0.5f, "%.2f");
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        u8"Фундаментальное решение уравнения теплопроводности.\n"
                        u8"При t\u21920+: G \u2192 \u03b4(x \u2212 \u03be).\n"
                        u8"При t\u2192\u221e: G расширяется и уплощается.");
                    break;

                case GreenMode::Wave1D:
                    prettyFormula("1/(2*a)");
                    ImGui::TextColored({0.7f,0.9f,1.f,1.f},
                        u8"Внутри светового конуса  |x\u2212\u03be| \u2264 at");
                    ImGui::Spacing();
                    sliderF(u8"Длина    L", &green.L, 0.5f, 3.f, 0.1f, "%.2f");
                    green.xi = std::clamp(green.xi, 0.f, green.L);
                    sliderF(u8"Источник \u03be", &green.xi, 0.f, green.L, 0.02f, "%.3f");
                    sliderF(u8"Время    t",  &green.t,  0.001f, gTMax, 0.02f, "%.3f");
                    sliderF(u8"Скорость a",  &green.a,  0.1f, 3.f, 0.1f, "%.2f");
                    ImGui::Spacing();
                    if (ImGui::Button(gPlay ? u8"\u23f8 Стоп авто-t" : u8"\u25b6 Авто-t")) gPlay=!gPlay;
                    sliderF(u8"Скорость t", &gSpeed, 0.001f, 2.f, 0.05f, "%.3f");
                    sliderF(u8"t_max",      &gTMax,  0.1f, 50.f, 0.5f, "%.2f");
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        u8"Сечение конуса Маха (характеристический конус).\n"
                        u8"Возмущение распространяется со скоростью a в обе стороны.\n"
                        u8"Принцип Гюйгенса в 1D.");
                    break;

                case GreenMode::SturmLiouville:
                    ImGui::TextColored({0.7f,0.9f,1.f,1.f}, u8"G(x, \u03be) =");
                    prettyFormula(
                        "sum(n, 1, N, (2/L) * sin(n*pi*x/L) * sin(n*pi*xi/L) / (n*pi/L)^2)");
                    ImGui::Spacing();
                    sliderF(u8"Длина    L",  &green.L,      0.5f, 3.f, 0.1f, "%.2f");
                    green.xi = std::clamp(green.xi, 0.f, green.L);
                    sliderF(u8"Источник \u03be",  &green.xi, 0.f, green.L, 0.02f, "%.3f");
                    sliderI(u8"Членов   N",  &green.nTerms, 5,    200, 5);
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        u8"Задача: -u'' = f(x),  u(0)=u(L)=0.\n"
                        u8"G(x,\u03be) — отклик на точечный источник \u03b4(x\u2212\u03be).\n"
                        u8"Явление Гиббса вблизи \u03be при малом N.");
                    break;

                case GreenMode::Custom1D: {
                    ImGui::TextColored({0.7f,0.9f,1.f,1.f},
                        u8"G(x, \u03be, t) — кусочная функция");
                    ImGui::Spacing();
                    sliderF(u8"Длина    L",  &green.L,  0.1f, 10.f, 0.1f, "%.3f");
                    if (ImGui::SmallButton(u8"L = π"))    green.L = 3.14159265f;
                    ImGui::SameLine();
                    if (ImGui::SmallButton(u8"L = 2π"))  green.L = 6.28318531f;
                    ImGui::SameLine();
                    if (ImGui::SmallButton(u8"L = 1"))   green.L = 1.f;
                    if (green.xi > green.L) green.xi = green.L;
                    sliderF(u8"Источник \u03be", &green.xi, 0.f, green.L, 0.02f, "%.3f");
                    sliderF(u8"Время    t",  &green.t,  0.001f, gTMax, 0.02f, "%.4f");
                    sliderF(u8"Скорость a",  &green.a,  0.1f, 3.f, 0.1f, "%.2f");
                    ImGui::Spacing();
                    if (ImGui::Button(gPlay ? u8"\u23f8 Стоп авто-t" : u8"\u25b6 Авто-t")) gPlay=!gPlay;
                    sliderF(u8"Скорость t", &gSpeed, 0.001f, 2.f, 0.05f, "%.3f");
                    sliderF(u8"t_max",      &gTMax,  0.1f, 50.f, 0.5f, "%.2f");
                    ImGui::Separator();

                    // Diagnostics for piecewise input
                    if (green.customTotalCount == 0) {
                        ImGui::TextColored({1.f,0.55f,0.55f,1.f},
                            u8"\u26a0 Ни одна точка не попадает в заданные "
                            u8"куски. Проверь границы — домены не покрывают [0, L]?");
                    } else if (green.customNanCount > 0) {
                        float frac = 100.f * float(green.customNanCount)
                                            / float(std::max(1, green.customTotalCount));
                        ImGui::TextColored({1.f,0.75f,0.35f,1.f},
                            u8"\u26a0 Формула не определена в %d из %d точек "
                            u8"(%.0f%%). Возможно деление на 0 / log(\u22640) / "
                            u8"sqrt отрицательного.",
                            green.customNanCount, green.customTotalCount, frac);
                        ImGui::TextDisabled(
                            u8"Типичная причина: целое значение a даёт sin(a\u00b7\u03c0)=0 "
                            u8"в знаменателе. Сдвинь a на 0.1.");
                    } else {
                        int total = green.N + 1;
                        float missFrac = float(green.customMissCount) / float(total);
                        // Ignore 1-2 points of slack from FP rounding at the edges
                        if (missFrac > 0.05f) {
                            ImGui::TextColored({0.85f,0.85f,0.55f,1.f},
                                u8"i %.0f%% отрезка не покрыто кусками — там G = 0.",
                                missFrac * 100.f);
                        }
                    }
                    ImGui::Spacing();
                    ImGui::TextDisabled(u8"Куски (проверяются по порядку):");
                    int pieceToDelete = -1;
                    for (int i = 0; i < (int)green.customPieces.size(); ++i) {
                        auto& p = green.customPieces[i];
                        ImGui::PushID(i);
                        ImGui::Separator();
                        ImGui::Text(u8"Кусок %d", i + 1);
                        ImGui::SameLine();
                        if (ImGui::SmallButton(u8"\u2212 удалить")) pieceToDelete = i;
                        boundExprInput(u8"Нижняя граница  x \u2265", p.xLoExpr);
                        boundExprInput(u8"Верхняя граница x \u2264", p.xHiExpr);
                        customExprInput(u8"Выражение G(x, \u03be, t) =", p.expr,
                                        u8"x, xi, t, a, L, pi, e");
                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    if (pieceToDelete >= 0 && green.customPieces.size() > 1)
                        green.customPieces.erase(green.customPieces.begin() + pieceToDelete);
                    if (ImGui::Button(u8"+ добавить кусок")) {
                        GreenSolver::Piece np;
                        // new piece starts where the last one ended
                        std::string prevHi = green.customPieces.empty()
                                             ? std::string("0")
                                             : green.customPieces.back().xHiExpr.source;
                        np.xLoExpr.source = prevHi;
                        np.xHiExpr.source = "L";
                        np.expr.source    = "0";
                        np.xLoExpr.reparse();
                        np.xHiExpr.reparse();
                        np.expr.reparse();
                        green.customPieces.push_back(std::move(np));
                    }
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        u8"Первый кусок, чей диапазон [xLo, xHi] содержит x, будет "
                        u8"использован. Вне всех диапазонов — 0.\n"
                        u8"Функции: sin cos tan exp log sqrt abs pow min max step.");
                    break;
                }

                case GreenMode::Laplace2D:
                    ImGui::TextColored({0.7f,0.9f,1.f,1.f},
                        u8"G(r) =   где  r = |(x,y) \u2212 (\u03be, \u03b7)|");
                    prettyFormula("(1/(2*pi)) * log(1/r)");
                    ImGui::Spacing();
                    sliderF(u8"Область  L",  &green.L,   0.1f, 5.f, 0.1f, "%.2f");
                    green.xi  = std::clamp(green.xi,  0.f, green.L);
                    green.eta = std::clamp(green.eta, 0.f, green.L);
                    sliderF(u8"Источник \u03be",  &green.xi,  0.f, green.L, 0.02f, "%.3f");
                    sliderF(u8"Источник \u03b7",  &green.eta, 0.f, green.L, 0.02f, "%.3f");
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        u8"Фундаментальное решение уравнения Лапласа.\n"
                        u8"Эквивалентно потенциалу 2D точечного заряда.\n"
                        u8"Логарифмическая особенность в точке (\u03be, \u03b7).");
                    break;
                }

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (ImGui::Button(u8"Пересчитать", {120,0})) green.compute();
                ImGui::SameLine();
                if (ImGui::Button(u8"Сброс вида", {110,0})) { vGreen.reset(); lGreen.yLocked=false; }
                axisLockCheckbox(u8"Зафиксировать ось Y", lGreen);
                ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
                ImGui::EndTabItem();
            }

            // Fourier tab
            if (ImGui::BeginTabItem(u8"Фурье")) {
                gTab = TAB_FOURIER;
                ImGui::Spacing();
                ImGui::TextColored({1.f,0.85f,0.4f,1.f},
                    u8"Частичная сумма ряда Фурье по синусам:");
                prettyFormula(
                    "S_N(x) = sum(n, 1, N, b_n * sin(n*pi*x/L))",
                    {1.f, 0.9f, 0.6f, 1.f});
                ImGui::TextDisabled(u8"Коэффициенты:");
                prettyFormula(
                    "b_n = (2/L) * integral(x, 0, L, f(x) * sin(n*pi*x/L))");
                ImGui::Separator(); ImGui::Spacing();

                { int p=(int)fourier.preset;
                  ImGui::SetNextItemWidth(-1);
                  if (ImGui::Combo("##ic_f",&p,ICPresetNames,IM_ARRAYSIZE(ICPresetNames)))
                      fourier.preset=(ICPreset)p;
                  ImGui::TextDisabled(u8"  Функция f(x)"); }
                if (fourier.preset == ICPreset::Custom) {
                    if (customExprInput(u8"Ваше f(x):", fourier.customIC))
                        fourier.compute();
                }
                sliderF(u8"Длина  L", &fourier.L, 0.5f, 3.f, 0.1f, "%.2f");
                ImGui::Spacing();

                ImGui::Text(u8"Частичная сумма S_N(x):");
                if (sliderI(u8"Членов N", &fourier.nShow, 1, fourier.nMax, 1))
                    fourier.compute();
                ImGui::Spacing();
                ImGui::Checkbox(u8"Отдельные гармоники", &fourier.showHarmonics);
                ImGui::Checkbox(u8"График ошибки  |f \u2212 S_N|", &fourier.showError);
                ImGui::Spacing();
                if (ImGui::Button(u8"Пересчитать", {120,0})) fourier.compute();
                ImGui::SameLine();
                if (ImGui::Button(u8"Сброс вида", {110,0})) { vFTop.reset(); vFBot.reset(); }
                ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // Error metrics
                if (!fourier.error.empty()) {
                    double e2sum = 0.0, emax = 0.0;
                    for (float e : fourier.error) {
                        e2sum += double(e)*e;
                        if (e > emax) emax = e;
                    }
                    double l2 = std::sqrt(e2sum * (fourier.L / fourier.error.size()));
                    ImGui::Text(u8"Ошибки при N = %d:", fourier.nShow);
                    ImGui::BulletText(u8"max |f \u2212 S_N|  =  %.4f", emax);
                    ImGui::BulletText(u8"\u2016 f \u2212 S_N \u2016\u2082  =  %.4f", l2);
                    ImGui::Spacing();
                }

                // Spectrum |bn|
                if (!fourier.coeffs.empty() &&
                    ImGui::CollapsingHeader(u8"Спектр коэффициентов |bn|",
                                            ImGuiTreeNodeFlags_DefaultOpen))
                {
                    std::vector<float> bars(fourier.nMax);
                    for (int i=0;i<fourier.nMax;++i) bars[i]=float(std::fabs(fourier.coeffs[i]));
                    float bmax = *std::max_element(bars.begin(),bars.end());
                    ImGui::PlotHistogram("##sp", bars.data(), fourier.nMax,
                                         0, nullptr, 0.f, bmax, {-1,100});
                    ImGui::TextDisabled(u8"n = 1 .. %d  (по оси X — номер гармоники)",
                                        fourier.nMax);
                }

                // Table of first 12 coefficients
                if (ImGui::CollapsingHeader(u8"Таблица коэффициентов")) {
                    if (ImGui::BeginTable("##cf", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchSame))
                    {
                        ImGui::TableSetupColumn("n");
                        ImGui::TableSetupColumn(u8"bn");
                        ImGui::TableHeadersRow();
                        int rows = std::min(12, (int)fourier.coeffs.size());
                        for (int i = 0; i < rows; ++i) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i+1);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%+ .5f", fourier.coeffs[i]);
                        }
                        ImGui::EndTable();
                    }
                }

                // Legend
                if (ImGui::CollapsingHeader(u8"Легенда и пояснение",
                                            ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextColored({1.f,0.87f,0.25f,1.f},
                        u8"\u25cf  Жёлтый   — точная функция f(x)");
                    ImGui::TextColored({0.35f,0.75f,1.f,1.f},
                        u8"\u25cf  Голубой  — частичная сумма S_N(x)");
                    ImGui::TextColored({1.f,0.45f,0.45f,1.f},
                        u8"\u25cf  Красный  — график ошибки |f \u2212 S_N|");
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        u8"Ряд Фурье по синусам строится в предположении "
                        u8"f(0)=f(L)=0 (условие Дирихле). При гладкой f(x) "
                        u8"сходимость равномерная, коэффициенты убывают как 1/n^k.\n\n"
                        u8"Для разрывных функций (ступенька) вблизи скачка всегда "
                        u8"остаётся выброс ~9% — явление Гиббса. "
                        u8"С ростом N выброс не исчезает, только сужается.");
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();

        // Render the active tab
        window.clear(sf::Color(24, 26, 31));

        // Splitter bar between panel and plots
        {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            bool hot = splitterDrag ||
                       (mp.x >= gPanelW - 2 && mp.x <= gPanelW + 8 &&
                        mp.y >= 0 && mp.y <= gWinH);
            sf::Color col = splitterDrag ? sf::Color(130, 180, 255, 240)
                          : hot          ? sf::Color(100, 140, 200, 220)
                                         : sf::Color( 70,  85, 110, 200);
            sf::RectangleShape sep({hot ? 4.f : 2.f, float(gWinH)});
            sep.setPosition(float(gPanelW) + (hot ? 2.f : 3.f), 0.f);
            sep.setFillColor(col);
            window.draw(sep);
        }

        unsigned PW = plotW(), PH = plotH(), HH = halfH();

        switch (gTab) {

        case TAB_WAVE: {
            std::vector<float> y;
            if (wMode == 1) wave.getFDProfile(y);
            else            wave.getFourierProfile(wave.time, y);

            auto [ylo, yhi] = autoRangeSymm(y);
            lWave.apply(ylo, yhi);
            float xBase0 = 0.f, xBase1 = wave.L, yBase0 = ylo, yBase1 = yhi;
            float xlo=xBase0, xhi=xBase1, ylo2=yBase0, yhi2=yBase1;
            vWave.apply(xlo, xhi, ylo2, yhi2);

            pWave.setup(PW, PH, xlo, xhi, ylo2, yhi2);
            pWave.xLabel = u8"x"; pWave.yLabel = u8"u(x, t)";
            pWave.beginFrame(); pWave.drawAxes();
            PlotStyle s;
            if (wMode == 1) { s.lineColor={255,170,60,230};  s.fillColor={255,170,60,28}; }
            else            { s.lineColor={90,200,255,235};  s.fillColor={90,200,255,28}; }
            s.filled = true;
            pWave.drawCurve(y, 0.f, wave.L, s);
            pWave.endFrame();
            drawTex(window, pWave.texture(), px0, py0);
            registerPlot(px0, py0, pWave, &vWave, xBase0, xBase1, yBase0, yBase1);
            {
                float dx = wave.L / std::max(1,(int)y.size()-1);
                Stats st = calcStats(y, dx);
                drawStatsHUD(px0, py0, float(PW),
                    wMode==1 ? u8"Волна — МКР" : u8"Волна — Фурье", st);
            }
            break;
        }

        case TAB_HEAT: {
            std::vector<float> y;
            if (hMode == 1) heat.getCNProfile(y);
            else            heat.getFourierProfile(heat.time, y);

            auto [ylo, yhi] = autoRangePos(y);
            lHeat.apply(ylo, yhi);
            float xBase0 = 0.f, xBase1 = heat.L, yBase0 = ylo, yBase1 = yhi;
            float xlo=xBase0, xhi=xBase1, ylo2=yBase0, yhi2=yBase1;
            vHeat.apply(xlo, xhi, ylo2, yhi2);
            pHeat.setup(PW, PH, xlo, xhi, ylo2, yhi2);
            pHeat.xLabel = u8"x"; pHeat.yLabel = u8"u(x, t)";
            pHeat.beginFrame(); pHeat.drawAxes();
            PlotStyle s;
            if (hMode == 1) { s.lineColor={255,170,60,230};  s.fillColor={255,170,60,28}; }
            else            { s.lineColor={110,230,150,235}; s.fillColor={110,230,150,28}; }
            s.filled = true;
            pHeat.drawCurve(y, 0.f, heat.L, s);
            pHeat.endFrame();
            drawTex(window, pHeat.texture(), px0, py0);
            registerPlot(px0, py0, pHeat, &vHeat, xBase0, xBase1, yBase0, yBase1);
            {
                float dx = heat.L / std::max(1,(int)y.size()-1);
                Stats st = calcStats(y, dx);
                drawStatsHUD(px0, py0, float(PW),
                    hMode==1 ? u8"Тепло — КН" : u8"Тепло — Фурье", st);
            }
            break;
        }

        case TAB_GREEN: {
            if (green.mode == GreenMode::Laplace2D) {
                hmGreen.setup(PW, PH);
                hmGreen.update(green.g2d, green.g2dW, green.g2dH, green.gMin, green.gMax);
                sf::Sprite sp(hmGreen.texture()); sp.setPosition(px0, py0); window.draw(sp);
            } else {
                float xMin = (green.mode==GreenMode::Wave1D)
                             ? green.xi - 2.f*std::max(green.a*green.t, green.L*0.5f)
                             : 0.f;
                float xMax = (green.mode==GreenMode::Wave1D)
                             ? green.xi + 2.f*std::max(green.a*green.t, green.L*0.5f)
                             : green.L;

                auto [ylo,yhi] = autoRangePos(green.g1d);
                lGreen.apply(ylo, yhi);
                float xBase0=xMin, xBase1=xMax, yBase0=ylo, yBase1=yhi;
                float xlo=xBase0, xhi=xBase1, ylo2=yBase0, yhi2=yBase1;
                vGreen.apply(xlo, xhi, ylo2, yhi2);
                pGreen.setup(PW, PH, xlo, xhi, ylo2, yhi2);
                pGreen.xLabel = u8"x"; pGreen.yLabel = u8"G(x, \u03be, t)";
                pGreen.beginFrame(); pGreen.drawAxes();
                pGreen.drawVLine(green.xi, {255,210,60,200});
                PlotStyle s; s.lineColor={185,100,255,230};
                s.filled=true; s.fillColor={185,100,255,35};
                pGreen.drawCurve(green.g1d, xMin, xMax, s);
                pGreen.endFrame();
                drawTex(window, pGreen.texture(), px0, py0);
                registerPlot(px0, py0, pGreen, &vGreen, xBase0, xBase1, yBase0, yBase1);
                {
                    float dx = (xMax-xMin) / std::max(1,(int)green.g1d.size()-1);
                    Stats s = calcStats(green.g1d, dx);
                    drawStatsHUD(px0, py0, float(PW), u8"Грина — статистика", s);
                }
            }
            break;
        }

        case TAB_FOURIER: {
            // Top panel: f(x), partial sum, optional harmonics
            float fxB0=0.f, fxB1=fourier.L, fyB0=0.f, fyB1=0.f;
            {
                auto [ylo,yhi] = autoRangeSymm(fourier.exact, 0.25f);
                fyB0 = ylo; fyB1 = yhi;
                float xlo=fxB0, xhi=fxB1, ylo2=fyB0, yhi2=fyB1;
                vFTop.apply(xlo, xhi, ylo2, yhi2);
                pFTop.setup(PW, HH, xlo, xhi, ylo2, yhi2);
                pFTop.xLabel = u8"x"; pFTop.yLabel = u8"f(x), S_N(x)";
                pFTop.beginFrame(); pFTop.drawAxes();

                if (fourier.showHarmonics) {
                    int Nh = std::min(fourier.nShow, (int)fourier.harmonics.size());
                    for (int k = 0; k < Nh; ++k) {
                        PlotStyle sh;
                        sh.lineColor = harmCol(k);
                        sh.lineColor.a = 180;
                        pFTop.drawCurve(fourier.harmonics[k], 0.f, fourier.L, sh);
                    }
                }
                { PlotStyle s; s.lineColor={255,220,55,220};
                  pFTop.drawCurve(fourier.exact, 0.f, fourier.L, s); }
                { PlotStyle s; s.lineColor={80,200,255,220}; s.lineWidth=2.f;
                  pFTop.drawCurve(fourier.partial, 0.f, fourier.L, s); }
                pFTop.endFrame();
                drawTex(window, pFTop.texture(), px0, py0);
                registerPlot(px0, py0, pFTop, &vFTop, fxB0, fxB1, fyB0, fyB1);
            }
            // Bottom panel: pointwise error
            if (fourier.showError) {
                auto [ylo,yhi] = autoRangePos(fourier.error, 0.2f);
                ylo = std::min(ylo, 0.f);
                float bxB0=0.f, bxB1=fourier.L, byB0=ylo, byB1=yhi;
                float xlo=bxB0, xhi=bxB1, ylo2=byB0, yhi2=byB1;
                vFBot.apply(xlo, xhi, ylo2, yhi2);
                pFBot.setup(PW, HH, xlo, xhi, ylo2, yhi2);
                pFBot.xLabel = u8"x"; pFBot.yLabel = u8"|f - S_N|";
                pFBot.beginFrame(); pFBot.drawAxes();
                PlotStyle s; s.lineColor={255,80,80,220};
                s.filled=true; s.fillColor={255,80,80,35};
                pFBot.drawCurve(fourier.error, 0.f, fourier.L, s);
                pFBot.endFrame();
                drawTex(window, pFBot.texture(), px0, py0 + float(HH) + 8.f);
                registerPlot(px0, py0 + float(HH) + 8.f, pFBot, &vFBot,
                             bxB0, bxB1, byB0, byB1);
            }
            {
                float dx = fourier.L / std::max(1,(int)fourier.partial.size()-1);
                Stats s = calcStats(fourier.partial, dx);
                drawStatsHUD(px0, py0, float(PW), u8"Фурье — S_N(x)", s);
            }
            break;
        }
        } // switch

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
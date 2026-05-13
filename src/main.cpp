#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include "ui/AppState.h"
#include "ui/Theme.h"
#include "ui/Panel.h"
#include "ui/Plotting.h"

#include <cstring>
#include <algorithm>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#endif

// Update solvers when their parameters change, then advance the simulation
// for the active tab.
static void tickSolvers(AppState& s, float dt) {
    auto& w = s.wave;
    auto& h = s.heat;
    auto& g = s.green;
    auto& f = s.fourier;

    if (s.wNprev != w.nTerms || s.wIprev != (int)w.preset ||
        s.wLprev != w.L      || s.wAprev != w.a)
    {
        w.init();
        s.wNprev = w.nTerms; s.wIprev = (int)w.preset;
        s.wLprev = w.L;      s.wAprev = w.a;
    }
    if (s.hNprev  != h.nTerms       || s.hIprev != (int)h.preset ||
        s.hLprev  != h.L            || s.hAprev != h.a           ||
        s.hBLprev != h.bcLeft.value || s.hBRprev != h.bcRight.value)
    {
        h.init();
        s.hNprev  = h.nTerms;        s.hIprev  = (int)h.preset;
        s.hLprev  = h.L;             s.hAprev  = h.a;
        s.hBLprev = h.bcLeft.value;  s.hBRprev = h.bcRight.value;
    }
    if (s.fIprev != (int)f.preset || s.fLprev != f.L) {
        f.compute();
        s.fIprev = (int)f.preset;
        s.fLprev = f.L;
    }

    // Mode switch = clean slate: new coefficients, fresh grid, t=0
    static int wModePrev = 0, hModePrev = 0;
    if (s.wMode != wModePrev) {
        w.init(); s.wPlay = false;
        s.vWave.reset();
        s.lWave.yLocked = true; s.lWave.needCapture = true;
    }
    if (s.hMode != hModePrev) {
        h.init(); s.hPlay = false;
        s.vHeat.reset();
        s.lHeat.yLocked = true; s.lHeat.needCapture = true;
    }
    wModePrev = s.wMode; hModePrev = s.hMode;

    // Pause auto-playback on tab switch — otherwise t drifts in the background
    static int prevTab = -1;
    if (s.tab != prevTab) {
        s.wPlay = s.hPlay = s.gPlay = false;
        prevTab = s.tab;
    }

    if (s.wPlay && s.tab == TAB_WAVE) {
        float wDt = dt * s.wSpeed;
        if (s.wMode == 1) w.stepFD     (wDt);
        else              w.stepFourier(wDt);
    }
    if (s.hPlay && s.tab == TAB_HEAT) {
        float hDt = dt * s.hSpeed;
        if (s.hMode == 1) h.stepCN(hDt);
        else              h.time += hDt;
    }

    // Green mode change resets t and pauses playback
    static GreenMode gModePrev = g.mode;
    if (g.mode != gModePrev) {
        s.gPlay = false;
        g.t = 0.01f;
        s.vGreen.reset();
        s.lGreen.yLocked = true; s.lGreen.needCapture = true;
        gModePrev = g.mode;
    }
    if (s.gPlay && s.tab == TAB_GREEN) {
        g.t += dt * s.gSpeed;
        if (g.t > s.gTMax) g.t = 0.001f;
        if (g.t < 0.001f)  g.t = 0.001f;
    }
    // Recompute every frame so xi/t/a sliders feel live
    if (s.tab == TAB_GREEN) g.compute();
}

static void seedDefaults(AppState& s) {
    // Wave: variant 3 setup (see var3.md)
    s.wave.L          = 3.0f;
    s.wave.a          = 1.0f;
    s.wave.nTerms     = 20;
    s.wave.preset     = ICPreset::Custom;
    s.wave.customIC.source    = "1 - x/L + sin(pi*x/L)";
    s.wave.useNonhomogBC      = true;
    s.wave.alphaExpr.source   = "1";
    s.wave.betaExpr .source   = "(t/10)^2 - t/10";
    s.wave.useNonzeroPsi      = false;
    s.wave.psiExpr  .source   = "0";
    s.wave.useForcing         = true;
    s.wave.forcingExpr.source = "x - 2";

    s.heat.customIC.source    = "exp(-40*(x/L - 0.5)^2)";
    s.fourier.customIC.source = "sin(pi*x/L) + 0.3*sin(3*pi*x/L)";
    s.wave.customIC.reparse();
    s.heat.customIC.reparse();
    s.fourier.customIC.reparse();
    s.wave.alphaExpr  .reparse();
    s.wave.betaExpr   .reparse();
    s.wave.psiExpr    .reparse();
    s.wave.forcingExpr.reparse();

    // Green's default: piecewise Green for -u'' - a^2 u = f, u(0)=u(pi)=0.
    // Two pieces split at x = xi (see README example).
    s.green.mode = GreenMode::Custom1D;
    s.green.L    = float(kPi);
    s.green.xi   = float(kPi) * 0.5f;
    s.green.a    = 0.7f;            // away from integers; sin(a*pi) != 0
    s.green.t    = 0.05f;
    {
        GreenSolver::Piece lo, hi;
        lo.xLoExpr.source = "0";
        lo.xHiExpr.source = "xi";
        lo.expr.source    = "sin(a*x)*sin(a*(pi - xi)) / (a*sin(a*pi))";
        hi.xLoExpr.source = "xi";
        hi.xHiExpr.source = "L";
        hi.expr.source    = "sin(a*xi)*sin(a*(pi - x)) / (a*sin(a*pi))";
        for (auto* p : {&lo, &hi}) {
            p->xLoExpr.reparse(); p->xHiExpr.reparse(); p->expr.reparse();
        }
        s.green.customPieces.push_back(std::move(lo));
        s.green.customPieces.push_back(std::move(hi));
    }

    s.wave.init();
    s.heat.init();
    s.green.compute();
    s.fourier.compute();
}

// Right edge of the panel, 10-px sensitive strip
static void handleSplitter(const sf::Event& ev, AppState& s) {
    auto rect = [&]() {
        return sf::FloatRect(float(s.panelW) - 2.f, 0.f, 10.f, float(s.winH));
    };
    if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f mp(ev.mouseButton.x, ev.mouseButton.y);
        if (rect().contains(mp)) s.splitterDrag = true;
    }
    if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Left) {
        s.splitterDrag = false;
    }
    if (ev.type == sf::Event::MouseMoved && s.splitterDrag) {
        int w = std::clamp(ev.mouseMove.x, s.panelMinW, s.panelMaxW);
        w = std::min(w, s.winW - 220);   // keep at least 220 px for plots
        s.panelW = w;
    }
}

static void drawSplitter(sf::RenderWindow& win, const AppState& s) {
    sf::Vector2i mp = sf::Mouse::getPosition(win);
    bool hot = s.splitterDrag ||
               (mp.x >= s.panelW - 2 && mp.x <= s.panelW + 8 &&
                mp.y >= 0 && mp.y <= s.winH);
    sf::Color col = s.splitterDrag ? sf::Color(130, 180, 255, 240)
                  : hot            ? sf::Color(100, 140, 200, 220)
                                   : sf::Color( 70,  85, 110, 200);
    sf::RectangleShape sep({hot ? 4.f : 2.f, float(s.winH)});
    sep.setPosition(float(s.panelW) + (hot ? 2.f : 3.f), 0.f);
    sep.setFillColor(col);
    win.draw(sep);
}

int main() {
    // SFML takes Latin-1 by default; wrap through fromUtf8 for Cyrillic
    const char* titleUtf8 = u8"УМФ — Визуализатор уравнений математической физики";
    sf::String  titleStr  = sf::String::fromUtf8(titleUtf8, titleUtf8 + std::strlen(titleUtf8));

    sf::VideoMode dm = sf::VideoMode::getDesktopMode();
    AppState s;
    s.winW = std::max(1200, int(dm.width)  - 200);
    s.winH = std::max( 800, int(dm.height) - 200);

    sf::RenderWindow window(sf::VideoMode(s.winW, s.winH), titleStr, sf::Style::Default);
#ifdef _WIN32
    // Open maximized. SFML 2.5 has no portable way to do this.
    ::ShowWindow((HWND)window.getSystemHandle(), SW_MAXIMIZE);
    sf::Vector2u sz = window.getSize();
    s.winW = int(sz.x);
    s.winH = int(sz.y);
    window.setView(sf::View(sf::FloatRect(0.f, 0.f, float(s.winW), float(s.winH))));
#endif
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    ui::initFonts();
    ImGui::SFML::UpdateFontTexture();
    ui::applyDarkSlateTheme();

    seedDefaults(s);

    sf::Clock clk;
    while (window.isOpen()) {
        sf::Event ev;
        while (window.pollEvent(ev)) {
            ImGui::SFML::ProcessEvent(window, ev);
            if (ev.type == sf::Event::Closed) window.close();
            if (ev.type == sf::Event::Resized) {
                s.winW = int(ev.size.width);
                s.winH = int(ev.size.height);
                window.setView(sf::View(sf::FloatRect(0.f, 0.f, float(s.winW), float(s.winH))));
            }
            handleSplitter(ev, s);
            if (s.splitterDrag) continue;
            if (ImGui::GetIO().WantCaptureMouse) continue;
            ui::handlePlotMouseEvent(ev, s);
        }
        // Rebuild plot registry each frame (after rendering)
        s.plots.clear();

        sf::Time dt_sf = clk.restart();
        ImGui::SFML::Update(window, dt_sf);

        tickSolvers(s, dt_sf.asSeconds());

        ui::buildPanel(s);

        window.clear(sf::Color(18, 22, 26));
        drawSplitter(window, s);

        float px0 = float(s.panelW) + 8.f;
        float py0 = 8.f;
        ui::renderActiveTab(window, s, px0, py0);
        ui::drawHoverReadout(s);

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}

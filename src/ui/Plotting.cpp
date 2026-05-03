#include "ui/Plotting.h"
#include "ui/Widgets.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace ui {

void handlePlotMouseEvent(const sf::Event& ev, AppState& s) {
    // Mouse-wheel zoom centered on the cursor
    if (ev.type == sf::Event::MouseWheelScrolled) {
        sf::Vector2f mp(ev.mouseWheelScroll.x, ev.mouseWheelScroll.y);
        for (auto& h : s.plots) {
            if (!h.rect.contains(mp)) continue;
            float xlo = h.xBase0, xhi = h.xBase1, ylo = h.yBase0, yhi = h.yBase1;
            h.view->apply(xlo, xhi, ylo, yhi);
            float fx = (mp.x - h.rect.left) / h.rect.width;
            float fy = 1.f - (mp.y - h.rect.top) / h.rect.height;
            float dataX = xlo + fx * (xhi - xlo);
            float dataY = ylo + fy * (yhi - ylo);

            float factor  = (ev.mouseWheelScroll.delta > 0) ? 1.25f : 1.f / 1.25f;
            float newZoom = std::clamp(h.view->zoom * factor, 1e-4f, 1e8f);

            // Keep the point under the cursor anchored
            float xs0  = h.xBase1 - h.xBase0;
            float ys0  = h.yBase1 - h.yBase0;
            float nspx = xs0 / newZoom;
            float nspy = ys0 / newZoom;
            float ncx  = dataX - (fx - 0.5f) * nspx;
            float ncy  = dataY - (fy - 0.5f) * nspy;
            h.view->zoom = newZoom;
            h.view->panX = (ncx - 0.5f * (h.xBase0 + h.xBase1)) / xs0;
            h.view->panY = (ncy - 0.5f * (h.yBase0 + h.yBase1)) / ys0;
            return;
        }
    }
    // Right-drag pan
    if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Right) {
        sf::Vector2f mp(ev.mouseButton.x, ev.mouseButton.y);
        for (auto& h : s.plots) {
            if (h.rect.contains(mp)) {
                s.dragging  = true;
                s.dragStart = {ev.mouseButton.x, ev.mouseButton.y};
                s.dragView  = h.view;
                s.dragHit   = h;
                break;
            }
        }
    }
    if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Right) {
        s.dragging = false; s.dragView = nullptr;
    }
    if (ev.type == sf::Event::MouseMoved && s.dragging && s.dragView) {
        float dx = float(ev.mouseMove.x - s.dragStart.x);
        float dy = float(ev.mouseMove.y - s.dragStart.y);
        s.dragStart = {ev.mouseMove.x, ev.mouseMove.y};
        s.dragView->panX -= dx / s.dragHit.rect.width  / s.dragView->zoom;
        s.dragView->panY += dy / s.dragHit.rect.height / s.dragView->zoom;
    }
    // Double-click resets the view under the cursor
    if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
        static sf::Clock    dblClk;
        static sf::Vector2i lastPos{-999, -999};
        sf::Vector2i p{ev.mouseButton.x, ev.mouseButton.y};
        if (dblClk.getElapsedTime().asMilliseconds() < 350 &&
            std::abs(p.x - lastPos.x) < 5 && std::abs(p.y - lastPos.y) < 5)
        {
            sf::Vector2f mp(float(p.x), float(p.y));
            for (auto& h : s.plots)
                if (h.rect.contains(mp)) { h.view->reset(); break; }
        }
        dblClk.restart();
        lastPos = p;
    }
}

static void renderWaveTab(sf::RenderWindow& win, AppState& s, float px, float py) {
    auto& w = s.wave;
    unsigned PW = s.plotW(), PH = s.plotH();

    std::vector<float> y;
    if (s.wMode == 1) w.getFDProfile(y);
    else              w.getFourierProfile(w.time, y);

    auto [ylo, yhi] = autoRangeSymm(y);
    s.lWave.apply(ylo, yhi);
    float xBase0 = 0.f, xBase1 = w.L, yBase0 = ylo, yBase1 = yhi;
    float xlo = xBase0, xhi = xBase1, ylo2 = yBase0, yhi2 = yBase1;
    s.vWave.apply(xlo, xhi, ylo2, yhi2);

    s.pWave.setup(PW, PH, xlo, xhi, ylo2, yhi2);
    s.pWave.xLabel = u8"x"; s.pWave.yLabel = u8"u(x, t)";
    s.pWave.beginFrame(); s.pWave.drawAxes();
    PlotStyle st;
    if (s.wMode == 1) { st.lineColor = {255, 170,  60, 230}; st.fillColor = {255, 170,  60, 28}; }
    else              { st.lineColor = { 90, 200, 255, 235}; st.fillColor = { 90, 200, 255, 28}; }
    st.filled = true;
    s.pWave.drawCurve(y, 0.f, w.L, st);
    s.pWave.endFrame();
    drawTex(win, s.pWave.texture(), px, py);
    registerPlot(s.plots, px, py, s.pWave, &s.vWave, xBase0, xBase1, yBase0, yBase1);

    float dx = w.L / std::max(1, (int)y.size() - 1);
    Stats stats = calcStats(y, dx);
    drawStatsHUD(px, py, float(PW),
                 s.wMode == 1 ? u8"Волна — МКР" : u8"Волна — Фурье", stats);
}

static void renderHeatTab(sf::RenderWindow& win, AppState& s, float px, float py) {
    auto& h = s.heat;
    unsigned PW = s.plotW(), PH = s.plotH();

    std::vector<float> y;
    if (s.hMode == 1) h.getCNProfile(y);
    else              h.getFourierProfile(h.time, y);

    auto [ylo, yhi] = autoRangePos(y);
    s.lHeat.apply(ylo, yhi);
    float xBase0 = 0.f, xBase1 = h.L, yBase0 = ylo, yBase1 = yhi;
    float xlo = xBase0, xhi = xBase1, ylo2 = yBase0, yhi2 = yBase1;
    s.vHeat.apply(xlo, xhi, ylo2, yhi2);
    s.pHeat.setup(PW, PH, xlo, xhi, ylo2, yhi2);
    s.pHeat.xLabel = u8"x"; s.pHeat.yLabel = u8"u(x, t)";
    s.pHeat.beginFrame(); s.pHeat.drawAxes();
    PlotStyle st;
    if (s.hMode == 1) { st.lineColor = {255, 170,  60, 230}; st.fillColor = {255, 170,  60, 28}; }
    else              { st.lineColor = {110, 230, 150, 235}; st.fillColor = {110, 230, 150, 28}; }
    st.filled = true;
    s.pHeat.drawCurve(y, 0.f, h.L, st);
    s.pHeat.endFrame();
    drawTex(win, s.pHeat.texture(), px, py);
    registerPlot(s.plots, px, py, s.pHeat, &s.vHeat, xBase0, xBase1, yBase0, yBase1);

    float dx = h.L / std::max(1, (int)y.size() - 1);
    Stats stats = calcStats(y, dx);
    drawStatsHUD(px, py, float(PW),
                 s.hMode == 1 ? u8"Тепло — КН" : u8"Тепло — Фурье", stats);
}

static void renderGreenTab(sf::RenderWindow& win, AppState& s, float px, float py) {
    auto& g = s.green;
    unsigned PW = s.plotW(), PH = s.plotH();

    if (g.mode == GreenMode::Laplace2D) {
        s.hmGreen.setup(PW, PH);
        s.hmGreen.update(g.g2d, g.g2dW, g.g2dH, g.gMin, g.gMax);
        sf::Sprite sp(s.hmGreen.texture()); sp.setPosition(px, py); win.draw(sp);
        return;
    }

    float xMin = (g.mode == GreenMode::Wave1D)
                 ? g.xi - 2.f * std::max(g.a * g.t, g.L * 0.5f)
                 : 0.f;
    float xMax = (g.mode == GreenMode::Wave1D)
                 ? g.xi + 2.f * std::max(g.a * g.t, g.L * 0.5f)
                 : g.L;

    auto [ylo, yhi] = autoRangePos(g.g1d);
    s.lGreen.apply(ylo, yhi);
    float xBase0 = xMin, xBase1 = xMax, yBase0 = ylo, yBase1 = yhi;
    float xlo = xBase0, xhi = xBase1, ylo2 = yBase0, yhi2 = yBase1;
    s.vGreen.apply(xlo, xhi, ylo2, yhi2);
    s.pGreen.setup(PW, PH, xlo, xhi, ylo2, yhi2);
    s.pGreen.xLabel = u8"x"; s.pGreen.yLabel = u8"G(x, ξ, t)";
    s.pGreen.beginFrame(); s.pGreen.drawAxes();
    s.pGreen.drawVLine(g.xi, {255, 210, 60, 200});
    PlotStyle st; st.lineColor = {185, 100, 255, 230};
    st.filled = true; st.fillColor = {185, 100, 255, 35};
    s.pGreen.drawCurve(g.g1d, xMin, xMax, st);
    s.pGreen.endFrame();
    drawTex(win, s.pGreen.texture(), px, py);
    registerPlot(s.plots, px, py, s.pGreen, &s.vGreen, xBase0, xBase1, yBase0, yBase1);

    float dx = (xMax - xMin) / std::max(1, (int)g.g1d.size() - 1);
    Stats stats = calcStats(g.g1d, dx);
    drawStatsHUD(px, py, float(PW), u8"Грина — статистика", stats);
}

static void renderFourierTab(sf::RenderWindow& win, AppState& s, float px, float py) {
    auto& f = s.fourier;
    unsigned PW = s.plotW(), HH = s.halfH();

    // Top: f(x), partial sum, optional harmonics
    float fxB0 = 0.f, fxB1 = f.L, fyB0 = 0.f, fyB1 = 0.f;
    {
        auto [ylo, yhi] = autoRangeSymm(f.exact, 0.25f);
        fyB0 = ylo; fyB1 = yhi;
        float xlo = fxB0, xhi = fxB1, ylo2 = fyB0, yhi2 = fyB1;
        s.vFTop.apply(xlo, xhi, ylo2, yhi2);
        s.pFTop.setup(PW, HH, xlo, xhi, ylo2, yhi2);
        s.pFTop.xLabel = u8"x"; s.pFTop.yLabel = u8"f(x), S_N(x)";
        s.pFTop.beginFrame(); s.pFTop.drawAxes();

        if (f.showHarmonics) {
            int Nh = std::min(f.nShow, (int)f.harmonics.size());
            for (int k = 0; k < Nh; ++k) {
                PlotStyle sh;
                sh.lineColor = harmCol(k);
                sh.lineColor.a = 180;
                s.pFTop.drawCurve(f.harmonics[k], 0.f, f.L, sh);
            }
        }
        { PlotStyle st; st.lineColor = {255, 220,  55, 220};
          s.pFTop.drawCurve(f.exact,   0.f, f.L, st); }
        { PlotStyle st; st.lineColor = { 80, 200, 255, 220}; st.lineWidth = 2.f;
          s.pFTop.drawCurve(f.partial, 0.f, f.L, st); }
        s.pFTop.endFrame();
        drawTex(win, s.pFTop.texture(), px, py);
        registerPlot(s.plots, px, py, s.pFTop, &s.vFTop, fxB0, fxB1, fyB0, fyB1);
    }
    // Bottom: pointwise error
    if (f.showError) {
        auto [ylo, yhi] = autoRangePos(f.error, 0.2f);
        ylo = std::min(ylo, 0.f);
        float bxB0 = 0.f, bxB1 = f.L, byB0 = ylo, byB1 = yhi;
        float xlo = bxB0, xhi = bxB1, ylo2 = byB0, yhi2 = byB1;
        s.vFBot.apply(xlo, xhi, ylo2, yhi2);
        s.pFBot.setup(PW, HH, xlo, xhi, ylo2, yhi2);
        s.pFBot.xLabel = u8"x"; s.pFBot.yLabel = u8"|f - S_N|";
        s.pFBot.beginFrame(); s.pFBot.drawAxes();
        PlotStyle st; st.lineColor = {255, 80, 80, 220};
        st.filled = true; st.fillColor = {255, 80, 80, 35};
        s.pFBot.drawCurve(f.error, 0.f, f.L, st);
        s.pFBot.endFrame();
        drawTex(win, s.pFBot.texture(), px, py + float(HH) + 8.f);
        registerPlot(s.plots, px, py + float(HH) + 8.f, s.pFBot, &s.vFBot,
                     bxB0, bxB1, byB0, byB1);
    }
    float dx = f.L / std::max(1, (int)f.partial.size() - 1);
    Stats stats = calcStats(f.partial, dx);
    drawStatsHUD(px, py, float(PW), u8"Фурье — S_N(x)", stats);
}

void renderActiveTab(sf::RenderWindow& win, AppState& s, float px, float py) {
    switch (s.tab) {
        case TAB_WAVE:    renderWaveTab   (win, s, px, py); break;
        case TAB_HEAT:    renderHeatTab   (win, s, px, py); break;
        case TAB_GREEN:   renderGreenTab  (win, s, px, py); break;
        case TAB_FOURIER: renderFourierTab(win, s, px, py); break;
    }
}

} // namespace ui

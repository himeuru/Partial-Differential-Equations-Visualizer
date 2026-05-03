#pragma once
#include "solvers/WaveSolver.h"
#include "solvers/HeatSolver.h"
#include "solvers/GreenSolver.h"
#include "solvers/FourierViz.h"
#include "renderer/Renderer.h"
#include "ui/PlotState.h"
#include <vector>
#include <algorithm>

enum Tab { TAB_WAVE = 0, TAB_HEAT, TAB_GREEN, TAB_FOURIER };

// Single context object passed through the UI / render layer so that
// individual tab functions don't drag long argument lists.
struct AppState {
    // Solvers
    WaveSolver  wave;
    HeatSolver  heat;
    GreenSolver green;
    FourierViz  fourier;

    // Plot resources kept across frames
    PlotArea pWave, pHeat, pGreen, pFTop, pFBot;
    Heatmap  hmGreen;

    // View / axis state per plot
    View     vWave, vHeat, vGreen, vFTop, vFBot;
    AxisLock lWave, lHeat, lGreen, lFTop, lFBot;

    // Per-tab playback / settings
    bool  wPlay = false, hPlay = false, gPlay = false;
    float wSpeed = 1.f, hSpeed = 0.3f, gSpeed = 0.05f;
    float gTMax = 5.f;
    int   wMode = 0;   // 0 = Fourier, 1 = FD
    int   hMode = 0;   // 0 = Fourier, 1 = CN

    // Active tab
    int tab = TAB_WAVE;

    // Window sizing kept in sync with SFML
    int winW = 1600, winH = 1000;
    int panelW = 480, panelMinW = 460, panelMaxW = 1000;

    // Mouse hit-tests for the current frame, rebuilt each render pass
    std::vector<PlotHit> plots;

    // Right-drag pan state on plots
    bool         dragging = false;
    sf::Vector2i dragStart;
    View*        dragView = nullptr;
    PlotHit      dragHit{};

    // Panel splitter drag state
    bool splitterDrag = false;

    // Param-change tracking — re-init solvers when a slider crosses
    int   wNprev = 0, wIprev = 0, hNprev = 0, hIprev = 0, fIprev = 0;
    float wLprev = 0, wAprev = 0, hLprev = 0, hAprev = 0;
    float hBLprev = 0, hBRprev = 0, fLprev = 0;

    unsigned plotW() const { return unsigned(std::max(200, winW - panelW - 16)); }
    unsigned plotH() const { return unsigned(std::max(200, winH - 16)); }
    unsigned halfH() const { return plotH() / 2 - 4; }

    View* viewForTab(int t, int sub = 0) {
        switch (t) {
            case TAB_WAVE:    return &vWave;
            case TAB_HEAT:    return &vHeat;
            case TAB_GREEN:   return &vGreen;
            case TAB_FOURIER: return sub == 0 ? &vFTop : &vFBot;
        }
        return nullptr;
    }
};

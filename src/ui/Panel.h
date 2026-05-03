#pragma once
#include "ui/AppState.h"

namespace ui {

// Each builder lays out one tab's worth of ImGui widgets and updates
// `s.tab` when its tab item is the active one. They never touch SFML —
// rendering for plots happens in Plotting.cpp afterwards.
void buildWaveTab   (AppState& s);
void buildHeatTab   (AppState& s);
void buildGreenTab  (AppState& s);
void buildFourierTab(AppState& s);

// Top-level: tab bar + Begin/End("##ctrl") wrapper.
void buildPanel(AppState& s);

} // namespace ui

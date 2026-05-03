#pragma once
#include "ui/AppState.h"
#include <SFML/Graphics.hpp>

namespace ui {

// Draws the active tab's plot(s) into `win` and appends hit-test rects to
// `s.plots` for the next mouse-event pass. Origin is (px, py) of the plot
// region (right of the control panel).
void renderActiveTab(sf::RenderWindow& win, AppState& s, float px, float py);

// Pan/zoom/double-click dispatch for the plot region. Returns false if the
// event should not be considered "consumed" (so the caller can keep going).
void handlePlotMouseEvent(const sf::Event& ev, AppState& s);

} // namespace ui

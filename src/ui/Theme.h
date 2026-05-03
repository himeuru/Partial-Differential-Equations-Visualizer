#pragma once

namespace ui {

// Load Latin/Cyrillic/Greek/math glyphs from system fonts; falls back to
// the built-in ImGui font (no Cyrillic) if nothing matches.
void initFonts();

// Slate-grey window with blue accents — applied once after Init.
void applyDarkSlateTheme();

} // namespace ui

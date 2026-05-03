#include "ui/Theme.h"
#include <imgui.h>

namespace ui {

void initFonts() {
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
}

void applyDarkSlateTheme() {
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
    C[ImGuiCol_WindowBg]            = {0.16f, 0.17f, 0.20f, 1.00f};
    C[ImGuiCol_ChildBg]             = {0.18f, 0.20f, 0.23f, 1.00f};
    C[ImGuiCol_PopupBg]             = {0.20f, 0.22f, 0.26f, 0.98f};
    C[ImGuiCol_Border]              = {0.32f, 0.36f, 0.42f, 0.50f};
    C[ImGuiCol_FrameBg]             = {0.24f, 0.27f, 0.31f, 1.00f};
    C[ImGuiCol_FrameBgHovered]      = {0.32f, 0.36f, 0.42f, 1.00f};
    C[ImGuiCol_FrameBgActive]       = {0.38f, 0.44f, 0.52f, 1.00f};
    C[ImGuiCol_TitleBg]             = {0.18f, 0.20f, 0.24f, 1.00f};
    C[ImGuiCol_TitleBgActive]       = {0.26f, 0.32f, 0.40f, 1.00f};
    C[ImGuiCol_MenuBarBg]           = {0.20f, 0.22f, 0.26f, 1.00f};
    C[ImGuiCol_ScrollbarBg]         = {0.14f, 0.16f, 0.19f, 1.00f};
    C[ImGuiCol_ScrollbarGrab]       = {0.35f, 0.40f, 0.48f, 1.00f};
    C[ImGuiCol_ScrollbarGrabHovered]= {0.45f, 0.52f, 0.62f, 1.00f};
    C[ImGuiCol_SliderGrab]          = {0.55f, 0.75f, 1.00f, 1.00f};
    C[ImGuiCol_SliderGrabActive]    = {0.70f, 0.85f, 1.00f, 1.00f};
    C[ImGuiCol_Button]              = {0.30f, 0.42f, 0.58f, 1.00f};
    C[ImGuiCol_ButtonHovered]       = {0.40f, 0.55f, 0.75f, 1.00f};
    C[ImGuiCol_ButtonActive]        = {0.50f, 0.68f, 0.90f, 1.00f};
    C[ImGuiCol_Tab]                 = {0.22f, 0.26f, 0.32f, 1.00f};
    C[ImGuiCol_TabHovered]          = {0.42f, 0.55f, 0.75f, 1.00f};
    C[ImGuiCol_TabActive]           = {0.34f, 0.48f, 0.68f, 1.00f};
    C[ImGuiCol_TabUnfocused]        = {0.20f, 0.23f, 0.28f, 1.00f};
    C[ImGuiCol_TabUnfocusedActive]  = {0.28f, 0.36f, 0.48f, 1.00f};
    C[ImGuiCol_Header]              = {0.30f, 0.40f, 0.55f, 0.70f};
    C[ImGuiCol_HeaderHovered]       = {0.40f, 0.55f, 0.75f, 0.80f};
    C[ImGuiCol_HeaderActive]        = {0.50f, 0.68f, 0.90f, 0.80f};
    C[ImGuiCol_Separator]           = {0.35f, 0.40f, 0.48f, 0.55f};
    C[ImGuiCol_SeparatorHovered]    = {0.50f, 0.68f, 0.90f, 0.80f};
    C[ImGuiCol_SeparatorActive]     = {0.60f, 0.78f, 1.00f, 1.00f};
    C[ImGuiCol_CheckMark]           = {0.55f, 0.85f, 1.00f, 1.00f};
    C[ImGuiCol_Text]                = {0.95f, 0.96f, 0.97f, 1.00f};
    C[ImGuiCol_TextDisabled]        = {0.62f, 0.67f, 0.74f, 1.00f};
    C[ImGuiCol_PlotHistogram]       = {0.95f, 0.75f, 0.30f, 1.00f};
    C[ImGuiCol_PlotHistogramHovered]= {1.00f, 0.85f, 0.45f, 1.00f};
}

} // namespace ui

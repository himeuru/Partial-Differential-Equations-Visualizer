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
    // Carbon-teal: near-black background, cyan-green accent. Cool palette
    // stays out of the way of the warm plot curves.
    ImGui::StyleColorsDark();
    ImGuiStyle& sty = ImGui::GetStyle();
    sty.WindowRounding    = 6.f;
    sty.ChildRounding     = 4.f;
    sty.FrameRounding     = 5.f;
    sty.GrabRounding      = 6.f;
    sty.TabRounding       = 6.f;
    sty.PopupRounding     = 6.f;
    sty.ScrollbarRounding = 6.f;
    sty.ScrollbarSize     = 12.f;
    sty.FramePadding      = {9, 6};
    sty.ItemSpacing       = {10, 7};
    sty.ItemInnerSpacing  = {6, 4};
    sty.WindowPadding     = {14, 12};
    sty.IndentSpacing     = 14.f;
    sty.GrabMinSize       = 12.f;
    sty.FrameBorderSize   = 1.f;        // subtle outline around frames
    sty.WindowBorderSize  = 0.f;
    sty.SeparatorTextPadding = {20, 3};

    ImVec4* C = sty.Colors;
    C[ImGuiCol_WindowBg]            = {0.085f, 0.100f, 0.115f, 1.00f};
    C[ImGuiCol_ChildBg]             = {0.120f, 0.140f, 0.160f, 1.00f};
    C[ImGuiCol_PopupBg]             = {0.140f, 0.165f, 0.190f, 0.98f};
    C[ImGuiCol_Border]              = {0.260f, 0.330f, 0.360f, 0.55f};
    C[ImGuiCol_FrameBg]             = {0.155f, 0.190f, 0.215f, 1.00f};
    C[ImGuiCol_FrameBgHovered]      = {0.215f, 0.275f, 0.310f, 1.00f};
    C[ImGuiCol_FrameBgActive]       = {0.280f, 0.395f, 0.445f, 1.00f};
    C[ImGuiCol_TitleBg]             = {0.140f, 0.160f, 0.180f, 1.00f};
    C[ImGuiCol_TitleBgActive]       = {0.220f, 0.350f, 0.400f, 1.00f};
    C[ImGuiCol_MenuBarBg]           = {0.140f, 0.165f, 0.190f, 1.00f};
    C[ImGuiCol_ScrollbarBg]         = {0.090f, 0.110f, 0.125f, 1.00f};
    C[ImGuiCol_ScrollbarGrab]       = {0.320f, 0.420f, 0.450f, 1.00f};
    C[ImGuiCol_ScrollbarGrabHovered]= {0.420f, 0.560f, 0.600f, 1.00f};
    C[ImGuiCol_SliderGrab]          = {0.350f, 0.800f, 0.840f, 1.00f};
    C[ImGuiCol_SliderGrabActive]    = {0.500f, 0.950f, 1.000f, 1.00f};
    C[ImGuiCol_Button]              = {0.200f, 0.410f, 0.450f, 1.00f};
    C[ImGuiCol_ButtonHovered]       = {0.310f, 0.580f, 0.625f, 1.00f};
    C[ImGuiCol_ButtonActive]        = {0.420f, 0.750f, 0.800f, 1.00f};
    C[ImGuiCol_Tab]                 = {0.155f, 0.215f, 0.245f, 1.00f};
    C[ImGuiCol_TabHovered]          = {0.330f, 0.560f, 0.605f, 1.00f};
    C[ImGuiCol_TabActive]           = {0.255f, 0.470f, 0.515f, 1.00f};
    C[ImGuiCol_TabUnfocused]        = {0.155f, 0.200f, 0.225f, 1.00f};
    C[ImGuiCol_TabUnfocusedActive]  = {0.225f, 0.380f, 0.420f, 1.00f};
    C[ImGuiCol_Header]              = {0.250f, 0.450f, 0.490f, 0.70f};
    C[ImGuiCol_HeaderHovered]       = {0.350f, 0.600f, 0.640f, 0.80f};
    C[ImGuiCol_HeaderActive]        = {0.460f, 0.760f, 0.800f, 0.80f};
    C[ImGuiCol_Separator]           = {0.300f, 0.400f, 0.440f, 0.55f};
    C[ImGuiCol_SeparatorHovered]    = {0.460f, 0.760f, 0.800f, 0.80f};
    C[ImGuiCol_SeparatorActive]     = {0.550f, 0.900f, 0.950f, 1.00f};
    C[ImGuiCol_CheckMark]           = {0.500f, 0.950f, 1.000f, 1.00f};
    C[ImGuiCol_Text]                = {0.940f, 0.955f, 0.960f, 1.00f};
    C[ImGuiCol_TextDisabled]        = {0.560f, 0.640f, 0.670f, 1.00f};
    C[ImGuiCol_PlotHistogram]       = {0.950f, 0.720f, 0.300f, 1.00f};
    C[ImGuiCol_PlotHistogramHovered]= {1.000f, 0.830f, 0.450f, 1.00f};
}

} // namespace ui

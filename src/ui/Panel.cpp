#include "ui/Panel.h"
#include "ui/Widgets.h"
#include <algorithm>

namespace ui {

void buildWaveTab(AppState& s) {
    if (!ImGui::BeginTabItem(u8"Волновое")) return;
    s.tab = TAB_WAVE;
    auto& w = s.wave;

    ImGui::Spacing();
    ImGui::TextColored({0.6f, 0.85f, 1.f, 1.f}, u8"Волновое уравнение:");
    prettyFormula("u_tt = a^2 * u_xx", {0.6f, 0.85f, 1.f, 1.f});
    ImGui::TextDisabled(u8"Аналитическое решение (ряд по собственным модам):");
    prettyFormula("u(x,t) = sum(n, 1, N, A_n * cos(n*pi*a*t/L) * sin(n*pi*x/L))");
    ImGui::Separator(); ImGui::Spacing();

    sliderF(u8"Скорость a",    &w.a,      0.1f, 5.f, 0.1f, "%.2f");
    sliderF(u8"Длина     L",   &w.L,      0.5f, 3.f, 0.1f, "%.2f");
    sliderI(u8"Членов ряда N", &w.nTerms, 1,    60,  1);
    ImGui::Spacing();
    {
        int p = (int)w.preset;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##ic_w", &p, ICPresetNames, IM_ARRAYSIZE(ICPresetNames)))
            w.preset = (ICPreset)p;
        ImGui::TextDisabled(u8"  Начальное условие f(x)");
    }
    if (w.preset == ICPreset::Custom) {
        if (customExprInput(u8"Ваше f(x):", w.customIC)) w.init();
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::TextColored({1.f, 0.85f, 0.6f, 1.f}, u8"Неоднородные условия:");
    {
        bool bvpChanged = false;
        if (ImGui::Checkbox(u8"α(t), β(t) — ненулевые концы", &w.useNonhomogBC))
            bvpChanged = true;
        if (w.useNonhomogBC) {
            if (customExprInput(u8"α(t) = u(0, t):", w.alphaExpr,
                                u8"переменная: t (доступны L, a, pi, e)"))
                bvpChanged = true;
            if (customExprInput(u8"β(t) = u(L, t):", w.betaExpr,
                                u8"переменная: t (доступны L, a, pi, e)"))
                bvpChanged = true;
        }

        if (ImGui::Checkbox(u8"ψ(x) — ненулевая начальная скорость", &w.useNonzeroPsi))
            bvpChanged = true;
        if (w.useNonzeroPsi) {
            if (customExprInput(u8"ψ(x) = u_t(x, 0):", w.psiExpr,
                                u8"переменная: x (доступны xi, L, a, pi, e)"))
                bvpChanged = true;
        }

        if (ImGui::Checkbox(u8"f(x) — правая часть", &w.useForcing))
            bvpChanged = true;
        if (w.useForcing) {
            if (customExprInput(u8"f(x):", w.forcingExpr,
                                u8"переменная: x (доступны xi, L, a, pi, e)"))
                bvpChanged = true;
        }

        if (bvpChanged) w.init();
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::TextDisabled(u8"Метод решения:");
    ImGui::RadioButton(u8"Ряд Фурье (аналит.)", &s.wMode, 0);
    ImGui::RadioButton(u8"МКР — явная схема",   &s.wMode, 1);
    ImGui::Spacing();

    sliderF(u8"Скорость воспр.", &s.wSpeed, 0.05f, 5.f, 0.1f, "x%.2f");
    ImGui::Spacing();
    if (ImGui::Button(s.wPlay ? u8"⏸ Пауза " : u8"▶ Пуск  ", {120, 0}))
        s.wPlay = !s.wPlay;
    ImGui::SameLine();
    if (ImGui::Button(u8"↺ Сброс", {80, 0}))   { w.init(); s.wPlay = false; }
    ImGui::SameLine();
    if (ImGui::Button(u8"Сброс вида", {110, 0})) {
        s.vWave.reset();
        s.lWave.yLocked = true;
        s.lWave.needCapture = true;
    }
    axisLockCheckbox(u8"Зафиксировать ось Y", s.lWave);
    ImGui::Spacing();
    ImGui::Text(u8"t = %.4f с    zoom x%.2f", w.time, s.vWave.zoom);
    ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextWrapped(
        u8"An = (2/L) ∫ f(x) sin(nπx/L) dx\n\n"
        u8"МКР — явная центральная схема 2-го порядка;\n"
        u8"условие устойчивости Куранта: r = a·Δt/Δx ≤ 1.\n"
        u8"Под-шаг выбирается автоматически (r = 0.7).");
    ImGui::EndTabItem();
}

void buildHeatTab(AppState& s) {
    if (!ImGui::BeginTabItem(u8"Теплопр.")) return;
    s.tab = TAB_HEAT;
    auto& h = s.heat;

    ImGui::Spacing();
    ImGui::TextColored({0.6f, 1.f, 0.75f, 1.f}, u8"Уравнение теплопроводности:");
    prettyFormula("u_t = a^2 * u_xx", {0.6f, 1.f, 0.75f, 1.f});
    ImGui::TextDisabled(u8"Аналитическое решение (собственные моды затухают):");
    prettyFormula("u(x,t) = sum(n, 1, N, B_n * exp(-a^2*(n*pi/L)^2*t) * sin(n*pi*x/L))");
    ImGui::Separator(); ImGui::Spacing();

    sliderF(u8"Диффузия  a", &h.a,      0.1f, 3.f, 0.1f, "%.2f");
    sliderF(u8"Длина     L", &h.L,      0.5f, 3.f, 0.1f, "%.2f");
    sliderI(u8"Членов ряда N", &h.nTerms, 1, 80, 1);
    ImGui::Spacing();
    {
        int p = (int)h.preset;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##ic_h", &p, ICPresetNames, IM_ARRAYSIZE(ICPresetNames)))
            h.preset = (ICPreset)p;
        ImGui::TextDisabled(u8"  Начальное условие f(x)");
    }
    if (h.preset == ICPreset::Custom) {
        if (customExprInput(u8"Ваше f(x):", h.customIC)) h.init();
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::TextDisabled(u8"Граничные условия Дирихле:");
    sliderF("u(0)", &h.bcLeft.value,  -1.f, 1.f, 0.05f, "%.3f");
    sliderF("u(L)", &h.bcRight.value, -1.f, 1.f, 0.05f, "%.3f");
    ImGui::Spacing();

    ImGui::TextDisabled(u8"Метод решения:");
    ImGui::RadioButton(u8"Ряд Фурье (аналит.)", &s.hMode, 0);
    ImGui::RadioButton(u8"Кранк – Николсон",   &s.hMode, 1);
    ImGui::Spacing();

    sliderF(u8"Скорость сим.", &s.hSpeed, 0.001f, 2.f, 0.05f, "%.3f");
    ImGui::Spacing();
    if (ImGui::Button(s.hPlay ? u8"⏸ Пауза " : u8"▶ Пуск  ", {120, 0}))
        s.hPlay = !s.hPlay;
    ImGui::SameLine();
    if (ImGui::Button(u8"↺ Сброс", {80, 0})) { h.init(); s.hPlay = false; }
    ImGui::SameLine();
    if (ImGui::Button(u8"Сброс вида", {110, 0})) {
        s.vHeat.reset();
        s.lHeat.yLocked = true;
        s.lHeat.needCapture = true;
    }
    axisLockCheckbox(u8"Зафиксировать ось Y", s.lHeat);
    ImGui::Spacing();
    ImGui::Text(u8"t = %.4f с    zoom x%.2f", h.time, s.vHeat.zoom);
    ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextWrapped(
        u8"Мода n затухает как exp(-a²λ²n t).\n"
        u8"Высокие гармоники исчезают первыми — сглаживание сигнала.\n"
        u8"Схема Кранка–Николсона безусловно устойчива.");
    ImGui::EndTabItem();
}

static void buildGreenHeat   (AppState& s);
static void buildGreenWave   (AppState& s);
static void buildGreenSL     (AppState& s);
static void buildGreenCustom (AppState& s);
static void buildGreenLaplace(AppState& s);

void buildGreenTab(AppState& s) {
    if (!ImGui::BeginTabItem(u8"Грина")) return;
    s.tab = TAB_GREEN;
    auto& g = s.green;

    ImGui::Spacing();
    ImGui::TextColored({0.85f, 0.7f, 1.f, 1.f}, u8"Функции Грина");
    ImGui::Separator(); ImGui::Spacing();

    {
        int gm = (int)g.mode;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##gm", &gm, GreenModeNames, IM_ARRAYSIZE(GreenModeNames)))
            g.mode = (GreenMode)gm;
    }
    ImGui::Spacing();

    switch (g.mode) {
        case GreenMode::Heat1D:         buildGreenHeat   (s); break;
        case GreenMode::Wave1D:         buildGreenWave   (s); break;
        case GreenMode::SturmLiouville: buildGreenSL     (s); break;
        case GreenMode::Custom1D:       buildGreenCustom (s); break;
        case GreenMode::Laplace2D:      buildGreenLaplace(s); break;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button(u8"Пересчитать", {120, 0})) g.compute();
    ImGui::SameLine();
    if (ImGui::Button(u8"Сброс вида", {110, 0})) {
        s.vGreen.reset();
        s.lGreen.yLocked = true;
        s.lGreen.needCapture = true;
    }
    axisLockCheckbox(u8"Зафиксировать ось Y", s.lGreen);
    ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
    ImGui::EndTabItem();
}

static void buildGreenHeat(AppState& s) {
    auto& g = s.green;
    prettyFormula("G(x, xi, t) = 1/sqrt(4*pi*a^2*t) * exp(-(x-xi)^2/(4*a^2*t))");
    ImGui::Spacing();
    sliderF(u8"Длина    L",  &g.L,  0.5f, 2.f, 0.1f, "%.2f");
    if (g.xi > g.L) g.xi = g.L;
    sliderF(u8"Источник ξ", &g.xi, 0.f, g.L, 0.02f, "%.3f");
    sliderF(u8"Время    t",  &g.t,  0.001f, s.gTMax, 0.005f, "%.4f");
    sliderF(u8"Скорость a",  &g.a,  0.1f, 2.f, 0.1f, "%.2f");
    ImGui::Spacing();
    if (ImGui::Button(s.gPlay ? u8"⏸ Стоп авто-t" : u8"▶ Авто-t")) s.gPlay = !s.gPlay;
    sliderF(u8"Скорость t", &s.gSpeed, 0.001f, 2.f, 0.05f, "%.3f");
    sliderF(u8"t_max",      &s.gTMax,  0.1f, 50.f, 0.5f, "%.2f");
    ImGui::Spacing();
    ImGui::TextWrapped(
        u8"Фундаментальное решение уравнения теплопроводности.\n"
        u8"При t→0+: G → δ(x − ξ).\n"
        u8"При t→∞: G расширяется и уплощается.");
}

static void buildGreenWave(AppState& s) {
    auto& g = s.green;
    prettyFormula("1/(2*a)");
    ImGui::TextColored({0.7f, 0.9f, 1.f, 1.f}, u8"Внутри светового конуса  |x−ξ| ≤ at");
    ImGui::Spacing();
    sliderF(u8"Длина    L", &g.L, 0.5f, 3.f, 0.1f, "%.2f");
    g.xi = std::clamp(g.xi, 0.f, g.L);
    sliderF(u8"Источник ξ", &g.xi, 0.f, g.L, 0.02f, "%.3f");
    sliderF(u8"Время    t",  &g.t,  0.001f, s.gTMax, 0.02f, "%.3f");
    sliderF(u8"Скорость a",  &g.a,  0.1f, 3.f, 0.1f, "%.2f");
    ImGui::Spacing();
    if (ImGui::Button(s.gPlay ? u8"⏸ Стоп авто-t" : u8"▶ Авто-t")) s.gPlay = !s.gPlay;
    sliderF(u8"Скорость t", &s.gSpeed, 0.001f, 2.f, 0.05f, "%.3f");
    sliderF(u8"t_max",      &s.gTMax,  0.1f, 50.f, 0.5f, "%.2f");
    ImGui::Spacing();
    ImGui::TextWrapped(
        u8"Сечение конуса Маха (характеристический конус).\n"
        u8"Возмущение распространяется со скоростью a в обе стороны.\n"
        u8"Принцип Гюйгенса в 1D.");
}

static void buildGreenSL(AppState& s) {
    auto& g = s.green;
    ImGui::TextColored({0.7f, 0.9f, 1.f, 1.f}, u8"G(x, ξ) =");
    prettyFormula("sum(n, 1, N, (2/L) * sin(n*pi*x/L) * sin(n*pi*xi/L) / (n*pi/L)^2)");
    ImGui::Spacing();
    sliderF(u8"Длина    L",  &g.L,      0.5f, 3.f, 0.1f, "%.2f");
    g.xi = std::clamp(g.xi, 0.f, g.L);
    sliderF(u8"Источник ξ",  &g.xi, 0.f, g.L, 0.02f, "%.3f");
    sliderI(u8"Членов   N",  &g.nTerms, 5, 200, 5);
    ImGui::Spacing();
    ImGui::TextWrapped(
        u8"Задача: -u'' = f(x),  u(0)=u(L)=0.\n"
        u8"G(x,ξ) — отклик на точечный источник δ(x−ξ).\n"
        u8"Явление Гиббса вблизи ξ при малом N.");
}

static void buildGreenCustom(AppState& s) {
    auto& g = s.green;
    ImGui::TextColored({0.7f, 0.9f, 1.f, 1.f}, u8"G(x, ξ, t) — кусочная функция");
    ImGui::Spacing();
    sliderF(u8"Длина    L",  &g.L,  0.1f, 10.f, 0.1f, "%.3f");
    if (ImGui::SmallButton(u8"L = π"))    g.L = 3.14159265f;
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"L = 2π"))  g.L = 6.28318531f;
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"L = 1"))   g.L = 1.f;
    if (g.xi > g.L) g.xi = g.L;
    sliderF(u8"Источник ξ", &g.xi, 0.f, g.L, 0.02f, "%.3f");
    sliderF(u8"Время    t",  &g.t,  0.001f, s.gTMax, 0.02f, "%.4f");
    sliderF(u8"Скорость a",  &g.a,  0.1f, 3.f, 0.1f, "%.2f");
    ImGui::Spacing();
    if (ImGui::Button(s.gPlay ? u8"⏸ Стоп авто-t" : u8"▶ Авто-t")) s.gPlay = !s.gPlay;
    sliderF(u8"Скорость t", &s.gSpeed, 0.001f, 2.f, 0.05f, "%.3f");
    sliderF(u8"t_max",      &s.gTMax,  0.1f, 50.f, 0.5f, "%.2f");
    ImGui::Separator();

    if (g.customTotalCount == 0) {
        ImGui::TextColored({1.f, 0.55f, 0.55f, 1.f},
            u8"⚠ Ни одна точка не попадает в заданные "
            u8"куски. Проверь границы — домены не покрывают [0, L]?");
    } else if (g.customNanCount > 0) {
        float frac = 100.f * float(g.customNanCount)
                          / float(std::max(1, g.customTotalCount));
        ImGui::TextColored({1.f, 0.75f, 0.35f, 1.f},
            u8"⚠ Формула не определена в %d из %d точек "
            u8"(%.0f%%). Возможно деление на 0 / log(≤0) / "
            u8"sqrt отрицательного.",
            g.customNanCount, g.customTotalCount, frac);
        ImGui::TextDisabled(
            u8"Типичная причина: целое значение a даёт sin(a·π)=0 "
            u8"в знаменателе. Сдвинь a на 0.1.");
    } else {
        int total = g.N + 1;
        float missFrac = float(g.customMissCount) / float(total);
        if (missFrac > 0.05f) {
            ImGui::TextColored({0.85f, 0.85f, 0.55f, 1.f},
                u8"i %.0f%% отрезка не покрыто кусками — там G = 0.",
                missFrac * 100.f);
        }
    }
    ImGui::Spacing();
    ImGui::TextDisabled(u8"Куски (проверяются по порядку):");
    int pieceToDelete = -1;
    for (int i = 0; i < (int)g.customPieces.size(); ++i) {
        auto& p = g.customPieces[i];
        ImGui::PushID(i);
        ImGui::Separator();
        ImGui::Text(u8"Кусок %d", i + 1);
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"− удалить")) pieceToDelete = i;
        boundExprInput(u8"Нижняя граница  x ≥", p.xLoExpr);
        boundExprInput(u8"Верхняя граница x ≤", p.xHiExpr);
        customExprInput(u8"Выражение G(x, ξ, t) =", p.expr,
                        u8"x, xi, t, a, L, pi, e");
        ImGui::Spacing();
        ImGui::PopID();
    }
    if (pieceToDelete >= 0 && g.customPieces.size() > 1)
        g.customPieces.erase(g.customPieces.begin() + pieceToDelete);
    if (ImGui::Button(u8"+ добавить кусок")) {
        GreenSolver::Piece np;
        // new piece starts where the last one ended
        std::string prevHi = g.customPieces.empty()
                             ? std::string("0")
                             : g.customPieces.back().xHiExpr.source;
        np.xLoExpr.source = prevHi;
        np.xHiExpr.source = "L";
        np.expr.source    = "0";
        np.xLoExpr.reparse();
        np.xHiExpr.reparse();
        np.expr.reparse();
        g.customPieces.push_back(std::move(np));
    }
    ImGui::Spacing();
    ImGui::TextWrapped(
        u8"Первый кусок, чей диапазон [xLo, xHi] содержит x, будет "
        u8"использован. Вне всех диапазонов — 0.\n"
        u8"Функции: sin cos tan exp log sqrt abs pow min max step.");
}

static void buildGreenLaplace(AppState& s) {
    auto& g = s.green;
    ImGui::TextColored({0.7f, 0.9f, 1.f, 1.f}, u8"G(r) =   где  r = |(x,y) − (ξ, η)|");
    prettyFormula("(1/(2*pi)) * log(1/r)");
    ImGui::Spacing();
    sliderF(u8"Область  L",  &g.L,   0.1f, 5.f, 0.1f, "%.2f");
    g.xi  = std::clamp(g.xi,  0.f, g.L);
    g.eta = std::clamp(g.eta, 0.f, g.L);
    sliderF(u8"Источник ξ",  &g.xi,  0.f, g.L, 0.02f, "%.3f");
    sliderF(u8"Источник η",  &g.eta, 0.f, g.L, 0.02f, "%.3f");
    ImGui::Spacing();
    ImGui::TextWrapped(
        u8"Фундаментальное решение уравнения Лапласа.\n"
        u8"Эквивалентно потенциалу 2D точечного заряда.\n"
        u8"Логарифмическая особенность в точке (ξ, η).");
}

void buildFourierTab(AppState& s) {
    if (!ImGui::BeginTabItem(u8"Фурье")) return;
    s.tab = TAB_FOURIER;
    auto& f = s.fourier;

    ImGui::Spacing();
    ImGui::TextColored({1.f, 0.85f, 0.4f, 1.f}, u8"Частичная сумма ряда Фурье по синусам:");
    prettyFormula("S_N(x) = sum(n, 1, N, b_n * sin(n*pi*x/L))",
                  {1.f, 0.9f, 0.6f, 1.f});
    ImGui::TextDisabled(u8"Коэффициенты:");
    prettyFormula("b_n = (2/L) * integral(x, 0, L, f(x) * sin(n*pi*x/L))");
    ImGui::Separator(); ImGui::Spacing();

    {
        int p = (int)f.preset;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##ic_f", &p, ICPresetNames, IM_ARRAYSIZE(ICPresetNames)))
            f.preset = (ICPreset)p;
        ImGui::TextDisabled(u8"  Функция f(x)");
    }
    if (f.preset == ICPreset::Custom) {
        if (customExprInput(u8"Ваше f(x):", f.customIC)) f.compute();
    }
    sliderF(u8"Длина  L", &f.L, 0.5f, 3.f, 0.1f, "%.2f");
    ImGui::Spacing();

    ImGui::Text(u8"Частичная сумма S_N(x):");
    if (sliderI(u8"Членов N", &f.nShow, 1, f.nMax, 1))
        f.compute();
    ImGui::Spacing();
    ImGui::Checkbox(u8"Отдельные гармоники", &f.showHarmonics);
    ImGui::Checkbox(u8"График ошибки  |f − S_N|", &f.showError);
    ImGui::Spacing();
    if (ImGui::Button(u8"Пересчитать", {120, 0})) f.compute();
    ImGui::SameLine();
    if (ImGui::Button(u8"Сброс вида", {110, 0})) { s.vFTop.reset(); s.vFBot.reset(); }
    ImGui::TextDisabled(u8"колесо — зум, ПКМ — пан, 2×ЛКМ — сброс");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (!f.error.empty()) {
        double e2sum = 0.0, emax = 0.0;
        for (float e : f.error) {
            e2sum += double(e) * e;
            if (e > emax) emax = e;
        }
        double l2 = std::sqrt(e2sum * (f.L / f.error.size()));
        ImGui::Text(u8"Ошибки при N = %d:", f.nShow);
        ImGui::BulletText(u8"max |f − S_N|  =  %.4f", emax);
        ImGui::BulletText(u8"‖ f − S_N ‖₂  =  %.4f", l2);
        ImGui::Spacing();
    }

    if (!f.coeffs.empty() &&
        ImGui::CollapsingHeader(u8"Спектр коэффициентов |bn|", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::vector<float> bars(f.nMax);
        for (int i = 0; i < f.nMax; ++i) bars[i] = float(std::fabs(f.coeffs[i]));
        float bmax = *std::max_element(bars.begin(), bars.end());
        ImGui::PlotHistogram("##sp", bars.data(), f.nMax,
                             0, nullptr, 0.f, bmax, {-1, 100});
        ImGui::TextDisabled(u8"n = 1 .. %d  (по оси X — номер гармоники)", f.nMax);
    }

    if (ImGui::CollapsingHeader(u8"Таблица коэффициентов")) {
        if (ImGui::BeginTable("##cf", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn("n");
            ImGui::TableSetupColumn(u8"bn");
            ImGui::TableHeadersRow();
            int rows = std::min(12, (int)f.coeffs.size());
            for (int i = 0; i < rows; ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i + 1);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%+ .5f", f.coeffs[i]);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader(u8"Легенда и пояснение", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored({1.f, 0.87f, 0.25f, 1.f}, u8"●  Жёлтый   — точная функция f(x)");
        ImGui::TextColored({0.35f, 0.75f, 1.f, 1.f}, u8"●  Голубой  — частичная сумма S_N(x)");
        ImGui::TextColored({1.f, 0.45f, 0.45f, 1.f}, u8"●  Красный  — график ошибки |f − S_N|");
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

void buildPanel(AppState& s) {
    ImGui::SetNextWindowPos ({0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({float(s.panelW), float(s.winH)}, ImGuiCond_Always);
    ImGui::Begin("##ctrl", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
        buildWaveTab   (s);
        buildHeatTab   (s);
        buildGreenTab  (s);
        buildFourierTab(s);
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace ui

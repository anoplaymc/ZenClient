// ZenClient - customizable main-menu UI and its settings tab.
// Included from ui.cpp inside its anonymous namespace (uses its fonts, colours and widgets).

// ================================================================== menu widgets for the settings
// Card with a title and a segmented control. title may be null (sub-tab strip).
bool SegRow(const char* id, const char* title, int* v, std::initializer_list<const char*> opts, int base = 0) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x, pad = 12 * S;
    int n = (int)opts.size();
    float segH = 32 * S;
    float titleH = title ? F_Body->FontSize + 8 * S : 0.f;
    float h = pad + titleH + segH + pad;
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 11 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 11 * S);
    if (title) Text(dl, F_Body, ImVec2(p.x + pad + 2 * S, p.y + pad), C_TEXT, title);
    float sx = p.x + pad, sy = p.y + pad + titleH, sw = w - pad * 2;
    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + segH), A(RGBA(13, 15, 20)), 8 * S);
    float cw = sw / n;
    int cur = *v - base;
    ImGui::PushID(id);
    float ia = Anim(ImGui::GetID("ind"), (float)cur, 16.f);
    if (cur >= 0 && cur < n)
        dl->AddRectFilled(ImVec2(sx + ia * cw + 3 * S, sy + 3 * S), ImVec2(sx + (ia + 1) * cw - 3 * S, sy + segH - 3 * S), A(Accent(0.92f)), 6 * S);
    bool changed = false;
    int i = 0;
    for (const char* o : opts) {
        ImGui::SetCursorScreenPos(ImVec2(sx + i * cw, sy));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##o", ImVec2(cw, segH)) && cur != i) { *v = i + base; changed = true; ConfigMarkDirty(); }
        float hv = Anim(ImGui::GetID("h"), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
        ImGui::PopID();
        float sel = std::clamp(1.f - std::fabs(ia - i), 0.f, 1.f);
        ImFont* f = TextSize(F_Body, o).x < cw - 10 * S ? F_Body : F_Small;
        ImVec2 ts = TextSize(f, o);
        ImU32 tc = LerpCol(LerpCol(C_DIM, C_TEXT, hv), OnAccent(), sel);
        dl->PushClipRect(ImVec2(sx + i * cw + 2 * S, sy), ImVec2(sx + (i + 1) * cw - 2 * S, sy + segH), true);
        Text(dl, f, ImVec2(sx + i * cw + (cw - ts.x) * 0.5f, sy + (segH - ts.y) * 0.5f), tc, o);
        dl->PopClipRect();
        ++i;
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, 6 * S));
    return changed;
}

bool ColorRow(const char* id, const char* title, float* col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x, h = 46 * S, pad = 14 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool click = ImGui::InvisibleButton("##c", ImVec2(w, h));
    float hv = Anim(ImGui::GetID("hv"), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(LerpCol(RGBA(24, 27, 34), RGBA(30, 34, 43), hv)), 11 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 11 * S);
    Text(dl, F_Body, ImVec2(p.x + pad, p.y + (h - F_Body->FontSize) * 0.5f), C_TEXT, title);
    ImVec2 sa(p.x + w - pad - 52 * S, p.y + 10 * S), sb(p.x + w - pad, p.y + h - 10 * S);
    dl->AddRectFilled(sa, sb, A(ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1))), 7 * S);
    dl->AddRect(sa, sb, A(RGBA(255, 255, 255, 0.25f + 0.3f * hv)), 7 * S);
    if (click) ImGui::OpenPopup("##pick");
    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * S, 14 * S));
    if (ImGui::BeginPopup("##pick")) {
        ImGui::PushFont(F_Semi);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        ImGui::SetNextItemWidth(230 * S);
        if (ImGui::ColorPicker3("##p", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoAlpha)) {
            changed = true;
            ConfigMarkDirty();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 2 * S));
    return changed;
}

bool TextRow(const char* id, const char* title, std::string* s, const char* hint) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x, h = 50 * S, pad = 14 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 11 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 11 * S);
    Text(dl, F_Body, ImVec2(p.x + pad, p.y + (h - F_Body->FontSize) * 0.5f), C_TEXT, title);
    char buf[256];
    snprintf(buf, sizeof buf, "%s", s->c_str());
    float iw = w * 0.56f;
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - pad - iw, p.y + 8 * S));
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.08f, 0.10f, g_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.09f, 0.10f, 0.13f, g_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.11f, 0.14f, g_alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * S, (h - 16 * S - F_Body->FontSize) * 0.5f));
    ImGui::SetNextItemWidth(iw);
    bool changed = ImGui::InputTextWithHint("##t", hint, buf, sizeof buf);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopID();
    if (changed) { *s = buf; ConfigMarkDirty(); }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, 6 * S));
    return changed;
}

bool IntSliderRow(const char* id, const char* title, int* v, int mn, int mx) {
    float f = (float)*v;
    char fmt[8] = "%.0f";
    if (SliderRow(id, title, &f, (float)mn, (float)mx, fmt)) {
        int nv = (int)std::lround(f);
        if (nv != *v) { *v = nv; return true; }
    }
    return false;
}

// Percent slider for a 0..1 value.
bool PctRow(const char* id, const char* title, float* v, float mn = 0.f, float mx = 1.f) {
    float p = *v * 100.f;
    if (SliderRow(id, title, &p, mn * 100.f, mx * 100.f, "%.0f%%")) { *v = p / 100.f; return true; }
    return false;
}

// ================================================================== custom UI: keys & rules
std::string HashHex(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    char b[20];
    snprintf(b, sizeof b, "%016llx", (unsigned long long)h);
    return b;
}
std::string ElemKey(const UiElem& e) { return HashHex(std::to_string((int)e.kind) + "|" + e.group + "|" + e.label); }
std::string PageGroup(const std::vector<UiElem>& v) {
    std::unordered_map<std::string, int> cnt;
    std::string best;
    int bc = 0;
    for (auto& e : v) if (int c = ++cnt[e.group]; c > bc) { bc = c; best = e.group; }
    return best;
}
std::string PageKey(const std::vector<UiElem>& v) { return HashHex("page|" + PageGroup(v)); }

const char* PlaceName(int p) {
    static const char* n[] = {"Авто", "Список", "Внизу", "Всплывашка", "Скрыто"};
    return n[std::clamp(p, 0, 4)];
}
bool RuleIsDefault(const CuiElemRule& r) { return r.place == CP_Auto && r.order == 0 && !r.accent && !r.wide && r.label.empty(); }

// ================================================================== custom UI: presets
void CuiPreset(int i) {
    CuiResetSettings();
    Config& c = g_cfg;
    switch (i) {
    case 1:  // compact
        c.cuiPanelW = 0.30f; c.cuiScale = 0.9f; c.cuiItemH = 36; c.cuiGap = 3; c.cuiRadius = 8; c.cuiBtnStyle = 1;
        c.cuiArrows = false; c.cuiPrimaryH = 44; c.cuiTitleScale = 0.7f; c.cuiTag = false;
        break;
    case 2:  // centre
        c.cuiLayout = 2; c.cuiHeaderAlign = 1; c.cuiAlign = 1; c.cuiPanelW = 0.42f; c.cuiBtnStyle = 3; c.cuiArrows = false;
        c.cuiBar = false; c.cuiSubMode = 1; c.cuiPrimaryStyle = 1; c.cuiDim = 0.4f; c.cuiPageAnim = 1;
        break;
    case 3:  // floating card
        c.cuiLayout = 3; c.cuiPanelW = 0.34f; c.cuiBg = 2; c.cuiBtnStyle = 5; c.cuiRadius = 16; c.cuiSubMode = 1;
        c.cuiPopupAnim = 2; c.cuiAutoMore = true; c.cuiTitleScale = 0.8f; c.cuiPanelAlpha = 0.85f;
        break;
    case 4:  // console
        c.cuiLayout = 2; c.cuiScale = 1.2f; c.cuiItemH = 58; c.cuiBtnStyle = 4; c.cuiAlign = 1; c.cuiHeaderAlign = 1;
        c.cuiPrimaryStyle = 3; c.cuiPulse = true; c.cuiDim = 0.55f; c.cuiBg = 3; c.cuiSubMode = 2; c.cuiArrows = false;
        c.cuiBar = false; c.cuiPrimaryH = 66; c.cuiPopupAnim = 1;
        break;
    case 5:  // tiles
        c.cuiColumns = 2; c.cuiItemH = 74; c.cuiAlign = 1; c.cuiArrows = false; c.cuiPanelW = 0.5f; c.cuiSubMode = 3;
        c.cuiRadius = 16; c.cuiGap = 10; c.cuiBar = false;
        break;
    default: break;
    }
    ConfigMarkDirty();
}

// ================================================================== settings tab
std::string g_editKey, g_editPageKey;  // element / screen open in the editor

void TabInterface() {
    SectionLabel("ГЛАВНОЕ МЕНЮ");
    ToggleRow("cui", "Кастомный интерфейс",
              "Главное меню игры превращается в современное приложение. Все кнопки, переключатели и настройки игры остаются доступны",
              &g_cfg.customUi);
    ToggleRow("cuied", "Режим редактора",
              "Нажмите на любой элемент меню игры: его можно скрыть, переименовать, выделить, закрепить внизу, поднять выше или убрать во всплывашку",
              &g_cfg.cuiEdit);

    SectionLabel("ГОТОВЫЕ СТИЛИ");
    {
        static const char* names[] = {"Стандарт", "Компакт", "По центру", "Карточка", "Консоль", "Плитки"};
        float w = ImGui::GetContentRegionAvail().x, bw = (w - 2 * 8 * S) / 3;
        for (int i = 0; i < 6; ++i) {
            if (i % 3) ImGui::SameLine(0, 8 * S);
            char id[16]; snprintf(id, sizeof id, "pre%d", i);
            if (Button(id, names[i], ImVec2(bw, 36 * S), false, F_Body)) { CuiPreset(i); Toast(std::string("Стиль: ") + names[i], 1); }
        }
        ImGui::Dummy(ImVec2(0, 8 * S));
    }

    SectionLabel("НАСТРОЙКИ");
    SegRow("sub", nullptr, &g_cfg.cuiSubTab, {"Макет", "Фон", "Шапка", "Кнопки", "Экраны", "Анимация", "Правила"});
    ImGui::Dummy(ImVec2(0, 2 * S));
    Config& c = g_cfg;
    switch (c.cuiSubTab) {
    case 0:
        SegRow("lay", "Расположение", &c.cuiLayout, {"Слева", "Справа", "По центру", "Карточка"});
        PctRow("pw", "Ширина панели", &c.cuiPanelW, 0.2f, 1.f);
        PctRow("sc", "Размер элементов и текста", &c.cuiScale, 0.6f, 1.6f);
        SliderRow("ox", "Сдвиг по горизонтали", &c.cuiOffX, -600.f, 600.f, "%.0f px");
        SliderRow("oy", "Сдвиг по вертикали", &c.cuiOffY, -300.f, 300.f, "%.0f px");
        IntSliderRow("col", "Колонки (сетка кнопок)", &c.cuiColumns, 1, 4);
        break;
    case 1:
        SegRow("bg", "Фон панели", &c.cuiBg, {"Градиент", "Сплошной", "Стекло", "Нет"});
        PctRow("pa", "Непрозрачность панели", &c.cuiPanelAlpha);
        ColorRow("pc", "Цвет панели", c.cuiPanelCol);
        PctRow("dim", "Затемнение сцены", &c.cuiDim);
        PctRow("glow", "Акцентное свечение", &c.cuiGlow, 0.f, 0.4f);
        ToggleRow("oac", "Свой акцент для меню", "Отдельный цвет только для кастомного интерфейса", &c.cuiOwnAccent);
        if (BeginExpand("##oacsub", c.cuiOwnAccent, 54 * S)) {
            ColorRow("acc", "Акцент интерфейса", c.cuiAccent);
            EndExpand();
        }
        break;
    case 2:
        ToggleRow("hdr", "Заголовок", "Надпись и название игры над кнопками", &c.cuiHeader);
        if (BeginExpand("##hdrsub", c.cuiHeader, 5 * 56 * S + 64 * S + 30 * S)) {
            ToggleRow("tag", "Строка над заголовком", nullptr, &c.cuiTag, true);
            TextRow("tagt", "Текст строки", &c.cuiTagText, "Z E N C L I E N T");
            TextRow("t1", "Заголовок", &c.cuiTitle1, "Granny");
            TextRow("t2", "Вторая часть (акцент)", &c.cuiTitle2, "Legacy");
            SliderRow("ts", "Размер заголовка", &c.cuiTitleScale, 0.3f, 1.6f, "x%.2f");
            SegRow("ha", "Выравнивание шапки", &c.cuiHeaderAlign, {"Слева", "По центру"});
            EndExpand();
        }
        ToggleRow("coin", "Баланс монет", "Капсула с Granny Coin под заголовком", &c.cuiCoins);
        ToggleRow("back", "Кнопка «Назад»", "На вложенных экранах (Esc работает всегда)", &c.cuiBackBtn);
        ToggleRow("hint", "Подсказка внизу", "Горячие клавиши в нижней части экрана", &c.cuiHint);
        break;
    case 3:
        SegRow("bs", "Стиль кнопок", &c.cuiBtnStyle, {"Карта", "Плоский", "Контур", "Пилюля", "Минимал", "Стекло"});
        SliderRow("ih", "Высота кнопок", &c.cuiItemH, 30.f, 90.f, "%.0f px");
        SliderRow("gap", "Отступ между кнопками", &c.cuiGap, 0.f, 30.f, "%.0f px");
        SliderRow("rad", "Скругление", &c.cuiRadius, 0.f, 45.f, "%.0f px");
        PctRow("ia", "Непрозрачность кнопок", &c.cuiItemAlpha);
        SegRow("al", "Текст кнопок", &c.cuiAlign, {"Слева", "По центру"});
        ToggleRow("arr", "Стрелки справа", nullptr, &c.cuiArrows, true);
        ToggleRow("bar", "Акцентная полоска слева", nullptr, &c.cuiBar, true);
        ToggleRow("hs", "Сдвиг текста при наведении", nullptr, &c.cuiHoverShift, true);
        ToggleRow("grp", "Заголовки групп", nullptr, &c.cuiGroups, true);
        ImGui::Dummy(ImVec2(0, 6 * S));
        SectionLabel("ГЛАВНАЯ КНОПКА («ИГРАТЬ», «НАЧАТЬ»)");
        SegRow("pp", "Где показывать", &c.cuiPrimaryPlace, {"Закреплена внизу", "Первой в списке", "Как в игре"});
        SegRow("ps", "Стиль", &c.cuiPrimaryStyle, {"Заливка", "Градиент", "Контур", "Неон"});
        SliderRow("ph", "Высота", &c.cuiPrimaryH, 36.f, 110.f, "%.0f px");
        ToggleRow("pul", "Пульсирующее свечение", nullptr, &c.cuiPulse, true);
        break;
    case 4:
        SegRow("sm", "Вложенные экраны (после «Играть», «Настройки»…)", &c.cuiSubMode, {"Списком", "Всплывашка", "Шторка снизу", "Панель сбоку"});
        SegRow("pan", "Анимация всплывашки", &c.cuiPopupAnim, {"Масштаб", "Снизу", "Пружина", "Плавно", "Сбоку"});
        SliderRow("pw2", "Ширина всплывашки", &c.cuiPopupW, 340.f, 1400.f, "%.0f px");
        SliderRow("pr", "Скругление всплывашки", &c.cuiPopupR, 0.f, 48.f, "%.0f px");
        PctRow("pd", "Затемнение под всплывашкой", &c.cuiPopupDim, 0.f, 0.95f);
        ToggleRow("po", "Клик мимо — закрыть", "Клик вне всплывашки работает как «Назад»", &c.cuiPopupOutside);
        ToggleRow("ppi", "«Играть» внизу всплывашки", "Иначе главная кнопка будет над настройками", &c.cuiPopupPrimaryIn);
        ImGui::Dummy(ImVec2(0, 6 * S));
        SectionLabel("ВСПЛЫВАШКА «ДОПОЛНИТЕЛЬНО»");
        ToggleRow("am", "Настройки рядом с «Играть» — во всплывашку",
                  "На экранах с главной кнопкой переключатели, ползунки и списки прячутся за кнопку-шестерёнку и открываются в скруглённом окне",
                  &c.cuiAutoMore);
        TextRow("ml", "Название всплывашки", &c.cuiMoreLabel, "Дополнительно");
        break;
    case 5:
        SegRow("pga", "Переход между экранами", &c.cuiPageAnim, {"Сдвиг", "Подъём", "Плавно", "Без анимации"});
        PctRow("pgs", "Скорость переходов", &c.cuiPageSpeed, 0.3f, 3.f);
        ToggleRow("stg", "Каскадное появление", "Кнопки появляются по очереди", &c.cuiStagger);
        if (BeginExpand("##stgsub", c.cuiStagger, 62 * S)) {
            float ms = c.cuiStaggerDelay * 1000.f;
            if (SliderRow("sd", "Задержка между кнопками", &ms, 0.f, 150.f, "%.0f мс")) c.cuiStaggerDelay = ms / 1000.f;
            EndExpand();
        }
        break;
    case 6: {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float w = ImGui::GetContentRegionAvail().x;
        const char* info = "Правила создаются в режиме редактора: включите его и нажмите на кнопку или настройку в меню игры.";
        ImVec2 p = ImGui::GetCursorScreenPos();
        float ih = WrappedHeight(F_Small, info, w - 28 * S) + 20 * S;
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + ih), A(Accent(0.08f)), 11 * S);
        TextWrapped(dl, F_Small, ImVec2(p.x + 14 * S, p.y + 10 * S), C_DIM, info, w - 28 * S);
        ImGui::Dummy(ImVec2(w, ih + 6 * S));
        std::string eraseKey;
        bool erasePage = false;
        auto ruleRow = [&](const std::string& key, const std::string& name, const std::string& what, bool page) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            float rh = 50 * S;
            dl->AddRectFilled(rp, ImVec2(rp.x + w, rp.y + rh), A(RGBA(24, 27, 34)), 11 * S);
            dl->PushClipRect(rp, ImVec2(rp.x + w - 110 * S, rp.y + rh), true);
            Text(dl, F_Semi, ImVec2(rp.x + 14 * S, rp.y + 7 * S), C_TEXT, name.c_str());
            Text(dl, F_Small, ImVec2(rp.x + 14 * S, rp.y + 27 * S), C_DIM, what.c_str());
            dl->PopClipRect();
            ImGui::SetCursorScreenPos(ImVec2(rp.x + w - 100 * S, rp.y + 10 * S));
            if (Button(("x" + key).c_str(), "Удалить", ImVec2(90 * S, 30 * S), false, F_Small)) { eraseKey = key; erasePage = page; }
            ImGui::SetCursorScreenPos(ImVec2(rp.x, rp.y + rh + 6 * S));
            ImGui::Dummy(ImVec2(w, 0));
        };
        if (g_cuiRules.empty() && g_cuiPages.empty()) SectionLabel("ПРАВИЛ ПОКА НЕТ");
        for (auto& [k, r] : g_cuiRules) {
            std::string what = std::string(PlaceName(r.place));
            if (!r.label.empty()) what += "  ·  «" + r.label + "»";
            if (r.order) what += "  ·  порядок " + std::to_string(r.order);
            if (r.accent) what += "  ·  акцент";
            if (r.wide) what += "  ·  широкая";
            ruleRow(k, r.orig.empty() ? "Элемент" : r.orig, what, false);
        }
        static const char* modes[] = {"Списком", "Всплывашка", "Шторка", "Панель сбоку"};
        for (auto& [k, r] : g_cuiPages) {
            std::string what = r.mode < 0 ? "Как в настройках" : modes[std::clamp(r.mode, 0, 3)];
            ruleRow(k, "Экран" + (r.title.empty() ? std::string() : ": " + r.title), what, true);
        }
        if (!eraseKey.empty()) {
            if (erasePage) g_cuiPages.erase(eraseKey); else g_cuiRules.erase(eraseKey);
            ConfigMarkDirty();
        }
        if (!g_cuiRules.empty() || !g_cuiPages.empty()) {
            ImGui::Dummy(ImVec2(0, 4 * S));
            if (Button("clrrules", "Удалить все правила", ImVec2(210 * S, 36 * S), false, F_Body)) {
                g_cuiRules.clear(); g_cuiPages.clear(); ConfigMarkDirty(); Toast("Правила удалены", 0);
            }
        }
        break;
    }
    }
    ImGui::Dummy(ImVec2(0, 8 * S));
    if (Button("cuireset", "Сбросить всё", ImVec2(230 * S, 36 * S), false, F_Body)) {
        CuiResetSettings();          // all cui_* settings
        g_cuiRules.clear();          // editor: per-element rules
        g_cuiPages.clear();          // editor: per-screen rules
        g_editKey.clear();
        g_editPageKey.clear();
        ConfigMarkDirty();
        Toast("Кастомный интерфейс сброшен: настройки и правила редактора", 0);
    }
}

// ================================================================== custom UI: page tracking
// The game's menu is mirrored from snapshots taken on the Unity thread every ~0.1 s. To keep the
// presentation smooth the UI works on a "committed page":
//  * elements are keyed by their object address, so hover/press/toggle animations never reset;
//  * a user change is shown immediately (optimistic override) until the game confirms it;
//  * a different set of elements (another menu screen) is committed only after it has been stable
//    briefly, then animates in;
//  * short empty gaps while the game swaps panels never flash a loading screen.
struct UiOverride { bool on; float value; float ttl; };
std::unordered_map<uintptr_t, UiOverride> g_uiOv;
std::vector<UiElem> g_page;                 // committed page
std::vector<std::vector<uintptr_t>> g_pageHistory;
std::vector<std::string> g_titleStack;      // label of the button that opened each deeper screen
std::vector<uintptr_t> g_pendingIds;
std::vector<UiElem> g_rootCopy;             // last first-level screen (drawn under popups)
std::string g_lastPress;
float g_pendingTime = 0.f, g_emptyTime = 0.f, g_pageTime = 0.f;
float g_pageAnim = 1.f;
int g_pageDir = 1;
bool g_pageScrollReset = false, g_wasInCustom = false, g_moreOpen = false;

std::vector<uintptr_t> IdsOf(const std::vector<UiElem>& v) {
    std::vector<uintptr_t> ids;
    for (auto& e : v) ids.push_back(e.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
float Overlap(const std::vector<uintptr_t>& a, const std::vector<uintptr_t>& b) {
    if (a.empty() || b.empty()) return 0.f;
    size_t i = 0, j = 0, common = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) { ++common; ++i; ++j; } else if (a[i] < b[j]) ++i; else ++j;
    }
    return (float)common / (float)std::max(a.size(), b.size());
}

void CommitPage(const std::vector<UiElem>& snap, int dir) {
    g_page = snap;
    g_pageDir = dir;
    g_pageAnim = g_cfg.cuiPageAnim == 3 ? 1.f : 0.f;
    g_pageTime = 0.f;
    g_pageScrollReset = true;
    g_pendingIds.clear();
    g_moreOpen = false;
}

void UpdatePage(const std::vector<UiElem>& snap) {
    auto cur = IdsOf(g_page), next = IdsOf(snap);
    if (snap.empty()) {
        g_emptyTime += g_dt;
        if (g_emptyTime > 0.35f && !g_page.empty()) { g_page.clear(); g_pageAnim = 0.f; }
        return;
    }
    g_emptyTime = 0.f;
    if (g_page.empty() || Overlap(cur, next) >= 0.5f) {  // same screen: update values/labels in place
        if (g_page.empty()) CommitPage(snap, 1);
        else { g_page = snap; g_pendingIds.clear(); }
        return;
    }
    // another screen: wait until it is stable for a moment (the game animates between panels)
    if (g_pendingIds != next) { g_pendingIds = next; g_pendingTime = 0.f; return; }
    g_pendingTime += g_dt;
    if (g_pendingTime < 0.12f) return;
    bool back = !g_pageHistory.empty() && Overlap(g_pageHistory.back(), next) >= 0.5f;
    if (back) {
        g_pageHistory.pop_back();
        if (!g_titleStack.empty()) g_titleStack.pop_back();
    } else {
        g_pageHistory.push_back(cur);
        g_titleStack.push_back(g_lastPress);
    }
    if (g_pageHistory.size() > 16) { g_pageHistory.erase(g_pageHistory.begin()); g_titleStack.erase(g_titleStack.begin()); }
    g_lastPress.clear();
    CommitPage(snap, back ? -1 : 1);
}

void TickOverrides() {
    for (auto it = g_uiOv.begin(); it != g_uiOv.end();) {
        it->second.ttl -= g_dt;
        if (it->second.ttl <= 0) it = g_uiOv.erase(it); else ++it;
    }
}

void SendEscape() {
    g_escPassUntil = GetTickCount64() + 400;  // let this one reach the game
    INPUT in[2]{};
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wVk = in[1].ki.wVk = VK_ESCAPE;
    in[0].ki.wScan = in[1].ki.wScan = 0x01;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

bool IsBackLabel(const std::string& l) {
    std::string k;
    for (char c : l) k += (char)tolower((unsigned char)c);
    return l == "Назад" || l == "Закрыть" || l == "←  Назад" || k == "back" || k == "close" || k == "return" ||
           k.rfind("back ", 0) == 0 || k.rfind("close ", 0) == 0;
}

float g_backCooldown = 0.f;

// One "back" for every control (popup X, Back button, click outside, Escape):
// closes editor popups / the "More" popup first, then presses the screen's own Back/Close button,
// and only if the screen has none sends Escape to the game.
void GoBack() {
    if (g_backCooldown > 0.f) return;
    g_backCooldown = 0.3f;
    if (ImGui::GetCurrentContext()->OpenPopupStack.Size > 0) { ImGui::ClosePopupToLevel(0, true); return; }
    if (g_moreOpen) { g_moreOpen = false; return; }
    for (auto& el : g_page)
        if (el.kind == UiKind::Button && IsBackLabel(el.label)) {
            LOG("custom ui back: pressing '%s'", el.label.c_str());
            ActionUiPress(el);
            return;
        }
    if (g_pageHistory.empty()) return;  // first screen: nothing to go back to
    LOG("custom ui back: escape");
    SendEscape();
}

// ================================================================== custom UI: classification
struct ElemView {
    UiElem el;
    std::string key, label;
    int place = CP_List, order = 0, idx = 0;
    bool accent = false, wide = false, hidden = false, primary = false;
};
struct PageView { std::vector<ElemView> list, pinned, more; };

PageView Classify(const std::vector<UiElem>& page) {
    PageView pv;
    bool hasPrimary = false;
    for (auto& e : page) if (e.kind == UiKind::Button && e.primary) hasPrimary = true;
    for (auto& e : page) {
        ElemView v;
        v.el = e;
        v.key = ElemKey(e);
        v.primary = e.kind == UiKind::Button && e.primary;
        auto it = g_cuiRules.find(v.key);
        const CuiElemRule* r = it == g_cuiRules.end() ? nullptr : &it->second;
        v.label = r && !r->label.empty() ? r->label : !e.label.empty() ? e.label : v.primary ? "Играть" : "Кнопка";
        v.place = r ? r->place : CP_Auto;
        v.order = r ? r->order : 0;
        v.accent = r && r->accent;
        v.wide = r && r->wide;
        if (v.place == CP_Auto) {
            if (v.primary) v.place = g_cfg.cuiPrimaryPlace == 0 ? CP_Pinned : CP_List;
            else if (g_cfg.cuiAutoMore && hasPrimary && e.kind != UiKind::Button) v.place = CP_More;
            else v.place = CP_List;
            if (v.primary && g_cfg.cuiPrimaryPlace == 1) v.order -= 1000;
        }
        if (v.place == CP_Hidden) {
            if (!g_cfg.cuiEdit) continue;
            v.hidden = true;
            v.place = CP_List;
        }
        (v.place == CP_Pinned ? pv.pinned : v.place == CP_More ? pv.more : pv.list).push_back(std::move(v));
    }
    auto byOrder = [](const ElemView& a, const ElemView& b) { return a.order < b.order; };
    std::stable_sort(pv.list.begin(), pv.list.end(), byOrder);
    std::stable_sort(pv.pinned.begin(), pv.pinned.end(), byOrder);
    std::stable_sort(pv.more.begin(), pv.more.end(), byOrder);
    return pv;
}

// ================================================================== custom UI: drawing primitives
inline float CK() { return S * g_cfg.cuiScale; }
inline float FS(ImFont* f) { return f->FontSize * g_cfg.cuiScale; }
ImVec2 TS(ImFont* f, const char* t) { return f->CalcTextSizeA(FS(f), FLT_MAX, 0.f, t); }
void TX(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* t) { dl->AddText(f, FS(f), p, A(c), t); }
inline float EaseOutBack(float t) { const float c1 = 1.70158f, c3 = c1 + 1.f; float x = t - 1.f; return 1.f + c3 * x * x * x + c1 * x * x; }
ImU32 PanelCol(float a, float lighten = 0.f) {
    const float* c = g_cfg.cuiPanelCol;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0] + (1 - c[0]) * lighten, c[1] + (1 - c[1]) * lighten, c[2] + (1 - c[2]) * lighten, a));
}

float ItemRadius(float h) { return g_cfg.cuiBtnStyle == 3 ? h * 0.5f : std::min(g_cfg.cuiRadius * CK(), h * 0.5f); }

// Background of a regular element in the selected style. on = toggle state (accent tint).
void ItemFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, float hv, bool accent, float on = 0.f) {
    float K = CK(), r = ItemRadius(b.y - a.y), ia = g_cfg.cuiItemAlpha;
    int st = g_cfg.cuiBtnStyle;
    switch (st) {
    case 1:  // flat
        dl->AddRectFilled(a, b, A(LerpCol(RGBA(255, 255, 255, 0.045f * ia), Accent(0.18f), hv)), r);
        break;
    case 2:  // outline
        if (hv > 0.01f) dl->AddRectFilled(a, b, A(Accent(0.08f * hv)), r);
        dl->AddRect(a, b, A(LerpCol(RGBA(255, 255, 255, 0.18f), Accent(0.85f), std::max(hv, on * 0.6f))), r, 0, 1.5f * K);
        break;
    case 4:  // minimal
        if (hv > 0.01f) dl->AddRectFilled(a, b, A(RGBA(255, 255, 255, 0.035f * hv)), r);
        break;
    case 5:  // glass
        dl->AddRectFilled(a, b, A(RGBA(255, 255, 255, (0.05f + 0.05f * hv) * (0.4f + ia * 0.6f))), r);
        GradientRect(dl, a, b, A(RGBA(255, 255, 255, 0.06f)), A(RGBA(255, 255, 255, 0.f)), r);
        dl->AddRect(a, b, A(LerpCol(RGBA(255, 255, 255, 0.12f), Accent(0.55f), hv)), r);
        break;
    default:  // card / pill
        dl->AddRectFilled(a, b, A(LerpCol(PanelCol(ia, 0.06f), PanelCol(std::min(1.f, ia + 0.1f), 0.11f), hv)), r);
        dl->AddRect(a, b, A(LerpCol(RGBA(255, 255, 255, 0.05f), Accent(0.55f), std::max(hv, on * 0.5f))), r);
        break;
    }
    if (accent) {
        dl->AddRectFilled(a, b, A(Accent(0.10f)), r);
        dl->AddRect(a, b, A(Accent(0.75f)), r, 0, 1.5f * K);
    }
    if (g_cfg.cuiBar && st != 3 && st != 4 && (b.y - a.y) > 20 * K)
        dl->AddRectFilled(ImVec2(a.x, a.y + (b.y - a.y) * 0.24f), ImVec2(a.x + 3 * K * (0.3f + std::max(hv, on)), b.y - (b.y - a.y) * 0.24f),
                          A(Accent(0.4f + 0.6f * std::max(hv, on))), 2 * K);
}

// Main call-to-action button frame + label.
void PrimaryFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, float hv, const char* label, ImFont* font) {
    float K = CK(), h = b.y - a.y;
    float r = g_cfg.cuiBtnStyle == 3 ? h * 0.5f : std::min(g_cfg.cuiRadius * K, h * 0.5f);
    float pulse = g_cfg.cuiPulse ? 0.5f + 0.5f * std::sin(g_time * 2.6f) : 0.f;
    float glow = std::max(hv, pulse * 0.8f);
    ImU32 tc = OnAccent();
    switch (g_cfg.cuiPrimaryStyle) {
    case 1: {  // vertical gradient
        if (glow > 0.01f) dl->AddRectFilled(ImVec2(a.x - 4 * K, a.y - 4 * K), ImVec2(b.x + 4 * K, b.y + 4 * K), A(Accent(0.16f * glow)), r + 4 * K);
        ImU32 top = AccentMix(0.22f + 0.12f * hv), bot = AccentMix(-0.32f + 0.1f * hv);
        GradientRect(dl, a, b, A(top), A(bot), r);
        dl->AddRect(a, b, A(RGBA(255, 255, 255, 0.14f)), r);
        break;
    }
    case 2:  // outline
        dl->AddRectFilled(a, b, A(Accent(0.08f + 0.12f * hv)), r);
        dl->AddRect(a, b, A(Accent(0.95f)), r, 0, 2 * K);
        if (glow > 0.01f) dl->AddRect(ImVec2(a.x - 3 * K, a.y - 3 * K), ImVec2(b.x + 3 * K, b.y + 3 * K), A(Accent(0.25f * glow)), r + 3 * K, 0, 2 * K);
        tc = AccentMix(0.35f);
        break;
    case 3:  // neon
        for (int i = 4; i >= 1; --i)
            dl->AddRectFilled(ImVec2(a.x - i * 3 * K, a.y - i * 3 * K), ImVec2(b.x + i * 3 * K, b.y + i * 3 * K), A(Accent(0.035f * (1.f + glow * 1.5f))), r + i * 3 * K);
        dl->AddRectFilled(a, b, A(RGBA(8, 9, 12, 0.92f)), r);
        dl->AddRect(a, b, A(AccentMix(0.15f * hv)), r, 0, 2 * K);
        tc = AccentMix(0.5f);
        break;
    default:  // fill
        if (glow > 0.01f) dl->AddRectFilled(ImVec2(a.x - 3 * K, a.y - 3 * K), ImVec2(b.x + 3 * K, b.y + 3 * K), A(Accent(0.18f * glow)), r + 3 * K);
        dl->AddRectFilled(a, b, A(AccentMix(0.10f * hv)), r);
        GradientRect(dl, a, b, A(RGBA(255, 255, 255, 0.13f + 0.06f * hv)), A(RGBA(255, 255, 255, 0.f)), r);
        break;
    }
    ImVec2 ts = TS(font, label);
    float x = g_cfg.cuiAlign == 0 && g_cfg.cuiPrimaryPlace == 2 ? a.x + 20 * K : (a.x + b.x - ts.x) * 0.5f;
    TX(dl, font, ImVec2(x, (a.y + b.y - ts.y) * 0.5f), tc, label);
}

void SwitchK(ImDrawList* dl, ImVec2 p, float t, float hover) {
    float K = CK(), w = 40 * K, h = 22 * K;
    ImU32 off = LerpCol(RGBA(44, 48, 58), RGBA(56, 61, 73), hover);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(LerpCol(off, Accent(), t)), h * 0.5f);
    float r = h * 0.5f - 3 * K, x = p.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(x, p.y + h * 0.5f + 1 * K), r + 1 * K, A(IM_COL32(0, 0, 0, 60)), 24);
    dl->AddCircleFilled(ImVec2(x, p.y + h * 0.5f), r, A(IM_COL32(255, 255, 255, 255)), 24);
}

// ================================================================== custom UI: editor
const ImGuiID kEditPopup = 0x5a43e001, kPagePopup = 0x5a43e002;
UiElem g_editEl{};
bool g_editWasOpen = false, g_pageEdWasOpen = false;

void OpenEditor(const ElemView& v) {
    g_editKey = v.key;
    g_editEl = v.el;
    ImGui::OpenPopupEx(kEditPopup);
}

bool g_interactive = true;   // element presses are executed (false for stale/blocked content)
// Returns true when a press should reach the game (in editor mode it opens the editor instead).
bool Act(bool pressed, const ElemView& v) {
    if (!pressed || !g_interactive || g_pageAnim < 0.5f) return false;
    if (g_cfg.cuiEdit) { OpenEditor(v); return false; }
    if (v.el.kind == UiKind::Button) g_lastPress = v.label;
    return true;
}

void PopupStyleBegin() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * S, 16 * S));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 18 * S);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.06f, 0.07f, 0.09f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, AccentV(0.35f));
}
void PopupStyleEnd() { ImGui::PopStyleColor(2); ImGui::PopStyleVar(2); }

void DrawElemEditor() {
    bool open = ImGui::IsPopupOpen(kEditPopup, ImGuiPopupFlags_None);
    if (!open) {
        if (g_editWasOpen && !g_editKey.empty()) {
            auto it = g_cuiRules.find(g_editKey);
            if (it != g_cuiRules.end() && RuleIsDefault(it->second)) g_cuiRules.erase(it);
            ConfigMarkDirty();
            g_editKey.clear();
        }
        g_editWasOpen = false;
        return;
    }
    g_editWasOpen = true;
    float sa = g_alpha; g_alpha = 1.f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(460 * S, 0), ImVec2(460 * S, FLT_MAX));
    PopupStyleBegin();
    if (ImGui::BeginPopupEx(kEditPopup, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        CuiElemRule& r = g_cuiRules[g_editKey];
        r.orig = g_editEl.label;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        static const char* kinds[] = {"Кнопка", "Переключатель", "Ползунок", "Список"};
        ImVec2 p = ImGui::GetCursorScreenPos();
        Text(dl, F_Small, p, Accent(), (std::string("РЕДАКТОР  ·  ") + kinds[(int)g_editEl.kind]).c_str());
        Text(dl, F_Title, ImVec2(p.x, p.y + 18 * S), C_TEXT, g_editEl.label.empty() ? "Без названия" : g_editEl.label.c_str());
        ImGui::Dummy(ImVec2(0, 50 * S));
        SegRow("pl", "Где показывать", &r.place, {"Авто", "Список", "Внизу", "Всплывашка", "Скрыть"});
        TextRow("lb", "Своё название", &r.label, g_editEl.label.c_str());
        IntSliderRow("ord", "Порядок (меньше — выше)", &r.order, -10, 10);
        ToggleRow("acc", "Выделить акцентом", nullptr, &r.accent, true);
        ToggleRow("wd", "Во всю ширину (для сетки)", nullptr, &r.wide, true);
        ImGui::Dummy(ImVec2(0, 6 * S));
        float w = ImGui::GetContentRegionAvail().x, bw = (w - 16 * S) / 3;
        if (Button("run", "Нажать в игре", ImVec2(bw, 38 * S), false, F_Body)) {
            // use the live element (handles of older snapshots are released)
            for (auto& el : g_page)
                if (ElemKey(el) == g_editKey) {
                    g_lastPress = r.label.empty() ? el.label : r.label;
                    ActionUiPress(el);
                    break;
                }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 8 * S);
        if (Button("rst", "Сбросить", ImVec2(bw, 38 * S), false, F_Body)) { std::string o = r.orig; r = CuiElemRule{}; r.orig = o; }
        ImGui::SameLine(0, 8 * S);
        if (Button("ok", "Готово", ImVec2(bw, 38 * S), true, F_Body)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    PopupStyleEnd();
    g_alpha = sa;
}

void DrawPageEditor() {
    bool open = ImGui::IsPopupOpen(kPagePopup, ImGuiPopupFlags_None);
    if (!open) {
        if (g_pageEdWasOpen && !g_editPageKey.empty()) {
            auto it = g_cuiPages.find(g_editPageKey);
            if (it != g_cuiPages.end() && it->second.mode < 0 && it->second.title.empty()) g_cuiPages.erase(it);
            ConfigMarkDirty();
        }
        g_pageEdWasOpen = false;
        return;
    }
    g_pageEdWasOpen = true;
    float sa = g_alpha; g_alpha = 1.f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(520 * S, 0), ImVec2(520 * S, FLT_MAX));
    PopupStyleBegin();
    if (ImGui::BeginPopupEx(kPagePopup, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        CuiPageRule& r = g_cuiPages[g_editPageKey];
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        Text(dl, F_Small, p, Accent(), "РЕДАКТОР  ·  ЭТОТ ЭКРАН");
        Text(dl, F_Title, ImVec2(p.x, p.y + 18 * S), C_TEXT, "Как показывать экран");
        ImGui::Dummy(ImVec2(0, 50 * S));
        SegRow("md", "Вид", &r.mode, {"Как в настр.", "Списком", "Всплывашка", "Шторка", "Сбоку"}, -1);
        TextRow("tt", "Заголовок окна", &r.title, "по кнопке, которая его открыла");
        ImGui::Dummy(ImVec2(0, 4 * S));
        float w = ImGui::GetContentRegionAvail().x;
        if (Button("ok", "Готово", ImVec2(w, 38 * S), true, F_Body)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    PopupStyleEnd();
    g_alpha = sa;
}

// ================================================================== custom UI: elements
float ElemH(const ElemView& v, bool pinned) {
    float K = CK();
    if (v.el.kind == UiKind::Button && (pinned || v.primary)) return g_cfg.cuiPrimaryH * K;
    if (v.el.kind == UiKind::Slider) return std::max(g_cfg.cuiItemH, 54.f) * K;
    return g_cfg.cuiItemH * K;
}

// Draws one element at p (screen) with width w. Handles presses, overrides and the editor.
void DrawElem(ElemView& v, ImVec2 p, float w, bool pinned) {
    float K = CK();
    const UiElem& el = v.el;
    ImGui::PushID(reinterpret_cast<void*>(el.id));
    // items that appear inside an already visible screen fade in
    ImGuiID apId = ImGui::GetID("##appear");
    if (g_anim.find(apId) == g_anim.end()) g_anim[apId] = g_pageAnim < 0.99f ? 1.f : 0.f;
    float ap = EaseOut(Anim(apId, 1.f, 10.f));
    // cascade
    float st = 1.f;
    if (g_cfg.cuiStagger && g_cfg.animations && g_cfg.cuiPageAnim != 3) {
        float tt = g_pageTime * g_cfg.animSpeed * g_cfg.cuiPageSpeed - v.idx * g_cfg.cuiStaggerDelay;
        st = EaseOut(std::clamp(tt / 0.25f, 0.f, 1.f));
    }
    ImVec2 off(0, 0);
    if (g_cfg.cuiPageAnim == 0) off.x = (1 - st) * 18 * K * (float)g_pageDir;
    else if (g_cfg.cuiPageAnim == 1) off.y = (1 - st) * 16 * K;
    float sa = g_alpha;
    g_alpha = sa * ap * st * (v.hidden ? 0.4f : 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(g_alpha, 0.001f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = ElemH(v, pinned);
    ImGui::SetCursorScreenPos(p);
    ImVec2 a(p.x + off.x, p.y + off.y), b(a.x + w, a.y + h);
    bool edit = g_cfg.cuiEdit;
    auto ov = g_uiOv.find(el.id);
    const char* label = v.label.c_str();
    std::string hiddenLabel;
    if (v.hidden) { hiddenLabel = v.label + "  (скрыто)"; label = hiddenLabel.c_str(); }
    float hvOut = 0.f;

    if (el.kind == UiKind::Button) {
        bool pressed = ImGui::InvisibleButton("##btn", ImVec2(w, h));
        float hv = Anim(ImGui::GetID("hv"), ImGui::IsItemHovered() ? 1.f : 0.f, 15.f);
        float pr = Anim(ImGui::GetID("pr"), ImGui::IsItemActive() ? 1.f : 0.f, 30.f);
        hvOut = hv;
        ImVec2 aa(a.x + pr * 2 * K, a.y + pr * 1 * K), bb(b.x - pr * 2 * K, b.y - pr * 1 * K);
        if (pinned || v.primary) {
            PrimaryFrame(dl, aa, bb, hv, label, F_Title);
            if (v.accent) dl->AddRect(aa, bb, A(RGBA(255, 255, 255, 0.6f)), ItemRadius(h), 0, 1.5f * K);
        } else {
            ItemFrame(dl, aa, bb, hv, v.accent);
            ImVec2 ts = TS(F_Semi, label);
            float shift = g_cfg.cuiHoverShift ? hv * 8 * K : 0.f;
            float tx = g_cfg.cuiAlign == 0 ? aa.x + 18 * K + shift : (aa.x + bb.x - ts.x) * 0.5f;
            float ty = (aa.y + bb.y - ts.y) * 0.5f;
            dl->PushClipRect(aa, ImVec2(bb.x - (g_cfg.cuiArrows ? 30 * K : 6 * K), bb.y), true);
            TX(dl, F_Semi, ImVec2(tx, ty), LerpCol(RGBA(220, 224, 232), C_TEXT, hv), label);
            dl->PopClipRect();
            if (g_cfg.cuiBtnStyle == 4 && hv > 0.01f)
                dl->AddRectFilled(ImVec2(tx, ty + ts.y + 3 * K), ImVec2(tx + ts.x * hv, ty + ts.y + 5 * K), A(Accent(hv)), 1 * K);
            if (g_cfg.cuiArrows) DrawIcon(dl, IC_ARROW, ImVec2(bb.x - 22 * K + hv * 4 * K, (aa.y + bb.y) * 0.5f), 9 * K, A(LerpCol(C_FAINT, Accent(), hv)));
        }
        if (Act(pressed, v)) ActionUiPress(el);
    } else if (el.kind == UiKind::Toggle) {
        bool on = el.on;
        if (ov != g_uiOv.end()) { if (ov->second.on == el.on && ov->second.ttl < 0.9f) g_uiOv.erase(ov); else on = ov->second.on; }
        bool pressed = ImGui::InvisibleButton("##tg", ImVec2(w, h));
        float hv = Anim(ImGui::GetID("hv"), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
        float t = Anim(ImGui::GetID("on"), on ? 1.f : 0.f, 14.f);
        hvOut = hv;
        ItemFrame(dl, a, b, hv, v.accent, t);
        float swX = b.x - 18 * K - 40 * K;
        ImVec2 ts = TS(F_Semi, label);
        float tx = g_cfg.cuiAlign == 0 || w < 200 * K ? a.x + 18 * K : (a.x + swX - ts.x) * 0.5f;
        dl->PushClipRect(a, ImVec2(swX - 6 * K, b.y), true);
        TX(dl, F_Semi, ImVec2(tx, (a.y + b.y - ts.y) * 0.5f), LerpCol(RGBA(214, 218, 226), C_TEXT, std::max(t, hv)), label);
        dl->PopClipRect();
        SwitchK(dl, ImVec2(swX, (a.y + b.y) * 0.5f - 11 * K), t, hv);
        if (Act(pressed, v)) {
            g_uiOv[el.id] = {!on, 0.f, 1.2f};
            UiElem c = el; c.on = on;
            ActionUiPress(c);
        }
    } else if (el.kind == UiKind::Slider) {
        float val = el.value;
        bool hasOv = ov != g_uiOv.end();
        if (hasOv) val = ov->second.value;
        bool pressed = ImGui::InvisibleButton("##sl", ImVec2(w, h));
        bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive() && !edit && g_interactive;
        if (edit) Act(pressed, v);
        float x0 = a.x + 18 * K, x1 = b.x - 18 * K;
        float mn = el.vmin, mx = el.vmax == el.vmin ? el.vmin + 1 : el.vmax;
        bool changed = false;
        if (act) {
            float tt = std::clamp((ImGui::GetIO().MousePos.x - x0) / (x1 - x0), 0.f, 1.f);
            float nv = mn + (mx - mn) * tt;
            if (el.whole) nv = std::round(nv);
            if (nv != val) { val = nv; changed = true; }
        }
        if (act || changed) {
            if (changed && (!hasOv || std::fabs(ov->second.value - val) > 1e-4f)) ActionUiPress(el, 0, val);
            g_uiOv[el.id] = {false, val, 0.8f};
        } else if (hasOv && std::fabs(ov->second.value - el.value) < 1e-3f && ov->second.ttl < 0.7f) {
            g_uiOv.erase(el.id);
        }
        float hv = Anim(ImGui::GetID("hv"), (hov || act) ? 1.f : 0.f, 16.f);
        float norm = (val - mn) / (mx - mn);
        float tv = act ? norm : Anim(ImGui::GetID("val"), norm, 20.f);
        if (act) g_anim[ImGui::GetID("val")] = tv;
        float grab = Anim(ImGui::GetID("grab"), act ? 1.f : 0.f, 20.f);
        hvOut = hv;
        ItemFrame(dl, a, b, hv * 0.7f, v.accent);
        TX(dl, F_Body, ImVec2(x0, a.y + 9 * K), C_TEXT, label);
        char buf[32];
        snprintf(buf, sizeof buf, el.whole ? "%.0f" : (mx - mn <= 2.f ? "%.2f" : "%.1f"), val);
        ImVec2 vs = TS(F_Semi, buf);
        TX(dl, F_Semi, ImVec2(x1 - vs.x, a.y + 8 * K), Accent(), buf);
        float ty = b.y - 15 * K;
        dl->AddRectFilled(ImVec2(x0, ty - 2 * K), ImVec2(x1, ty + 2 * K), A(RGBA(48, 53, 64)), 2 * K);
        float xf = x0 + (x1 - x0) * std::clamp(tv, 0.f, 1.f);
        dl->AddRectFilled(ImVec2(x0, ty - 2 * K), ImVec2(xf, ty + 2 * K), A(Accent()), 2 * K);
        float kr = (6 + 2 * grab + hv) * K;
        dl->AddCircleFilled(ImVec2(xf, ty), kr + 4 * K * grab, A(Accent(0.25f * (hv + grab) * 0.5f)), 24);
        dl->AddCircleFilled(ImVec2(xf, ty), kr, A(IM_COL32(255, 255, 255, 255)), 24);
    } else if (el.kind == UiKind::Dropdown) {
        int shownVal = (int)el.value;
        if (ov != g_uiOv.end()) { if ((int)ov->second.value == (int)el.value && ov->second.ttl < 0.9f) g_uiOv.erase(ov); else shownVal = (int)ov->second.value; }
        int n = std::max(1, el.count);
        bool hovRow = ImGui::IsMouseHoveringRect(a, b) && ImGui::IsWindowHovered();
        float hv = Anim(ImGui::GetID("hv"), hovRow ? 1.f : 0.f, 16.f);
        hvOut = hv;
        ItemFrame(dl, a, b, hv * 0.6f, v.accent);
        float bw2 = 32 * K, bh = h - 14 * K;
        float valW = 64 * K;
        float rx = b.x - 10 * K;
        ImVec2 nextP(rx - bw2, a.y + 7 * K), prevP(rx - bw2 - valW - bw2, a.y + 7 * K);
        dl->PushClipRect(a, ImVec2(prevP.x - 6 * K, b.y), true);
        TX(dl, F_Semi, ImVec2(a.x + 18 * K, (a.y + b.y - FS(F_Semi)) * 0.5f), C_TEXT, label);
        dl->PopClipRect();
        char vb[32]; snprintf(vb, sizeof vb, "%d / %d", shownVal + 1, n);
        ImVec2 vs = TS(F_Body, vb);
        TX(dl, F_Body, ImVec2(prevP.x + bw2 + (valW - vs.x) * 0.5f, (a.y + b.y - vs.y) * 0.5f), Accent(), vb);
        if (edit) {
            bool pressed = ImGui::InvisibleButton("##dd", ImVec2(w, h));
            Act(pressed, v);
        }
        auto stepBtn = [&](const char* id, ImVec2 sp, int dir) {
            ImGui::SetCursorScreenPos(sp);
            bool pr = ImGui::InvisibleButton(id, ImVec2(bw2, bh));
            float sh = Anim(ImGui::GetID(id), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
            ImVec2 sb(sp.x + bw2, sp.y + bh);
            float r = std::min(ItemRadius(h) * 0.7f, bh * 0.5f);
            dl->AddRectFilled(sp, sb, A(LerpCol(RGBA(255, 255, 255, 0.06f), Accent(0.25f), sh)), r);
            ImVec2 c((sp.x + sb.x) * 0.5f, (sp.y + sb.y) * 0.5f);
            float s = 4 * K * (float)dir;
            dl->PathLineTo(ImVec2(c.x - s * 0.6f, c.y - 5 * K));
            dl->PathLineTo(ImVec2(c.x + s * 0.6f, c.y));
            dl->PathLineTo(ImVec2(c.x - s * 0.6f, c.y + 5 * K));
            dl->PathStroke(A(LerpCol(C_TEXT, AccentMix(0.4f), sh)), 0, 1.8f * K);
            if (pr && g_interactive && g_pageAnim > 0.5f) {
                g_uiOv[el.id] = {false, (float)(((shownVal + dir) % n + n) % n), 1.2f};
                ActionUiPress(el, dir);
            }
        };
        if (!edit) {
            stepBtn("##prev", prevP, -1);
            stepBtn("##next", nextP, 1);
        }
    }
    if (edit) {
        float r = ItemRadius(h);
        dl->AddRect(ImVec2(a.x - 2 * K, a.y - 2 * K), ImVec2(b.x + 2 * K, b.y + 2 * K), A(Accent(0.25f + 0.5f * hvOut)), r + 2 * K, 0, 1.2f * K);
        auto it = g_cuiRules.find(v.key);
        if (it != g_cuiRules.end() && !RuleIsDefault(it->second)) {
            const char* tag = PlaceName(it->second.place);
            if (it->second.place == CP_Auto) tag = "Изменено";
            ImVec2 ts = TextSize(F_Small, tag);
            ImVec2 ta(b.x - ts.x - 14 * S, a.y - ts.y * 0.5f - 3 * S), tb(b.x - 4 * S, a.y + ts.y * 0.5f + 3 * S);
            dl->AddRectFilled(ta, tb, A(Accent()), 6 * S);
            Text(dl, F_Small, ImVec2(ta.x + 5 * S, ta.y + 3 * S), OnAccent(), tag);
        }
    }
    ImGui::PopStyleVar();
    g_alpha = sa;
    ImGui::PopID();
}

// Square or wide button that opens the "More" popup.
void DrawMoreButton(ImVec2 p, float w, float h, int count) {
    float K = CK();
    ImGui::SetCursorScreenPos(p);
    bool pressed = ImGui::InvisibleButton("##morebtn", ImVec2(w, h));
    float hv = Anim(ImGui::GetID("##morehv"), ImGui::IsItemHovered() ? 1.f : 0.f, 15.f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = p, b(p.x + w, p.y + h);
    ItemFrame(dl, a, b, hv, false, g_moreOpen ? 1.f : 0.f);
    bool square = w < h * 1.5f;
    if (square) {
        DrawIcon(dl, IC_GEAR, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), 11 * K, A(LerpCol(C_TEXT, Accent(), hv)));
    } else {
        DrawIcon(dl, IC_GEAR, ImVec2(a.x + 26 * K, (a.y + b.y) * 0.5f), 9 * K, A(LerpCol(C_DIM, Accent(), hv)));
        TX(dl, F_Semi, ImVec2(a.x + 46 * K + (g_cfg.cuiHoverShift ? hv * 6 * K : 0), (a.y + b.y - FS(F_Semi)) * 0.5f), C_TEXT, g_cfg.cuiMoreLabel.c_str());
    }
    // count badge
    char cb[8]; snprintf(cb, sizeof cb, "%d", count);
    ImVec2 ts = TextSize(F_Small, cb);
    float br = std::max(ts.x * 0.5f + 5 * S, 9 * S);
    ImVec2 bc(b.x - 22 * K, (a.y + b.y) * 0.5f);
    if (square) {  // on the outline of the (possibly round) button, top-right
        float r = ItemRadius(h);
        bc = ImVec2(b.x - r + r * 0.7071f, a.y + r - r * 0.7071f);
    }
    dl->AddCircleFilled(bc, br, A(Accent()), 20);
    Text(dl, F_Small, ImVec2(bc.x - ts.x * 0.5f, bc.y - ts.y * 0.5f), OnAccent(), cb);
    if (pressed && g_interactive && g_pageAnim > 0.5f) g_moreOpen = !g_moreOpen;
}

// Lays out elements from the cursor (grid aware). Returns the used height.
float LayoutList(std::vector<ElemView>& v, float w, int& idx, bool allowGroups = true) {
    float K = CK(), gap = g_cfg.cuiGap * K;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float y = p0.y, rowH = 0;
    int cols = std::max(1, g_cfg.cuiColumns), col = 0;
    auto flush = [&] { if (col > 0) { y += rowH + gap; col = 0; rowH = 0; } };
    bool multiGroup = false;
    for (auto& e : v) if (e.el.group != v.front().el.group) { multiGroup = true; break; }
    std::string lastGroup = "\x01";
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (auto& e : v) {
        if (e.el.group != lastGroup) {
            if (lastGroup != "\x01") { flush(); }
            lastGroup = e.el.group;
            if (allowGroups && g_cfg.cuiGroups && multiGroup && !e.el.group.empty()) {
                flush();
                if (y > p0.y) y += 6 * K;
                std::string up = e.el.group;
                for (auto& ch : up) ch = (char)toupper((unsigned char)ch);
                TX(dl, F_Small, ImVec2(p0.x + 4 * K, y), C_FAINT, up.c_str());
                y += FS(F_Small) + 8 * K;
            }
        }
        e.idx = idx++;
        float h = ElemH(e, false);
        bool wide = cols == 1 || e.wide || e.el.kind == UiKind::Slider || e.el.kind == UiKind::Dropdown || e.primary;
        if (wide) {
            flush();
            DrawElem(e, ImVec2(p0.x, y), w, false);
            y += h + gap;
        } else {
            float cw = (w - (cols - 1) * gap) / cols;
            DrawElem(e, ImVec2(p0.x + col * (cw + gap), y), cw, false);
            rowH = std::max(rowH, h);
            if (++col == cols) flush();
        }
    }
    flush();
    ImGui::SetCursorScreenPos(ImVec2(p0.x, y));
    ImGui::Dummy(ImVec2(w, 0));
    return y - p0.y;
}

// Pinned (footer) elements; the first row gets a square "More" button when needed.
float PinnedHeight(const std::vector<ElemView>& pins, bool moreRow) {
    float K = CK(), gap = g_cfg.cuiGap * K + 3 * K, h = 0;
    for (auto& e : pins) h += ElemH(e, true) + gap;
    if (pins.empty() && moreRow) h += g_cfg.cuiItemH * K + gap;
    return h;
}
void DrawPinned(std::vector<ElemView>& pins, ImVec2 p, float w, int moreCount, int& idx) {
    float K = CK(), gap = g_cfg.cuiGap * K + 3 * K, y = p.y;
    for (size_t i = 0; i < pins.size(); ++i) {
        float h = ElemH(pins[i], true);
        float ew = w;
        if (i == 0 && moreCount > 0) {
            ew = w - h - gap;
            DrawMoreButton(ImVec2(p.x + ew + gap, y), h, h, moreCount);
        }
        pins[i].idx = idx++;
        DrawElem(pins[i], ImVec2(p.x, y), ew, true);
        y += h + gap;
    }
    if (pins.empty() && moreCount > 0) DrawMoreButton(ImVec2(p.x, y), w, g_cfg.cuiItemH * K, moreCount);
}

// ================================================================== custom UI: popups
struct CuiPopup {
    const char* id;
    bool open = false, isMore = false;
    float t = 0.f, shownH = 0.f, contentH = 0.f;
    int mode = 1;  // 1 centre, 2 bottom sheet, 3 side drawer
    PageView view;
    std::string title;
};
CuiPopup g_subPop{"##cuisub"}, g_morePop{"##cuimore"};

// onClose: what "close" means (Escape for game screens, hiding for "More").
template <class F>
void DrawPopup(CuiPopup& P, ImVec2 disp, float uiAlpha, F onClose) {
    float speed = 10.f * g_cfg.cuiPageSpeed;
    P.t = Approach(P.t, P.open ? 1.f : 0.f, P.open ? speed : speed * 1.3f);
    if (P.t <= 0.002f) { P.t = 0.f; P.shownH = 0.f; return; }
    float K = CK(), e = EaseOut(P.t);
    float alpha = e;
    float maxH = disp.y - 150 * S;  // keeps clear of the watermark
    if (P.shownH <= 0.f) P.shownH = std::max(P.contentH, 120 * K);
    else P.shownH = Approach(P.shownH, std::max(P.contentH, 120 * K), 14.f);
    float R = g_cfg.cuiPopupR * K;
    ImVec2 a, b;
    ImDrawFlags rf = 0;
    if (P.mode == 2) {  // bottom sheet
        float W = std::min(std::max(g_cfg.cuiPopupW * 1.3f * K, disp.x * 0.5f), disp.x);
        float H = std::min(P.shownH, disp.y * 0.86f);
        a = ImVec2((disp.x - W) * 0.5f, disp.y - H * e);
        b = ImVec2(a.x + W, a.y + H);
        rf = ImDrawFlags_RoundCornersTop;
    } else if (P.mode == 3) {  // side drawer
        float W = std::min(g_cfg.cuiPopupW * 0.85f * K, disp.x * 0.7f);
        a = ImVec2(disp.x - W * e, 0);
        b = ImVec2(a.x + W, disp.y);
        rf = ImDrawFlags_RoundCornersLeft;
    } else {
        float W = std::min(g_cfg.cuiPopupW * K, disp.x - 48 * S);
        float H = std::min(P.shownH, maxH);
        ImVec2 c(disp.x * 0.5f, disp.y * 0.5f);
        float s = 1.f;
        switch (g_cfg.cuiPopupAnim) {
        case 0: s = 0.86f + 0.14f * e; break;
        case 1: c.y += (1 - e) * 70 * K; break;
        case 2: s = 0.6f + 0.4f * EaseOutBack(std::clamp(P.t, 0.f, 1.f)); alpha = std::min(1.f, P.t * 2.f); break;
        case 4: c.x += (1 - e) * 110 * K; break;
        default: break;
        }
        a = ImVec2(c.x - W * s * 0.5f, c.y - H * s * 0.5f);
        b = ImVec2(c.x + W * s * 0.5f, c.y + H * s * 0.5f);
    }
    bool live = P.open && P.t > 0.35f;
    std::string blkId = std::string(P.id) + "_blk";
    // ---- blocker, dim, shadow and background
    ImGui::SetCursorScreenPos(ImVec2(0, 0));
    ImGuiWindowFlags cf = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild(blkId.c_str(), disp, ImGuiChildFlags_None, cf | (P.open ? 0 : ImGuiWindowFlags_NoInputs));
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float sa = g_alpha; g_alpha = uiAlpha;
        dl->AddRectFilled(ImVec2(0, 0), disp, A(RGBA(0, 0, 0, g_cfg.cuiPopupDim * e)));
        if (ImGui::InvisibleButton("##outside", disp) && live && g_cfg.cuiPopupOutside) onClose();
        g_alpha = uiAlpha * alpha;
        Shadow(dl, a, b, R, 24 * S, 0.8f);
        dl->AddRectFilled(a, b, A(PanelCol(std::max(0.96f, g_cfg.cuiPanelAlpha), 0.035f)), R, rf);
        dl->AddRectFilledMultiColor(ImVec2(a.x + R, a.y), ImVec2(b.x - R, a.y + 2 * S), A(Accent(0.f)), A(Accent(0.9f)), A(Accent(0.9f)), A(Accent(0.f)));
        dl->AddRect(a, b, A(RGBA(255, 255, 255, 0.08f)), R, rf);
        if (P.mode == 2) dl->AddRectFilled(ImVec2((a.x + b.x) * 0.5f - 22 * S, a.y + 8 * S), ImVec2((a.x + b.x) * 0.5f + 22 * S, a.y + 12 * S), A(RGBA(255, 255, 255, 0.2f)), 2 * S);
        g_alpha = sa;
    }
    ImGui::EndChild();
    // ---- content
    ImGui::SetCursorScreenPos(a);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(uiAlpha * alpha, 0.001f));
    ImGui::BeginChild(P.id, ImVec2(b.x - a.x, b.y - a.y), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | (P.open ? 0 : ImGuiWindowFlags_NoInputs));
    {
        float sa = g_alpha; g_alpha = uiAlpha * alpha;
        bool si = g_interactive; g_interactive = live;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float pad = 26 * K, w = (b.x - a.x) - pad * 2;
        float top = P.mode == 2 ? 24 * S : pad;
        ImVec2 o = ImGui::GetCursorScreenPos();
        float pageOff = P.isMore ? 0.f : (1 - EaseOut(g_pageAnim)) * (g_cfg.cuiPageAnim == 0 ? 30 * K * g_pageDir : 0.f);
        float pageOffY = P.isMore ? 0.f : (g_cfg.cuiPageAnim == 1 ? (1 - EaseOut(g_pageAnim)) * 20 * K : 0.f);
        // header: title + close
        TX(dl, F_Small, ImVec2(o.x + pad, o.y + top), Accent(), P.isMore ? "ZENCLIENT  ·  НАСТРОЙКИ" : "ZENCLIENT");
        TX(dl, F_Big, ImVec2(o.x + pad, o.y + top + FS(F_Small) + 4 * K), C_TEXT, P.title.c_str());
        float cs = 34 * K;
        ImGui::SetCursorScreenPos(ImVec2(o.x + pad + w - cs, o.y + top));
        bool close = ImGui::InvisibleButton("##close", ImVec2(cs, cs));
        float ch = Anim(ImGui::GetID("##closehv"), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
        ImVec2 cc(o.x + pad + w - cs * 0.5f, o.y + top + cs * 0.5f);
        dl->AddCircleFilled(cc, cs * 0.5f, A(LerpCol(RGBA(255, 255, 255, 0.06f), Accent(0.25f), ch)), 32);
        float xs = 6 * K;
        ImU32 xc = A(LerpCol(C_DIM, C_TEXT, ch));
        dl->AddLine(ImVec2(cc.x - xs, cc.y - xs), ImVec2(cc.x + xs, cc.y + xs), xc, 2 * K);
        dl->AddLine(ImVec2(cc.x + xs, cc.y - xs), ImVec2(cc.x - xs, cc.y + xs), xc, 2 * K);
        if (close && live) onClose();
        float headerH = top + FS(F_Small) + 4 * K + FS(F_Big) + 20 * K;
        float y = o.y + headerH;
        int idx = 0;
        float sav = g_alpha;
        g_alpha = sav * (P.isMore ? 1.f : EaseOut(g_pageAnim));
        bool primTop = !g_cfg.cuiPopupPrimaryIn;
        int moreN = P.isMore ? 0 : (int)P.view.more.size();
        bool hasFoot = !P.view.pinned.empty() || moreN > 0;
        float footH = hasFoot ? PinnedHeight(P.view.pinned, moreN > 0) + 12 * K : 0.f;
        if (primTop && hasFoot) {  // Play/Start above the list
            DrawPinned(P.view.pinned, ImVec2(o.x + pad + pageOff, y + pageOffY), w, moreN, idx);
            y += footH;
        }
        // scrolling list between the fixed header and footer
        float bottomPad = pad * 0.8f;
        float listAvail = std::max(40 * K, (b.y - a.y) - (y - o.y) - (primTop ? 0.f : footH) - bottomPad);
        float listH = 0.f;
        if (!P.view.list.empty()) {
            ImGui::SetCursorScreenPos(ImVec2(o.x + pad - 4 * K, y));
            ImGui::BeginChild("##plist", ImVec2(w + 8 * K + 6 * S, listAvail), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
            ImVec2 lp = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(lp.x + 4 * K + pageOff, lp.y + pageOffY));
            listH = LayoutList(P.view.list, w, idx);
            ImGui::EndChild();
            float shown = std::min(listH, listAvail);
            y += shown;
            if (listH > listAvail + 1.f) {  // hint that the list scrolls
                float fh = 30 * K;
                ImU32 bgc = PanelCol(1.f, 0.035f), bg0 = PanelCol(0.f, 0.035f);
                dl->AddRectFilledMultiColor(ImVec2(o.x + pad, y - fh), ImVec2(o.x + pad + w, y), A(bg0), A(bg0), A(bgc), A(bgc));
            }
        } else if (P.view.pinned.empty()) {
            TX(dl, F_Body, ImVec2(o.x + pad, y), C_DIM, "Здесь пока пусто");
            listH = FS(F_Body) + 10 * K;
            y += listH;
        }
        if (!primTop && hasFoot) {
            if (!P.view.list.empty()) {
                dl->AddLine(ImVec2(o.x + pad, y + 5 * K), ImVec2(o.x + pad + w, y + 5 * K), A(RGBA(255, 255, 255, 0.06f)));
                y += 12 * K;
            } else y += 12 * K;
            DrawPinned(P.view.pinned, ImVec2(o.x + pad + pageOff, y + pageOffY), w, moreN, idx);
            y += footH - 12 * K;
        }
        g_alpha = sav;
        P.contentH = headerH + listH + footH + bottomPad;
        if (P.mode == 3) P.contentH = disp.y;
        ImGui::SetCursorScreenPos(ImVec2(o.x, std::min(y, b.y - 1)));
        ImGui::Dummy(ImVec2(1, 1));
        g_interactive = si;
        g_alpha = sa;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ================================================================== custom UI: main
void DrawCustomUi(ImVec2 disp) {
    bool inMenu;
    std::vector<UiElem> snap;
    float coins;
    {
        std::lock_guard<std::mutex> lk(g_snap.mtx);
        inMenu = g_snap.inMainMenu && !g_snap.uiSuspended;
        snap = g_snap.ui;
        coins = g_snap.coins;
    }
    bool want = g_cfg.customUi && inMenu;
    if (want && !g_wasInCustom) {
        g_page.clear(); g_pageHistory.clear(); g_titleStack.clear(); g_uiOv.clear(); g_pendingIds.clear();
        g_rootCopy.clear(); g_emptyTime = 0.f; g_moreOpen = false;
    }
    g_wasInCustom = want;
    g_customAnim = Approach(g_customAnim, want ? 1.f : 0.f, 9.f);
    if (g_customAnim <= 0.002f) { g_subPop.t = g_morePop.t = 0.f; return; }

    // own accent for the custom UI only
    float savedAccent[3];
    memcpy(savedAccent, g_cfg.accent, sizeof savedAccent);
    if (g_cfg.cuiOwnAccent) memcpy(g_cfg.accent, g_cfg.cuiAccent, sizeof savedAccent);

    if (want) UpdatePage(snap);
    TickOverrides();
    g_backCooldown = std::max(0.f, g_backCooldown - g_dt);
    if (g_escBackRequest.exchange(false) && want) GoBack();
    if (g_pageHistory.empty() && !g_page.empty()) g_rootCopy = g_page;
    g_pageAnim = g_cfg.cuiPageAnim == 3 ? 1.f : Approach(g_pageAnim, 1.f, 9.f * g_cfg.cuiPageSpeed);
    g_pageTime += g_dt;

    const Config& c = g_cfg;
    float K = CK();
    float e = EaseOut(g_customAnim);
    float pe = EaseOut(g_pageAnim);
    g_alpha = e;

    // ---- which screen goes where
    int depth = (int)g_pageHistory.size();
    std::string pkey = PageKey(g_page);
    int pmode = 0;
    std::string pageTitle;
    if (depth > 0 && !g_page.empty()) {
        auto it = g_cuiPages.find(pkey);
        pmode = it != g_cuiPages.end() && it->second.mode >= 0 ? it->second.mode : c.cuiSubMode;
        if (it != g_cuiPages.end() && !it->second.title.empty()) pageTitle = it->second.title;
    }
    if (pageTitle.empty() && !g_titleStack.empty()) pageTitle = g_titleStack.back();
    if (pageTitle.empty()) pageTitle = "Настройки";
    bool subPopup = pmode > 0;
    PageView cur = Classify(g_page);
    PageView panel = subPopup ? Classify(g_rootCopy) : cur;
    if (want && subPopup) { g_subPop.open = true; g_subPop.view = cur; g_subPop.mode = pmode; g_subPop.title = pageTitle; g_subPop.isMore = false; }
    else g_subPop.open = false;
    if (g_moreOpen && (!want || cur.more.empty())) g_moreOpen = false;
    if (g_moreOpen) {
        g_morePop.open = true; g_morePop.isMore = true; g_morePop.mode = 1;
        g_morePop.view = PageView{}; g_morePop.view.list = cur.more;
        g_morePop.title = c.cuiMoreLabel.empty() ? "Дополнительно" : c.cuiMoreLabel;
    } else g_morePop.open = false;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse;
    if (!want) fl |= ImGuiWindowFlags_NoInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
    ImGui::Begin("##customui", nullptr, fl);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ---- geometry
    float panelW = std::clamp(disp.x * c.cuiPanelW, 300 * S, disp.x);
    float offX = c.cuiOffX * S, offY = c.cuiOffY * S;
    float px0;
    float cardM = 44 * S;
    switch (c.cuiLayout) {
    case 1: px0 = disp.x - panelW; break;
    case 2: px0 = (disp.x - panelW) * 0.5f; break;
    case 3: panelW = std::min(panelW, disp.x - cardM * 2); px0 = cardM; break;
    default: px0 = 0; break;
    }
    px0 += offX;
    float px1 = px0 + panelW;
    float padX = c.cuiLayout == 3 ? 40 * S : (c.cuiLayout == 2 ? 48 * S : 64 * S);
    float listW = std::max(220 * S, panelW - padX * 2);
    float slideDir = c.cuiLayout == 1 ? 1.f : c.cuiLayout == 2 ? 0.f : -1.f;
    float slide = (1 - e) * 40 * S * slideDir;
    float riseY = c.cuiLayout == 2 ? (1 - e) * 24 * S : 0.f;
    float x0 = px0 + padX + slide;
    float topY = (c.cuiLayout == 3 ? cardM + 36 * S : 70 * S) + offY + riseY;
    float bottomY = std::min(disp.y - 20 * S, (c.cuiLayout == 3 ? disp.y - cardM - 54 * S : disp.y - 70 * S) + offY + riseY);

    // ---- backdrop
    dl->AddRectFilled(ImVec2(0, 0), disp, A(RGBA(5, 6, 8, c.cuiDim)));
    ImU32 solid = PanelCol(c.cuiPanelAlpha), clear = PanelCol(0.f);
    if (c.cuiLayout == 3) {
        ImVec2 ca(px0 + slide, cardM + offY + riseY), cb(px1 + slide, disp.y - cardM + offY + riseY);
        float R = std::max(c.cuiRadius * 2.f, 16.f) * S;
        if (c.cuiBg != 3) {
            Shadow(dl, ca, cb, R, 26 * S, 0.8f);
            float al = c.cuiBg == 2 ? c.cuiPanelAlpha * 0.6f : c.cuiPanelAlpha;
            dl->AddRectFilled(ca, cb, A(PanelCol(al, c.cuiBg == 2 ? 0.08f : 0.f)), R);
            if (c.cuiGlow > 0.001f) {
                float gy = ca.y + (cb.y - ca.y) * 0.55f;
                GradientRect(dl, ImVec2(ca.x, gy), cb, A(Accent(0.f)), A(Accent(c.cuiGlow)), R, ImDrawFlags_RoundCornersBottom);
            }
            dl->AddRect(ca, cb, A(RGBA(255, 255, 255, c.cuiBg == 2 ? 0.14f : 0.07f)), R);
            dl->AddRectFilledMultiColor(ImVec2(ca.x + R, ca.y), ImVec2(cb.x - R, ca.y + 2 * S), A(Accent(0.f)), A(Accent(0.8f)), A(Accent(0.8f)), A(Accent(0.f)));
        }
    } else {
        float sx0 = px0 + slide, sx1 = px1 + slide;
        if (c.cuiLayout == 0) sx0 = std::min(sx0, 0.f);
        if (c.cuiLayout == 1) sx1 = std::max(sx1, disp.x);
        switch (c.cuiBg) {
        case 0: {
            float fade = panelW * 0.9f;
            if (c.cuiLayout == 0) {
                float s1 = sx0 + (sx1 - sx0) * 0.85f;
                dl->AddRectFilled(ImVec2(sx0, 0), ImVec2(s1, disp.y), A(solid));
                dl->AddRectFilledMultiColor(ImVec2(s1 - 1, 0), ImVec2(s1 + fade, disp.y), A(solid), A(clear), A(clear), A(solid));
            } else if (c.cuiLayout == 1) {
                float s0 = sx1 - (sx1 - sx0) * 0.85f;
                dl->AddRectFilled(ImVec2(s0, 0), ImVec2(sx1, disp.y), A(solid));
                dl->AddRectFilledMultiColor(ImVec2(s0 - fade, 0), ImVec2(s0 + 1, disp.y), A(clear), A(solid), A(solid), A(clear));
            } else {
                float in = panelW * 0.12f;
                dl->AddRectFilled(ImVec2(sx0 + in, 0), ImVec2(sx1 - in, disp.y), A(solid));
                dl->AddRectFilledMultiColor(ImVec2(sx0 + in - fade * 0.6f, 0), ImVec2(sx0 + in + 1, disp.y), A(clear), A(solid), A(solid), A(clear));
                dl->AddRectFilledMultiColor(ImVec2(sx1 - in - 1, 0), ImVec2(sx1 - in + fade * 0.6f, disp.y), A(solid), A(clear), A(clear), A(solid));
            }
            break;
        }
        case 1:
            dl->AddRectFilled(ImVec2(sx0, 0), ImVec2(sx1, disp.y), A(solid));
            if (c.cuiLayout != 0) dl->AddLine(ImVec2(sx0, 0), ImVec2(sx0, disp.y), A(RGBA(255, 255, 255, 0.07f)));
            if (c.cuiLayout != 1) dl->AddLine(ImVec2(sx1, 0), ImVec2(sx1, disp.y), A(RGBA(255, 255, 255, 0.07f)));
            break;
        case 2:
            dl->AddRectFilled(ImVec2(sx0, 0), ImVec2(sx1, disp.y), A(PanelCol(c.cuiPanelAlpha * 0.55f, 0.08f)));
            dl->AddRectFilledMultiColor(ImVec2(sx0, 0), ImVec2(sx1, disp.y * 0.4f), A(RGBA(255, 255, 255, 0.04f)), A(RGBA(255, 255, 255, 0.04f)), A(RGBA(255, 255, 255, 0.f)), A(RGBA(255, 255, 255, 0.f)));
            if (c.cuiLayout != 0) dl->AddLine(ImVec2(sx0, 0), ImVec2(sx0, disp.y), A(RGBA(255, 255, 255, 0.14f)));
            if (c.cuiLayout != 1) dl->AddLine(ImVec2(sx1, 0), ImVec2(sx1, disp.y), A(RGBA(255, 255, 255, 0.14f)));
            break;
        default: break;
        }
        if (c.cuiGlow > 0.001f && c.cuiBg != 3)
            dl->AddRectFilledMultiColor(ImVec2(sx0, disp.y * 0.6f), ImVec2(sx1, disp.y), A(Accent(0.f)), A(Accent(0.f)), A(Accent(c.cuiGlow)), A(Accent(c.cuiGlow)));
    }

    // ---- header
    float y = topY;
    auto alignX = [&](float w) { return c.cuiHeaderAlign == 1 ? x0 + (listW - w) * 0.5f : x0; };
    if (c.cuiHeader) {
        if (c.cuiTag && !c.cuiTagText.empty()) {
            ImVec2 ts = TS(F_Small, c.cuiTagText.c_str());
            TX(dl, F_Small, ImVec2(alignX(ts.x), y), Accent(), c.cuiTagText.c_str());
            y += ts.y + 4 * K;
        }
        if (!c.cuiTitle1.empty() || !c.cuiTitle2.empty()) {
            float fs = F_Huge->FontSize * c.cuiScale * c.cuiTitleScale;
            float w1 = c.cuiTitle1.empty() ? 0 : F_Huge->CalcTextSizeA(fs, FLT_MAX, 0, c.cuiTitle1.c_str()).x;
            float w2 = c.cuiTitle2.empty() ? 0 : F_Huge->CalcTextSizeA(fs, FLT_MAX, 0, c.cuiTitle2.c_str()).x;
            float g = (w1 > 0 && w2 > 0) ? 14 * K * c.cuiTitleScale : 0.f;
            float tx = alignX(w1 + g + w2) - 3 * K * c.cuiTitleScale;
            if (w1 > 0) dl->AddText(F_Huge, fs, ImVec2(tx, y), A(C_TEXT), c.cuiTitle1.c_str());
            if (w2 > 0) dl->AddText(F_Huge, fs, ImVec2(tx + w1 + g, y), A(Accent()), c.cuiTitle2.c_str());
            y += fs * 1.08f + 10 * K;
        }
    }
    float backT = Anim("##cuiback", (c.cuiBackBtn && depth > 0 && !subPopup && !g_page.empty()) ? 1.f : 0.f, 12.f);
    if (c.cuiCoins || backT > 0.01f || (c.cuiBackBtn && depth > 0 && !subPopup)) {
        float rowH = 32 * K;
        if (c.cuiCoins) {
            static float coinsShown = -1.f, pillW = 0.f;
            coinsShown = coinsShown < 0 ? coins : Approach(coinsShown, coins, 8.f);
            if (std::fabs(coinsShown - coins) < 0.5f) coinsShown = coins;
            char cb[48];
            snprintf(cb, sizeof cb, "%.0f", coinsShown);
            ImVec2 cs = TS(F_Semi, cb);
            pillW = pillW <= 0 ? cs.x : Approach(pillW, cs.x, 14.f);
            float pw = pillW + 52 * K;
            float pxp = c.cuiHeaderAlign == 1 && backT < 0.01f ? alignX(pw) : x0;
            ImVec2 pa(pxp, y), pb(pxp + pw, y + rowH);
            dl->AddRectFilled(pa, pb, A(RGBA(255, 196, 60, 0.12f)), rowH * 0.5f);
            dl->AddRect(pa, pb, A(RGBA(255, 196, 60, 0.35f)), rowH * 0.5f);
            dl->AddCircleFilled(ImVec2(pa.x + 17 * K, (pa.y + pb.y) * 0.5f), 8 * K, A(RGBA(255, 196, 60)), 20);
            dl->AddCircle(ImVec2(pa.x + 17 * K, (pa.y + pb.y) * 0.5f), 5 * K, A(RGBA(200, 140, 20)), 20, 1.5f * K);
            TX(dl, F_Semi, ImVec2(pa.x + 32 * K, (pa.y + pb.y - cs.y) * 0.5f), RGBA(255, 222, 150), cb);
        }
        if (backT > 0.01f) {
            float bw = 112 * K;
            ImGui::SetCursorScreenPos(ImVec2(x0 + listW - bw + (1 - backT) * 20 * S, y));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(e * backT, 0.001f));
            float sa = g_alpha; g_alpha = e * backT;
            if (Button("##cuibackbtn", "←  Назад", ImVec2(bw, rowH), false, F_Body) && backT > 0.5f) GoBack();
            g_alpha = sa;
            ImGui::PopStyleVar();
        }
        y += rowH + 18 * K;
    } else y += 8 * K;
    float listY = y;

    // ---- footer (pinned)
    int moreN = subPopup ? 0 : (int)panel.more.size();
    float footerH = (panel.pinned.empty() && moreN == 0) ? 0.f : PinnedHeight(panel.pinned, moreN > 0) + 10 * K;
    static float footerShown = 0.f;
    footerShown = Approach(footerShown, footerH, 12.f);
    float pageOff = 0.f, pageOffY = 0.f;
    if (!subPopup) {
        if (c.cuiPageAnim == 0) pageOff = (1 - pe) * 46 * K * (float)g_pageDir;
        else if (c.cuiPageAnim == 1) pageOffY = (1 - pe) * 30 * K;
    }
    float pageAlpha = e * (subPopup ? 1.f : pe);

    // ---- list
    g_interactive = !subPopup && want;
    ImGui::SetCursorScreenPos(ImVec2(x0 + pageOff, listY + pageOffY));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(pageAlpha, 0.001f));
    float listH = std::max(60 * S, bottomY - listY - footerShown);
    ImGui::BeginChild("##cuilist", ImVec2(listW + 12 * S, listH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    if (g_pageScrollReset && !subPopup) { ImGui::SetScrollY(0.f); g_pageScrollReset = false; }
    float sa = g_alpha;
    g_alpha = pageAlpha;
    int idx = 0;
    if (g_page.empty() && !subPopup) {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 cc(p.x + 22 * S, p.y + 22 * S);
        for (int i = 0; i < 12; ++i) {
            float ang = g_time * 6.f + i * IM_PI * 2 / 12;
            cdl->AddCircleFilled(ImVec2(cc.x + std::cos(ang) * 14 * S, cc.y + std::sin(ang) * 14 * S), 2.6f * S, A(Accent((float)i / 12.f)), 10);
        }
        Text(cdl, F_Title, ImVec2(p.x + 52 * S, p.y + 9 * S), C_TEXT, "Загрузка…");
        ImGui::Dummy(ImVec2(listW, 50 * S));
    } else if (!panel.list.empty()) {
        LayoutList(panel.list, listW, idx);
    }
    g_alpha = sa;
    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (!panel.pinned.empty() || moreN > 0 || footerShown > 1.f) {
        float fy = bottomY - footerShown + 10 * K;
        float ft = footerH > 0 ? std::clamp(footerShown / std::max(footerH, 1.f), 0.f, 1.f) : 0.f;
        dl->AddLine(ImVec2(x0, fy - 6 * K), ImVec2(x0 + listW, fy - 6 * K), A(RGBA(255, 255, 255, 0.06f * ft)));
        float s2 = g_alpha; g_alpha = pageAlpha * ft;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(pageAlpha * ft, 0.001f));
        DrawPinned(panel.pinned, ImVec2(x0 + pageOff, fy + pageOffY), listW, moreN, idx);
        ImGui::PopStyleVar();
        g_alpha = s2;
    }
    // footer hint
    if (c.cuiHint) {
        std::string hintS = (depth == 0 ? std::string() : std::string("Esc — назад   ·   ")) + MenuKeyName() + " — меню ZenClient";
        const char* hint = hintS.c_str();
        ImVec2 hs = TextSize(F_Small, hint);
        float hx = c.cuiHeaderAlign == 1 ? x0 + (listW - hs.x) * 0.5f : x0;
        Text(dl, F_Small, ImVec2(hx, std::min(disp.y - hs.y - 12 * S, bottomY + (c.cuiLayout == 3 ? 20 * S : 26 * S))), C_FAINT, hint);
    }
    g_interactive = true;

    // ---- popups (children on top of everything above)
    DrawPopup(g_subPop, disp, e, [] { GoBack(); });
    DrawPopup(g_morePop, disp, e, [] { g_moreOpen = false; });

    // ---- editor bar
    float edT = Anim("##cuiedbar", c.cuiEdit && want ? 1.f : 0.f, 12.f);
    if (edT > 0.01f) {
        const char* msg = "Режим редактора — нажмите на элемент, чтобы настроить его";
        ImVec2 ms = TextSize(F_Body, msg);
        bool pageBtn = depth > 0 && !g_page.empty();
        float bw = 110 * S, bh = 32 * S;
        float W = 24 * S + ms.x + 16 * S + (pageBtn ? bw + 8 * S : 0) + bw + 10 * S, H = 48 * S;
        ImVec2 ba((disp.x - W) * 0.5f, disp.y - H - 22 * S + (1 - EaseOut(edT)) * 40 * S);
        ImGui::SetCursorScreenPos(ba);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(edT, 0.001f));
        ImGui::BeginChild("##cuiedbarw", ImVec2(W, H), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        float s3 = g_alpha; g_alpha = edT;
        ImDrawList* edl = ImGui::GetWindowDrawList();
        edl->AddRectFilled(ba, ImVec2(ba.x + W, ba.y + H), A(RGBA(10, 11, 14, 0.96f)), H * 0.5f);
        edl->AddRect(ba, ImVec2(ba.x + W, ba.y + H), A(Accent(0.6f)), H * 0.5f, 0, 1.5f * S);
        Text(edl, F_Body, ImVec2(ba.x + 24 * S, ba.y + (H - F_Body->FontSize) * 0.5f), C_TEXT, msg);
        float bx = ba.x + 24 * S + ms.x + 16 * S;
        if (pageBtn) {
            ImGui::SetCursorScreenPos(ImVec2(bx, ba.y + (H - bh) * 0.5f));
            if (Button("##edpage", "Этот экран", ImVec2(bw, bh), false, F_Small)) { g_editPageKey = pkey; ImGui::OpenPopupEx(kPagePopup); }
            bx += bw + 8 * S;
        }
        ImGui::SetCursorScreenPos(ImVec2(bx, ba.y + (H - bh) * 0.5f));
        if (Button("##eddone", "Готово", ImVec2(bw, bh), true, F_Small)) { g_cfg.cuiEdit = false; ConfigMarkDirty(); }
        g_alpha = s3;
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    DrawElemEditor();
    DrawPageEditor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    g_alpha = 1.f;
    memcpy(g_cfg.accent, savedAccent, sizeof savedAccent);
}

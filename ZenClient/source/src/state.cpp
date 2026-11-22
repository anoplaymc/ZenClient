#include "state.hpp"
#include "log.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cmath>

Config g_cfg;
Snapshot g_snap;
std::string g_baseDir;
std::atomic<bool> g_menuOpen{false};
std::atomic<bool> g_ready{false};
std::atomic<bool> g_splashSkipped{false};
std::atomic<bool> g_overlayReady{false};
std::atomic<bool> g_disclaimerOpen{true};
std::atomic<unsigned long long> g_escPassUntil{0};
std::atomic<bool> g_escBackRequest{false};
std::atomic<int> g_shotRequest{0};
std::string g_shotName;
std::map<std::string, CuiElemRule> g_cuiRules;
std::map<std::string, CuiPageRule> g_cuiPages;

static std::mutex g_qMtx;
static std::vector<std::function<void()>> g_queue;

void PostMain(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(g_qMtx);
    g_queue.push_back(std::move(fn));
}

void RunMainQueue() {
    std::vector<std::function<void()>> q;
    { std::lock_guard<std::mutex> lk(g_qMtx); q.swap(g_queue); }
    for (auto& f : q) f();
}

// ---------------- config file ----------------
namespace {
struct Entry { const char* key; char type; void* ptr; };
Entry g_entries[] = {
    {"watermark", 'b', &g_cfg.watermark},
    {"hitboxes", 'b', &g_cfg.hitboxes},
    {"hitboxes_npc", 'b', &g_cfg.hbNpc},
    {"hitboxes_items", 'b', &g_cfg.hbItems},
    {"hitboxes_labels", 'b', &g_cfg.hbLabels},
    {"full_bright", 'b', &g_cfg.fullBright},
    {"fb_no_fog", 'b', &g_cfg.fbNoFog},
    {"fb_strength", 'f', &g_cfg.fbStrength},
    {"god", 'b', &g_cfg.god},
    {"invisible", 'b', &g_cfg.invisible},
    {"freeze_enemies", 'b', &g_cfg.freezeEnemies},
    {"fly", 'b', &g_cfg.fly},
    {"noclip", 'b', &g_cfg.noclip},
    {"speed", 'b', &g_cfg.speed},
    {"speed_mul", 'f', &g_cfg.speedMul},
    {"fly_speed", 'f', &g_cfg.flySpeed},
    {"unlock_all", 'b', &g_cfg.unlockAll},
    {"custom_ui", 'b', &g_cfg.customUi},
    {"accent_r", 'f', &g_cfg.accent[0]},
    {"accent_g", 'f', &g_cfg.accent[1]},
    {"accent_b", 'f', &g_cfg.accent[2]},
    {"opacity", 'f', &g_cfg.opacity},
    {"animations", 'b', &g_cfg.animations},
    {"anim_speed", 'f', &g_cfg.animSpeed},
    {"tab", 'i', &g_cfg.tab},
    {"profile", 's', &g_cfg.profile},
    // menu / watermark / esp / toasts / world
    {"menu_scale", 'f', &g_cfg.menuScale},
    {"menu_w", 'f', &g_cfg.menuW},
    {"menu_h", 'f', &g_cfg.menuH},
    {"menu_side", 'f', &g_cfg.menuSide},
    {"menu_radius", 'f', &g_cfg.menuRadius},
    {"menu_dim", 'f', &g_cfg.menuDim},
    {"menu_bg_r", 'f', &g_cfg.menuBg[0]},
    {"menu_bg_g", 'f', &g_cfg.menuBg[1]},
    {"menu_bg_b", 'f', &g_cfg.menuBg[2]},
    {"menu_anim", 'i', &g_cfg.menuAnim},
    {"menu_key", 'i', &g_cfg.menuKey},
    {"menu_status", 'b', &g_cfg.menuStatus},
    {"wm_pos", 'i', &g_cfg.wmPos},
    {"wm_style", 'i', &g_cfg.wmStyle},
    {"wm_name", 'b', &g_cfg.wmName},
    {"wm_fps", 'b', &g_cfg.wmFps},
    {"wm_time", 'b', &g_cfg.wmTime},
    {"wm_coins", 'b', &g_cfg.wmCoins},
    {"wm_status", 'b', &g_cfg.wmStatus},
    {"wm_dot", 'b', &g_cfg.wmDot},
    {"wm_day", 'b', &g_cfg.wmDay},
    {"wm_text", 's', &g_cfg.wmText},
    {"wm_scale", 'f', &g_cfg.wmScale},
    {"wm_alpha", 'f', &g_cfg.wmAlpha},
    {"esp_box", 'i', &g_cfg.espBox},
    {"esp_npc_r", 'f', &g_cfg.espNpcCol[0]},
    {"esp_npc_g", 'f', &g_cfg.espNpcCol[1]},
    {"esp_npc_b", 'f', &g_cfg.espNpcCol[2]},
    {"esp_item_accent", 'b', &g_cfg.espItemAccent},
    {"esp_item_r", 'f', &g_cfg.espItemCol[0]},
    {"esp_item_g", 'f', &g_cfg.espItemCol[1]},
    {"esp_item_b", 'f', &g_cfg.espItemCol[2]},
    {"esp_thick", 'f', &g_cfg.espThick},
    {"esp_fill", 'f', &g_cfg.espFill},
    {"esp_label_scale", 'f', &g_cfg.espLabelScale},
    {"esp_max_dist", 'f', &g_cfg.espMaxDist},
    {"esp_dist", 'b', &g_cfg.espDist},
    {"esp_tracers", 'b', &g_cfg.espTracers},
    {"toast_on", 'b', &g_cfg.toastOn},
    {"toast_pos", 'i', &g_cfg.toastPos},
    {"toast_time", 'f', &g_cfg.toastTime},
    {"fov_on", 'b', &g_cfg.fovOn},
    {"fov", 'f', &g_cfg.fov},
    {"time_on", 'b', &g_cfg.timeOn},
    {"time_scale", 'f', &g_cfg.timeScale},
    {"skip_intro", 'b', &g_cfg.skipIntro},
    {"skip_intro_new_day", 'b', &g_cfg.skipIntroNewDay},
    {"skip_loading", 'b', &g_cfg.skipLoading},
    // custom UI
    {"cui_subtab", 'i', &g_cfg.cuiSubTab},
    {"cui_layout", 'i', &g_cfg.cuiLayout},
    {"cui_panel_w", 'f', &g_cfg.cuiPanelW},
    {"cui_off_x", 'f', &g_cfg.cuiOffX},
    {"cui_off_y", 'f', &g_cfg.cuiOffY},
    {"cui_scale", 'f', &g_cfg.cuiScale},
    {"cui_columns", 'i', &g_cfg.cuiColumns},
    {"cui_bg", 'i', &g_cfg.cuiBg},
    {"cui_dim", 'f', &g_cfg.cuiDim},
    {"cui_panel_alpha", 'f', &g_cfg.cuiPanelAlpha},
    {"cui_panel_r", 'f', &g_cfg.cuiPanelCol[0]},
    {"cui_panel_g", 'f', &g_cfg.cuiPanelCol[1]},
    {"cui_panel_b", 'f', &g_cfg.cuiPanelCol[2]},
    {"cui_glow", 'f', &g_cfg.cuiGlow},
    {"cui_own_accent", 'b', &g_cfg.cuiOwnAccent},
    {"cui_accent_r", 'f', &g_cfg.cuiAccent[0]},
    {"cui_accent_g", 'f', &g_cfg.cuiAccent[1]},
    {"cui_accent_b", 'f', &g_cfg.cuiAccent[2]},
    {"cui_header", 'b', &g_cfg.cuiHeader},
    {"cui_tag", 'b', &g_cfg.cuiTag},
    {"cui_coins", 'b', &g_cfg.cuiCoins},
    {"cui_hint", 'b', &g_cfg.cuiHint},
    {"cui_back_btn", 'b', &g_cfg.cuiBackBtn},
    {"cui_header_align", 'i', &g_cfg.cuiHeaderAlign},
    {"cui_title_scale", 'f', &g_cfg.cuiTitleScale},
    {"cui_title1", 's', &g_cfg.cuiTitle1},
    {"cui_title2", 's', &g_cfg.cuiTitle2},
    {"cui_tag_text", 's', &g_cfg.cuiTagText},
    {"cui_btn_style", 'i', &g_cfg.cuiBtnStyle},
    {"cui_item_h", 'f', &g_cfg.cuiItemH},
    {"cui_gap", 'f', &g_cfg.cuiGap},
    {"cui_radius", 'f', &g_cfg.cuiRadius},
    {"cui_item_alpha", 'f', &g_cfg.cuiItemAlpha},
    {"cui_align", 'i', &g_cfg.cuiAlign},
    {"cui_arrows", 'b', &g_cfg.cuiArrows},
    {"cui_bar", 'b', &g_cfg.cuiBar},
    {"cui_hover_shift", 'b', &g_cfg.cuiHoverShift},
    {"cui_groups", 'b', &g_cfg.cuiGroups},
    {"cui_primary_place", 'i', &g_cfg.cuiPrimaryPlace},
    {"cui_primary_style", 'i', &g_cfg.cuiPrimaryStyle},
    {"cui_primary_h", 'f', &g_cfg.cuiPrimaryH},
    {"cui_pulse", 'b', &g_cfg.cuiPulse},
    {"cui_sub_mode", 'i', &g_cfg.cuiSubMode},
    {"cui_popup_anim", 'i', &g_cfg.cuiPopupAnim},
    {"cui_popup_w", 'f', &g_cfg.cuiPopupW},
    {"cui_popup_r", 'f', &g_cfg.cuiPopupR},
    {"cui_popup_dim", 'f', &g_cfg.cuiPopupDim},
    {"cui_popup_outside", 'b', &g_cfg.cuiPopupOutside},
    {"cui_popup_primary_in", 'b', &g_cfg.cuiPopupPrimaryIn},
    {"cui_auto_more", 'b', &g_cfg.cuiAutoMore},
    {"cui_more_label", 's', &g_cfg.cuiMoreLabel},
    {"cui_page_anim", 'i', &g_cfg.cuiPageAnim},
    {"cui_page_speed", 'f', &g_cfg.cuiPageSpeed},
    {"cui_stagger", 'b', &g_cfg.cuiStagger},
    {"cui_stagger_delay", 'f', &g_cfg.cuiStaggerDelay},
};
void SetEntry(const Entry& e, const char* v) {
    if (e.type == 'b') *static_cast<bool*>(e.ptr) = atoi(v) != 0;
    else if (e.type == 'f') *static_cast<float*>(e.ptr) = (float)atof(v);
    else if (e.type == 's') *static_cast<std::string*>(e.ptr) = v;
    else *static_cast<int*>(e.ptr) = atoi(v);
}
std::string Clean(const std::string& s) {  // one line, no field separators
    std::string o;
    for (char c : s) o += (c == '\n' || c == '\r' || c == '|') ? '/' : c;
    return o;
}
// "a|b|c" -> fields; the last field keeps the rest of the line
std::vector<std::string> Split(const std::string& s, size_t n) {
    std::vector<std::string> out;
    size_t p = 0;
    while (out.size() + 1 < n) {
        size_t q = s.find('|', p);
        if (q == std::string::npos) break;
        out.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    out.push_back(s.substr(p));
    while (out.size() < n) out.emplace_back();
    return out;
}
float g_dirtyTimer = -1.f;
std::wstring W(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
}

// Keeps every value inside the range the UI allows (config files can be edited by hand).
static void ConfigSanitize() {
    if (g_cfg.speedMul < 1.1f || g_cfg.speedMul > 6.f) g_cfg.speedMul = 2.f;
    if (g_cfg.flySpeed < 1.f || g_cfg.flySpeed > 40.f) g_cfg.flySpeed = 7.f;
    if (g_cfg.opacity < 0.5f || g_cfg.opacity > 1.f) g_cfg.opacity = 0.95f;
    if (g_cfg.animSpeed < 0.25f || g_cfg.animSpeed > 3.f) g_cfg.animSpeed = 1.f;
    for (float& c : g_cfg.accent) if (c < 0 || c > 1) c = 0.5f;
    if (g_cfg.tab < 0 || g_cfg.tab > 6) g_cfg.tab = 0;
    {
        Config& c = g_cfg;
        auto clampi = [](int& v, int lo, int hi) { v = std::clamp(v, lo, hi); };
        auto clampf = [](float& v, float lo, float hi) { if (!(v >= lo && v <= hi)) v = std::clamp(v == v ? v : lo, lo, hi); };
        clampi(c.cuiSubTab, 0, 6); clampi(c.cuiLayout, 0, 3); clampi(c.cuiColumns, 1, 4); clampi(c.cuiBg, 0, 3);
        clampi(c.cuiHeaderAlign, 0, 1); clampi(c.cuiBtnStyle, 0, 5); clampi(c.cuiAlign, 0, 1);
        clampi(c.cuiPrimaryPlace, 0, 2); clampi(c.cuiPrimaryStyle, 0, 3); clampi(c.cuiSubMode, 0, 3);
        clampi(c.cuiPopupAnim, 0, 4); clampi(c.cuiPageAnim, 0, 3);
        clampf(c.cuiPanelW, 0.2f, 1.f); clampf(c.cuiOffX, -800.f, 800.f); clampf(c.cuiOffY, -400.f, 400.f);
        clampf(c.cuiScale, 0.6f, 1.6f); clampf(c.cuiDim, 0.f, 1.f); clampf(c.cuiPanelAlpha, 0.f, 1.f);
        clampf(c.cuiGlow, 0.f, 0.4f); clampf(c.cuiTitleScale, 0.3f, 1.6f); clampf(c.cuiItemH, 30.f, 90.f);
        clampf(c.cuiGap, 0.f, 30.f); clampf(c.cuiRadius, 0.f, 45.f); clampf(c.cuiItemAlpha, 0.f, 1.f);
        clampf(c.cuiPrimaryH, 36.f, 110.f); clampf(c.cuiPopupW, 340.f, 1400.f); clampf(c.cuiPopupR, 0.f, 48.f);
        clampf(c.cuiPopupDim, 0.f, 0.95f); clampf(c.cuiPageSpeed, 0.3f, 3.f); clampf(c.cuiStaggerDelay, 0.f, 0.15f);
        for (float& x : c.cuiPanelCol) clampf(x, 0.f, 1.f);
        for (float& x : c.cuiAccent) clampf(x, 0.f, 1.f);
        clampf(c.menuScale, 0.7f, 1.5f); clampf(c.menuW, 640.f, 1300.f); clampf(c.menuH, 500.f, 950.f);
        clampf(c.menuSide, 150.f, 300.f); clampf(c.menuRadius, 0.f, 32.f); clampf(c.menuDim, 0.f, 0.9f);
        for (float& x : c.menuBg) clampf(x, 0.f, 1.f);
        clampi(c.menuAnim, 0, 3); clampi(c.menuKey, 0, 8); clampi(c.wmPos, 0, 5); clampi(c.wmStyle, 0, 2);
        clampf(c.wmScale, 0.6f, 2.f); clampf(c.wmAlpha, 0.2f, 1.f); clampi(c.espBox, 0, 2);
        for (float& x : c.espNpcCol) clampf(x, 0.f, 1.f);
        for (float& x : c.espItemCol) clampf(x, 0.f, 1.f);
        clampf(c.espThick, 0.5f, 4.f); clampf(c.espFill, 0.f, 0.5f); clampf(c.espLabelScale, 0.6f, 2.f);
        clampf(c.espMaxDist, 10.f, 300.f); clampi(c.toastPos, 0, 3); clampf(c.toastTime, 1.f, 10.f);
        clampf(c.fov, 40.f, 130.f); clampf(c.timeScale, 0.1f, 3.f);
    }
}

void ConfigLoad() {
    FILE* f = _wfopen(W(g_baseDir + "config.ini").c_str(), L"r");
    if (!f) { LOG("config: none, defaults"); return; }
    char line[2048];
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* k = line;
        const char* v = eq + 1;
        if (!strncmp(k, "el.", 3)) {
            auto fl = Split(v, 5);
            CuiElemRule r;
            r.place = std::clamp(atoi(fl[0].c_str()), 0, (int)CP_Hidden);
            r.order = atoi(fl[1].c_str());
            int bits = atoi(fl[2].c_str());
            r.accent = bits & 1; r.wide = (bits & 2) != 0;
            r.orig = fl[3]; r.label = fl[4];
            g_cuiRules[k + 3] = r;
            continue;
        }
        if (!strncmp(k, "pg.", 3)) {
            auto fl = Split(v, 2);
            CuiPageRule r;
            r.mode = std::clamp(atoi(fl[0].c_str()), -1, 3);
            r.title = fl[1];
            g_cuiPages[k + 3] = r;
            continue;
        }
        for (auto& e : g_entries) if (strcmp(e.key, k) == 0) SetEntry(e, v);
    }
    fclose(f);
    ConfigSanitize();
    LOG("config loaded");
}

void ConfigSave() {
    std::string tmp = g_baseDir + "config.ini.tmp";
    FILE* f = _wfopen(W(tmp).c_str(), L"w");
    if (!f) return;
    fprintf(f, "; ZenClient settings (saved automatically)\n");
    for (auto& e : g_entries) {
        if (e.type == 'b') fprintf(f, "%s=%d\n", e.key, *static_cast<bool*>(e.ptr) ? 1 : 0);
        else if (e.type == 'f') fprintf(f, "%s=%.4f\n", e.key, *static_cast<float*>(e.ptr));
        else if (e.type == 's') fprintf(f, "%s=%s\n", e.key, Clean(*static_cast<std::string*>(e.ptr)).c_str());
        else fprintf(f, "%s=%d\n", e.key, *static_cast<int*>(e.ptr));
    }
    fprintf(f, "; custom UI rules: el.<id>=place|order|flags|original|label, pg.<id>=mode|title\n");
    for (auto& [k, r] : g_cuiRules)
        fprintf(f, "el.%s=%d|%d|%d|%s|%s\n", k.c_str(), r.place, r.order, (r.accent ? 1 : 0) | (r.wide ? 2 : 0), Clean(r.orig).c_str(), Clean(r.label).c_str());
    for (auto& [k, r] : g_cuiPages)
        fprintf(f, "pg.%s=%d|%s\n", k.c_str(), r.mode, Clean(r.title).c_str());
    fclose(f);
    MoveFileExW(W(tmp).c_str(), W(g_baseDir + "config.ini").c_str(), MOVEFILE_REPLACE_EXISTING);
}

void ConfigMarkDirty() { g_dirtyTimer = 0.6f; }
void ConfigTick(float dt) {
    if (g_dirtyTimer < 0) return;
    g_dirtyTimer -= dt;
    if (g_dirtyTimer < 0) ConfigSave();
}

bool ConfigSetByName(const std::string& key, const std::string& value) {
    if (key == "cui_edit") { g_cfg.cuiEdit = atoi(value.c_str()) != 0; return true; }  // not saved
    for (auto& e : g_entries) {
        if (key != e.key) continue;
        SetEntry(e, value.c_str());
        ConfigMarkDirty();
        return true;
    }
    return false;
}

void CuiResetSettings() {
    Config d;
    bool on = g_cfg.customUi, edit = g_cfg.cuiEdit;
    int sub = g_cfg.cuiSubTab;
    for (auto& e : g_entries) {
        if (strncmp(e.key, "cui_", 4) != 0) continue;
        void* src = (char*)&d + ((char*)e.ptr - (char*)&g_cfg);
        if (e.type == 'b') *static_cast<bool*>(e.ptr) = *static_cast<bool*>(src);
        else if (e.type == 'f') *static_cast<float*>(e.ptr) = *static_cast<float*>(src);
        else if (e.type == 's') *static_cast<std::string*>(e.ptr) = *static_cast<std::string*>(src);
        else *static_cast<int*>(e.ptr) = *static_cast<int*>(src);
    }
    g_cfg.customUi = on; g_cfg.cuiEdit = edit; g_cfg.cuiSubTab = sub;
    ConfigMarkDirty();
}

// ======================================================================== JSON configs
namespace {
std::string U8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
// keys that describe the UI session rather than settings
bool NotInProfile(const char* k) { return !strcmp(k, "tab") || !strcmp(k, "cui_subtab") || !strcmp(k, "profile"); }

void JStr(std::string& o, const std::string& s) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += (char)c;
        }
    }
    o += '"';
}

struct JVal {
    enum T { Null, Bool, Num, Str, Obj, Arr } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<std::pair<std::string, JVal>> o;
    std::vector<JVal> a;
    const JVal* Get(const char* k) const {
        for (auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
};

struct JParser {
    const char* p;
    const char* end;
    std::string err;
    void Ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }
    bool Fail(const char* m) { if (err.empty()) err = m; return false; }
    static void PutUtf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    bool Hex4(unsigned& v) {
        if (end - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; ++i, ++p) {
            char c = *p; v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return false;
        }
        return true;
    }
    bool String(std::string& out) {
        if (p >= end || *p != '"') return Fail("ожидалась строка");
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                if (++p >= end) break;
                char c = *p++;
                switch (c) {
                case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp;
                    if (!Hex4(cp)) return Fail("неверный \\u");
                    if (cp >= 0xD800 && cp < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        p += 2; unsigned lo;
                        if (!Hex4(lo)) return Fail("неверный \\u");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    PutUtf8(out, cp);
                    break;
                }
                default: out += c;
                }
            } else out += *p++;
        }
        if (p >= end) return Fail("незакрытая строка");
        ++p;
        return true;
    }
    bool Value(JVal& v, int depth = 0) {
        if (depth > 32) return Fail("слишком глубокая вложенность");
        Ws();
        if (p >= end) return Fail("неожиданный конец файла");
        char c = *p;
        if (c == '{') {
            v.t = JVal::Obj; ++p; Ws();
            if (p < end && *p == '}') { ++p; return true; }
            for (;;) {
                Ws();
                std::string k;
                if (!String(k)) return false;
                Ws();
                if (p >= end || *p != ':') return Fail("ожидалось ':'");
                ++p;
                JVal child;
                if (!Value(child, depth + 1)) return false;
                v.o.emplace_back(std::move(k), std::move(child));
                Ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return true; }
                return Fail("ожидалось ',' или '}'");
            }
        }
        if (c == '[') {
            v.t = JVal::Arr; ++p; Ws();
            if (p < end && *p == ']') { ++p; return true; }
            for (;;) {
                JVal child;
                if (!Value(child, depth + 1)) return false;
                v.a.push_back(std::move(child));
                Ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return true; }
                return Fail("ожидалось ',' или ']'");
            }
        }
        if (c == '"') { v.t = JVal::Str; return String(v.s); }
        if (!strncmp(p, "true", 4) && end - p >= 4) { v.t = JVal::Bool; v.b = true; p += 4; return true; }
        if (!strncmp(p, "false", 5) && end - p >= 5) { v.t = JVal::Bool; v.b = false; p += 5; return true; }
        if (!strncmp(p, "null", 4) && end - p >= 4) { v.t = JVal::Null; p += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char* e = nullptr;
            std::string tmp(p, std::min<size_t>(end - p, 64));
            v.n = strtod(tmp.c_str(), &e);
            if (e == tmp.c_str()) return Fail("неверное число");
            v.t = JVal::Num;
            p += e - tmp.c_str();
            return true;
        }
        return Fail("неизвестное значение");
    }
};

std::string ProfilesDir() { return g_baseDir + "configs\\"; }
std::string SafeName(std::string n) {
    std::string o;
    for (char c : n) if (!strchr("\\/:*?\"<>|", c) && (unsigned char)c >= 0x20) o += c;
    while (!o.empty() && (o.back() == ' ' || o.back() == '.')) o.pop_back();
    while (!o.empty() && o.front() == ' ') o.erase(o.begin());
    if (o.size() > 60) o.resize(60);
    return o;
}
std::string ProfilePath(const std::string& name) { return ProfilesDir() + name + ".json"; }
bool WriteFileU8(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* f = _wfopen(W(tmp).c_str(), L"wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (!ok) { DeleteFileW(W(tmp).c_str()); return false; }
    return MoveFileExW(W(tmp).c_str(), W(path).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}
}  // namespace

bool ReadFileU8(const std::string& path, std::string& out) {
    FILE* f = _wfopen(W(path).c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0 && out.size() < 4 * 1024 * 1024) out.append(buf, n);
    fclose(f);
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF) out.erase(0, 3);
    return true;
}
bool WriteFileUtf8(const std::string& path, const std::string& data) { return WriteFileU8(path, data); }

std::string ConfigToJson(const std::string& name) {
    std::string o = "{\n  \"zenclient\": 1,\n  \"name\": ";
    JStr(o, name);
    o += ",\n  \"settings\": {";
    bool first = true;
    char num[64];
    for (auto& e : g_entries) {
        if (NotInProfile(e.key)) continue;
        o += first ? "\n    " : ",\n    ";
        first = false;
        JStr(o, e.key);
        o += ": ";
        if (e.type == 'b') o += *static_cast<bool*>(e.ptr) ? "true" : "false";
        else if (e.type == 'f') { snprintf(num, sizeof num, "%.4f", *static_cast<float*>(e.ptr)); o += num; }
        else if (e.type == 's') JStr(o, *static_cast<std::string*>(e.ptr));
        else { snprintf(num, sizeof num, "%d", *static_cast<int*>(e.ptr)); o += num; }
    }
    o += "\n  },\n  \"ui_rules\": {";
    first = true;
    for (auto& [k, r] : g_cuiRules) {
        o += first ? "\n    " : ",\n    ";
        first = false;
        JStr(o, k);
        snprintf(num, sizeof num, ": {\"place\": %d, \"order\": %d, ", r.place, r.order);
        o += num;
        o += std::string("\"accent\": ") + (r.accent ? "true" : "false") + ", \"wide\": " + (r.wide ? "true" : "false") + ", \"orig\": ";
        JStr(o, r.orig);
        o += ", \"label\": ";
        JStr(o, r.label);
        o += "}";
    }
    o += first ? "}" : "\n  }";
    o += ",\n  \"ui_screens\": {";
    first = true;
    for (auto& [k, r] : g_cuiPages) {
        o += first ? "\n    " : ",\n    ";
        first = false;
        JStr(o, k);
        snprintf(num, sizeof num, ": {\"mode\": %d, \"title\": ", r.mode);
        o += num;
        JStr(o, r.title);
        o += "}";
    }
    o += first ? "}" : "\n  }";
    o += "\n}\n";
    return o;
}

bool ConfigFromJson(const std::string& text, std::string* name, std::string* err) {
    JParser ps{text.data(), text.data() + text.size(), {}};
    JVal root;
    if (!ps.Value(root)) { if (err) *err = "Ошибка JSON: " + ps.err; return false; }
    const JVal* st = root.t == JVal::Obj ? root.Get("settings") : nullptr;
    if (!st || st->t != JVal::Obj) { if (err) *err = "Это не конфиг ZenClient (нет раздела settings)"; return false; }
    // everything not in the file goes back to its default, so a config always looks the same
    Config d;
    bool edit = g_cfg.cuiEdit;
    int applied = 0;
    for (auto& e : g_entries) {
        if (NotInProfile(e.key)) continue;
        void* src = (char*)&d + ((char*)e.ptr - (char*)&g_cfg);
        const JVal* v = st->Get(e.key);
        switch (e.type) {
        case 'b': *static_cast<bool*>(e.ptr) = v ? (v->t == JVal::Bool ? v->b : v->t == JVal::Num && v->n != 0) : *static_cast<bool*>(src); break;
        case 'f': *static_cast<float*>(e.ptr) = v && v->t == JVal::Num ? (float)v->n : *static_cast<float*>(src); break;
        case 'i': *static_cast<int*>(e.ptr) = v && v->t == JVal::Num ? (int)std::lround(v->n) : *static_cast<int*>(src); break;
        case 's': *static_cast<std::string*>(e.ptr) = v && v->t == JVal::Str ? v->s : *static_cast<std::string*>(src); break;
        }
        if (v) ++applied;
    }
    g_cfg.cuiEdit = edit;
    g_cuiRules.clear();
    g_cuiPages.clear();
    if (const JVal* rr = root.Get("ui_rules"); rr && rr->t == JVal::Obj) {
        for (auto& [k, v] : rr->o) {
            if (v.t != JVal::Obj) continue;
            CuiElemRule r;
            if (auto* x = v.Get("place"); x && x->t == JVal::Num) r.place = std::clamp((int)x->n, 0, (int)CP_Hidden);
            if (auto* x = v.Get("order"); x && x->t == JVal::Num) r.order = std::clamp((int)x->n, -1000, 1000);
            if (auto* x = v.Get("accent"); x && x->t == JVal::Bool) r.accent = x->b;
            if (auto* x = v.Get("wide"); x && x->t == JVal::Bool) r.wide = x->b;
            if (auto* x = v.Get("orig"); x && x->t == JVal::Str) r.orig = x->s;
            if (auto* x = v.Get("label"); x && x->t == JVal::Str) r.label = x->s;
            g_cuiRules[k] = r;
        }
    }
    if (const JVal* rp = root.Get("ui_screens"); rp && rp->t == JVal::Obj) {
        for (auto& [k, v] : rp->o) {
            if (v.t != JVal::Obj) continue;
            CuiPageRule r;
            if (auto* x = v.Get("mode"); x && x->t == JVal::Num) r.mode = std::clamp((int)x->n, -1, 3);
            if (auto* x = v.Get("title"); x && x->t == JVal::Str) r.title = x->s;
            g_cuiPages[k] = r;
        }
    }
    ConfigSanitize();
    if (name) { const JVal* n = root.Get("name"); *name = n && n->t == JVal::Str ? n->s : std::string(); }
    ConfigMarkDirty();
    LOG("config json applied: %d settings, %zu rules, %zu screens", applied, g_cuiRules.size(), g_cuiPages.size());
    return true;
}

void ConfigResetAll() {
    Config d;
    for (auto& e : g_entries) {
        if (NotInProfile(e.key)) continue;
        void* src = (char*)&d + ((char*)e.ptr - (char*)&g_cfg);
        if (e.type == 'b') *static_cast<bool*>(e.ptr) = *static_cast<bool*>(src);
        else if (e.type == 'f') *static_cast<float*>(e.ptr) = *static_cast<float*>(src);
        else if (e.type == 's') *static_cast<std::string*>(e.ptr) = *static_cast<std::string*>(src);
        else *static_cast<int*>(e.ptr) = *static_cast<int*>(src);
    }
    g_cfg.cuiEdit = false;
    g_cfg.profile.clear();
    g_cuiRules.clear();
    g_cuiPages.clear();
    ConfigMarkDirty();
    LOG("config reset to defaults");
}

std::string ProfilesFolder() { return ProfilesDir(); }
std::string ProfileSafeName(const std::string& n) { return SafeName(n); }

std::vector<ProfileInfo> ProfileList() {
    std::vector<ProfileInfo> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(W(ProfilesDir() + "*.json").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring fn = fd.cFileName;
        if (fn.size() < 6) continue;
        ProfileInfo pi;
        pi.name = U8(fn.substr(0, fn.size() - 5));
        FILETIME lt; SYSTEMTIME st;
        FileTimeToLocalFileTime(&fd.ftLastWriteTime, &lt);
        FileTimeToSystemTime(&lt, &st);
        char b[48];
        snprintf(b, sizeof b, "%02d.%02d.%02d %02d:%02d", st.wDay, st.wMonth, st.wYear % 100, st.wHour, st.wMinute);
        pi.modified = b;
        out.push_back(pi);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const ProfileInfo& a, const ProfileInfo& b) { return a.name < b.name; });
    return out;
}

bool ProfileExists(const std::string& name) {
    return GetFileAttributesW(W(ProfilePath(name)).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool ProfileSave(const std::string& rawName, std::string* err) {
    std::string name = SafeName(rawName);
    if (name.empty()) { if (err) *err = "Введите название"; return false; }
    CreateDirectoryW(W(ProfilesDir()).c_str(), nullptr);
    if (!WriteFileU8(ProfilePath(name), ConfigToJson(name))) { if (err) *err = "Не удалось записать файл"; return false; }
    g_cfg.profile = name;
    ConfigMarkDirty();
    LOG("profile saved: %s", name.c_str());
    return true;
}

bool ProfileLoad(const std::string& name, std::string* err) {
    std::string text;
    if (!ReadFileU8(ProfilePath(name), text)) { if (err) *err = "Файл не найден"; return false; }
    if (!ConfigFromJson(text, nullptr, err)) return false;
    g_cfg.profile = name;
    ConfigMarkDirty();
    return true;
}

bool ProfileRename(const std::string& from, const std::string& rawTo, std::string* err) {
    std::string to = SafeName(rawTo);
    if (to.empty()) { if (err) *err = "Введите название"; return false; }
    if (to == from) return true;
    if (ProfileExists(to)) { if (err) *err = "Конфиг с таким названием уже есть"; return false; }
    if (!MoveFileW(W(ProfilePath(from)).c_str(), W(ProfilePath(to)).c_str())) { if (err) *err = "Не удалось переименовать"; return false; }
    // keep the stored name in sync
    std::string text;
    if (ReadFileU8(ProfilePath(to), text)) {
        size_t p = text.find("\"name\":");
        if (p != std::string::npos) {
            size_t q1 = text.find('"', p + 7), q2 = q1;
            while (q2 != std::string::npos) { q2 = text.find('"', q2 + 1); if (q2 == std::string::npos || text[q2 - 1] != '\\') break; }
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string js; JStr(js, to);
                text.replace(q1, q2 - q1 + 1, js);
                WriteFileU8(ProfilePath(to), text);
            }
        }
    }
    if (g_cfg.profile == from) { g_cfg.profile = to; ConfigMarkDirty(); }
    return true;
}

bool ProfileDelete(const std::string& name) {
    bool ok = DeleteFileW(W(ProfilePath(name)).c_str()) != 0;
    if (ok && g_cfg.profile == name) { g_cfg.profile.clear(); ConfigMarkDirty(); }
    return ok;
}

// Imports JSON text as a new config (unique name) and applies it.
bool ProfileImport(const std::string& text, const std::string& fallbackName, std::string* outName, std::string* err) {
    std::string name;
    if (!ConfigFromJson(text, &name, err)) return false;
    name = SafeName(name.empty() ? fallbackName : name);
    if (name.empty()) name = "Импорт";
    std::string base = name;
    for (int i = 2; ProfileExists(name) && i < 1000; ++i) name = base + " (" + std::to_string(i) + ")";
    if (!ProfileSave(name, err)) return false;
    if (outName) *outName = name;
    return true;
}

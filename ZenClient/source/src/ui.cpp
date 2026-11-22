// ZenClient - user interface: menu, watermark, hitboxes, toasts and the custom main-menu UI.
#include "ui.hpp"
#include "state.hpp"
#include "features.hpp"
#include "log.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <commdlg.h>
#include <shellapi.h>

namespace {

// ------------------------------------------------------------------ fonts & scale
float S = 1.f;  // UI scale
ImFont *F_Body, *F_Semi, *F_Small, *F_Title, *F_Big, *F_Huge;
float g_dt = 1 / 60.f, g_time = 0;
float g_fontH = 1080.f, g_appliedMenuScale = 1.f;

struct MenuKey { const char* name; int vk; };
const MenuKey kMenuKeys[] = {{"Правый Shift", VK_RSHIFT}, {"Insert", VK_INSERT}, {"Home", VK_HOME}, {"End", VK_END},
                             {"Delete", VK_DELETE}, {"F1", VK_F1}, {"F2", VK_F2}, {"F5", VK_F5}, {"F12", VK_F12}};
const int kMenuKeyCount = 9;
const char* MenuKeyName() { return kMenuKeys[std::clamp(g_cfg.menuKey, 0, kMenuKeyCount - 1)].name; }

// ------------------------------------------------------------------ colors
inline ImU32 RGBA(int r, int g, int b, float a = 1.f) { return IM_COL32(r, g, b, (int)(a * 255.f)); }
inline ImVec4 AccentV(float a = 1.f) { return ImVec4(g_cfg.accent[0], g_cfg.accent[1], g_cfg.accent[2], a); }
inline ImU32 Accent(float a = 1.f) { return ImGui::ColorConvertFloat4ToU32(AccentV(a)); }
inline ImU32 AccentMix(float k, float a = 1.f) {  // k>0 lighten, k<0 darken
    ImVec4 c = AccentV(a);
    if (k > 0) { c.x += (1 - c.x) * k; c.y += (1 - c.y) * k; c.z += (1 - c.z) * k; }
    else { c.x *= 1 + k; c.y *= 1 + k; c.z *= 1 + k; }
    return ImGui::ColorConvertFloat4ToU32(c);
}
inline ImU32 LerpCol(ImU32 a, ImU32 b, float t) {
    ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}
inline ImU32 WithAlpha(ImU32 c, float a) { ImVec4 v = ImGui::ColorConvertU32ToFloat4(c); v.w *= a; return ImGui::ColorConvertFloat4ToU32(v); }
float AccentLuma() { return 0.299f * g_cfg.accent[0] + 0.587f * g_cfg.accent[1] + 0.114f * g_cfg.accent[2]; }
ImU32 OnAccent(float a = 1.f) { return AccentLuma() > 0.6f ? RGBA(12, 14, 18, a) : RGBA(255, 255, 255, a); }

const ImU32 C_TEXT = IM_COL32(236, 238, 242, 255);
const ImU32 C_DIM = IM_COL32(140, 146, 158, 255);
const ImU32 C_FAINT = IM_COL32(92, 98, 110, 255);

float g_alpha = 1.f;  // global alpha multiplier for the menu fade
inline ImU32 A(ImU32 c) { return WithAlpha(c, g_alpha); }

// ------------------------------------------------------------------ animation
std::unordered_map<ImGuiID, float> g_anim;
float Approach(float cur, float target, float speed) {
    if (!g_cfg.animations) return target;
    float k = 1.f - std::exp(-speed * g_cfg.animSpeed * g_dt);
    float v = cur + (target - cur) * k;
    if (std::fabs(v - target) < 0.0005f) v = target;
    return v;
}
float Anim(ImGuiID id, float target, float speed = 14.f) {
    auto it = g_anim.find(id);
    if (it == g_anim.end()) { g_anim[id] = target; return target; }
    it->second = Approach(it->second, target, speed);
    return it->second;
}
float Anim(const char* key, float target, float speed = 14.f) { return Anim(ImHashStr(key), target, speed); }
inline float EaseOut(float t) { return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }

// ------------------------------------------------------------------ toasts
struct ToastItem { std::string text; int kind; float t; float life; float y; };
std::vector<ToastItem> g_toasts;
std::mutex g_toastMtx;

// ------------------------------------------------------------------ menu state
bool g_open = false;
float g_openAnim = 0.f;
ImVec2 g_menuPos(-1, -1);
bool g_dragging = false;
ImVec2 g_dragOff;
float g_tabAnim = 0.f;  // animated indicator position
float g_contentAnim = 1.f;
int g_lastTab = -1;
char g_coinBuf[24] = "1000";
float g_coinShown = 0.f;
float g_fpsShown = 0.f, g_fpsTimer = 0.f;
float g_wmAnim = 0.f, g_wmWidth = 0.f;
float g_customAnim = 0.f;

// ------------------------------------------------------------------ text helpers
ImVec2 TextSize(ImFont* f, const char* t) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.f, t); }
void Text(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* t) { dl->AddText(f, f->FontSize, p, A(c), t); }
void TextWrapped(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* t, float w) {
    dl->AddText(f, f->FontSize, p, A(c), t, nullptr, w);
}
float WrappedHeight(ImFont* f, const char* t, float w) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, w, t).y; }

// ------------------------------------------------------------------ icons (vector)
enum Icon { IC_EYE, IC_USER, IC_COIN, IC_LAYOUT, IC_PALETTE, IC_GEAR, IC_CHECK, IC_INFO, IC_WARN, IC_ARROW, IC_FOLDER };
void DrawIcon(ImDrawList* dl, Icon ic, ImVec2 c, float r, ImU32 col) {
    float th = 1.6f * S;
    switch (ic) {
    case IC_EYE: {
        dl->PathArcTo(ImVec2(c.x, c.y + r * 0.9f), r * 1.25f, -IM_PI * 0.78f, -IM_PI * 0.22f, 16);
        dl->PathArcTo(ImVec2(c.x, c.y - r * 0.9f), r * 1.25f, IM_PI * 0.22f, IM_PI * 0.78f, 16);
        dl->PathStroke(col, ImDrawFlags_Closed, th);
        dl->AddCircleFilled(c, r * 0.36f, col, 16);
        break;
    }
    case IC_USER:
        dl->AddCircle(ImVec2(c.x, c.y - r * 0.38f), r * 0.42f, col, 20, th);
        dl->PathArcTo(ImVec2(c.x, c.y + r * 1.05f), r * 0.85f, IM_PI * 1.08f, IM_PI * 1.92f, 20);
        dl->PathStroke(col, 0, th);
        break;
    case IC_COIN:
        dl->AddCircle(c, r * 0.9f, col, 24, th);
        dl->AddCircle(c, r * 0.5f, col, 20, th * 0.8f);
        break;
    case IC_LAYOUT:
        dl->AddRect(ImVec2(c.x - r * 0.9f, c.y - r * 0.8f), ImVec2(c.x + r * 0.9f, c.y + r * 0.8f), col, 3 * S, 0, th);
        dl->AddLine(ImVec2(c.x - r * 0.2f, c.y - r * 0.8f), ImVec2(c.x - r * 0.2f, c.y + r * 0.8f), col, th);
        dl->AddLine(ImVec2(c.x - r * 0.2f, c.y - r * 0.05f), ImVec2(c.x + r * 0.9f, c.y - r * 0.05f), col, th);
        break;
    case IC_PALETTE:
        dl->AddCircleFilled(ImVec2(c.x - r * 0.45f, c.y + r * 0.3f), r * 0.38f, col, 16);
        dl->AddCircle(ImVec2(c.x + r * 0.45f, c.y + r * 0.3f), r * 0.38f, col, 16, th);
        dl->AddCircle(ImVec2(c.x, c.y - r * 0.45f), r * 0.38f, col, 16, th);
        break;
    case IC_GEAR: {
        for (int i = 0; i < 8; ++i) {
            float a = i * IM_PI / 4 + g_time * 0.0f;
            dl->AddLine(ImVec2(c.x + std::cos(a) * r * 0.55f, c.y + std::sin(a) * r * 0.55f),
                        ImVec2(c.x + std::cos(a) * r * 0.95f, c.y + std::sin(a) * r * 0.95f), col, th * 1.4f);
        }
        dl->AddCircle(c, r * 0.6f, col, 20, th);
        dl->AddCircleFilled(c, r * 0.22f, col, 12);
        break;
    }
    case IC_CHECK:
        dl->PathLineTo(ImVec2(c.x - r * 0.55f, c.y));
        dl->PathLineTo(ImVec2(c.x - r * 0.12f, c.y + r * 0.45f));
        dl->PathLineTo(ImVec2(c.x + r * 0.6f, c.y - r * 0.45f));
        dl->PathStroke(col, 0, th * 1.3f);
        break;
    case IC_INFO:
        dl->AddLine(ImVec2(c.x, c.y - r * 0.05f), ImVec2(c.x, c.y + r * 0.55f), col, th * 1.3f);
        dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.45f), th * 0.9f, col);
        break;
    case IC_WARN:
        dl->AddLine(ImVec2(c.x, c.y - r * 0.55f), ImVec2(c.x, c.y + r * 0.1f), col, th * 1.3f);
        dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.5f), th * 0.9f, col);
        break;
    case IC_FOLDER:
        dl->AddRect(ImVec2(c.x - r * 0.9f, c.y - r * 0.42f), ImVec2(c.x + r * 0.9f, c.y + r * 0.72f), col, 2.5f * S, 0, th);
        dl->PathLineTo(ImVec2(c.x - r * 0.9f, c.y - r * 0.42f));
        dl->PathLineTo(ImVec2(c.x - r * 0.9f, c.y - r * 0.72f));
        dl->PathLineTo(ImVec2(c.x - r * 0.18f, c.y - r * 0.72f));
        dl->PathLineTo(ImVec2(c.x + r * 0.05f, c.y - r * 0.42f));
        dl->PathStroke(col, 0, th);
        break;
    case IC_ARROW:
        dl->PathLineTo(ImVec2(c.x - r * 0.25f, c.y - r * 0.5f));
        dl->PathLineTo(ImVec2(c.x + r * 0.3f, c.y));
        dl->PathLineTo(ImVec2(c.x - r * 0.25f, c.y + r * 0.5f));
        dl->PathStroke(col, 0, th * 1.2f);
        break;
    }
}

// Soft drop shadow made of stacked translucent rounded rects.
void Shadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float size, float alpha) {
    const int steps = 6;
    for (int i = steps; i >= 1; --i) {
        float e = size * i / steps;
        dl->AddRectFilled(ImVec2(a.x - e, a.y - e + size * 0.35f), ImVec2(b.x + e, b.y + e + size * 0.35f),
                          IM_COL32(0, 0, 0, (int)(alpha * 255.f * g_alpha / (steps * 1.6f))), rounding + e);
    }
}

// Rounded rectangle with a vertical gradient (colours incl. alpha). One shape, so pills and any corner
// radius stay clean (no seams or square corners from stitched pieces).
void GradientRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 top, ImU32 bot, float r, ImDrawFlags fl = 0) {
    if (b.y - a.y <= 0.f || b.x - a.x <= 0.f) return;
    int v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, r, fl);
    int v1 = dl->VtxBuffer.Size;
    ImVec4 ct = ImGui::ColorConvertU32ToFloat4(top), cb = ImGui::ColorConvertU32ToFloat4(bot);
    float inv = 1.f / (b.y - a.y);
    for (int i = v0; i < v1; ++i) {
        ImDrawVert& v = dl->VtxBuffer[i];
        float t = std::clamp((v.pos.y - a.y) * inv, 0.f, 1.f);
        ImVec4 c(ct.x + (cb.x - ct.x) * t, ct.y + (cb.y - ct.y) * t, ct.z + (cb.z - ct.z) * t, ct.w + (cb.w - ct.w) * t);
        c.w *= ImGui::ColorConvertU32ToFloat4(v.col).w;  // keep anti-aliasing fringe alpha
        v.col = ImGui::ColorConvertFloat4ToU32(c);
    }
}

// ------------------------------------------------------------------ widgets
void Switch(ImDrawList* dl, ImVec2 p, float t, float hover) {
    float w = 40 * S, h = 22 * S;
    ImU32 off = LerpCol(RGBA(44, 48, 58), RGBA(56, 61, 73), hover);
    ImU32 bg = LerpCol(off, Accent(), t);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(bg), h * 0.5f);
    if (t > 0.01f) dl->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + w + 1, p.y + h + 1), A(Accent(0.25f * t)), h * 0.5f + 1, 0, 2 * S);
    float r = h * 0.5f - 3 * S;
    float x = p.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(x, p.y + h * 0.5f + 1 * S), r + 1 * S, A(IM_COL32(0, 0, 0, 60)), 24);
    dl->AddCircleFilled(ImVec2(x, p.y + h * 0.5f), r, A(IM_COL32(255, 255, 255, 255)), 24);
}

// Card row with title, optional description and a switch. Returns true when toggled.
bool ToggleRow(const char* id, const char* title, const char* desc, bool* v, bool sub = false) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    float pad = 14 * S;
    float descW = w - pad * 3 - 40 * S;
    float h = sub ? 38 * S : (desc ? std::max(56 * S, 24 * S + 10 * S + WrappedHeight(F_Small, desc, descW) + 10 * S) : 46 * S);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool pressed = ImGui::InvisibleButton("##row", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    if (pressed) { *v = !*v; ConfigMarkDirty(); }
    float ht = Anim(ImGui::GetID("hov"), hov ? 1.f : 0.f, 16.f);
    float t = Anim(ImGui::GetID("on"), *v ? 1.f : 0.f, 14.f);
    float press = Anim(ImGui::GetID("press"), ImGui::IsItemActive() ? 1.f : 0.f, 25.f);
    ImGui::PopID();
    ImVec2 a = ImVec2(p.x + press * 2 * S, p.y), b = ImVec2(p.x + w - press * 2 * S, p.y + h);
    if (!sub) {
        dl->AddRectFilled(a, b, A(LerpCol(RGBA(24, 27, 34), RGBA(30, 34, 43), ht)), 11 * S);
        dl->AddRect(a, b, A(LerpCol(RGBA(255, 255, 255, 0.05f), Accent(0.35f), t * 0.6f + ht * 0.15f)), 11 * S, 0, 1.f);
        if (t > 0.01f) dl->AddRectFilled(ImVec2(a.x, a.y + h * 0.28f), ImVec2(a.x + 3 * S, b.y - h * 0.28f), A(Accent(t)), 2 * S);
    } else if (ht > 0.01f) {
        dl->AddRectFilled(a, b, A(RGBA(255, 255, 255, 0.03f * ht)), 8 * S);
    }
    ImFont* tf = sub ? F_Body : F_Semi;
    float ty = desc && !sub ? p.y + 11 * S : p.y + (h - tf->FontSize) * 0.5f;
    Text(dl, tf, ImVec2(p.x + pad, ty), LerpCol(RGBA(214, 218, 226), C_TEXT, std::max(t, ht)), title);
    if (desc && !sub) TextWrapped(dl, F_Small, ImVec2(p.x + pad, ty + tf->FontSize + 4 * S), C_DIM, desc, descW);
    Switch(dl, ImVec2(b.x - pad - 40 * S, p.y + (h - 22 * S) * 0.5f), t, ht);
    ImGui::Dummy(ImVec2(0, sub ? 2 * S : 6 * S));
    return pressed;
}

bool g_lastSliderActive = false;
bool SliderRow(const char* id, const char* title, float* v, float mn, float mx, const char* fmt) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    float pad = 14 * S, h = 54 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("##sl", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    float x0 = p.x + pad, x1 = p.x + w - pad;
    bool changed = false;
    if (act) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - x0) / (x1 - x0), 0.f, 1.f);
        float nv = mn + (mx - mn) * t;
        if (nv != *v) { *v = nv; changed = true; ConfigMarkDirty(); }
    }
    g_lastSliderActive = act;
    float ht = Anim(ImGui::GetID("hov"), (hov || act) ? 1.f : 0.f, 16.f);
    float tv = act ? (*v - mn) / (mx - mn) : Anim(ImGui::GetID("val"), (*v - mn) / (mx - mn), 20.f);
    if (act) g_anim[ImGui::GetID("val")] = tv;
    float grab = Anim(ImGui::GetID("grab"), act ? 1.f : 0.f, 20.f);
    ImGui::PopID();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(LerpCol(RGBA(24, 27, 34), RGBA(30, 34, 43), ht * 0.7f)), 11 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 11 * S);
    Text(dl, F_Body, ImVec2(x0, p.y + 9 * S), C_TEXT, title);
    char buf[32];
    snprintf(buf, sizeof buf, fmt, *v);
    ImVec2 vs = TextSize(F_Semi, buf);
    Text(dl, F_Semi, ImVec2(x1 - vs.x, p.y + 8 * S), Accent(), buf);
    float ty = p.y + h - 15 * S;
    dl->AddRectFilled(ImVec2(x0, ty - 2 * S), ImVec2(x1, ty + 2 * S), A(RGBA(48, 53, 64)), 2 * S);
    float xf = x0 + (x1 - x0) * tv;
    dl->AddRectFilled(ImVec2(x0, ty - 2 * S), ImVec2(xf, ty + 2 * S), A(Accent()), 2 * S);
    float kr = (6 + 2 * grab + ht) * S;
    dl->AddCircleFilled(ImVec2(xf, ty), kr + 4 * S * grab, A(Accent(0.25f * (ht + grab) * 0.5f)), 24);
    dl->AddCircleFilled(ImVec2(xf, ty), kr, A(IM_COL32(255, 255, 255, 255)), 24);
    ImGui::Dummy(ImVec2(0, 6 * S));
    return changed;
}

bool Button(const char* id, const char* label, ImVec2 size, bool primary, ImFont* font = nullptr) {
    if (!font) font = F_Semi;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool pressed = ImGui::InvisibleButton("##b", size);
    bool hov = ImGui::IsItemHovered();
    float ht = Anim(ImGui::GetID("hov"), hov ? 1.f : 0.f, 16.f);
    float pr = Anim(ImGui::GetID("pr"), ImGui::IsItemActive() ? 1.f : 0.f, 30.f);
    float flash = Anim(ImGui::GetID("flash"), 0.f, 6.f);
    if (pressed) g_anim[ImGui::GetID("flash")] = 1.f;
    ImGui::PopID();
    float sh = pr * 1.5f * S;
    ImVec2 a(p.x + sh, p.y + sh), b(p.x + size.x - sh, p.y + size.y - sh);
    float r = std::min(10 * S, size.y * 0.5f);
    if (primary) {
        if (ht > 0.01f) dl->AddRectFilled(ImVec2(a.x - 3 * S, a.y - 3 * S), ImVec2(b.x + 3 * S, b.y + 3 * S), A(Accent(0.18f * ht)), r + 3 * S);
        dl->AddRectFilled(a, b, A(AccentMix(0.10f * ht)), r);
        GradientRect(dl, a, b, A(RGBA(255, 255, 255, 0.13f + 0.06f * ht)), A(RGBA(255, 255, 255, 0.f)), r);
        ImVec2 ts = TextSize(font, label);
        Text(dl, font, ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f), OnAccent(), label);
    } else {
        dl->AddRectFilled(a, b, A(LerpCol(RGBA(34, 38, 47), RGBA(44, 49, 60), ht)), r);
        dl->AddRect(a, b, A(LerpCol(RGBA(255, 255, 255, 0.06f), Accent(0.5f), ht * 0.6f)), r);
        ImVec2 ts = TextSize(font, label);
        Text(dl, font, ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f), LerpCol(C_TEXT, AccentMix(0.35f), ht * 0.5f), label);
    }
    if (flash > 0.01f) dl->AddRect(ImVec2(a.x - 4 * S * (1 - flash), a.y - 4 * S * (1 - flash)), ImVec2(b.x + 4 * S * (1 - flash), b.y + 4 * S * (1 - flash)), A(Accent(flash * 0.8f)), r + 3 * S, 0, 2 * S);
    return pressed;
}

void SectionLabel(const char* t) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    Text(dl, F_Small, ImVec2(p.x + 4 * S, p.y), C_FAINT, t);
    ImGui::Dummy(ImVec2(0, F_Small->FontSize + 8 * S));
}

// Smoothly expanding area for sub-options. Returns true if contents should be drawn.
bool BeginExpand(const char* id, bool open, float fullH) {
    float t = Anim(id, open ? 1.f : 0.f, 12.f);
    if (t <= 0.002f) return false;
    float e = EaseOut(t);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_alpha * e);
    ImGui::BeginChild(id, ImVec2(0, fullH * e), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(p.x + 6 * S, p.y + 2 * S), ImVec2(p.x + 8 * S, p.y + fullH - 10 * S), A(Accent(0.5f)), 1 * S);
    ImGui::Indent(18 * S);
    return true;
}
void EndExpand() {
    ImGui::Unindent(18 * S);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0, 4 * S));
}

// ------------------------------------------------------------------ style
void ApplyStyle() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 16 * S;
    st.ChildRounding = 10 * S;
    st.FrameRounding = 9 * S;
    st.PopupRounding = 12 * S;
    st.GrabRounding = 8 * S;
    st.ScrollbarRounding = 8 * S;
    st.ScrollbarSize = 8 * S;
    st.WindowBorderSize = 0;
    st.ChildBorderSize = 0;
    st.PopupBorderSize = 1;
    st.FrameBorderSize = 0;
    st.WindowPadding = ImVec2(0, 0);
    st.FramePadding = ImVec2(12 * S, 9 * S);
    st.ItemSpacing = ImVec2(8 * S, 6 * S);
    st.ItemInnerSpacing = ImVec2(8 * S, 6 * S);
    st.AntiAliasedFill = true;
    st.AntiAliasedLines = true;
}
void ApplyColors() {
    ImVec4* c = ImGui::GetStyle().Colors;
    ImVec4 acc = AccentV();
    c[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.95f, 1);
    c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.57f, 0.62f, 1);
    c[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.055f, 0.07f, 1);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.11f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.07f);
    c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.16f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.16f, 0.20f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.18f, 0.22f, 1);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(acc.x, acc.y, acc.z, 0.5f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(acc.x, acc.y, acc.z, 0.8f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(acc.x, acc.y, acc.z, 0.35f);
    c[ImGuiCol_NavHighlight] = ImVec4(acc.x, acc.y, acc.z, 0.6f);
    c[ImGuiCol_CheckMark] = acc;
    c[ImGuiCol_SliderGrab] = acc;
    c[ImGuiCol_SliderGrabActive] = acc;
    c[ImGuiCol_Button] = ImVec4(0.14f, 0.15f, 0.19f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(acc.x, acc.y, acc.z, 0.35f);
    c[ImGuiCol_ButtonActive] = ImVec4(acc.x, acc.y, acc.z, 0.55f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.4f);
}

void BuildFonts(float h) {
    ImGuiIO& io = ImGui::GetIO();
    S = std::clamp(h / 1080.f, 0.8f, 2.2f) * g_cfg.menuScale;
    g_fontH = h;
    g_appliedMenuScale = g_cfg.menuScale;
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.OversampleH = 2; cfg.OversampleV = 2; cfg.PixelSnapH = false;
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x0400, 0x052F, 0x2000, 0x206F, 0x2190, 0x21FF, 0x2500, 0x25FF, 0};
    std::string med = g_baseDir + "fonts\\Inter-Medium.ttf";
    std::string semi = g_baseDir + "fonts\\Inter-SemiBold.ttf";
    std::string bold = g_baseDir + "fonts\\Inter-Bold.ttf";
    auto load = [&](const std::string& path, float px) -> ImFont* {
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
        return io.Fonts->AddFontFromFileTTF(path.c_str(), px, &cfg, ranges);
    };
    F_Body = load(med, 15.5f * S);
    if (!F_Body) { LOG("fonts missing in %s", g_baseDir.c_str()); F_Body = io.Fonts->AddFontDefault(); }
    F_Small = load(med, 13.f * S);
    F_Semi = load(semi, 15.5f * S);
    F_Title = load(semi, 21.f * S);
    F_Big = load(bold, 30.f * S);
    F_Huge = load(bold, 58.f * S);
    if (!F_Small) F_Small = F_Body;
    if (!F_Semi) F_Semi = F_Body;
    if (!F_Title) F_Title = F_Semi;
    if (!F_Big) F_Big = F_Title;
    if (!F_Huge) F_Huge = F_Big;
    io.FontDefault = F_Body;
    io.Fonts->Build();
    ApplyStyle();
}

// ------------------------------------------------------------------ watermark
void DrawWatermark(ImDrawList* dl, ImVec2 disp) {
    const Config& c = g_cfg;
    bool any = c.wmName || c.wmFps || c.wmTime || c.wmCoins || c.wmStatus;
    g_wmAnim = Approach(g_wmAnim, c.watermark && any ? 1.f : 0.f, 9.f);
    if (g_wmAnim <= 0.002f) return;
    ImGuiIO& io = ImGui::GetIO();
    g_fpsTimer -= g_dt;
    if (g_fpsTimer <= 0) { g_fpsTimer = 0.25f; g_fpsShown = io.Framerate; }
    // segments
    struct Seg { std::string t; ImFont* f; ImU32 col; };
    std::vector<Seg> segs;
    if (c.wmName) segs.push_back({c.wmText.empty() ? "ZenClient" : c.wmText, F_Semi, C_TEXT});
    char buf[64];
    if (c.wmFps) { snprintf(buf, sizeof buf, "%d FPS", (int)std::lround(g_fpsShown)); segs.push_back({buf, F_Body, RGBA(200, 205, 214)}); }
    if (c.wmTime) { SYSTEMTIME st; GetLocalTime(&st); snprintf(buf, sizeof buf, "%02d:%02d", st.wHour, st.wMinute); segs.push_back({buf, F_Body, RGBA(200, 205, 214)}); }
    bool inGame, inMenu; float coins;
    { std::lock_guard<std::mutex> lk(g_snap.mtx); inGame = g_snap.inGame; inMenu = g_snap.inMainMenu; coins = g_snap.coins; }
    if (c.wmCoins) { snprintf(buf, sizeof buf, "%.0f монет", coins); segs.push_back({buf, F_Body, RGBA(255, 214, 120)}); }
    if (c.wmStatus) segs.push_back({inMenu ? "Главное меню" : "В игре", F_Body, Accent()});
    (void)inGame;
    float k = c.wmScale;
    auto size = [&](const Seg& sg) { return sg.f->CalcTextSizeA(sg.f->FontSize * k, FLT_MAX, 0.f, sg.t.c_str()); };
    float h = 34 * S * k, dot = c.wmDot ? 5 * S * k : 0.f, pad = (c.wmStyle == 2 ? 4 : 14) * S * k, gap = 9 * S * k;
    float sepW = F_Body->CalcTextSizeA(F_Body->FontSize * k, FLT_MAX, 0.f, "|").x;
    float wTarget = pad * 2 + (dot > 0 ? dot * 2 + gap : 0.f);
    for (size_t i = 0; i < segs.size(); ++i) wTarget += size(segs[i]).x + (i ? gap * 2 + sepW : 0.f);
    g_wmWidth = g_wmWidth <= 0 ? wTarget : Approach(g_wmWidth, wTarget, 12.f);
    float e = EaseOut(g_wmAnim), m = 12 * S;
    bool top = c.wmPos <= 2;
    float y = top ? m - (1 - e) * (h + 16 * S) : disp.y - h - m + (1 - e) * (h + 16 * S);
    int col = c.wmPos == 1 || c.wmPos == 3 ? 0 : (c.wmPos == 2 || c.wmPos == 5) ? 2 : 1;
    float x = col == 0 ? m : col == 2 ? disp.x - m - g_wmWidth : (disp.x - g_wmWidth) * 0.5f;
    ImVec2 a(x, y), b(x + g_wmWidth, y + h);
    float sa = g_alpha;
    g_alpha = e;
    float r = c.wmStyle == 0 ? h * 0.5f : 7 * S * k;
    if (c.wmStyle != 2) {
        Shadow(dl, a, b, r, 10 * S, 0.55f * c.wmAlpha);
        dl->AddRectFilled(a, b, A(RGBA(6, 7, 9, c.wmAlpha)), r);
        dl->AddRect(a, b, A(RGBA(255, 255, 255, 0.08f * c.wmAlpha / 0.93f)), r, 0, 1.f);
        if (c.wmStyle == 1) dl->AddRectFilled(ImVec2(a.x, a.y + h * 0.22f), ImVec2(a.x + 3 * S, b.y - h * 0.22f), A(Accent()), 2 * S);
    }
    float tx = a.x + pad, cy = (a.y + b.y) * 0.5f;
    if (dot > 0) {
        ImVec2 dc(tx + dot, cy);
        float pulse = 0.5f + 0.5f * std::sin(g_time * 3.0f);
        dl->AddCircleFilled(dc, dot + 4 * S * k * pulse, A(RGBA(52, 211, 120, 0.18f * (1 - pulse * 0.6f))), 24);
        dl->AddCircleFilled(dc, dot, A(RGBA(52, 211, 120)), 24);
        tx += dot * 2 + gap;
    }
    dl->PushClipRect(ImVec2(a.x - 4, a.y - 4), ImVec2(b.x + 4, b.y + 4), true);
    for (size_t i = 0; i < segs.size(); ++i) {
        if (i) {
            tx += gap;
            dl->AddText(F_Body, F_Body->FontSize * k, ImVec2(tx, cy - F_Body->FontSize * k * 0.5f), A(RGBA(255, 255, 255, 0.22f)), "|");
            tx += sepW + gap;
        }
        ImVec2 ts = size(segs[i]);
        ImVec2 tp(tx, cy - ts.y * 0.5f);
        if (c.wmStyle == 2) dl->AddText(segs[i].f, segs[i].f->FontSize * k, ImVec2(tp.x + 1, tp.y + 1), A(RGBA(0, 0, 0, 0.7f)), segs[i].t.c_str());
        dl->AddText(segs[i].f, segs[i].f->FontSize * k, tp, A(segs[i].col), segs[i].t.c_str());
        tx += ts.x;
    }
    dl->PopClipRect();
    g_alpha = sa;
}

// ------------------------------------------------------------------ hitboxes
void DrawEsp(ImDrawList* dl, ImVec2 disp) {
    static float espAnim = 0;
    espAnim = Approach(espAnim, g_cfg.hitboxes ? 1.f : 0.f, 10.f);
    if (espAnim <= 0.002f) return;
    std::vector<EspEntry> esp;
    int sw, sh;
    {
        std::lock_guard<std::mutex> lk(g_snap.mtx);
        esp = g_snap.esp;
        sw = g_snap.screenW; sh = g_snap.screenH;
    }
    if (sw <= 0 || sh <= 0) return;
    float kx = disp.x / sw, ky = disp.y / sh;
    const Config& c = g_cfg;
    static const int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    ImU32 npcBase = ImGui::ColorConvertFloat4ToU32(ImVec4(c.espNpcCol[0], c.espNpcCol[1], c.espNpcCol[2], 1));
    ImU32 itemBase = c.espItemAccent ? Accent() : ImGui::ColorConvertFloat4ToU32(ImVec4(c.espItemCol[0], c.espItemCol[1], c.espItemCol[2], 1));
    ImFont* lf = F_Small;
    float lfs = lf->FontSize * c.espLabelScale;
    for (auto& e : esp) {
        float fade = e.npc ? 1.f : std::clamp(1.2f - e.dist / c.espMaxDist, 0.25f, 1.f);
        ImU32 col = WithAlpha(e.npc ? npcBase : itemBase, espAnim * fade);
        ImVec2 p[8];
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (int i = 0; i < 8; ++i) {
            p[i] = ImVec2(e.pts[i][0] * kx, e.pts[i][1] * ky);
            minx = std::min(minx, p[i].x); maxx = std::max(maxx, p[i].x);
            miny = std::min(miny, p[i].y); maxy = std::max(maxy, p[i].y);
        }
        if (maxx < -200 || minx > disp.x + 200) continue;
        float th = (e.npc ? 1.6f : 1.2f) * c.espThick * S;
        ImU32 lc = WithAlpha(col, 0.9f);
        if (c.espTracers) dl->AddLine(ImVec2(disp.x * 0.5f, disp.y), ImVec2((minx + maxx) * 0.5f, maxy), WithAlpha(col, 0.55f), std::max(1.f, th * 0.8f));
        if (c.espFill > 0.001f) dl->AddRectFilled(ImVec2(minx, miny), ImVec2(maxx, maxy), WithAlpha(col, c.espFill), 2 * S);
        if (c.espBox == 0) {
            for (auto& ed : edges) dl->AddLine(p[ed[0]], p[ed[1]], lc, th);
        } else if (c.espBox == 1) {
            dl->AddRect(ImVec2(minx, miny), ImVec2(maxx, maxy), lc, 2 * S, 0, th);
        } else {
            float lx = (maxx - minx) * 0.25f, ly = (maxy - miny) * 0.25f;
            const ImVec2 cs[4] = {{minx, miny}, {maxx, miny}, {minx, maxy}, {maxx, maxy}};
            for (int i = 0; i < 4; ++i) {
                float sx = (i & 1) ? -1.f : 1.f, sy = (i & 2) ? -1.f : 1.f;
                dl->AddLine(cs[i], ImVec2(cs[i].x + lx * sx, cs[i].y), lc, th);
                dl->AddLine(cs[i], ImVec2(cs[i].x, cs[i].y + ly * sy), lc, th);
            }
        }
        if (c.hbLabels) {
            char buf[160];
            if (c.espDist) snprintf(buf, sizeof buf, "%s  %.0fм", e.label.c_str(), e.dist);
            else snprintf(buf, sizeof buf, "%s", e.label.c_str());
            ImVec2 ts = lf->CalcTextSizeA(lfs, FLT_MAX, 0.f, buf);
            ImVec2 cc((minx + maxx) * 0.5f, miny - 8 * S);
            float px = 7 * S * c.espLabelScale, py = 3 * S * c.espLabelScale;
            ImVec2 a(cc.x - ts.x * 0.5f - px, cc.y - ts.y - py * 2), b(cc.x + ts.x * 0.5f + px, cc.y);
            dl->AddRectFilled(a, b, RGBA(8, 9, 12, 0.78f * espAnim * fade), 6 * S);
            dl->AddRectFilled(ImVec2(a.x, b.y - 2 * S), ImVec2(b.x, b.y), WithAlpha(col, 0.9f), 2 * S, ImDrawFlags_RoundCornersBottom);
            dl->AddText(lf, lfs, ImVec2(a.x + px, a.y + py), RGBA(240, 242, 246, espAnim * fade), buf);
        }
    }
}

// ------------------------------------------------------------------ toasts
void DrawToasts(ImDrawList* dl, ImVec2 disp) {
    std::lock_guard<std::mutex> lk(g_toastMtx);
    bool topSide = g_cfg.toastPos >= 2, leftSide = g_cfg.toastPos == 1 || g_cfg.toastPos == 3;
    // keep clear of a watermark in the same corner
    bool wmHere = g_cfg.watermark && ((topSide && ((leftSide && g_cfg.wmPos == 1) || (!leftSide && g_cfg.wmPos == 2))) ||
                                      (!topSide && ((leftSide && g_cfg.wmPos == 3) || (!leftSide && g_cfg.wmPos == 5))));
    float wmOff = wmHere ? 34 * S * g_cfg.wmScale + 10 * S : 0.f;
    float y = topSide ? 24 * S + wmOff : disp.y - 24 * S - wmOff;
    for (int i = (int)g_toasts.size() - 1; i >= 0; --i) {
        auto& t = g_toasts[i];
        t.t += g_dt;
        float in = std::clamp(t.t / 0.35f, 0.f, 1.f), out = std::clamp((t.life - t.t) / 0.4f, 0.f, 1.f);
        if (!g_cfg.animations) { in = 1; out = t.t < t.life ? 1.f : 0.f; }
        float vis = EaseOut(in) * out;
        ImVec2 ts = F_Body->CalcTextSizeA(F_Body->FontSize, FLT_MAX, 340 * S, t.text.c_str());
        float h = std::max(46 * S, ts.y + 24 * S), w = ts.x + 74 * S;
        float targetY = topSide ? y : y - h;
        t.y = t.y == 0 ? targetY : Approach(t.y, targetY, 14.f);
        float slideX = (1 - EaseOut(in)) * (w + 40 * S);
        float x = leftSide ? 24 * S - slideX : disp.x - 24 * S - w + slideX;
        ImVec2 a(x, t.y), b(x + w, t.y + h);
        float sa = g_alpha;
        g_alpha = vis;
        ImU32 kc = t.kind == 1 ? Accent() : t.kind == 2 ? RGBA(255, 186, 72) : RGBA(120, 170, 255);
        Shadow(dl, a, b, 12 * S, 12 * S, 0.6f);
        dl->AddRectFilled(a, b, A(RGBA(14, 16, 20, 0.96f)), 12 * S);
        dl->AddRect(a, b, A(RGBA(255, 255, 255, 0.07f)), 12 * S);
        ImVec2 ic(a.x + 24 * S, (a.y + b.y) * 0.5f);
        dl->AddCircleFilled(ic, 12 * S, A(WithAlpha(kc, 0.16f)), 24);
        DrawIcon(dl, t.kind == 1 ? IC_CHECK : t.kind == 2 ? IC_WARN : IC_INFO, ic, 9 * S, A(kc));
        dl->AddText(F_Body, F_Body->FontSize, ImVec2(a.x + 46 * S, (a.y + b.y - ts.y) * 0.5f), A(C_TEXT), t.text.c_str(), nullptr, 340 * S);
        float prog = std::clamp(1.f - t.t / t.life, 0.f, 1.f);
        dl->AddRectFilled(ImVec2(a.x + 12 * S, b.y - 3 * S), ImVec2(a.x + 12 * S + (w - 24 * S) * prog, b.y - 1 * S), A(WithAlpha(kc, 0.7f)), 1 * S);
        g_alpha = sa;
        y = topSide ? t.y + h + 10 * S : t.y - 10 * S;
    }
    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(), [](const ToastItem& t) { return t.t >= t.life; }), g_toasts.end());
}

#include "ui_custom.inl"

// Card with a title and a value switched by < > buttons.
bool ChoiceRow(const char* id, const char* title, int* v, const char* const* names, int n) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x, h = 46 * S, pad = 14 * S, bw = 34 * S, vw = 150 * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 11 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 11 * S);
    Text(dl, F_Body, ImVec2(p.x + pad, p.y + (h - F_Body->FontSize) * 0.5f), C_TEXT, title);
    ImGui::PushID(id);
    bool changed = false;
    float x1 = p.x + w - 8 * S - bw;
    ImGui::SetCursorScreenPos(ImVec2(x1 - vw - bw, p.y + 6 * S));
    if (Button("prev", "<", ImVec2(bw, h - 12 * S), false)) { *v = (*v - 1 + n) % n; changed = true; }
    ImGui::SetCursorScreenPos(ImVec2(x1, p.y + 6 * S));
    if (Button("next", ">", ImVec2(bw, h - 12 * S), false)) { *v = (*v + 1) % n; changed = true; }
    ImGui::PopID();
    const char* val = names[std::clamp(*v, 0, n - 1)];
    ImVec2 vs = TextSize(F_Semi, val);
    Text(dl, F_Semi, ImVec2(x1 - vw * 0.5f - vs.x * 0.5f, p.y + (h - vs.y) * 0.5f), Accent(), val);
    if (changed) ConfigMarkDirty();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, 6 * S));
    return changed;
}

// ------------------------------------------------------------------ menu tabs
struct TabDef { const char* name; const char* sub; Icon icon; };
const TabDef kTabs[] = {
    {"Визуал", "Хитбоксы NPC и предметов, их вид", IC_EYE},
    {"Игрок", "Бессмертие, движение, обзор и мир", IC_USER},
    {"Ресурсы", "Монеты и открытие предметов", IC_COIN},
    {"Интерфейс", "Новый вид главного меню", IC_LAYOUT},
    {"Тема", "Цвета, анимации и окно меню", IC_PALETTE},
    {"Клиент", "Клавиша, водяной знак, уведомления", IC_GEAR},
    {"Конфиги", "Сохранение, импорт и экспорт настроек", IC_FOLDER},
};
const int kTabCount = 7;

void TabVisuals() {
    SectionLabel("ХИТБОКСЫ");
    ToggleRow("hb", "Показывать хитбоксы", "Рамки вокруг врагов и предметов, видимые сквозь стены", &g_cfg.hitboxes);
    if (BeginExpand("##hbsub", g_cfg.hitboxes, 3 * 40 * S + 8 * S)) {
        ToggleRow("hbn", "Показывать NPC", nullptr, &g_cfg.hbNpc, true);
        ToggleRow("hbi", "Показывать предметы", nullptr, &g_cfg.hbItems, true);
        ToggleRow("hbl", "Подписи", nullptr, &g_cfg.hbLabels, true);
        EndExpand();
    }
    Config& c = g_cfg;
    SectionLabel("ВИД");
    SegRow("eb", "Тип рамки", &c.espBox, {"3D-коробка", "2D-рамка", "Уголки"});
    ColorRow("enc", "Цвет NPC", c.espNpcCol);
    ToggleRow("eia", "Предметы цветом акцента", nullptr, &c.espItemAccent, true);
    if (BeginExpand("##eicsub", !c.espItemAccent, 54 * S)) {
        ColorRow("eic", "Цвет предметов", c.espItemCol);
        EndExpand();
    }
    SliderRow("eth", "Толщина линий", &c.espThick, 0.5f, 4.f, "x%.1f");
    PctRow("efl", "Заливка рамки", &c.espFill, 0.f, 0.5f);
    ToggleRow("etr", "Линии к объектам", "Линия от низа экрана к каждой рамке", &c.espTracers);
    SectionLabel("ПОДПИСИ И ДАЛЬНОСТЬ");
    ToggleRow("edi", "Расстояние в подписи", nullptr, &c.espDist, true);
    SliderRow("els", "Размер подписей", &c.espLabelScale, 0.6f, 2.f, "x%.1f");
    SliderRow("emd", "Дальность для предметов", &c.espMaxDist, 10.f, 300.f, "%.0f м");
}

void TabPlayer() {
    SectionLabel("ВЫЖИВАНИЕ");
    ToggleRow("god", "God Mode", "Гренни, Дед, пауки, ловушки и падения не могут вас убить", &g_cfg.god);
    ToggleRow("inv", "Невидимость", "Гренни и Дед перестают вас замечать", &g_cfg.invisible);
    ToggleRow("frz", "Заморозка врагов", "Все враги замирают на месте", &g_cfg.freezeEnemies);
    SectionLabel("ДВИЖЕНИЕ");
    if (ToggleRow("fly", "Полёт", "WASD — движение, Пробел — вверх, Ctrl — вниз, Shift — быстрее", &g_cfg.fly) && g_cfg.fly) g_cfg.noclip = false;
    if (ToggleRow("nc", "Noclip", "Полёт сквозь стены и пол", &g_cfg.noclip) && g_cfg.noclip) g_cfg.fly = false;
    if (BeginExpand("##flysub", g_cfg.fly || g_cfg.noclip, 62 * S)) {
        SliderRow("fs", "Скорость полёта", &g_cfg.flySpeed, 2.f, 25.f, "%.1f");
        EndExpand();
    }
    ToggleRow("spd", "Ускорение", "Быстрее ходьба и бег", &g_cfg.speed);
    if (BeginExpand("##spdsub", g_cfg.speed, 62 * S)) {
        SliderRow("sm", "Множитель скорости", &g_cfg.speedMul, 1.2f, 5.f, "x%.1f");
        EndExpand();
    }
    SectionLabel("ОБЗОР");
    ToggleRow("fov", "Свой угол обзора", "Поле зрения камеры (FOV) во время игры", &g_cfg.fovOn);
    if (BeginExpand("##fovsub", g_cfg.fovOn, 62 * S)) {
        SliderRow("fv", "Угол обзора", &g_cfg.fov, 40.f, 130.f, "%.0f°");
        EndExpand();
    }
    SectionLabel("МИР");
    ToggleRow("tsc", "Скорость времени", "Замедляет или ускоряет всю игру. Пауза игры продолжает работать", &g_cfg.timeOn);
    if (BeginExpand("##tscsub", g_cfg.timeOn, 62 * S)) {
        SliderRow("ts", "Скорость", &g_cfg.timeScale, 0.1f, 3.f, "x%.2f");
        EndExpand();
    }
}

void TabResources() {
    float coins;
    { std::lock_guard<std::mutex> lk(g_snap.mtx); coins = g_snap.coins; }
    g_coinShown = Approach(g_coinShown, coins, 8.f);
    if (std::fabs(g_coinShown - coins) < 0.5f) g_coinShown = coins;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    SectionLabel("МОНЕТЫ");
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 150 * S;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 12 * S);
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w * 0.6f, p.y + h), A(Accent(0.10f)), A(Accent(0.0f)), A(Accent(0.0f)), A(Accent(0.06f)));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 12 * S);
    ImVec2 cc(p.x + 34 * S, p.y + 36 * S);
    dl->AddCircleFilled(cc, 17 * S, A(RGBA(255, 196, 60)), 32);
    dl->AddCircle(cc, 11 * S, A(RGBA(200, 140, 20)), 32, 2.f * S);
    dl->AddCircleFilled(ImVec2(cc.x - 5 * S, cc.y - 6 * S), 3 * S, A(RGBA(255, 240, 190, 0.9f)), 12);
    char bal[48];
    snprintf(bal, sizeof bal, "%.0f", g_coinShown);
    Text(dl, F_Small, ImVec2(p.x + 62 * S, p.y + 16 * S), C_DIM, "Баланс Granny Coin  ·  поле задаёт баланс, кнопки ниже добавляют");
    Text(dl, F_Big, ImVec2(p.x + 62 * S, p.y + 30 * S), C_TEXT, bal);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 16 * S, p.y + 82 * S));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.09f, 0.10f, 0.13f, g_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.11f, 0.12f, 0.15f, g_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.13f, 0.17f, g_alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::IsWindowFocused() ? AccentV(0.35f) : ImVec4(1, 1, 1, 0.08f));
    ImGui::PushFont(F_Semi);
    float bw = 120 * S;
    ImGui::SetNextItemWidth(w - 32 * S - bw - 10 * S);
    bool enter = ImGui::InputTextWithHint("##coins", "Новый баланс монет", g_coinBuf, sizeof g_coinBuf,
                                          ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopFont();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 10 * S);
    float ih = ImGui::GetItemRectSize().y;
    bool give = Button("give", "Выдать", ImVec2(bw, ih), true);
    if (give || enter) {
        if (g_coinBuf[0] == 0) Toast("Введите количество монет", 2);
        else ActionGiveCoins(atof(g_coinBuf), true);  // sets the balance (0 is allowed)
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 10 * S));
    const struct { const char* l; double v; } chips[] = {{"+100", 100}, {"+1 000", 1000}, {"+10 000", 10000}, {"+100 000", 100000}};
    float cw = (w - 3 * 8 * S) / 4;
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine(0, 8 * S);
        char id[16]; snprintf(id, sizeof id, "chip%d", i);
        if (Button(id, chips[i].l, ImVec2(cw, 34 * S), false, F_Body)) ActionGiveCoins(chips[i].v, false);
    }
    ImGui::Dummy(ImVec2(0, 10 * S));
    SectionLabel("ПРЕДМЕТЫ");
    ToggleRow("unl", "Открыть все предметы",
              "Все 50 скинов предметов и все покупки магазина (режимы, бабушки, ловушки). Выключите, чтобы вернуть как было",
              &g_cfg.unlockAll);
}

void TabTheme() {
    SectionLabel("АКЦЕНТНЫЙ ЦВЕТ");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    struct Preset { const char* n; float c[3]; };
    static const Preset presets[] = {
        {"Мята", {0.27f, 0.87f, 0.55f}}, {"Океан", {0.23f, 0.62f, 1.f}}, {"Фиалка", {0.61f, 0.42f, 1.f}},
        {"Роза", {1.f, 0.36f, 0.54f}}, {"Закат", {1.f, 0.54f, 0.24f}}, {"Золото", {1.f, 0.78f, 0.24f}},
        {"Лёд", {0.36f, 0.88f, 0.90f}}, {"Графит", {0.86f, 0.87f, 0.90f}}};
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 92 * S;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 12 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 12 * S);
    float r = 15 * S, step = (w - 28 * S) / 9.f;
    for (int i = 0; i < 9; ++i) {
        ImVec2 c(p.x + 14 * S + step * (i + 0.5f), p.y + 34 * S);
        ImGui::SetCursorScreenPos(ImVec2(c.x - r, c.y - r));
        char id[16]; snprintf(id, sizeof id, "sw%d", i);
        bool clicked = ImGui::InvisibleButton(id, ImVec2(r * 2, r * 2));
        bool hov = ImGui::IsItemHovered();
        float ht = Anim(ImHashStr(id), hov ? 1.f : 0.f, 16.f);
        if (i < 8) {
            const Preset& pr = presets[i];
            bool sel = std::fabs(pr.c[0] - g_cfg.accent[0]) < 0.01f && std::fabs(pr.c[1] - g_cfg.accent[1]) < 0.01f && std::fabs(pr.c[2] - g_cfg.accent[2]) < 0.01f;
            float st = Anim(ImHashStr(id) + 1, sel ? 1.f : 0.f, 14.f);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(pr.c[0], pr.c[1], pr.c[2], 1));
            dl->AddCircleFilled(c, r * (0.86f + 0.08f * ht), A(col), 32);
            if (st > 0.01f) dl->AddCircle(c, r + 4 * S, A(WithAlpha(col, st)), 32, 2 * S);
            if (st > 0.5f) DrawIcon(dl, IC_CHECK, c, 8 * S, A(pr.c[0] * 0.3f + pr.c[1] * 0.59f + pr.c[2] * 0.11f > 0.6f ? RGBA(10, 12, 16) : RGBA(255, 255, 255)));
            ImVec2 ns = TextSize(F_Small, pr.n);
            Text(dl, F_Small, ImVec2(c.x - ns.x * 0.5f, c.y + r + 8 * S), sel ? C_TEXT : C_DIM, pr.n);
            if (clicked) { memcpy(g_cfg.accent, pr.c, sizeof pr.c); ConfigMarkDirty(); }
        } else {
            // custom colour: rainbow ring
            for (int k = 0; k < 24; ++k) {
                float a0 = k * IM_PI * 2 / 24, a1 = (k + 1) * IM_PI * 2 / 24;
                ImVec4 hc; ImGui::ColorConvertHSVtoRGB(k / 24.f, 0.7f, 1.f, hc.x, hc.y, hc.z); hc.w = 1;
                dl->PathArcTo(c, r * (0.72f + 0.08f * ht), a0, a1, 3);
                dl->PathStroke(A(ImGui::ColorConvertFloat4ToU32(hc)), 0, 5 * S);
            }
            dl->AddCircleFilled(c, r * 0.42f, A(Accent()), 24);
            ImVec2 ns = TextSize(F_Small, "Свой");
            Text(dl, F_Small, ImVec2(c.x - ns.x * 0.5f, c.y + r + 8 * S), C_DIM, "Свой");
            if (clicked) ImGui::OpenPopup("##accentpick");
        }
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * S, 14 * S));
    if (ImGui::BeginPopup("##accentpick")) {
        ImGui::PushFont(F_Semi);
        ImGui::TextUnformatted("Свой акцентный цвет");
        ImGui::PopFont();
        ImGui::SetNextItemWidth(230 * S);
        if (ImGui::ColorPicker3("##pick", g_cfg.accent, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoAlpha))
            ConfigMarkDirty();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 10 * S));
    SectionLabel("ОКНО И АНИМАЦИИ");
    float op = g_cfg.opacity * 100.f;
    if (SliderRow("op", "Непрозрачность меню", &op, 60.f, 100.f, "%.0f%%")) g_cfg.opacity = op / 100.f;
    ToggleRow("anim", "Анимации", "Плавные переходы, появления и переключатели", &g_cfg.animations);
    if (BeginExpand("##animsub", g_cfg.animations, 62 * S)) {
        SliderRow("as", "Скорость анимаций", &g_cfg.animSpeed, 0.4f, 2.5f, "x%.1f");
        EndExpand();
    }
    Config& c = g_cfg;
    SectionLabel("ОКНО МЕНЮ");
    {
        float pct = c.menuScale * 100.f;
        if (SliderRow("msc", "Масштаб интерфейса ZenClient", &pct, 70.f, 150.f, "%.0f%%")) c.menuScale = std::round(pct / 5.f) * 0.05f;
    }
    SliderRow("mw", "Ширина окна", &c.menuW, 640.f, 1300.f, "%.0f px");
    SliderRow("mh", "Высота окна", &c.menuH, 440.f, 950.f, "%.0f px");
    SliderRow("ms", "Ширина боковой панели", &c.menuSide, 150.f, 300.f, "%.0f px");
    SliderRow("mr", "Скругление углов", &c.menuRadius, 0.f, 32.f, "%.0f px");
    ColorRow("mbg", "Цвет фона меню", c.menuBg);
    PctRow("mdim", "Затемнение игры под меню", &c.menuDim, 0.f, 0.9f);
    SegRow("man", "Анимация открытия", &c.menuAnim, {"Подъём", "Сдвиг", "Плавно", "Без анимации"});
    ToggleRow("mst", "Статус игры в боковой панели", nullptr, &c.menuStatus, true);
    ImGui::Dummy(ImVec2(0, 4 * S));
    if (Button("mcenter", "Вернуть окно в центр", ImVec2(210 * S, 36 * S), false, F_Body)) g_menuPos = ImVec2(-1, -1);
    ImGui::SameLine(0, 8 * S);
    if (Button("resettheme", "Сбросить тему", ImVec2(170 * S, 36 * S), false, F_Body)) {
        Config d;
        memcpy(g_cfg.accent, d.accent, sizeof d.accent);
        g_cfg.opacity = d.opacity; g_cfg.animations = d.animations; g_cfg.animSpeed = d.animSpeed;
        g_cfg.menuScale = d.menuScale; g_cfg.menuW = d.menuW; g_cfg.menuH = d.menuH; g_cfg.menuSide = d.menuSide;
        g_cfg.menuRadius = d.menuRadius; g_cfg.menuDim = d.menuDim; g_cfg.menuAnim = d.menuAnim; g_cfg.menuStatus = d.menuStatus;
        memcpy(g_cfg.menuBg, d.menuBg, sizeof d.menuBg);
        g_menuPos = ImVec2(-1, -1);
        ConfigMarkDirty();
        Toast("Тема сброшена", 0);
    }
}

void InfoRow(const char* k, const char* v) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    Text(dl, F_Body, ImVec2(p.x + 14 * S, p.y + 6 * S), C_DIM, k);
    ImVec2 vs = TextSize(F_Semi, v);
    Text(dl, F_Semi, ImVec2(p.x + w - 14 * S - vs.x, p.y + 6 * S), C_TEXT, v);
    ImGui::Dummy(ImVec2(w, 30 * S));
}

void TabClient() {
    Config& c = g_cfg;
    SectionLabel("УПРАВЛЕНИЕ");
    {
        static const char* names[kMenuKeyCount];
        for (int i = 0; i < kMenuKeyCount; ++i) names[i] = kMenuKeys[i].name;
        ChoiceRow("key", "Клавиша меню", &c.menuKey, names, kMenuKeyCount);
    }
    SectionLabel("ВОДЯНОЙ ЗНАК");
    ToggleRow("wm", "Водяной знак", "Капсула с названием и информацией поверх игры", &c.watermark);
    if (c.watermark) {
        SegRow("wpos", "Положение", &c.wmPos, {"Верх", "Верх-лево", "Верх-право", "Низ-лево", "Низ", "Низ-право"});
        SegRow("wsty", "Стиль", &c.wmStyle, {"Капсула", "Плашка", "Только текст"});
        ToggleRow("wnm", "Название", nullptr, &c.wmName, true);
        if (c.wmName) TextRow("wtx", "Текст названия", &c.wmText, "ZenClient");
        ToggleRow("wfp", "FPS", nullptr, &c.wmFps, true);
        ToggleRow("wti", "Время", nullptr, &c.wmTime, true);
        ToggleRow("wco", "Монеты", nullptr, &c.wmCoins, true);
        ToggleRow("wst", "Где вы (меню / игра)", nullptr, &c.wmStatus, true);
        ToggleRow("wdt", "Зелёный индикатор", nullptr, &c.wmDot, true);
        SliderRow("wsc", "Размер", &c.wmScale, 0.6f, 2.f, "x%.2f");
        PctRow("wal", "Непрозрачность фона", &c.wmAlpha, 0.2f, 1.f);
    }
    SectionLabel("УВЕДОМЛЕНИЯ");
    ToggleRow("ton", "Уведомления", "Всплывающие сообщения о действиях клиента", &c.toastOn);
    if (c.toastOn) {
        SegRow("tpo", "Угол экрана", &c.toastPos, {"Справа внизу", "Слева внизу", "Справа вверху", "Слева вверху"});
        SliderRow("tti", "Длительность", &c.toastTime, 1.f, 10.f, "%.1f с");
        if (Button("ttest", "Показать пример", ImVec2(190 * S, 36 * S), false, F_Body)) Toast("Пример уведомления ZenClient", 1);
        ImGui::Dummy(ImVec2(0, 6 * S));
    }
    SectionLabel("О КЛИЕНТЕ");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = 4 * 34 * S + 12 * S;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 12 * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 12 * S);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 8 * S));
    InfoRow("Открыть / закрыть меню", MenuKeyName());
    InfoRow("Версия", "ZenClient 1.0");
    InfoRow("Платформа", "Windows · Steam Proton");
    InfoRow("Настройки", "Сохраняются автоматически");
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 10 * S));
    if (Button("closemenu", "Закрыть меню", ImVec2(170 * S, 36 * S), false, F_Body)) UiSetMenuOpen(false);
}

// ------------------------------------------------------------------ configs tab
std::wstring WideU(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n : 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
std::string Utf8W(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

// Windows file dialogs run on their own thread so the game keeps rendering.
struct FileDialogState { std::mutex m; bool busy = false, ready = false; int kind = 0; std::string path, exportName; } g_dlg;
void StartFileDialog(int kind, const std::string& defName) {  // kind 1 = import, 2 = export
    {
        std::lock_guard<std::mutex> lk(g_dlg.m);
        if (g_dlg.busy) return;
        g_dlg.busy = true; g_dlg.ready = false; g_dlg.kind = kind; g_dlg.exportName = defName;
    }
    std::string dir = ProfilesFolder();
    CreateDirectoryW(WideU(dir).c_str(), nullptr);
    std::thread([kind, defName, dir] {
        wchar_t file[1024] = {};
        if (kind == 2) wcsncpy(file, WideU(ProfileSafeName(defName.empty() ? "ZenClient" : defName) + ".json").c_str(), 1023);
        std::wstring wdir = WideU(dir);
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof ofn;
        ofn.lpstrFilter = L"Конфиг ZenClient (*.json)\0*.json\0Все файлы\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = 1024;
        ofn.lpstrInitialDir = wdir.c_str();
        ofn.lpstrDefExt = L"json";
        ofn.lpstrTitle = kind == 1 ? L"Импорт конфига ZenClient" : L"Экспорт конфига ZenClient";
        ofn.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | (kind == 1 ? (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST) : OFN_OVERWRITEPROMPT);
        BOOL ok = kind == 1 ? GetOpenFileNameW(&ofn) : GetSaveFileNameW(&ofn);
        std::lock_guard<std::mutex> lk(g_dlg.m);
        g_dlg.path = ok ? Utf8W(file) : std::string();
        g_dlg.ready = true;
        g_dlg.busy = false;
    }).detach();
}

std::vector<ProfileInfo> g_profiles;
float g_profilesTimer = 0.f;
void RefreshProfiles() { g_profiles = ProfileList(); g_profilesTimer = 1.5f; }

void ProcessFileDialog() {
    int kind; std::string path, name;
    {
        std::lock_guard<std::mutex> lk(g_dlg.m);
        if (!g_dlg.ready) return;
        g_dlg.ready = false;
        kind = g_dlg.kind; path = g_dlg.path; name = g_dlg.exportName;
    }
    if (path.empty()) return;  // cancelled
    std::string err;
    if (kind == 1) {
        std::string text, outName;
        size_t sl = path.find_last_of("\\/");
        std::string stem = path.substr(sl == std::string::npos ? 0 : sl + 1);
        if (stem.size() > 5 && stem.compare(stem.size() - 5, 5, ".json") == 0) stem.resize(stem.size() - 5);
        if (!ReadFileU8(path, text)) Toast("Не удалось прочитать файл", 2);
        else if (!ProfileImport(text, stem, &outName, &err)) Toast(err, 2);
        else Toast("Конфиг импортирован и применён: " + outName, 1);
    } else {
        if (WriteFileUtf8(path, ConfigToJson(name.empty() ? "ZenClient" : name))) Toast("Конфиг экспортирован", 1);
        else Toast("Не удалось сохранить файл", 2);
    }
    RefreshProfiles();
}

// Two clicks within 3 s confirm a destructive action.
std::string g_confirmId;
float g_confirmT = -10.f;
bool Confirming(const std::string& id) { return g_confirmId == id && g_time - g_confirmT < 3.f; }
bool ConfirmClick(const std::string& id) {
    if (Confirming(id)) { g_confirmId.clear(); return true; }
    g_confirmId = id; g_confirmT = g_time;
    return false;
}

bool InputField(const char* id, const char* hint, char* buf, size_t size, float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.08f, 0.10f, g_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.09f, 0.10f, 0.13f, g_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.11f, 0.14f, g_alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * S, std::max(2.f, (height - F_Body->FontSize) * 0.5f)));
    ImGui::SetNextItemWidth(width);
    bool enter = ImGui::InputTextWithHint(id, hint, buf, size, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return enter;
}

void TabConfigs() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    g_profilesTimer -= g_dt;
    if (g_profilesTimer <= 0.f) RefreshProfiles();
    std::string err;
    const std::string& active = g_cfg.profile;
    bool activeExists = !active.empty() && ProfileExists(active);

    // ---- active config
    SectionLabel("АКТИВНЫЙ КОНФИГ");
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = 64 * S;
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(RGBA(24, 27, 34)), 12 * S);
        dl->AddRectFilledMultiColor(p, ImVec2(p.x + w * 0.6f, p.y + h), A(Accent(0.10f)), A(Accent(0.f)), A(Accent(0.f)), A(Accent(0.05f)));
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.05f)), 12 * S);
        DrawIcon(dl, IC_FOLDER, ImVec2(p.x + 30 * S, p.y + h * 0.5f), 11 * S, A(activeExists ? Accent() : C_FAINT));
        dl->PushClipRect(p, ImVec2(p.x + w - 150 * S, p.y + h), true);
        Text(dl, F_Title, ImVec2(p.x + 54 * S, p.y + 11 * S), activeExists ? C_TEXT : C_DIM, activeExists ? active.c_str() : "Не выбран");
        Text(dl, F_Small, ImVec2(p.x + 54 * S, p.y + 38 * S), C_DIM,
             activeExists ? "Изменения не попадут в файл, пока не нажмёте «Сохранить»" : "Настройки хранятся только в config.ini");
        dl->PopClipRect();
        if (activeExists) {
            ImGui::SetCursorScreenPos(ImVec2(p.x + w - 136 * S, p.y + 14 * S));
            if (Button("save", "Сохранить", ImVec2(122 * S, 36 * S), true, F_Body)) {
                if (ProfileSave(active, &err)) Toast("Конфиг «" + active + "» сохранён", 1); else Toast(err, 2);
                RefreshProfiles();
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 8 * S));
        ImGui::Dummy(ImVec2(w, 0));
    }

    // ---- new config
    SectionLabel("НОВЫЙ КОНФИГ ИЗ ТЕКУЩИХ НАСТРОЕК");
    {
        static char newName[64] = "";
        float bw = 150 * S, h = 38 * S;
        bool enter = InputField("##newcfg", "Название, например «Стелс»", newName, sizeof newName, w - bw - 8 * S, h);
        ImGui::SameLine(0, 8 * S);
        std::string nm = ProfileSafeName(newName);
        bool exists = !nm.empty() && ProfileExists(nm);
        const char* lbl = exists ? (Confirming("over") ? "Точно?" : "Перезаписать") : "Сохранить";
        if (Button("create", lbl, ImVec2(bw, h), true, F_Body) || enter) {
            if (nm.empty()) Toast("Введите название конфига", 2);
            else if (!exists || ConfirmClick("over")) {
                if (ProfileSave(nm, &err)) { Toast("Конфиг «" + nm + "» сохранён", 1); newName[0] = 0; }
                else Toast(err, 2);
                RefreshProfiles();
            }
        }
        ImGui::Dummy(ImVec2(0, 6 * S));
    }

    // ---- list
    char sec[64];
    snprintf(sec, sizeof sec, "МОИ КОНФИГИ  ·  %zu", g_profiles.size());
    SectionLabel(sec);
    static std::string renaming;
    static char renameBuf[64] = "";
    static bool focusRename = false;
    if (g_profiles.empty()) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = 54 * S;
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(RGBA(255, 255, 255, 0.08f)), 12 * S);
        const char* t = "Пока пусто — сохраните текущие настройки или импортируйте JSON";
        ImVec2 ts = TextSize(F_Body, t);
        Text(dl, F_Body, ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), C_DIM, t);
        ImGui::Dummy(ImVec2(w, h + 6 * S));
    }
    std::string toLoad, toDelete, renameFrom, renameTo;
    for (auto& pi : g_profiles) {
        ImGui::PushID(pi.name.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = 58 * S;
        bool isActive = pi.name == active;
        float hv = Anim(ImGui::GetID("hv"), ImGui::IsMouseHoveringRect(p, ImVec2(p.x + w, p.y + h)) ? 1.f : 0.f, 16.f);
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), A(LerpCol(RGBA(24, 27, 34), RGBA(30, 34, 43), hv)), 12 * S);
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), A(isActive ? Accent(0.6f) : RGBA(255, 255, 255, 0.05f)), 12 * S);
        if (isActive) dl->AddRectFilled(ImVec2(p.x, p.y + h * 0.26f), ImVec2(p.x + 3 * S, p.y + h * 0.74f), A(Accent()), 2 * S);
        float btnH = 34 * S, by = p.y + (h - btnH) * 0.5f;
        if (renaming == pi.name) {
            ImGui::SetCursorScreenPos(ImVec2(p.x + 14 * S, by));
            float bw = 96 * S;
            if (focusRename) { ImGui::SetKeyboardFocusHere(); focusRename = false; }
            bool enter = InputField("##ren", "Новое название", renameBuf, sizeof renameBuf, w - 28 * S - bw * 2 - 16 * S, btnH);
            ImGui::SameLine(0, 8 * S);
            if (Button("ok", "OK", ImVec2(bw, btnH), true, F_Body) || enter) { renameFrom = pi.name; renameTo = renameBuf; }
            ImGui::SameLine(0, 8 * S);
            if (Button("cancel", "Отмена", ImVec2(bw, btnH), false, F_Body)) renaming.clear();
        } else {
            float bl = 94 * S, br = 118 * S, bd = 86 * S, gap = 6 * S;
            float bx = p.x + w - 12 * S - bl - br - bd - gap * 2;
            dl->PushClipRect(p, ImVec2(bx - 8 * S, p.y + h), true);
            Text(dl, F_Semi, ImVec2(p.x + 16 * S, p.y + 10 * S), C_TEXT, pi.name.c_str());
            std::string sub = (isActive ? std::string("Активный · ") : std::string()) + pi.modified;
            Text(dl, F_Small, ImVec2(p.x + 16 * S, p.y + 32 * S), isActive ? Accent() : C_DIM, sub.c_str());
            dl->PopClipRect();
            ImGui::SetCursorScreenPos(ImVec2(bx, by));
            if (Button("load", "Загрузить", ImVec2(bl, btnH), !isActive, F_Small)) toLoad = pi.name;
            ImGui::SameLine(0, gap);
            if (Button("ren", "Переименовать", ImVec2(br, btnH), false, F_Small)) {
                renaming = pi.name;
                focusRename = true;
                snprintf(renameBuf, sizeof renameBuf, "%s", pi.name.c_str());
            }
            ImGui::SameLine(0, gap);
            std::string cid = "del:" + pi.name;
            if (Button("del", Confirming(cid) ? "Точно?" : "Удалить", ImVec2(bd, btnH), Confirming(cid), F_Small) && ConfirmClick(cid)) toDelete = pi.name;
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 6 * S));
        ImGui::Dummy(ImVec2(w, 0));
        ImGui::PopID();
    }
    if (!toLoad.empty()) {
        if (ProfileLoad(toLoad, &err)) Toast("Конфиг «" + toLoad + "» загружен", 1); else Toast(err, 2);
        RefreshProfiles();
    }
    if (!toDelete.empty()) {
        if (ProfileDelete(toDelete)) Toast("Конфиг «" + toDelete + "» удалён", 0); else Toast("Не удалось удалить конфиг", 2);
        RefreshProfiles();
    }
    if (!renameFrom.empty()) {
        if (ProfileRename(renameFrom, renameTo, &err)) { Toast("Переименовано: " + ProfileSafeName(renameTo), 1); renaming.clear(); }
        else Toast(err, 2);
        RefreshProfiles();
    }

    // ---- import / export
    SectionLabel("ИМПОРТ И ЭКСПОРТ  ·  JSON");
    {
        float bw = (w - 8 * S) / 2, h = 38 * S;
        bool busy;
        { std::lock_guard<std::mutex> lk(g_dlg.m); busy = g_dlg.busy; }
        std::string exportName = activeExists ? active : std::string("ZenClient");
        if (Button("imp", "Импорт из файла…", ImVec2(bw, h), false, F_Body) && !busy) StartFileDialog(1, "");
        ImGui::SameLine(0, 8 * S);
        if (Button("exp", "Экспорт в файл…", ImVec2(bw, h), false, F_Body) && !busy) StartFileDialog(2, exportName);
        if (Button("copy", "Копировать JSON", ImVec2(bw, h), false, F_Body)) {
            ImGui::SetClipboardText(ConfigToJson(exportName).c_str());
            Toast("JSON со всеми настройками скопирован в буфер обмена", 1);
        }
        ImGui::SameLine(0, 8 * S);
        if (Button("paste", "Вставить из буфера", ImVec2(bw, h), false, F_Body)) {
            const char* clip = ImGui::GetClipboardText();
            std::string outName;
            if (!clip || !*clip) Toast("Буфер обмена пуст", 2);
            else if (ProfileImport(clip, "Из буфера", &outName, &err)) Toast("Конфиг импортирован и применён: " + outName, 1);
            else Toast(err, 2);
            RefreshProfiles();
        }
        if (Button("folder", "Открыть папку конфигов", ImVec2(w, h), false, F_Body)) {
            std::wstring d = WideU(ProfilesFolder());
            CreateDirectoryW(d.c_str(), nullptr);
            ShellExecuteW(nullptr, L"open", d.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (busy) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            Text(dl, F_Small, ImVec2(p.x + 4 * S, p.y + 2 * S), Accent(), "Открыто окно выбора файла…");
            ImGui::Dummy(ImVec2(w, F_Small->FontSize + 8 * S));
        }
        ImGui::Dummy(ImVec2(0, 6 * S));
    }

    // ---- reset
    SectionLabel("СБРОС");
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        const char* t = "Все параметры ZenClient вернутся к стандартным (включая правила редактора меню). Сохранённые конфиги не удаляются.";
        float th = WrappedHeight(F_Small, t, w - 8 * S);
        TextWrapped(dl, F_Small, ImVec2(p.x + 4 * S, p.y), C_DIM, t, w - 8 * S);
        ImGui::Dummy(ImVec2(w, th + 8 * S));
        if (Button("reset", Confirming("reset") ? "Нажмите ещё раз для сброса" : "Сбросить все настройки", ImVec2(w, 38 * S), Confirming("reset"), F_Body) &&
            ConfirmClick("reset")) {
            ConfigResetAll();
            g_menuPos = ImVec2(-1, -1);
            Toast("Все настройки сброшены к стандартным", 0);
        }
    }
}

// ------------------------------------------------------------------ main menu window
void DrawMenu(ImVec2 disp) {
    float target = g_open ? 1.f : 0.f;
    g_openAnim = g_cfg.menuAnim == 3 ? target : Approach(g_openAnim, target, 13.f);
    if (g_openAnim <= 0.002f) return;
    float e = EaseOut(g_openAnim);
    g_alpha = e;
    ImVec2 size(std::min(g_cfg.menuW * S, disp.x - 16 * S), std::min(g_cfg.menuH * S, disp.y - 16 * S));
    ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_cfg.menuBg[0], g_cfg.menuBg[1], g_cfg.menuBg[2], 1));
    ImU32 sideCol = LerpCol(bgCol, IM_COL32(255, 255, 255, 255), 0.025f);
    if (g_menuPos.x < 0) g_menuPos = ImVec2((disp.x - size.x) * 0.5f, (disp.y - size.y) * 0.5f);
    g_menuPos.x = std::clamp(g_menuPos.x, -size.x * 0.5f, disp.x - size.x * 0.5f);
    g_menuPos.y = std::clamp(g_menuPos.y, 0.f, disp.y - 60 * S);

    // background dim
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    if (!(g_cfg.customUi && g_customAnim > 0.5f) && g_cfg.menuDim > 0.001f) bg->AddRectFilled(ImVec2(0, 0), disp, IM_COL32(0, 0, 0, (int)(255 * g_cfg.menuDim * e)));

    ImVec2 pos = g_menuPos;
    if (g_cfg.menuAnim == 0) pos.y += (1 - e) * 18 * S;
    else if (g_cfg.menuAnim == 1) pos.x -= (1 - e) * 60 * S;
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!g_open) fl |= ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##zenmenu", nullptr, fl);
    if (g_open && ImGui::IsWindowAppearing()) ImGui::SetWindowFocus();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = pos, b(pos.x + size.x, pos.y + size.y);
    float R = g_cfg.menuRadius * S;
    Shadow(dl, a, b, R, 26 * S, 0.8f);
    dl->AddRectFilled(a, b, A(WithAlpha(bgCol, g_cfg.opacity)), R);
    float side = g_cfg.menuSide * S;
    dl->AddRectFilled(a, ImVec2(a.x + side, b.y), A(WithAlpha(sideCol, g_cfg.opacity)), R, ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(a.x + side, a.y + 14 * S), ImVec2(a.x + side, b.y - 14 * S), A(RGBA(255, 255, 255, 0.05f)));
    dl->AddRect(a, b, A(RGBA(255, 255, 255, 0.07f)), R);
    // accent glow at the top-left
    dl->AddRectFilledMultiColor(ImVec2(a.x + R, a.y), ImVec2(a.x + side, a.y + 2 * S), A(Accent(0.0f)), A(Accent(0.8f)), A(Accent(0.8f)), A(Accent(0.0f)));

    // ---- logo (doubles as drag handle)
    ImVec2 lp(a.x + 22 * S, a.y + 24 * S);
    ImGui::SetCursorScreenPos(ImVec2(a.x, a.y));
    ImGui::InvisibleButton("##drag", ImVec2(side, 74 * S));
    if (ImGui::IsItemActivated()) { g_dragging = true; g_dragOff = ImVec2(ImGui::GetIO().MousePos.x - g_menuPos.x, ImGui::GetIO().MousePos.y - g_menuPos.y); }
    if (g_dragging) {
        if (ImGui::IsMouseDown(0)) g_menuPos = ImVec2(ImGui::GetIO().MousePos.x - g_dragOff.x, ImGui::GetIO().MousePos.y - g_dragOff.y);
        else g_dragging = false;
    }
    dl->AddRectFilled(lp, ImVec2(lp.x + 30 * S, lp.y + 30 * S), A(RGBA(6, 7, 9)), 9 * S);
    dl->AddRect(lp, ImVec2(lp.x + 30 * S, lp.y + 30 * S), A(Accent(0.35f)), 9 * S);
    float pulse = 0.5f + 0.5f * std::sin(g_time * 3.0f);
    dl->AddCircleFilled(ImVec2(lp.x + 15 * S, lp.y + 15 * S), (5 + 3 * pulse) * S, A(RGBA(52, 211, 120, 0.2f)), 20);
    dl->AddCircleFilled(ImVec2(lp.x + 15 * S, lp.y + 15 * S), 5 * S, A(RGBA(52, 211, 120)), 20);
    Text(dl, F_Title, ImVec2(lp.x + 42 * S, lp.y - 1 * S), C_TEXT, "ZenClient");
    Text(dl, F_Small, ImVec2(lp.x + 42 * S, lp.y + 21 * S), C_DIM, "Granny Legacy · v1.0");

    // ---- tabs
    float ty0 = a.y + 96 * S, th = 44 * S;
    if (g_cfg.tab < 0 || g_cfg.tab >= kTabCount) g_cfg.tab = 0;
    g_tabAnim = g_lastTab < 0 ? (float)g_cfg.tab : Approach(g_tabAnim, (float)g_cfg.tab, 16.f);
    if (g_lastTab != g_cfg.tab) { if (g_lastTab >= 0) g_contentAnim = 0.f; g_lastTab = g_cfg.tab; }
    float iy = ty0 + g_tabAnim * (th + 4 * S);
    dl->AddRectFilled(ImVec2(a.x + 12 * S, iy), ImVec2(a.x + side - 12 * S, iy + th), A(Accent(0.13f)), 10 * S);
    dl->AddRectFilled(ImVec2(a.x + 12 * S, iy + 12 * S), ImVec2(a.x + 15 * S, iy + th - 12 * S), A(Accent()), 2 * S);
    for (int i = 0; i < kTabCount; ++i) {
        float y = ty0 + i * (th + 4 * S);
        ImGui::SetCursorScreenPos(ImVec2(a.x + 12 * S, y));
        char id[16]; snprintf(id, sizeof id, "##tab%d", i);
        if (ImGui::InvisibleButton(id, ImVec2(side - 24 * S, th))) { g_cfg.tab = i; ConfigMarkDirty(); }
        float hv = Anim(ImHashStr(id), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
        float sel = Anim(ImHashStr(id) + 7, g_cfg.tab == i ? 1.f : 0.f, 14.f);
        if (hv > 0.01f && g_cfg.tab != i) dl->AddRectFilled(ImVec2(a.x + 12 * S, y), ImVec2(a.x + side - 12 * S, y + th), A(RGBA(255, 255, 255, 0.035f * hv)), 10 * S);
        ImU32 ic = LerpCol(LerpCol(C_DIM, C_TEXT, hv * 0.6f), Accent(), sel);
        DrawIcon(dl, kTabs[i].icon, ImVec2(a.x + 38 * S + hv * 2 * S, y + th * 0.5f), 8 * S, A(ic));
        Text(dl, F_Semi, ImVec2(a.x + 58 * S + hv * 2 * S, y + (th - F_Semi->FontSize) * 0.5f), LerpCol(LerpCol(C_DIM, C_TEXT, hv), C_TEXT, sel), kTabs[i].name);
    }
    // ---- status at the bottom of the sidebar
    bool inGame, inMenu;
    { std::lock_guard<std::mutex> lk(g_snap.mtx); inGame = g_snap.inGame; inMenu = g_snap.inMainMenu; }
    const char* st = inGame ? "В игре" : inMenu ? "Главное меню" : "Загрузка";
    ImVec2 sp(a.x + 22 * S, b.y - 40 * S);
    if (g_cfg.menuStatus) {
        dl->AddCircleFilled(ImVec2(sp.x + 4 * S, sp.y + 8 * S), 4 * S, A(inGame || inMenu ? Accent() : RGBA(255, 186, 72)), 12);
        Text(dl, F_Small, ImVec2(sp.x + 14 * S, sp.y), C_DIM, st);
    }

    // ---- content
    g_contentAnim = Approach(g_contentAnim, 1.f, 12.f);
    float ce = EaseOut(g_contentAnim);
    float cx = a.x + side + 26 * S, cy = a.y + 24 * S;
    float sa = g_alpha;
    g_alpha = e * ce;
    Text(dl, F_Big, ImVec2(cx + (1 - ce) * 14 * S, cy), C_TEXT, kTabs[g_cfg.tab].name);
    Text(dl, F_Body, ImVec2(cx + (1 - ce) * 14 * S, cy + 38 * S), C_DIM, kTabs[g_cfg.tab].sub);
    ImGui::SetCursorScreenPos(ImVec2(cx + (1 - ce) * 22 * S, cy + 78 * S));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e * ce);
    ImGui::BeginChild("##content", ImVec2(size.x - side - 52 * S, size.y - 78 * S - 40 * S), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * S, 4 * S));
    switch (g_cfg.tab) {
    case 0: TabVisuals(); break;
    case 1: TabPlayer(); break;
    case 2: TabResources(); break;
    case 3: TabInterface(); break;
    case 4: TabTheme(); break;
    case 5: TabClient(); break;
    case 6: TabConfigs(); break;
    }
    ImGui::Dummy(ImVec2(0, 8 * S));
    ImGui::PopStyleVar();
    // Fade the bottom edge while more content is available below (hint that the list scrolls).
    float scrollLeft = ImGui::GetScrollMaxY() - ImGui::GetScrollY();
    ImVec2 cwp = ImGui::GetWindowPos(), cws = ImGui::GetWindowSize();
    ImGui::EndChild();
    float fadeT = Anim("##contentfade", scrollLeft > 4.f ? 1.f : 0.f, 10.f);
    if (fadeT > 0.01f) {
        float fh = 46 * S;
        ImU32 bgc = WithAlpha(bgCol, g_cfg.opacity * fadeT * e);
        dl->AddRectFilledMultiColor(ImVec2(cwp.x, cwp.y + cws.y - fh), ImVec2(cwp.x + cws.x, cwp.y + cws.y), WithAlpha(bgc, 0.f), WithAlpha(bgc, 0.f), bgc, bgc);
        ImVec2 ac(cwp.x + cws.x * 0.5f, cwp.y + cws.y - 14 * S + std::sin(g_time * 4.f) * 2 * S);
        dl->PathLineTo(ImVec2(ac.x - 6 * S, ac.y - 3 * S));
        dl->PathLineTo(ImVec2(ac.x, ac.y + 3 * S));
        dl->PathLineTo(ImVec2(ac.x + 6 * S, ac.y - 3 * S));
        dl->PathStroke(WithAlpha(Accent(), fadeT * e * 0.8f), 0, 2 * S);
    }
    ImGui::PopStyleVar();
    g_alpha = sa;
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    g_alpha = 1.f;
}

// ------------------------------------------------------------------ startup disclaimer
// Shown on every ZenClient launch (there is intentionally no option to disable it).
float g_discAnim = 0.f;
bool g_discClosing = false;

void DrawDisclaimer(ImVec2 disp) {
    if (!g_disclaimerOpen && g_discAnim <= 0.002f) return;
    g_discAnim = Approach(g_discAnim, g_discClosing ? 0.f : 1.f, g_discClosing ? 12.f : 7.f);
    if (g_discClosing && g_discAnim <= 0.002f) { g_discAnim = 0.f; return; }
    float e = EaseOut(g_discAnim);
    g_alpha = e;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (g_discClosing) fl |= ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##disclaimer", nullptr, fl);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), disp, A(RGBA(3, 4, 6, 0.72f)));

    float sc = 0.92f + 0.08f * e;
    ImVec2 size(660 * S * sc, 430 * S * sc);
    ImVec2 c(disp.x * 0.5f, disp.y * 0.5f + (1 - e) * 24 * S);
    ImVec2 a(c.x - size.x * 0.5f, c.y - size.y * 0.5f), b(c.x + size.x * 0.5f, c.y + size.y * 0.5f);
    float R = 24 * S;
    // accent glow + card
    for (int i = 6; i >= 1; --i)
        dl->AddRectFilled(ImVec2(a.x - i * 7 * S, a.y - i * 7 * S), ImVec2(b.x + i * 7 * S, b.y + i * 7 * S), A(Accent(0.035f)), R + i * 7 * S);
    Shadow(dl, a, b, R, 22 * S, 0.7f);
    // smooth vertical gradient: bright accent at the top, deep accent at the bottom (no seams)
    ImU32 topC = AccentMix(-0.04f), botC = AccentMix(-0.45f);
    GradientRect(dl, a, b, A(topC), A(botC), R);
    dl->AddRect(a, b, A(RGBA(255, 255, 255, 0.18f)), R, 0, 1.5f * S);
    // decorative rings
    dl->PushClipRect(a, b, true);
    for (int i = 0; i < 3; ++i)
        dl->AddCircle(ImVec2(b.x - 40 * S, a.y + 30 * S), (90 + i * 46) * S * sc, A(RGBA(255, 255, 255, 0.07f - i * 0.018f)), 64, 2 * S);
    dl->PopClipRect();

    bool lightText = !(AccentLuma() > 0.62f);
    ImU32 tc = lightText ? RGBA(255, 255, 255) : RGBA(14, 16, 20);
    ImU32 tc2 = lightText ? RGBA(255, 255, 255, 0.86f) : RGBA(14, 16, 20, 0.82f);
    float px = a.x + 40 * S * sc, py = a.y + 36 * S * sc;
    // warning badge
    ImVec2 ic(px + 26 * S, py + 26 * S);
    float pulse = 0.5f + 0.5f * std::sin(g_time * 2.6f);
    dl->AddCircleFilled(ic, (26 + 5 * pulse) * S, A(RGBA(255, 255, 255, 0.12f)), 40);
    dl->AddCircleFilled(ic, 26 * S, A(RGBA(255, 255, 255, 0.22f)), 40);
    dl->AddLine(ImVec2(ic.x, ic.y - 12 * S), ImVec2(ic.x, ic.y + 4 * S), A(tc), 4 * S);
    dl->AddCircleFilled(ImVec2(ic.x, ic.y + 12 * S), 2.6f * S, A(tc), 12);
    Text(dl, F_Small, ImVec2(px + 70 * S, py + 6 * S), tc2, "ZENCLIENT  ·  ВАЖНО");
    Text(dl, F_Big, ImVec2(px + 70 * S, py + 22 * S), tc, "Дисклеймер");

    const char* body = "Данная сборка игры неофициальная. Некоторые опции могут работать некорректно.";
    float bw = size.x - 80 * S * sc;
    dl->AddText(F_Title, F_Title->FontSize, ImVec2(px, py + 92 * S), A(tc), body, nullptr, bw);
    float by = py + 92 * S + WrappedHeight(F_Title, body, bw) + 22 * S;

    auto creditRow = [&](float y, const char* k, const char* v) {
        ImVec2 ra(px, y), rb(px + bw, y + 44 * S);
        dl->AddRectFilled(ra, rb, A(RGBA(0, 0, 0, lightText ? 0.20f : 0.10f)), 12 * S);
        dl->AddRect(ra, rb, A(RGBA(255, 255, 255, 0.10f)), 12 * S);
        Text(dl, F_Body, ImVec2(ra.x + 16 * S, ra.y + (44 * S - F_Body->FontSize) * 0.5f), tc2, k);
        ImVec2 vs = TextSize(F_Semi, v);
        Text(dl, F_Semi, ImVec2(rb.x - 16 * S - vs.x, ra.y + (44 * S - F_Semi->FontSize) * 0.5f), tc, v);
    };
    creditRow(by, "Оригинальный разработчик", "DVLoper");
    creditRow(by + 52 * S, "Сборку создал", "Zenvyd");

    // OK button
    ImVec2 okS(bw, 54 * S);
    ImVec2 okP(px, b.y - 36 * S * sc - okS.y);
    const char* note = "Сообщение показывается при каждом запуске ZenClient";
    ImVec2 ns = TextSize(F_Small, note);
    Text(dl, F_Small, ImVec2(px + (bw - ns.x) * 0.5f, okP.y - ns.y - 14 * S), tc2, note);
    ImGui::SetCursorScreenPos(okP);
    bool ok = ImGui::InvisibleButton("##discok", okS);
    float hv = Anim("##discokhv", ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
    float pr = Anim("##discokpr", ImGui::IsItemActive() ? 1.f : 0.f, 30.f);
    ImVec2 oa(okP.x + pr * 2 * S, okP.y + pr * 2 * S - hv * 2 * S), ob(okP.x + okS.x - pr * 2 * S, okP.y + okS.y - pr * 2 * S - hv * 2 * S);
    if (hv > 0.01f) dl->AddRectFilled(ImVec2(oa.x - 4 * S, oa.y - 2 * S), ImVec2(ob.x + 4 * S, ob.y + 6 * S), A(RGBA(255, 255, 255, 0.10f * hv)), 16 * S);
    dl->AddRectFilled(oa, ob, A(lightText ? RGBA(255, 255, 255) : RGBA(14, 16, 20)), 14 * S);
    ImVec2 ts = TextSize(F_Title, "OK");
    Text(dl, F_Title, ImVec2((oa.x + ob.x - ts.x) * 0.5f, (oa.y + ob.y - ts.y) * 0.5f), lightText ? AccentMix(-0.35f) : Accent(), "OK");
    bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
    if (!g_discClosing && g_discAnim > 0.6f && (ok || enter)) {
        g_discClosing = true;
        g_disclaimerOpen = false;
        LOG("disclaimer accepted");
    }
    ImGui::End();
    ImGui::PopStyleColor();
    g_alpha = 1.f;
}

} // namespace

// ------------------------------------------------------------------ public
void Toast(const std::string& text, int kind) {
    if (!g_cfg.toastOn) return;
    std::lock_guard<std::mutex> lk(g_toastMtx);
    g_toasts.push_back({text, kind, 0.f, std::clamp(g_cfg.toastTime, 1.f, 10.f), 0.f});
    if (g_toasts.size() > 5) g_toasts.erase(g_toasts.begin());
}

void UiInit(float w, float h) {
    BuildFonts(h);
    ApplyColors();
    LOG("ui init scale=%.2f", S);
}

void UiPreFrame() {
    // ZenClient scale changed: rebuild the fonts once the slider is released (outside of a frame)
    if (std::fabs(g_cfg.menuScale - g_appliedMenuScale) > 0.001f && !ImGui::GetIO().MouseDown[0]) {
        BuildFonts(g_fontH);
        ImGui_ImplDX11_InvalidateDeviceObjects();
        g_menuPos = ImVec2(-1, -1);
        LOG("ui scale -> %.2f (S=%.2f)", g_cfg.menuScale, S);
    }
}

int UiMenuKeyVk() { return kMenuKeys[std::clamp(g_cfg.menuKey, 0, kMenuKeyCount - 1)].vk; }
const char* UiMenuKeyName() { return MenuKeyName(); }

void UiOnResize(float w, float h) {
    float ns = std::clamp(h / 1080.f, 0.8f, 2.2f) * g_cfg.menuScale;
    if (std::fabs(ns - S) > 0.01f) {
        BuildFonts(h);
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
    g_menuPos = ImVec2(-1, -1);
    (void)w;
}

bool UiWantsInput() {
    bool inMenu;
    { std::lock_guard<std::mutex> lk(g_snap.mtx); inMenu = g_snap.inMainMenu && !g_snap.uiSuspended; }
    return g_open || g_disclaimerOpen || (g_cfg.customUi && inMenu);
}

bool UiCustomActive() {
    bool inMenu;
    { std::lock_guard<std::mutex> lk(g_snap.mtx); inMenu = g_snap.inMainMenu && !g_snap.uiSuspended; }
    return g_cfg.customUi && inMenu && !g_open && !g_disclaimerOpen;
}

void UiSetMenuOpen(bool open) {
    g_open = open;
    g_menuOpen = open;
    if (!open) g_dragging = false;
}

void UiFrame(float dt) {
    g_dt = dt;
    g_time += dt;
    ProcessFileDialog();
    ApplyColors();
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    DrawEsp(ImGui::GetBackgroundDrawList(), disp);
    DrawCustomUi(disp);
    DrawMenu(disp);
    DrawDisclaimer(disp);
    DrawWatermark(ImGui::GetForegroundDrawList(), disp);
    DrawToasts(ImGui::GetForegroundDrawList(), disp);
}

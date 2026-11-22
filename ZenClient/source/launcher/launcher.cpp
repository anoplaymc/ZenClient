// ZenClient Launcher: a small GUI (Dear ImGui + D3D11) that starts Granny Legacy either with ZenClient
// (ZenClient\ZenClient.dll is loaded into the game) or as the vanilla game.
// Works on Windows and under Steam Proton. Command line: --zen / --vanilla start immediately without the GUI.
#include <windows.h>
#include <d3d11.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
void ZenImAssert(const char*, const char*, int) {}

namespace {

// ------------------------------------------------------------------ paths & checks
std::wstring g_dir;
std::wstring Dir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    std::wstring s(buf, n);
    return s.substr(0, s.find_last_of(L"\\/") + 1);
}
bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
std::string Utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
bool IsWine() { return GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") != nullptr; }

float g_accent[3] = {0.27f, 0.87f, 0.55f};
void LoadAccent() {
    FILE* f = _wfopen((g_dir + L"ZenClient\\config.ini").c_str(), L"r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        float v;
        if (sscanf(line, "accent_r=%f", &v) == 1) g_accent[0] = v;
        if (sscanf(line, "accent_g=%f", &v) == 1) g_accent[1] = v;
        if (sscanf(line, "accent_b=%f", &v) == 1) g_accent[2] = v;
    }
    fclose(f);
    for (float& c : g_accent) c = std::clamp(c, 0.f, 1.f);
}

// ------------------------------------------------------------------ game launching (worker thread)
enum class St { Idle, Starting, Injecting, Running, Error };
std::atomic<St> g_state{St::Idle};
std::atomic<bool> g_withZen{true};
std::mutex g_msgMtx;
std::string g_msg;
HANDLE g_proc = nullptr;
void SetMsg(const std::string& m) { std::lock_guard<std::mutex> lk(g_msgMtx); g_msg = m; }
std::string GetMsg() { std::lock_guard<std::mutex> lk(g_msgMtx); return g_msg; }

bool ModuleLoaded(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{sizeof me};
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        if (_wcsicmp(me.szModule, name) == 0) { found = true; break; }
    CloseHandle(snap);
    return found;
}

std::wstring g_extraArgs;

bool Launch(bool zen) {
    std::wstring exe = g_dir + L"Granny Legacy.exe";
    std::wstring dll = g_dir + L"ZenClient\\ZenClient.dll";
    if (!Exists(exe)) { SetMsg("Не найден «Granny Legacy.exe» рядом с лаунчером"); g_state = St::Error; return false; }
    if (zen && !Exists(dll)) { SetMsg("Не найден ZenClient\\ZenClient.dll"); g_state = St::Error; return false; }
    g_state = St::Starting;
    SetMsg("Запускаем Granny Legacy…");
    std::wstring cmd = L"\"" + exe + L"\"" + g_extraArgs;
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, g_dir.c_str(), &si, &pi)) {
        SetMsg("Не удалось запустить игру (код " + std::to_string(GetLastError()) + ")");
        g_state = St::Error;
        return false;
    }
    CloseHandle(pi.hThread);
    g_proc = pi.hProcess;
    if (zen) {
        g_state = St::Injecting;
        SetMsg("Подключаем ZenClient…");
        for (int i = 0; i < 400 && !ModuleLoaded(pi.dwProcessId, L"UnityPlayer.dll"); ++i)
            if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) { SetMsg("Игра закрылась во время запуска"); g_state = St::Error; return false; }
        size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
        void* mem = VirtualAllocEx(pi.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        bool ok = false;
        if (mem && WriteProcessMemory(pi.hProcess, mem, dll.c_str(), bytes, nullptr)) {
            auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
            HANDLE th = CreateRemoteThread(pi.hProcess, nullptr, 0, loadLib, mem, 0, nullptr);
            if (th) {
                WaitForSingleObject(th, 15000);
                DWORD code = 0;
                GetExitCodeThread(th, &code);
                ok = code != 0;
                CloseHandle(th);
            }
            VirtualFreeEx(pi.hProcess, mem, 0, MEM_RELEASE);
        }
        if (!ok) { SetMsg("Игра запущена, но ZenClient подключить не удалось"); g_state = St::Error; return false; }
    }
    SetMsg(zen ? "Игра запущена с ZenClient · Правый Shift — меню" : "Оригинальная игра запущена");
    g_state = St::Running;
    return true;
}

// ------------------------------------------------------------------ D3D11 window
ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain* g_sc = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
HWND g_hwnd = nullptr;
const int W = 900, H = 540, TITLE_H = 54;

void CreateRTV() {
    ID3D11Texture2D* bb = nullptr;
    g_sc->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) { g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv); bb->Release(); }
}
bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_sc, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_sc, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) return false;
    CreateRTV();
    return true;
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return TRUE;
    switch (m) {
    case WM_NCHITTEST: {
        POINT p{(short)LOWORD(l), (short)HIWORD(l)};
        ScreenToClient(h, &p);
        if (p.y >= 0 && p.y < TITLE_H && p.x < W - 110) return HTCAPTION;  // drag by the title bar
        return HTCLIENT;
    }
    case WM_SIZE:
        if (g_dev && w != SIZE_MINIMIZED) {
            if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
            g_sc->ResizeBuffers(0, LOWORD(l), HIWORD(l), DXGI_FORMAT_UNKNOWN, 0);
            CreateRTV();
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ------------------------------------------------------------------ UI helpers
ImFont *F_Body, *F_Semi, *F_Small, *F_Title, *F_Huge;
float g_dt = 1 / 60.f, g_time = 0;
inline ImU32 RGBA(int r, int g, int b, float a = 1.f) { return IM_COL32(r, g, b, (int)(std::clamp(a, 0.f, 1.f) * 255.f)); }
inline ImU32 Accent(float a = 1.f) { return ImGui::ColorConvertFloat4ToU32(ImVec4(g_accent[0], g_accent[1], g_accent[2], a)); }
inline ImU32 AccentMix(float k) {
    ImVec4 c(g_accent[0], g_accent[1], g_accent[2], 1);
    c.x += (1 - c.x) * k; c.y += (1 - c.y) * k; c.z += (1 - c.z) * k;
    return ImGui::ColorConvertFloat4ToU32(c);
}
ImU32 OnAccent() { return 0.299f * g_accent[0] + 0.587f * g_accent[1] + 0.114f * g_accent[2] > 0.6f ? RGBA(12, 14, 18) : RGBA(255, 255, 255); }
ImU32 Lerp(ImU32 a, ImU32 b, float t) {
    ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}
ImU32 Fade(ImU32 c, float a) { ImVec4 v = ImGui::ColorConvertU32ToFloat4(c); v.w *= a; return ImGui::ColorConvertFloat4ToU32(v); }
float Approach(float cur, float target, float speed) {
    float v = cur + (target - cur) * (1.f - std::exp(-speed * g_dt));
    return std::fabs(v - target) < 0.0005f ? target : v;
}
inline float EaseOut(float t) { return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
ImGuiStorage g_animStore;
float Anim(ImGuiID id, float target, float speed = 14.f) {
    float* v = g_animStore.GetFloatRef(id, target);
    *v = Approach(*v, target, speed);
    return *v;
}
ImVec2 TS(ImFont* f, const char* t) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, t); }
float g_alpha = 1.f;
void Tx(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* t) { dl->AddText(f, f->FontSize, p, Fade(c, g_alpha), t); }

void Check(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathLineTo(ImVec2(c.x - r * 0.55f, c.y));
    dl->PathLineTo(ImVec2(c.x - r * 0.12f, c.y + r * 0.45f));
    dl->PathLineTo(ImVec2(c.x + r * 0.6f, c.y - r * 0.45f));
    dl->PathStroke(col, 0, 2.f);
}
void Cross(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float th = 2.f) {
    dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, th);
    dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, th);
}
void Play(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddTriangleFilled(ImVec2(c.x - r * 0.6f, c.y - r), ImVec2(c.x - r * 0.6f, c.y + r), ImVec2(c.x + r * 0.9f, c.y), col);
}

// Big launch button. Returns true when clicked.
bool BigButton(const char* id, const char* title, const char* sub, ImVec2 size, bool primary, bool enabled) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    bool pressed = ImGui::InvisibleButton("##b", size) && enabled;
    bool hov = ImGui::IsItemHovered() && enabled;
    float ht = Anim(ImGui::GetID("h"), hov ? 1.f : 0.f, 14.f);
    float pr = Anim(ImGui::GetID("p"), ImGui::IsItemActive() && enabled ? 1.f : 0.f, 30.f);
    float en = Anim(ImGui::GetID("e"), enabled ? 1.f : 0.f, 10.f);
    ImGui::PopID();
    float sh = pr * 2.f;
    ImVec2 a(p.x + sh, p.y + sh), b(p.x + size.x - sh, p.y + size.y - sh);
    float R = 14.f;
    float al = g_alpha * (0.45f + 0.55f * en);
    if (primary) {
        for (int i = 3; i >= 1; --i)
            dl->AddRectFilled(ImVec2(a.x - i * 4.f * ht, a.y - i * 4.f * ht + 4), ImVec2(b.x + i * 4.f * ht, b.y + i * 4.f * ht + 4), Fade(Accent(0.08f), al), R + i * 4.f);
        dl->AddRectFilled(a, b, Fade(AccentMix(0.08f * ht), al), R);
        dl->AddRectFilled(ImVec2(a.x + 2, a.y + 1), ImVec2(b.x - 2, (a.y + b.y) * 0.5f), Fade(RGBA(255, 255, 255, 0.08f + 0.05f * ht), al), R, ImDrawFlags_RoundCornersTop);
        ImU32 tc = OnAccent();
        ImVec2 ic(a.x + 34, (a.y + b.y) * 0.5f);
        dl->AddCircleFilled(ic, 17, Fade(RGBA(0, 0, 0, 0.14f), al), 32);
        Play(dl, ImVec2(ic.x + 1 + ht * 2, ic.y), 7, Fade(tc, al));
        dl->AddText(F_Title, F_Title->FontSize, ImVec2(a.x + 64, a.y + 15), Fade(tc, al), title);
        dl->AddText(F_Small, F_Small->FontSize, ImVec2(a.x + 64, a.y + 43), Fade(tc, al * 0.75f), sub);
    } else {
        dl->AddRectFilled(a, b, Fade(Lerp(RGBA(24, 27, 34), RGBA(32, 36, 45), ht), al), R);
        dl->AddRect(a, b, Fade(Lerp(RGBA(255, 255, 255, 0.07f), Accent(0.55f), ht), al), R, 0, 1.2f);
        ImVec2 ic(a.x + 34, (a.y + b.y) * 0.5f);
        dl->AddCircle(ic, 16, Fade(Lerp(RGBA(140, 146, 158), Accent(), ht), al), 32, 1.6f);
        Play(dl, ImVec2(ic.x + 1, ic.y), 6, Fade(Lerp(RGBA(140, 146, 158), Accent(), ht), al));
        dl->AddText(F_Semi, F_Semi->FontSize, ImVec2(a.x + 64, a.y + 15), Fade(RGBA(236, 238, 242), al), title);
        dl->AddText(F_Small, F_Small->FontSize, ImVec2(a.x + 64, a.y + 37), Fade(RGBA(140, 146, 158), al), sub);
    }
    return pressed;
}

bool TitleButton(const char* id, ImVec2 p, ImVec2 s, bool close) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(p);
    bool c = ImGui::InvisibleButton(id, s);
    float ht = Anim(ImGui::GetID(id), ImGui::IsItemHovered() ? 1.f : 0.f, 16.f);
    if (ht > 0.01f) dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), close ? RGBA(232, 72, 85, ht) : RGBA(255, 255, 255, 0.08f * ht), 8.f);
    ImVec2 c0(p.x + s.x * 0.5f, p.y + s.y * 0.5f);
    ImU32 col = Lerp(RGBA(160, 166, 178), RGBA(255, 255, 255), ht);
    if (close) Cross(dl, c0, 5.f, col, 1.6f);
    else dl->AddLine(ImVec2(c0.x - 6, c0.y), ImVec2(c0.x + 6, c0.y), col, 1.6f);
    return c;
}

// ------------------------------------------------------------------ main UI
float g_intro = 0.f, g_statusAnim = 0.f, g_hideTimer = -1.f;
bool g_exeOk = false, g_dllOk = false, g_fontsOk = false, g_wine = false;

void DrawUi() {
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    g_intro = Approach(g_intro, 1.f, 5.f);
    float e = EaseOut(g_intro);
    // background + slowly drifting accent glows
    bg->AddRectFilled(ImVec2(0, 0), disp, RGBA(9, 10, 13));
    for (int i = 0; i < 3; ++i) {
        float t = g_time * (0.12f + i * 0.05f) + i * 2.1f;
        ImVec2 c(disp.x * (0.70f + 0.18f * std::sin(t)), disp.y * (0.45f + 0.30f * std::cos(t * 1.3f)));
        float r = 260.f - i * 50.f;
        for (int k = 28; k >= 1; --k) bg->AddCircleFilled(c, r * k / 28.f, Accent(0.0042f * e), 96);
    }
    bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(disp.x * 0.62f, disp.y), RGBA(9, 10, 13, 0.95f), RGBA(9, 10, 13, 0.0f), RGBA(9, 10, 13, 0.0f), RGBA(9, 10, 13, 0.95f));
    bg->AddRect(ImVec2(0, 0), disp, RGBA(255, 255, 255, 0.07f));
    bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(disp.x * 0.5f, 2), Accent(0.0f), Accent(0.9f), Accent(0.9f), Accent(0.0f));

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    g_alpha = e;

    // ---- title bar
    ImVec2 lp(22, 15);
    dl->AddRectFilled(lp, ImVec2(lp.x + 124, lp.y + 26), Fade(RGBA(4, 5, 7), e), 13);
    dl->AddRect(lp, ImVec2(lp.x + 124, lp.y + 26), Fade(RGBA(255, 255, 255, 0.09f), e), 13);
    float pulse = 0.5f + 0.5f * std::sin(g_time * 3.f);
    dl->AddCircleFilled(ImVec2(lp.x + 15, lp.y + 13), 4.f + 3.f * pulse, Fade(RGBA(52, 211, 120, 0.2f), e), 20);
    dl->AddCircleFilled(ImVec2(lp.x + 15, lp.y + 13), 4.f, Fade(RGBA(52, 211, 120), e), 20);
    Tx(dl, F_Semi, ImVec2(lp.x + 27, lp.y + 4), RGBA(236, 238, 242), "ZenClient");
    Tx(dl, F_Small, ImVec2(lp.x + 140, lp.y + 6), RGBA(110, 116, 128), "Launcher  ·  v1.0");
    if (TitleButton("##min", ImVec2(disp.x - 96, 12), ImVec2(38, 30), false)) ShowWindow(g_hwnd, SW_MINIMIZE);
    if (TitleButton("##close", ImVec2(disp.x - 52, 12), ImVec2(38, 30), true)) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);

    // ---- left: title + launch buttons
    float x0 = 48 + (1 - e) * -24, y0 = 104;
    Tx(dl, F_Small, ImVec2(x0, y0), Accent(), "G R A N N Y   L E G A C Y");
    Tx(dl, F_Huge, ImVec2(x0 - 2, y0 + 20), RGBA(240, 242, 246), "Готовы");
    Tx(dl, F_Huge, ImVec2(x0 - 2, y0 + 76), RGBA(240, 242, 246), "к побегу?");
    Tx(dl, F_Body, ImVec2(x0, y0 + 146), RGBA(140, 146, 158), "Запустите игру с мод-меню ZenClient");
    Tx(dl, F_Body, ImVec2(x0, y0 + 168), RGBA(140, 146, 158), "или оригинальную версию без изменений.");

    St st = g_state.load();
    bool busy = st == St::Starting || st == St::Injecting || st == St::Running;
    ImGui::SetCursorScreenPos(ImVec2(x0, y0 + 214));
    if (BigButton("zen", "Запустить ZenClient", "Мод-меню · Правый Shift в игре", ImVec2(390, 72), true, !busy && g_exeOk && g_dllOk)) {
        g_withZen = true; g_state = St::Starting;
        std::thread([] { Launch(true); }).detach();
    }
    ImGui::SetCursorScreenPos(ImVec2(x0, y0 + 302));
    if (BigButton("van", "Ванильная игра", "Оригинальная Granny Legacy без мода", ImVec2(390, 62), false, !busy && g_exeOk)) {
        g_withZen = false; g_state = St::Starting;
        std::thread([] { Launch(false); }).detach();
    }

    // ---- status line under the buttons
    bool showStatus = st != St::Idle;
    g_statusAnim = Approach(g_statusAnim, showStatus ? 1.f : 0.f, 10.f);
    if (g_statusAnim > 0.01f) {
        float sa = g_alpha; g_alpha = e * g_statusAnim;
        ImVec2 sp(x0, y0 + 384 + (1 - g_statusAnim) * 8);
        std::string msg = GetMsg();
        ImU32 col = st == St::Error ? RGBA(255, 104, 116) : st == St::Running ? Accent() : RGBA(200, 205, 214);
        if (st == St::Starting || st == St::Injecting) {
            for (int i = 0; i < 10; ++i) {
                float a = g_time * 6.f + i * IM_PI * 2 / 10;
                dl->AddCircleFilled(ImVec2(sp.x + 9 + std::cos(a) * 7, sp.y + 9 + std::sin(a) * 7), 1.7f, Fade(Accent(i / 10.f), g_alpha), 8);
            }
        } else if (st == St::Running) {
            dl->AddCircleFilled(ImVec2(sp.x + 9, sp.y + 9), 9, Fade(Accent(0.18f), g_alpha), 20);
            Check(dl, ImVec2(sp.x + 9, sp.y + 9), 6, Fade(Accent(), g_alpha));
        } else {
            dl->AddCircleFilled(ImVec2(sp.x + 9, sp.y + 9), 9, Fade(RGBA(255, 104, 116, 0.18f), g_alpha), 20);
            Cross(dl, ImVec2(sp.x + 9, sp.y + 9), 3.5f, Fade(RGBA(255, 104, 116), g_alpha), 1.8f);
        }
        Tx(dl, F_Body, ImVec2(sp.x + 28, sp.y), col, msg.c_str());
        g_alpha = sa;
    }

    // ---- right: info card
    float cx = disp.x - 360 + (1 - e) * 24, cy = 104, cw = 312;
    float chH = 346;
    dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + chH), Fade(RGBA(16, 18, 23, 0.86f), e), 16);
    dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cw, cy + chH), Fade(RGBA(255, 255, 255, 0.07f), e), 16);
    Tx(dl, F_Small, ImVec2(cx + 20, cy + 18), RGBA(100, 106, 118), "ЧТО ВНУТРИ");
    const char* feats[] = {"Хитбоксы врагов и предметов", "God Mode, полёт, noclip", "Ускорение и заморозка врагов",
                           "Монеты и открытие всех предметов", "Новый интерфейс главного меню", "Темы и акцентные цвета"};
    for (int i = 0; i < 6; ++i) {
        float t = std::clamp(g_intro * 1.6f - i * 0.12f, 0.f, 1.f);
        float yy = cy + 46 + i * 30 + (1 - EaseOut(t)) * 6;
        float sa = g_alpha; g_alpha = e * t;
        dl->AddCircleFilled(ImVec2(cx + 29, yy + 9), 8, Fade(Accent(0.16f), g_alpha), 20);
        Check(dl, ImVec2(cx + 29, yy + 9), 5, Fade(Accent(), g_alpha));
        Tx(dl, F_Body, ImVec2(cx + 46, yy), RGBA(214, 218, 226), feats[i]);
        g_alpha = sa;
    }
    float sy = cy + 236;
    dl->AddLine(ImVec2(cx + 20, sy), ImVec2(cx + cw - 20, sy), Fade(RGBA(255, 255, 255, 0.06f), e));
    Tx(dl, F_Small, ImVec2(cx + 20, sy + 12), RGBA(100, 106, 118), "ПРОВЕРКА");
    auto row = [&](float y, const char* label, bool ok) {
        Tx(dl, F_Small, ImVec2(cx + 20, y), RGBA(170, 176, 188), label);
        ImVec2 c(cx + cw - 30, y + 8);
        if (ok) Check(dl, c, 5, Fade(Accent(), e)); else Cross(dl, c, 4, Fade(RGBA(255, 104, 116), e), 1.6f);
    };
    row(sy + 34, "Granny Legacy.exe", g_exeOk);
    row(sy + 54, "ZenClient\\ZenClient.dll", g_dllOk);
    Tx(dl, F_Small, ImVec2(cx + 20, sy + 74), RGBA(100, 106, 118), g_wine ? "Среда: Steam Proton / Wine" : "Среда: Windows");

    Tx(dl, F_Small, ImVec2(x0, disp.y - 34), RGBA(90, 96, 108), "Настройки клиента: ZenClient\\config.ini   ·   Правый Shift — меню в игре");
    ImGui::End();
    ImGui::PopStyleColor();
}

void BuildFonts() {
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0400, 0x052F, 0x2000, 0x206F, 0x2190, 0x21FF, 0};
    ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 2;
    std::string fd = Utf8(g_dir) + "ZenClient\\fonts\\";
    auto load = [&](const char* name, float px) -> ImFont* {
        std::string p = fd + name;
        if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
        return io.Fonts->AddFontFromFileTTF(p.c_str(), px, &cfg, ranges);
    };
    F_Body = load("Inter-Medium.ttf", 15.5f);
    g_fontsOk = F_Body != nullptr;
    if (!F_Body) F_Body = io.Fonts->AddFontDefault();
    F_Small = load("Inter-Medium.ttf", 13.f); if (!F_Small) F_Small = F_Body;
    F_Semi = load("Inter-SemiBold.ttf", 16.f); if (!F_Semi) F_Semi = F_Body;
    F_Title = load("Inter-SemiBold.ttf", 20.f); if (!F_Title) F_Title = F_Semi;
    F_Huge = load("Inter-Bold.ttf", 50.f); if (!F_Huge) F_Huge = F_Title;
    io.FontDefault = F_Body;
}

// Dev-only automation (only when ZenClient\dev\ exists): dev\launcher.txt = "shot <sec>" / "zen <sec>" / "vanilla <sec>".
float g_devShotAt = -1.f, g_devLaunchAt = -1.f; int g_devLaunch = 0; bool g_devLoaded = false;
void SaveShot();
void DevTick() {
    if (!g_devLoaded) {
        g_devLoaded = true;
        FILE* f = _wfopen((g_dir + L"ZenClient\\dev\\launcher.txt").c_str(), L"r");
        if (f) {
            char line[64]; float t;
            while (fgets(line, sizeof line, f)) {
                if (sscanf(line, "shot %f", &t) == 1) g_devShotAt = t;
                if (sscanf(line, "zen %f", &t) == 1) { g_devLaunchAt = t; g_devLaunch = 1; }
                if (sscanf(line, "vanilla %f", &t) == 1) { g_devLaunchAt = t; g_devLaunch = 2; }
            }
            fclose(f);
        }
    }
    if (g_devLaunchAt > 0 && g_time >= g_devLaunchAt) {
        g_devLaunchAt = -1; bool z = g_devLaunch == 1; g_withZen = z; g_state = St::Starting;
        std::thread([z] { Launch(z); }).detach();
    }
}
void SaveShot() {
    ID3D11Texture2D* bb = nullptr;
    g_sc->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (!bb) return;
    D3D11_TEXTURE2D_DESC td; bb->GetDesc(&td);
    D3D11_TEXTURE2D_DESC sd = td; sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
    ID3D11Texture2D* st = nullptr;
    if (SUCCEEDED(g_dev->CreateTexture2D(&sd, nullptr, &st))) {
        g_ctx->CopyResource(st, bb);
        D3D11_MAPPED_SUBRESOURCE ms;
        if (SUCCEEDED(g_ctx->Map(st, 0, D3D11_MAP_READ, 0, &ms))) {
            FILE* f = _wfopen((g_dir + L"ZenClient\\dev\\launcher.bmp").c_str(), L"wb");
            if (f) {
                int w = td.Width, h = td.Height, row = w * 3, pad = (4 - row % 4) % 4;
                BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
                fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + (row + pad) * h;
                ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = h; ih.biPlanes = 1; ih.biBitCount = 24;
                fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
                std::string buf(row + pad, '\0');
                for (int y = h - 1; y >= 0; --y) {
                    const unsigned char* src = (const unsigned char*)ms.pData + (size_t)y * ms.RowPitch;
                    for (int x = 0; x < w; ++x) { buf[x * 3] = src[x * 4 + 2]; buf[x * 3 + 1] = src[x * 4 + 1]; buf[x * 3 + 2] = src[x * 4]; }
                    fwrite(buf.data(), 1, buf.size(), f);
                }
                fclose(f);
            }
            g_ctx->Unmap(st, 0);
        }
        st->Release();
    }
    bb->Release();
}

int RunHeadless(bool zen) {
    if (!Launch(zen)) { MessageBoxW(nullptr, std::wstring(L"ZenClient: ошибка запуска").c_str(), L"ZenClient", MB_ICONERROR); return 1; }
    WaitForSingleObject(g_proc, INFINITE);
    DWORD code = 0; GetExitCodeProcess(g_proc, &code);
    return (int)code;
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    g_dir = Dir();
    // command line: --zen / --vanilla skip the GUI; other arguments are passed to the game
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int mode = 0;
    for (int i = 1; argv && i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--zen") == 0) mode = 1;
        else if (_wcsicmp(argv[i], L"--vanilla") == 0) mode = 2;
        else { g_extraArgs += L" \""; g_extraArgs += argv[i]; g_extraArgs += L"\""; }
    }
    if (argv) LocalFree(argv);
    if (mode) return RunHeadless(mode == 1);

    g_exeOk = Exists(g_dir + L"Granny Legacy.exe");
    g_dllOk = Exists(g_dir + L"ZenClient\\ZenClient.dll");
    g_wine = IsWine();
    LoadAccent();

    WNDCLASSEXW wc{sizeof wc};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"ZenClientLauncher";
    RegisterClassExW(&wc);
    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2, sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"ZenClient Launcher", WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU,
                             sx, sy, W, H, nullptr, nullptr, inst, nullptr);
    if (!CreateDevice(g_hwnd)) {
        // no D3D11 available: fall back to the plain behaviour (start with ZenClient)
        return RunHeadless(true);
    }
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    BuildFonts();

    LARGE_INTEGER f, last, now;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&last);
    bool done = false, hidden = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        // While the game runs the launcher stays hidden (Steam keeps tracking it) and exits with the game.
        if (g_state == St::Running) {
            if (g_hideTimer < 0) g_hideTimer = 1.6f;
            if (hidden) {
                if (WaitForSingleObject(g_proc, 100) == WAIT_OBJECT_0) break;
                continue;
            }
        }
        QueryPerformanceCounter(&now);
        g_dt = std::clamp((float)(now.QuadPart - last.QuadPart) / (float)f.QuadPart, 1e-4f, 0.1f);
        last = now;
        g_time += g_dt;
        if (g_hideTimer > 0 && (g_hideTimer -= g_dt) <= 0) { ShowWindow(g_hwnd, SW_HIDE); hidden = true; continue; }
        if (IsIconic(g_hwnd)) { Sleep(30); continue; }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawUi();
        DevTick();
        ImGui::Render();
        const float clear[4] = {0.035f, 0.04f, 0.05f, 1.f};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (g_devShotAt > 0 && g_time >= g_devShotAt) { g_devShotAt = -1; SaveShot(); }
        g_sc->Present(1, 0);
    }
    int code = 0;
    if (g_proc) { DWORD c = 0; GetExitCodeProcess(g_proc, &c); code = (int)c; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) g_rtv->Release();
    if (g_sc) g_sc->Release();
    if (g_ctx) g_ctx->Release();
    if (g_dev) g_dev->Release();
    DestroyWindow(g_hwnd);
    return code;
}

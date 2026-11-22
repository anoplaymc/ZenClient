// ZenClient - D3D11 overlay (works with native D3D11 and with DXVK under Steam Proton).
#include "overlay.hpp"
#include "ui.hpp"
#include "state.hpp"
#include "features.hpp"
#include "log.hpp"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <vector>
#include <mutex>
#include <deque>
#include <atomic>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(WINAPI*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

PresentFn o_Present = nullptr;
Present1Fn o_Present1 = nullptr;
ResizeFn o_Resize = nullptr;

ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
IDXGISwapChain* g_sc = nullptr;
HWND g_hwnd = nullptr;
WNDPROC o_WndProc = nullptr;
bool g_imguiReady = false;
std::recursive_mutex g_imguiMtx;
thread_local bool t_inPresent = false;
UINT g_bbW = 0, g_bbH = 0;
DXGI_FORMAT g_bbFmt = DXGI_FORMAT_UNKNOWN;
bool g_prevRShift = false;
LARGE_INTEGER g_qpf, g_lastQpc;

// Synthetic input for the dev channel (multi-frame click sequences).
struct SynthStep { int frames; float x, y; int btn; bool down; bool move; std::string text; int key; };
std::deque<SynthStep> g_synth;

bool IsInputMsg(UINT m) {
    return (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) || m == WM_INPUT || m == WM_KEYDOWN || m == WM_SYSKEYDOWN ||
           m == WM_CHAR || m == WM_SYSCHAR || m == WM_MOUSEWHEEL || m == WM_MOUSEHWHEEL || m == WM_SETCURSOR;
}
bool IsReleaseMsg(UINT m) {
    return m == WM_KEYUP || m == WM_SYSKEYUP || m == WM_LBUTTONUP || m == WM_RBUTTONUP || m == WM_MBUTTONUP || m == WM_XBUTTONUP;
}

std::atomic<bool> g_rshiftMsg{false};  // Right Shift state seen through window messages

bool IsRightShift(WPARAM w, LPARAM l) {
    if (w == VK_RSHIFT) return true;
    return w == VK_SHIFT && ((l >> 16) & 0xFF) == 0x36;
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if ((m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && IsRightShift(w, l)) g_rshiftMsg = true;
    if ((m == WM_KEYUP || m == WM_SYSKEYUP) && (w == VK_SHIFT || w == VK_RSHIFT)) g_rshiftMsg = false;
    if (g_imguiReady) {
        bool capture = UiWantsInput();
        {
            std::lock_guard<std::recursive_mutex> lk(g_imguiMtx);
            if (m != WM_MOUSEMOVE) ImGui_ImplWin32_WndProcHandler(h, m, w, l);  // mouse position is fed per frame (scaled)
        }
        // Escape closes the ZenClient menu instead of reaching the game (which would pause).
        if (g_menuOpen && m == WM_KEYDOWN && w == VK_ESCAPE) { UiSetMenuOpen(false); return 0; }
        // Escape under the custom main-menu UI: handled by the UI ("back" works on every screen, even those
        // where the game ignores Escape). Escapes the UI sends itself are let through to the game.
        if (m == WM_KEYDOWN && w == VK_ESCAPE && !g_menuOpen && !ImGui::GetIO().WantTextInput && UiCustomActive()) {
            if (GetTickCount64() < g_escPassUntil.load()) g_escPassUntil = 0;
            else {
                if (!(l & (1 << 30))) g_escBackRequest = true;  // ignore auto-repeat
                return 0;
            }
        }
        // While the startup disclaimer is shown the game gets no input at all.
        if (g_disclaimerOpen && IsInputMsg(m) && m != WM_SETCURSOR && !IsReleaseMsg(m)) return 0;
        // Mouse input is captured whenever ZenClient UI is on screen. The keyboard is only captured while the
        // ZenClient menu is open or a text field is being edited, so game keys (e.g. Escape = back in the
        // main menu) keep working under the custom interface.
        bool isKey = m == WM_KEYDOWN || m == WM_SYSKEYDOWN || m == WM_CHAR || m == WM_SYSCHAR || m == WM_INPUT;
        bool keyCapture = g_menuOpen || ImGui::GetIO().WantTextInput;
        if (capture && IsInputMsg(m) && !IsReleaseMsg(m) && m != WM_SETCURSOR && (!isKey || keyCapture)) return 0;
    }
    return CallWindowProcW(o_WndProc, h, m, w, l);
}

void CreateRTV() {
    ID3D11Texture2D* bb = nullptr;
    if (SUCCEEDED(g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&bb))) && bb) {
        D3D11_TEXTURE2D_DESC td;
        bb->GetDesc(&td);
        g_bbW = td.Width; g_bbH = td.Height; g_bbFmt = td.Format;
        g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
        bb->Release();
        LOG("backbuffer %ux%u fmt=%d", g_bbW, g_bbH, (int)g_bbFmt);
    }
}

void SaveScreenshot() {
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&bb))) || !bb) return;
    D3D11_TEXTURE2D_DESC td;
    bb->GetDesc(&td);
    D3D11_TEXTURE2D_DESC sd = td;
    sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
    sd.SampleDesc.Count = 1; sd.SampleDesc.Quality = 0; sd.MipLevels = 1; sd.ArraySize = 1;
    ID3D11Texture2D* st = nullptr;
    if (FAILED(g_dev->CreateTexture2D(&sd, nullptr, &st))) { bb->Release(); return; }
    if (td.SampleDesc.Count > 1) g_ctx->ResolveSubresource(st, 0, bb, 0, td.Format);
    else g_ctx->CopyResource(st, bb);
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(g_ctx->Map(st, 0, D3D11_MAP_READ, 0, &ms))) {
        std::string path = g_baseDir + "dev\\" + (g_shotName.empty() ? "shot" : g_shotName) + ".bmp";
        FILE* f = fopen(path.c_str(), "wb");
        if (f) {
            int w = td.Width, hgt = td.Height;
            int rowBytes = w * 3, pad = (4 - rowBytes % 4) % 4;
            int dataSize = (rowBytes + pad) * hgt;
            BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
            fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + dataSize;
            ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = hgt; ih.biPlanes = 1; ih.biBitCount = 24; ih.biSizeImage = dataSize;
            fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
            std::vector<unsigned char> row(rowBytes + pad, 0);
            bool bgra = td.Format == DXGI_FORMAT_B8G8R8A8_UNORM || td.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            bool r10 = td.Format == DXGI_FORMAT_R10G10B10A2_UNORM;
            for (int y = hgt - 1; y >= 0; --y) {
                const unsigned char* src = static_cast<const unsigned char*>(ms.pData) + (size_t)y * ms.RowPitch;
                for (int x = 0; x < w; ++x) {
                    unsigned char r, g, b;
                    if (r10) {
                        uint32_t px = reinterpret_cast<const uint32_t*>(src)[x];
                        r = (unsigned char)(((px >> 0) & 1023) >> 2); g = (unsigned char)(((px >> 10) & 1023) >> 2); b = (unsigned char)(((px >> 20) & 1023) >> 2);
                    } else if (bgra) { b = src[x * 4]; g = src[x * 4 + 1]; r = src[x * 4 + 2]; }
                    else { r = src[x * 4]; g = src[x * 4 + 1]; b = src[x * 4 + 2]; }
                    row[x * 3] = b; row[x * 3 + 1] = g; row[x * 3 + 2] = r;
                }
                fwrite(row.data(), 1, row.size(), f);
            }
            fclose(f);
            LOG("screenshot saved: %s", path.c_str());
        }
        g_ctx->Unmap(st, 0);
    }
    st->Release();
    bb->Release();
}

void HandleDevRender() {
    std::string c;
    while (DevPopRender(c)) {
        char a[128] = {}, b[128] = {};
        float x = 0, y = 0;
        if (sscanf(c.c_str(), "menu %f", &x) == 1) UiSetMenuOpen(x != 0);
        else if (sscanf(c.c_str(), "tab %f", &x) == 1) { g_cfg.tab = (int)x; }
        else if (sscanf(c.c_str(), "cfg %127s %127s", a, b) == 2) { if (!ConfigSetByName(a, b)) LOG("dev: unknown cfg %s", a); }
        else if (sscanf(c.c_str(), "shot %127s", a) == 1) { g_shotName = a; g_shotRequest++; }
        else if (sscanf(c.c_str(), "click %f %f", &x, &y) == 2) {
            g_synth.push_back({2, x, y, 0, false, true, "", 0});
            g_synth.push_back({2, x, y, 0, true, false, "", 0});
            g_synth.push_back({2, x, y, 0, false, false, "", 0});
        } else if (sscanf(c.c_str(), "move %f %f", &x, &y) == 2) g_synth.push_back({1, x, y, 0, false, true, "", 0});
        else if (c.rfind("type ", 0) == 0) g_synth.push_back({1, 0, 0, -1, false, false, c.substr(5), 0});
        else if (c == "enter") g_synth.push_back({1, 0, 0, -1, false, false, "", ImGuiKey_Enter});
        else if (c.rfind("psave ", 0) == 0) { std::string e; LOG("dev psave %d %s", ProfileSave(c.substr(6), &e), e.c_str()); }
        else if (c.rfind("pload ", 0) == 0) { std::string e; LOG("dev pload %d %s", ProfileLoad(c.substr(6), &e), e.c_str()); }
        else if (sscanf(c.c_str(), "pren %127s %127s", a, b) == 2) { std::string e; LOG("dev pren %d %s", ProfileRename(a, b, &e), e.c_str()); }
        else if (c.rfind("pdel ", 0) == 0) LOG("dev pdel %d", ProfileDelete(c.substr(5)));
        else if (c.rfind("pimport ", 0) == 0) {
            std::string t, e, n;
            bool r = ReadFileU8(g_baseDir + "dev\\" + c.substr(8), t) && ProfileImport(t, "dev", &n, &e);
            LOG("dev pimport %d name=%s err=%s", r, n.c_str(), e.c_str());
        } else if (c.rfind("pexport ", 0) == 0) LOG("dev pexport %d", WriteFileUtf8(g_baseDir + "dev\\" + c.substr(8), ConfigToJson(g_cfg.profile)));
        else if (c == "preset") ConfigResetAll();
        else if (c == "plist") { for (auto& p : ProfileList()) LOG("dev profile '%s' %s", p.name.c_str(), p.modified.c_str()); LOG("dev active='%s'", g_cfg.profile.c_str()); }
        else LOG("dev: unknown command '%s'", c.c_str());
    }
}

bool g_synthMouse = false;
float g_synthX = 0, g_synthY = 0;

void FeedInput() {
    ImGuiIO& io = ImGui::GetIO();
    // Mouse position: real cursor, scaled from window client space to backbuffer space.
    if (!g_synth.empty() || g_synthMouse) {
        if (!g_synth.empty()) {
            SynthStep& s = g_synth.front();
            if (s.move || s.btn == 0) { g_synthX = s.x; g_synthY = s.y; g_synthMouse = true; }
            if (s.btn == 0 && !s.move) io.AddMouseButtonEvent(0, s.down);
            if (s.btn == -1) {
                if (!s.text.empty()) io.AddInputCharactersUTF8(s.text.c_str());
                if (s.key) { io.AddKeyEvent((ImGuiKey)s.key, true); io.AddKeyEvent((ImGuiKey)s.key, false); }
                s.btn = -2;
            }
            if (--s.frames <= 0) g_synth.pop_front();
        }
        io.AddMousePosEvent(g_synthX, g_synthY);
        // real mouse movement cancels the synthetic cursor
        POINT p; GetCursorPos(&p); ScreenToClient(g_hwnd, &p);
        static POINT last{-99999, -99999};
        if (last.x != -99999 && (p.x != last.x || p.y != last.y) && g_synth.empty()) g_synthMouse = false;
        last = p;
        return;
    }
    POINT p;
    if (GetCursorPos(&p) && ScreenToClient(g_hwnd, &p)) {
        RECT rc; GetClientRect(g_hwnd, &rc);
        float cw = (float)(rc.right - rc.left), ch = (float)(rc.bottom - rc.top);
        float sx = cw > 0 ? (float)g_bbW / cw : 1.f, sy = ch > 0 ? (float)g_bbH / ch : 1.f;
        io.AddMousePosEvent(p.x * sx, p.y * sy);
    }
}

void Render(IDXGISwapChain* sc) {
    if (!g_overlayReady) return;
    if (!g_imguiReady) {
        if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_dev))) || !g_dev) return;
        g_dev->GetImmediateContext(&g_ctx);
        DXGI_SWAP_CHAIN_DESC d;
        sc->GetDesc(&d);
        g_hwnd = d.OutputWindow;
        g_sc = sc;
        CreateRTV();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_dev, g_ctx);
        UiInit((float)g_bbW, (float)g_bbH);
        o_WndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc)));
        QueryPerformanceFrequency(&g_qpf);
        QueryPerformanceCounter(&g_lastQpc);
        g_imguiReady = true;
        LOG("overlay ready, hwnd=%p", (void*)g_hwnd);
    }
    if (sc != g_sc) return;  // only draw on the game's main swapchain
    if (!g_rtv) CreateRTV();
    if (!g_rtv) return;

    // Menu key (configurable, Right Shift by default), edge-triggered, only while the game window is focused.
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
    int menuVk = UiMenuKeyVk();
    bool focused = pid == GetCurrentProcessId();
    bool rs = menuVk == VK_RSHIFT ? (g_rshiftMsg || (focused && (GetAsyncKeyState(VK_RSHIFT) & 0x8000)))
                                  : (focused && (GetAsyncKeyState(menuVk) & 0x8000));
    if (rs && !g_prevRShift && g_ready && !g_disclaimerOpen) UiSetMenuOpen(!g_menuOpen);
    g_prevRShift = rs;

    HandleDevRender();

    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_lastQpc.QuadPart) / (float)g_qpf.QuadPart;
    g_lastQpc = now;
    if (dt <= 0 || dt > 0.5f) dt = 1.f / 60.f;

    std::lock_guard<std::recursive_mutex> lk(g_imguiMtx);
    UiPreFrame();
    ImGui_ImplDX11_NewFrame();
    // ImGui_ImplWin32_NewFrame() is intentionally not called: it injects unscaled cursor positions that
    // would race with ours. Display size, timing and mouse position are provided here instead; keyboard and
    // mouse buttons still come from the Win32 message handler.
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_bbW, (float)g_bbH);
    io.DeltaTime = dt;
    FeedInput();
    io.MouseDrawCursor = false;  // the real (OS) cursor is shown instead; see UpdateCursor()
    ImGui::NewFrame();
    UiFrame(dt);
    ImGui::Render();

    ID3D11RenderTargetView* oldRtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* oldDsv = nullptr;
    g_ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRtv, &oldDsv);
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRtv, oldDsv);
    for (auto* r : oldRtv) if (r) r->Release();
    if (oldDsv) oldDsv->Release();

    if (g_shotRequest > 0) { g_shotRequest--; SaveScreenshot(); }
    // dev: startup frames (only when ZenClient\dev\startshots exists)
    static int startShot = -1; static float startT = 0;
    if (startShot == -1) startShot = GetFileAttributesA((g_baseDir + "dev\\startshots").c_str()) != INVALID_FILE_ATTRIBUTES ? 0 : 99;
    if (startShot < 6) {
        startT += dt;
        if (startT >= 0.3f + startShot * 0.8f) { g_shotName = "start" + std::to_string(startShot); SaveScreenshot(); ++startShot; }
    }
    ConfigTick(dt);
}

HRESULT WINAPI hk_Present(IDXGISwapChain* sc, UINT sync, UINT flags) {
    if (!t_inPresent && !(flags & DXGI_PRESENT_TEST)) {
        t_inPresent = true;
        Render(sc);
        HRESULT hr = o_Present(sc, sync, flags);
        t_inPresent = false;
        return hr;
    }
    return o_Present(sc, sync, flags);
}

HRESULT WINAPI hk_Present1(IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* p) {
    if (!t_inPresent && !(flags & DXGI_PRESENT_TEST)) {
        t_inPresent = true;
        Render(sc);
        HRESULT hr = o_Present1(sc, sync, flags, p);
        t_inPresent = false;
        return hr;
    }
    return o_Present1(sc, sync, flags, p);
}

HRESULT WINAPI hk_Resize(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl) {
    if (sc == g_sc && g_rtv) {
        std::lock_guard<std::recursive_mutex> lk(g_imguiMtx);
        g_rtv->Release();
        g_rtv = nullptr;
    }
    HRESULT hr = o_Resize(sc, n, w, h, f, fl);
    if (sc == g_sc && g_imguiReady) { CreateRTV(); UiOnResize((float)g_bbW, (float)g_bbH); }
    return hr;
}

LRESULT CALLBACK DummyProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

} // namespace

bool OverlayInit() {
    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = DummyProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ZenClientDummy";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width = 64; sd.BufferDesc.Height = 64;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr; IDXGISwapChain* sc = nullptr;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    if (FAILED(hr) || !sc) { LOG("dummy swapchain failed 0x%08lx", hr); DestroyWindow(hwnd); return false; }
    void** vt = *reinterpret_cast<void***>(sc);
    void* present = vt[8];
    void* resize = vt[13];
    void* present1 = nullptr;
    IDXGISwapChain1* sc1 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&sc1))) && sc1) {
        present1 = (*reinterpret_cast<void***>(sc1))[22];
        sc1->Release();
    }
    sc->Release(); ctx->Release(); dev->Release();
    DestroyWindow(hwnd);
    bool ok = MH_CreateHook(present, reinterpret_cast<void*>(&hk_Present), reinterpret_cast<void**>(&o_Present)) == MH_OK &&
              MH_EnableHook(present) == MH_OK;
    if (MH_CreateHook(resize, reinterpret_cast<void*>(&hk_Resize), reinterpret_cast<void**>(&o_Resize)) == MH_OK) MH_EnableHook(resize);
    if (present1 && present1 != present &&
        MH_CreateHook(present1, reinterpret_cast<void*>(&hk_Present1), reinterpret_cast<void**>(&o_Present1)) == MH_OK)
        MH_EnableHook(present1);
    LOG("overlay hooks: present=%p present1=%p resize=%p ok=%d", present, present1, resize, ok ? 1 : 0);
    return ok;
}

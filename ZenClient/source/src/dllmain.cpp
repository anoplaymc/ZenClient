// ZenClient - DLL entry point.
#include <windows.h>
#include <string>
#include "MinHook.h"
#include "il2cpp.hpp"
#include "features.hpp"
#include "overlay.hpp"
#include "state.hpp"
#include "log.hpp"

static HMODULE g_self = nullptr;

static std::string ModuleDir(HMODULE m) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(m, buf, MAX_PATH * 2);
    std::wstring w(buf, n);
    size_t p = w.find_last_of(L"\\/");
    if (p != std::wstring::npos) w.resize(p + 1);
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

static DWORD WINAPI InitThread(LPVOID) {
    g_baseDir = ModuleDir(g_self);
    zlog::Init(g_baseDir + "zen.log");
    LOG("ZenClient 1.0 starting, base=%s", g_baseDir.c_str());
    ConfigLoad();
    if (MH_Initialize() != MH_OK) { LOG("MinHook init failed"); return 1; }
    // The overlay does not depend on IL2CPP: hook Present right away so the startup disclaimer is
    // visible from the very first frames.
    if (!OverlayInit()) LOG("overlay failed to initialise");
    else g_overlayReady = true;
    if (!il2::Init()) return 1;
    il2::AttachThread();
    bool ok = FeaturesInit();
    il2::DetachThread();
    if (!ok) { LOG("features failed to initialise"); return 1; }
    g_ready = true;
    LOG("ZenClient ready");
    return 0;
}

// Skips the Unity splash screen (DVLoper / OmGi logos) as soon as the engine has registered its icalls.
static DWORD WINAPI SplashKillerThread(LPVOID) {
    HMODULE ga = nullptr;
    for (int i = 0; i < 2400 && !(ga = GetModuleHandleW(L"GameAssembly.dll")); ++i) Sleep(5);
    if (!ga) return 0;
    auto getCorlib = reinterpret_cast<void* (*)()>(GetProcAddress(ga, "il2cpp_get_corlib"));
    auto resolve = reinterpret_cast<void* (*)(const char*)>(GetProcAddress(ga, "il2cpp_resolve_icall"));
    if (!getCorlib || !resolve) return 0;
    for (int i = 0; i < 4000 && !getCorlib(); ++i) Sleep(2);
    void (*cancel)() = nullptr;
    for (int i = 0; i < 4000 && !cancel; ++i) {
        cancel = reinterpret_cast<void (*)()>(resolve("UnityEngine.Rendering.SplashScreen::CancelSplashScreen"));
        if (!cancel) Sleep(2);
    }
    if (!cancel) return 0;
    // The splash can start a moment after the engine registers its icalls, so keep cancelling until the
    // main menu is up (CancelSplashScreen is a no-op when no splash is running). isFinished is not used:
    // it is not safe to call before the splash object exists.
    for (int i = 0; i < 2000; ++i) {          // up to ~20 s
        cancel();
        bool menu;
        { std::lock_guard<std::mutex> lk(g_snap.mtx); menu = g_snap.inMainMenu || g_snap.inGame; }
        if (menu) break;
        Sleep(10);
    }
    g_splashSkipped = true;
    return 0;
}

BOOL WINAPI DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
        HANDLE t2 = CreateThread(nullptr, 0, SplashKillerThread, nullptr, 0, nullptr);
        if (t2) CloseHandle(t2);
    }
    return TRUE;
}

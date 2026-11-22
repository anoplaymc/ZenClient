// ZenClient - gameplay features. Everything here runs on the Unity main thread,
// driven by hooks on per-frame Update methods.
#include "features.hpp"
#include "unity.hpp"
#include "log.hpp"
#include "MinHook.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <deque>
#include <algorithm>
#include <cmath>

using U::V3;

namespace {

const char* GAME = "Assembly-CSharp.dll";

// ---------------------------------------------------------------- hooking helpers
template <typename T> bool Hook(void* target, void* detour, T** orig) {
    if (!target) return false;
    if (MH_CreateHook(target, detour, reinterpret_cast<void**>(orig)) != MH_OK) { LOG("hook create failed %p", target); return false; }
    if (MH_EnableHook(target) != MH_OK) { LOG("hook enable failed %p", target); return false; }
    return true;
}

void Noop() {}

// A group of game methods that are replaced by an empty function while a feature is on.
struct PatchGroup {
    const char* name;
    std::vector<void*> targets;
    bool active = false;
    void Add(const char* cls, const char* method, int argc) {
        void* k = il2::Class(GAME, "", cls);
        const MethodInfo* m = il2::Method(k, method, argc);
        if (!m || !m->methodPointer) return;
        if (!il2::MethodReturnsVoid(m)) { LOG("patch %s: %s.%s is not void, skipped", name, cls, method); return; }
        if (std::find(targets.begin(), targets.end(), m->methodPointer) != targets.end()) return;
        void* orig = nullptr;
        if (MH_CreateHook(m->methodPointer, reinterpret_cast<void*>(&Noop), &orig) != MH_OK) { LOG("patch %s: create failed for %s.%s", name, cls, method); return; }
        targets.push_back(m->methodPointer);
    }
    void Set(bool on) {
        if (on == active) return;
        for (void* t : targets) on ? MH_QueueEnableHook(t) : MH_QueueDisableHook(t);
        MH_ApplyQueued();
        active = on;
        LOG("patch group %s -> %d (%zu methods)", name, on ? 1 : 0, targets.size());
    }
};

PatchGroup g_godGroup{"god"}, g_invisGroup{"invisible"}, g_freezeGroup{"freeze"}, g_fallGroup{"fall"};

// ---------------------------------------------------------------- game classes / fields
void* k_MainMenu = nullptr;
void* k_MobileFPS = nullptr;
void* k_PlayerStatus = nullptr;
int off_cc = -1, off_moveDir = -1, off_killed = -1;
int off_moveSpeed = -1, off_speedMove = -1, off_speedCrouch = -1, off_crouched = -1;

struct NpcClass { void* klass; const char* label; };
std::vector<NpcClass> g_npcClasses;

// ---------------------------------------------------------------- original functions
using UpdateFn = void (*)(Il2CppObject*, const MethodInfo*);
UpdateFn o_EventSystemUpdate, o_MobileFPSUpdate, o_MainMenuUpdate, o_PickRayUpdate, o_PausedUpdate, o_KeyBindUpdate;
void (*o_SetLockState)(int) = nullptr;
void (*o_SetVisible)(bool) = nullptr;
int (*f_GetLockState)() = nullptr;

// ---------------------------------------------------------------- runtime state
int g_lastFrame = -1;
float g_sceneTimer = 0, g_espTimer = 0, g_uiTimer = 0, g_coinTimer = 0, g_devTimer = 0;
uint32_t g_mainMenuH = 0, g_playerH = 0;
bool g_inMenu = false, g_inGame = false;
int g_itemLayer = -2;

int g_wantLock = 0;      // what the game asked for
bool g_wantVisible = true;
bool g_cursorOverridden = false;

struct NpcCache { uint32_t obj, agent, anim; std::vector<uint32_t> rends; uint32_t col; const char* label; };
struct ItemCache { uint32_t col; uint32_t go; std::string label; };
std::vector<NpcCache> g_npcs;
std::vector<ItemCache> g_items;
bool g_frozenApplied = false;

std::vector<uint32_t> g_uiHandles, g_uiHandlesOld;
std::vector<uint32_t> g_hiddenCanvases;
// Custom UI suspension while a game screen that must stay original is open (Game Jolt).
bool g_cuiSuspended = false, g_cuiSeenOther = false;
float g_cuiSuspendTime = 0.f;
std::vector<uintptr_t> g_cuiRootIds;
std::vector<uintptr_t> SnapshotIds() {
    std::vector<uintptr_t> ids;
    std::lock_guard<std::mutex> lk(g_snap.mtx);
    for (auto& e : g_snap.ui) ids.push_back(e.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// God mode upkeep: enemies that "caught" the player are released so they keep moving.
struct CatcherClass { void* klass; int offCaught; int offAgent; int offVision; };
std::vector<CatcherClass> g_catchers;
std::vector<uint32_t> g_catcherObjs;
float g_catcherTimer = 0;
float g_devSeeTime = 0;  // dev: force enemy vision
int g_devKeyUp = 0;      // dev: pending key release
int g_devTrace = 0;
float g_devKeyTime = 0;

// Dev: synthetic movement input for fly/noclip tests
V3 g_devInput{0, 0, 0};
float g_devInputTime = 0;

// Dev command channel
std::deque<std::string> g_devCmds;
float g_devWait = 0;
std::mutex g_devRenderMtx;
std::deque<std::string> g_devRender;

// ---------------------------------------------------------------- helpers
bool GameFocused() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}
bool Key(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

void FreeHandles(std::vector<uint32_t>& v) { for (auto h : v) il2::GcFree(h); v.clear(); }

std::string CleanLabel(std::string s) {
    // strip rich-text tags and collapse whitespace
    std::string out;
    bool tag = false;
    for (char c : s) {
        if (c == '<') { tag = true; continue; }
        if (c == '>' && tag) { tag = false; continue; }
        if (tag) continue;
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        out += c;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string Prettify(std::string n) {
    size_t p = n.find("(Clone)");
    if (p != std::string::npos) n.erase(p);
    std::string out;
    for (size_t i = 0; i < n.size(); ++i) {
        char c = n[i];
        if (c == '_' || c == '-') c = ' ';
        if (i > 0 && isupper((unsigned char)c) && islower((unsigned char)n[i - 1])) out += ' ';
        out += c;
    }
    return CleanLabel(out);
}

// Friendly Russian labels for common menu buttons (falls back to the game's own text).
std::string Translate(const std::string& in, bool& primary, bool& external) {
    std::string k;
    for (char c : in) if (isalnum((unsigned char)c)) k += (char)tolower((unsigned char)c);
    // drop common suffixes used in object names
    for (const char* suf : {"button", "btn", "b"})
        if (k.size() > strlen(suf) + 2 && k.compare(k.size() - strlen(suf), strlen(suf), suf) == 0) { k.resize(k.size() - strlen(suf)); break; }
    static const struct { const char* key; const char* ru; bool primary; } dict[] = {
        {"play", "Играть", true}, {"start", "Начать", true}, {"startgame", "Начать игру", true}, {"continue", "Продолжить", true},
        {"resume", "Продолжить", true}, {"quit", "Выйти из игры", false}, {"exit", "Выход", false}, {"shop", "Магазин", false},
        {"achievements", "Достижения", false}, {"settings", "Настройки", false}, {"options", "Настройки", false},
        {"back", "Назад", false}, {"close", "Закрыть", false}, {"gamejolt", "Game Jolt", false}, {"credits", "Авторы", false},
        {"yes", "Да", false}, {"no", "Нет", false}, {"ok", "OK", false}, {"buy", "Купить", false}, {"next", "Далее", false},
        {"previous", "Назад", false}, {"keybinds", "Управление", false}, {"controls", "Управление", false}};
    if (k == "gamejolt") external = true;
    for (auto& d : dict) if (k == d.key) { primary = d.primary; return d.ru; }
    return in;
}

// "Main Car Key / 2", "Main Red Cogwheel(1) / 3" -> "Car Key", "Red Cogwheel"
std::string ItemLabel(const std::string& raw) {
    std::string n = raw;
    size_t p = n.find('/');
    if (p != std::string::npos) n.erase(p);
    while ((p = n.find('(')) != std::string::npos) {
        size_t q = n.find(')', p);
        n.erase(p, q == std::string::npos ? std::string::npos : q - p + 1);
    }
    n = Prettify(n);
    if (n.rfind("Main ", 0) == 0) n.erase(0, 5);
    while (!n.empty() && (n.back() == ' ' || isdigit((unsigned char)n.back()))) n.pop_back();
    return n.empty() ? Prettify(raw) : n;
}

// ---------------------------------------------------------------- unlocks
const char* kColors[] = {"Rainbow", "Pink", "Purple", "Red", "Yellow", "Orange", "Green", "Blue", "Grey", "Brown"};
const char* kItems[] = {"CarKey", "WPKey", "Screwdriver", "Pliers", "ChainCutter"};
const char* kBought[] = {"NM_BOUGHT", "CC_BOUGHT", "CH_BOUGHT", "EXTRAPS_BOUGHT", "EXTRAS_BOUGHT", "FLASH_BOUGHT",
                         "GRANDPA_BOUGHT", "HO_BOUGHT", "IM_BOUGHT", "ROBO_BOUGHT", "SH_BOUGHT", "TRAP_BOUGHT", "VERC_BOUGHT"};

std::vector<std::string> UnlockKeys() {
    std::vector<std::string> k;
    for (auto c : kColors) for (auto i : kItems) k.push_back(std::string(c) + "_" + i);
    for (auto b : kBought) k.push_back(b);
    return k;
}
std::wstring Wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
std::string BackupPath() { return g_baseDir + "unlock_backup.ini"; }
bool BackupExists() { return GetFileAttributesW(Wide(BackupPath()).c_str()) != INVALID_FILE_ATTRIBUTES; }

void ReloadMenuIfNeeded() {
    if (g_inMenu) {
        int idx = U::ActiveSceneIndex();
        if (idx >= 0) { LOG("reloading menu scene %d", idx); U::LoadScene(idx); }
    }
}

void ApplyUnlockAll() {
    if (!BackupExists()) {
        FILE* f = _wfopen(Wide(BackupPath()).c_str(), L"w");
        if (!f) { Toast("Не удалось создать резервную копию", 2); return; }
        for (auto& k : UnlockKeys()) {
            bool has = U::PrefHasKey(k.c_str());
            fprintf(f, "%s=%d\n", k.c_str(), has ? U::PrefGetInt(k.c_str(), 0) : -999);
        }
        fclose(f);
    }
    for (auto& k : UnlockKeys()) U::PrefSetInt(k.c_str(), 1);
    U::PrefSave();
    LOG("unlock all applied");
}

void RestoreUnlocks() {
    FILE* f = _wfopen(Wide(BackupPath()).c_str(), L"r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        int v = atoi(eq + 1);
        if (v == -999) U::PrefDeleteKey(line); else U::PrefSetInt(line, v);
    }
    fclose(f);
    U::PrefSave();
    DeleteFileW(Wide(BackupPath()).c_str());
    LOG("unlocks restored");
}

// ---------------------------------------------------------------- cursor
void UpdateCursor() {
    bool open = g_menuOpen.load();
    bool needOverride = open || g_disclaimerOpen || (g_cfg.customUi && g_inMenu);
    if (needOverride && o_SetLockState) {
        o_SetLockState(0);
        o_SetVisible(true);
        g_cursorOverridden = true;
    } else if (g_cursorOverridden && o_SetLockState) {
        o_SetLockState(g_wantLock);
        o_SetVisible(g_wantVisible);
        g_cursorOverridden = false;
    }
}
void hk_SetLockState(int v) {
    g_wantLock = v;
    if (g_cursorOverridden) return;
    o_SetLockState(v);
}
void hk_SetVisible(bool v) {
    g_wantVisible = v;
    if (g_cursorOverridden) return;
    o_SetVisible(v);
}

// ---------------------------------------------------------------- scene tracking
void TrackScene() {
    auto findOne = [](void* klass, uint32_t& handle) -> bool {
        Il2CppArray* a = klass ? U::FindObjectsOfType(klass) : nullptr;
        il2::GcFree(handle);
        handle = 0;
        if (il2::ArrayLen(a) > 0) { handle = il2::GcNew((Il2CppObject*)a->items[0]); return true; }
        return false;
    };
    bool wasMenu = g_inMenu;
    g_inMenu = findOne(k_MainMenu, g_mainMenuH);
    g_inGame = findOne(k_MobileFPS, g_playerH);
    if (wasMenu != g_inMenu) LOG("main menu %s", g_inMenu ? "entered" : "left");
}

// ---------------------------------------------------------------- ESP + NPC cache
void RefreshNpcs() {
    for (auto& n : g_npcs) { il2::GcFree(n.obj); il2::GcFree(n.agent); il2::GcFree(n.anim); il2::GcFree(n.col); for (auto r : n.rends) il2::GcFree(r); }
    g_npcs.clear();
    for (auto& nc : g_npcClasses) {
        Il2CppArray* a = U::FindObjectsOfType(nc.klass);
        for (int i = 0; i < il2::ArrayLen(a); ++i) {
            Il2CppObject* o = (Il2CppObject*)a->items[i];
            Il2CppObject* go = U::GameObjectOf(o);
            if (!U::ActiveInHierarchy(go)) continue;
            NpcCache c{};
            c.obj = il2::GcNew(o);
            c.label = nc.label;
            c.agent = il2::GcNew(U::GetComponent(o, U::C.NavMeshAgent));
            c.anim = il2::GcNew(U::GetComponentInChildren(o, U::C.Animator));
            Il2CppArray* rs = U::GetComponentsInChildren(o, U::C.SkinnedMeshRenderer);
            for (int r = 0; r < il2::ArrayLen(rs) && c.rends.size() < 12; ++r) c.rends.push_back(il2::GcNew((Il2CppObject*)rs->items[r]));
            if (c.rends.empty()) {
                Il2CppArray* cs = U::GetComponentsInChildren(o, U::C.Collider);
                for (int k = 0; k < il2::ArrayLen(cs); ++k) {
                    Il2CppObject* col = (Il2CppObject*)cs->items[k];
                    if (!U::ColliderIsTrigger(col)) { c.col = il2::GcNew(col); break; }
                }
            }
            g_npcs.push_back(std::move(c));
        }
    }
}

void RefreshItems() {
    for (auto& it : g_items) { il2::GcFree(it.col); il2::GcFree(it.go); }
    g_items.clear();
    if (g_itemLayer == -2) g_itemLayer = U::NameToLayer("Item");
    if (g_itemLayer < 0) return;
    Il2CppArray* a = U::FindObjectsOfType(U::C.Collider);
    std::vector<Il2CppObject*> seen;
    for (int i = 0; i < il2::ArrayLen(a); ++i) {
        Il2CppObject* col = (Il2CppObject*)a->items[i];
        Il2CppObject* go = U::GameObjectOf(col);
        if (!go || U::Layer(go) != g_itemLayer || !U::ColliderEnabled(col) || !U::ActiveInHierarchy(go)) continue;
        if (std::find(seen.begin(), seen.end(), go) != seen.end()) continue;
        seen.push_back(go);
        g_items.push_back({il2::GcNew(col), il2::GcNew(go), ItemLabel(U::Name(go))});
        if (g_items.size() >= 300) break;
    }
}

bool Project(Il2CppObject* cam, const U::Bounds& b, float out[8][2], int sw, int sh) {
    if (b.extents.x == 0 && b.extents.y == 0 && b.extents.z == 0) return false;
    for (int i = 0; i < 8; ++i) {
        V3 p{b.center.x + ((i & 1) ? b.extents.x : -b.extents.x),
             b.center.y + ((i & 2) ? b.extents.y : -b.extents.y),
             b.center.z + ((i & 4) ? b.extents.z : -b.extents.z)};
        V3 s = U::WorldToScreen(cam, p);
        if (s.z <= 0.05f) return false;
        out[i][0] = s.x;
        out[i][1] = (float)sh - s.y;
    }
    (void)sw;
    return true;
}

U::Bounds Union(const U::Bounds& a, const U::Bounds& b) {
    V3 amin = a.center - a.extents, amax = a.center + a.extents;
    V3 bmin = b.center - b.extents, bmax = b.center + b.extents;
    V3 mn{std::min(amin.x, bmin.x), std::min(amin.y, bmin.y), std::min(amin.z, bmin.z)};
    V3 mx{std::max(amax.x, bmax.x), std::max(amax.y, bmax.y), std::max(amax.z, bmax.z)};
    return {(mn + mx) * 0.5f, (mx - mn) * 0.5f};
}

void UpdateEsp(float dt) {
    bool wantNpc = (g_cfg.hitboxes && g_cfg.hbNpc) || g_cfg.freezeEnemies || g_frozenApplied;
    bool wantItems = g_cfg.hitboxes && g_cfg.hbItems;
    g_espTimer -= dt;
    if (g_espTimer <= 0) {
        g_espTimer = 1.0f;
        if (wantNpc) RefreshNpcs();
        if (wantItems) RefreshItems();
    }
    std::vector<EspEntry> out;
    Il2CppObject* cam = U::MainCamera();
    int sw = U::ScreenW(), sh = U::ScreenH();
    if (g_cfg.hitboxes && cam) {
        V3 cp = U::Pos(U::TransformOf(cam));
        if (g_cfg.hbNpc) {
            for (auto& n : g_npcs) {
                Il2CppObject* o = il2::GcGet(n.obj);
                if (!il2::Alive(o) || !U::ActiveInHierarchy(U::GameObjectOf(o))) continue;
                U::Bounds b{};
                bool has = false;
                for (auto rh : n.rends) {
                    Il2CppObject* r = il2::GcGet(rh);
                    if (!il2::Alive(r) || !U::Enabled(r)) continue;
                    U::Bounds rb = U::RendererBounds(r);
                    b = has ? Union(b, rb) : rb;
                    has = true;
                }
                if (!has) { Il2CppObject* c = il2::GcGet(n.col); if (il2::Alive(c)) { b = U::ColliderBounds(c); has = true; } }
                if (!has) continue;
                EspEntry e{};
                if (!Project(cam, b, e.pts, sw, sh)) continue;
                e.npc = true;
                e.label = n.label;
                e.dist = U::Len(b.center - cp);
                out.push_back(std::move(e));
            }
        }
        if (g_cfg.hbItems) {
            for (auto& it : g_items) {
                Il2CppObject* c = il2::GcGet(it.col);
                Il2CppObject* go = il2::GcGet(it.go);
                if (!il2::Alive(c) || !U::ActiveInHierarchy(go) || !U::ColliderEnabled(c)) continue;
                U::Bounds b = U::ColliderBounds(c);
                float d = U::Len(b.center - cp);
                if (d > g_cfg.espMaxDist) continue;
                EspEntry e{};
                if (!Project(cam, b, e.pts, sw, sh)) continue;
                e.npc = false;
                e.label = it.label;
                e.dist = d;
                out.push_back(std::move(e));
            }
        }
    }
    std::lock_guard<std::mutex> lk(g_snap.mtx);
    g_snap.esp.swap(out);
    g_snap.screenW = sw;
    g_snap.screenH = sh;
}

void UpdateGodUpkeep(float dt) {
    bool need = g_cfg.god || g_devSeeTime > 0;
    if (!need) return;
    g_catcherTimer -= dt;
    if (g_catcherTimer <= 0) {
        g_catcherTimer = 1.f;
        FreeHandles(g_catcherObjs);
        for (auto& c : g_catchers) {
            Il2CppArray* a = U::FindObjectsOfType(c.klass);
            for (int i = 0; i < il2::ArrayLen(a); ++i) g_catcherObjs.push_back(il2::GcNew((Il2CppObject*)a->items[i]));
        }
    }
    for (auto h : g_catcherObjs) {
        Il2CppObject* o = il2::GcGet(h);
        if (!il2::Alive(o)) continue;
        void* k = *reinterpret_cast<void**>(o);
        for (auto& c : g_catchers) {
            if (c.klass != k) continue;
            if (g_devSeeTime > 0 && c.offVision >= 0) {
                Il2CppObject* v = il2::At<Il2CppObject*>(o, c.offVision);
                if (v) il2::At<bool>(v, 0x39) = true;
            }
            if (g_cfg.god && c.offCaught >= 0 && il2::At<bool>(o, c.offCaught)) {
                il2::At<bool>(o, c.offCaught) = false;
                if (c.offAgent >= 0) U::AgentStop(il2::At<Il2CppObject*>(o, c.offAgent), false);
            }
        }
    }
    if (g_devSeeTime > 0) g_devSeeTime -= dt;
}

void UpdateFreeze() {
    bool want = g_cfg.freezeEnemies;
    g_freezeGroup.Set(want);
    if (want || g_frozenApplied) {
        for (auto& n : g_npcs) {
            U::AgentStop(il2::GcGet(n.agent), want);
            U::AnimatorSpeed(il2::GcGet(n.anim), want ? 0.f : 1.f);
        }
        g_frozenApplied = want;
    }
}

// ---------------------------------------------------------------- field of view / time speed
uint32_t g_fovCamH = 0;
float g_fovOrig = 0.f;
bool g_fovApplied = false;
float g_timeApplied = -1.f;

void RestoreFov() {
    Il2CppObject* prev = il2::GcGet(g_fovCamH);
    if (g_fovApplied && il2::Alive(prev) && g_fovOrig > 1.f) U::SetCamFov(prev, g_fovOrig);
    il2::GcFree(g_fovCamH);
    g_fovCamH = 0;
    g_fovApplied = false;
}

void UpdateView() {
    // FOV of the active game camera (the camera can change: bed, hiding spots, cutscenes)
    Il2CppObject* cam = (g_cfg.fovOn && !g_inMenu) ? U::MainCamera() : nullptr;
    if (cam) {
        if (il2::GcGet(g_fovCamH) != cam) {
            RestoreFov();
            g_fovCamH = il2::GcNew(cam);
            g_fovOrig = U::CamFov(cam);
        }
        U::SetCamFov(cam, g_cfg.fov);
        g_fovApplied = true;
    } else if (g_fovApplied) RestoreFov();
    // time speed; a paused game (timeScale 0) is left alone
    float cur = U::TimeScale();
    if (g_cfg.timeOn) {
        if (cur > 0.0001f && std::fabs(cur - g_cfg.timeScale) > 1e-4f) { U::SetTimeScale(g_cfg.timeScale); g_timeApplied = g_cfg.timeScale; }
    } else if (g_timeApplied >= 0.f) {
        if (cur > 0.0001f) U::SetTimeScale(1.f);
        g_timeApplied = -1.f;
    }
}

// ---------------------------------------------------------------- custom main-menu UI
void RestoreCanvases() {
    for (auto h : g_hiddenCanvases) {
        Il2CppObject* c = il2::GcGet(h);
        if (il2::Alive(c)) U::SetEnabled(c, true);
        il2::GcFree(h);
    }
    g_hiddenCanvases.clear();
}

void HideCanvases() {
    Il2CppArray* a = U::FindObjectsOfType(U::C.Canvas);
    for (int i = 0; i < il2::ArrayLen(a); ++i) {
        Il2CppObject* c = (Il2CppObject*)a->items[i];
        if (!U::CanvasIsRoot(c) || !U::Enabled(c)) continue;
        U::SetEnabled(c, false);
        g_hiddenCanvases.push_back(il2::GcNew(c));
    }
}

void CollectUi() {
    std::vector<UiElem> elems;
    FreeHandles(g_uiHandlesOld);
    g_uiHandlesOld.swap(g_uiHandles);
    Il2CppArray* a = U::FindObjectsOfType(U::C.Selectable);
    for (int i = 0; i < il2::ArrayLen(a); ++i) {
        Il2CppObject* s = (Il2CppObject*)a->items[i];
        Il2CppObject* go = U::GameObjectOf(s);
        if (!U::ActiveInHierarchy(go) || !U::Enabled(s) || !U::Interactable(s)) continue;
        void* k = *reinterpret_cast<void**>(s);
        UiElem e{};
        if (il2::IsSubclass(k, U::C.Toggle)) { e.kind = UiKind::Toggle; e.on = U::ToggleIsOn(s); }
        else if (il2::IsSubclass(k, U::C.Slider)) {
            e.kind = UiKind::Slider; e.value = U::SliderValue(s); e.vmin = U::SliderMin(s); e.vmax = U::SliderMax(s); e.whole = U::SliderWhole(s);
        } else if (il2::IsSubclass(k, U::C.TMPDropdown) || il2::IsSubclass(k, U::C.UIDropdown)) {
            e.kind = UiKind::Dropdown; e.value = (float)U::DropdownValue(s); e.count = U::DropdownCount(s);
        } else if (il2::IsSubclass(k, U::C.Button)) e.kind = UiKind::Button;
        else continue;
        Il2CppObject* tr = U::TransformOf(s);  // Component.transform (go is a GameObject, not a Component)
        e.label = CleanLabel(U::LabelFor(s));
        std::string goName = Prettify(U::Name(go));
        if (e.label.empty() || e.label.size() > 60) e.label = goName;
        if (e.kind == UiKind::Slider && e.label.empty()) e.label = goName;
        e.label = Translate(e.label, e.primary, e.external);
        e.group = Prettify(U::Name(U::Parent(tr)));
        V3 p = U::Pos(tr);
        e.sx = p.x; e.sy = p.y;
        e.id = reinterpret_cast<uintptr_t>(s);
        e.handle = il2::GcNew(s);
        g_uiHandles.push_back(e.handle);
        elems.push_back(std::move(e));
    }
    // Keep the game's visual order: top-to-bottom, then left-to-right.
    std::stable_sort(elems.begin(), elems.end(), [](const UiElem& x, const UiElem& y) {
        if (std::abs(x.sy - y.sy) > 8.f) return x.sy > y.sy;
        return x.sx < y.sx;
    });
    std::lock_guard<std::mutex> lk(g_snap.mtx);
    g_snap.ui.swap(elems);
    g_snap.uiLoading = g_snap.ui.empty();
}

void UpdateCustomUi(float dt) {
    if (g_cuiSuspended) {
        if (!g_cfg.customUi || !g_inMenu) g_cuiSuspended = false;
        else {
            if (!g_hiddenCanvases.empty()) RestoreCanvases();
            g_cuiSuspendTime += dt;
            g_uiTimer -= dt;
            if (g_uiTimer <= 0) {
                g_uiTimer = 0.2f;
                CollectUi();  // keeps watching the menu without hiding the game's canvases
                auto ids = SnapshotIds();
                bool same = ids == g_cuiRootIds;
                if (!same) g_cuiSeenOther = true;
                // resume once the external screen has been opened and closed again (or never opened)
                if ((g_cuiSeenOther && same) || (!g_cuiSeenOther && g_cuiSuspendTime > 3.f)) {
                    g_cuiSuspended = false;
                    LOG("custom ui resumed");
                }
            }
            std::lock_guard<std::mutex> lk(g_snap.mtx);
            g_snap.uiSuspended = g_cuiSuspended;
            if (g_cuiSuspended) return;
        }
        std::lock_guard<std::mutex> lk(g_snap.mtx);
        g_snap.uiSuspended = false;
    }
    bool want = g_cfg.customUi && g_inMenu;
    if (!want) {
        if (!g_hiddenCanvases.empty()) RestoreCanvases();
        if (!g_uiHandles.empty() || !g_uiHandlesOld.empty()) {
            FreeHandles(g_uiHandles); FreeHandles(g_uiHandlesOld);
            std::lock_guard<std::mutex> lk(g_snap.mtx);
            g_snap.ui.clear();
        }
        return;
    }
    g_uiTimer -= dt;
    if (g_uiTimer > 0) return;
    g_uiTimer = 0.1f;
    // Collect while canvases are enabled? Selectables stay active when the canvas is disabled,
    // so it is safe to keep them hidden permanently.
    HideCanvases();
    CollectUi();
}

// ---------------------------------------------------------------- dev commands
void DevLog(const char* what) { LOG("dev: %s", what); }

bool g_devEnabled = false;  // the dev command channel only exists when ZenClient\dev\ is present

void DevTick(float dt) {
    if (!g_devEnabled) return;
    g_devTimer -= dt;
    if (g_devTimer <= 0) {
        g_devTimer = 0.5f;
        std::wstring p = Wide(g_baseDir + "dev\\cmd.txt");
        FILE* f = _wfopen(p.c_str(), L"r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof line, f)) {
                std::string s(line);
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                if (!s.empty() && s[0] != '#') g_devCmds.push_back(s);
            }
            fclose(f);
            DeleteFileW(p.c_str());
        }
    }
    if (g_devKeyUp && (g_devKeyTime -= dt) <= 0) {
        INPUT in{}; in.type = INPUT_KEYBOARD; in.ki.wVk = (WORD)g_devKeyUp; in.ki.dwFlags = KEYEVENTF_KEYUP;
        in.ki.wScan = g_devKeyUp == VK_RSHIFT ? 0x36 : (WORD)MapVirtualKeyW(g_devKeyUp, MAPVK_VK_TO_VSC);
        SendInput(1, &in, sizeof in);
        g_devKeyUp = 0;
    }
    if (g_devWait > 0) { g_devWait -= dt; return; }
    while (!g_devCmds.empty() && g_devWait <= 0) {
        std::string c = g_devCmds.front();
        g_devCmds.pop_front();
        DevLog(c.c_str());
        char a[128] = {}; float v = 0;
        if (sscanf(c.c_str(), "wait %f", &v) == 1) { g_devWait = v; break; }
        else if (c == "play") {
            Il2CppObject* mm = il2::GcGet(g_mainMenuH);
            const MethodInfo* m = il2::Method(k_MainMenu, "OnClickPlay", 0);
            if (mm && m) il2::Invoke(m, mm, nullptr);
        } else if (c == "state") {
            Il2CppObject* pl = il2::GcGet(g_playerH);
            V3 p = U::Pos(U::TransformOf(pl));
            Il2CppArray* ps = k_PlayerStatus ? U::FindObjectsOfType(k_PlayerStatus) : nullptr;
            int killed = -1;
            if (il2::ArrayLen(ps) > 0 && off_killed >= 0) killed = il2::At<bool>(ps->items[0], off_killed);
            LOG("state: scene=%d menu=%d game=%d pos=(%.2f %.2f %.2f) killed=%d coins=%.0f npcs=%zu items=%zu",
                U::ActiveSceneIndex(), g_inMenu, g_inGame, p.x, p.y, p.z, killed, U::PrefGetFloat("Coins", 0), g_npcs.size(), g_items.size());
            Il2CppObject* cam = U::MainCamera();
            size_t espN; { std::lock_guard<std::mutex> lk(g_snap.mtx); espN = g_snap.esp.size(); }
            LOG("state: cam=%p camName=%s esp=%zu screen=%dx%d", (void*)cam, U::Name(cam).c_str(), espN, U::ScreenW(), U::ScreenH());
            LOG("state: fov=%.1f timeScale=%.2f", U::CamFov(cam), U::TimeScale());
            for (auto& n : g_npcs) {
                Il2CppObject* o = il2::GcGet(n.obj);
                V3 np = U::Pos(U::TransformOf(o));
                LOG("  npc %s alive=%d rends=%zu col=%u pos=(%.1f %.1f %.1f)", n.label, il2::Alive(o), n.rends.size(), n.col, np.x, np.y, np.z);
            }
            for (size_t i = 0; i < g_items.size() && i < 5; ++i) {
                U::Bounds b = U::ColliderBounds(il2::GcGet(g_items[i].col));
                V3 sp = U::WorldToScreen(cam, b.center);
                LOG("  item %s c=(%.1f %.1f %.1f) e=(%.2f %.2f %.2f) screen=(%.0f %.0f %.2f)", g_items[i].label.c_str(), b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z, sp.x, sp.y, sp.z);
            }
        } else if (sscanf(c.c_str(), "grannytp %127s", a) == 1) {
            // teleport the named NPC in front of the player (god-mode testing)
            Il2CppObject* pl = il2::GcGet(g_playerH);
            Il2CppObject* tr = U::TransformOf(pl);
            V3 p = U::Pos(tr) + U::Norm(V3{U::Forward(tr).x, 0, U::Forward(tr).z}) * 0.8f;
            for (auto& n : g_npcs) {
                if (strcmp(n.label, a) != 0) continue;
                Il2CppObject* ag = il2::GcGet(n.agent);
                if (ag) U::AgentWarp(ag, p); else U::SetPos(U::TransformOf(il2::GcGet(n.obj)), p);
                LOG("teleported %s", a);
                break;
            }
        } else if (sscanf(c.c_str(), "tp %f", &v) == 1) {
            float x, y, z;
            if (sscanf(c.c_str(), "tp %f %f %f", &x, &y, &z) == 3) {
                Il2CppObject* tr = U::TransformOf(il2::GcGet(g_playerH));
                Il2CppObject* cc = (off_cc >= 0 && il2::GcGet(g_playerH)) ? il2::At<Il2CppObject*>(il2::GcGet(g_playerH), off_cc) : nullptr;
                if (cc) U::SetColliderEnabled(cc, false);
                U::SetPos(tr, {x, y, z});
                if (cc) U::SetColliderEnabled(cc, true);
            }
        } else if (c.rfind("devmove ", 0) == 0) {
            float x, y, z, t;
            if (sscanf(c.c_str(), "devmove %f %f %f %f", &x, &y, &z, &t) == 4) { g_devInput = {x, y, z}; g_devInputTime = t; }
        } else if (c == "grannychase") {
            void* k = il2::Class(GAME, "", "AI_Granny");
            Il2CppArray* a = U::FindObjectsOfType(k);
            int oc = il2::FieldOffset(k, "IsChasing"), oa = il2::FieldOffset(k, "IsAngry");
            for (int i = 0; i < il2::ArrayLen(a); ++i) {
                if (oc >= 0) il2::At<bool>(a->items[i], oc) = true;
                if (oa >= 0) il2::At<bool>(a->items[i], oa) = true;
            }
            LOG("granny chase forced on %d", il2::ArrayLen(a));
        } else if (c.rfind("sendkey ", 0) == 0) {
            // sendkey <vk> <seconds>  -- real key press through SendInput (focus the game window first)
            int vk = 0; float secs = 0.1f;
            if (sscanf(c.c_str(), "sendkey %i %f", &vk, &secs) >= 1) {
                HWND w = FindWindowW(L"UnityWndClass", nullptr);
                if (w) { SetForegroundWindow(w); SetFocus(w); }
                INPUT in{}; in.type = INPUT_KEYBOARD; in.ki.wVk = (WORD)vk; in.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
                if (vk == VK_RSHIFT) in.ki.wScan = 0x36;
                SendInput(1, &in, sizeof in);
                g_devKeyUp = vk; g_devKeyTime = secs;
                LOG("sendkey %d down (fg=%d)", vk, GameFocused());
            }
        } else if (sscanf(c.c_str(), "grannysee %f", &v) == 1) { g_devSeeTime = v; g_catcherTimer = 0; }
        else if (c == "pstatus") {
            Il2CppArray* ps = U::FindObjectsOfType(k_PlayerStatus);
            if (il2::ArrayLen(ps) > 0) {
                void* o = ps->items[0];
                int oj = il2::FieldOffset(k_PlayerStatus, "IsJumpscared");
                LOG("pstatus killed=%d jumpscared=%d", il2::At<bool>(o, off_killed), oj >= 0 ? il2::At<bool>(o, oj) : -1);
            }
        } else if (sscanf(c.c_str(), "dumpobj %127s", a) == 1) {
            void* k = il2::Class(GAME, "", a);
            Il2CppArray* arr = k ? U::FindObjectsOfType(k) : nullptr;
            if (il2::ArrayLen(arr) > 0) il2::DumpObject((Il2CppObject*)arr->items[0], (g_baseDir + "dev\\obj_" + a + ".txt").c_str());
            LOG("dumpobj %s n=%d", a, il2::ArrayLen(arr));
        } else if (c == "dumpcams") {
            Il2CppArray* cs = U::FindObjectsOfType(U::C.Camera);
            for (int i = 0; i < il2::ArrayLen(cs); ++i) {
                Il2CppObject* cam = (Il2CppObject*)cs->items[i];
                Il2CppObject* tr = U::TransformOf(cam);
                std::string path;
                for (Il2CppObject* t = tr; t; t = U::Parent(t)) path = U::Name(t) + (path.empty() ? "" : "/") + path;
                V3 p = U::Pos(tr);
                LOG("cam %d en=%d act=%d %s pos=(%.1f %.1f %.1f)", i, U::Enabled(cam), U::ActiveInHierarchy(U::GameObjectOf(cam)), path.c_str(), p.x, p.y, p.z);
            }
        } else if (sscanf(c.c_str(), "fields %127s", a) == 1) {
            il2::DumpFields(il2::Class(GAME, "", a), (g_baseDir + "dev\\fields_" + a + ".txt").c_str());
        } else if (c.rfind("invoke ", 0) == 0) {
            char cls[128] = {}, meth[128] = {};
            if (sscanf(c.c_str(), "invoke %127s %127s", cls, meth) == 2) {
                void* k = il2::Class(GAME, "", cls);
                Il2CppArray* arr = k ? U::FindObjectsOfType(k) : nullptr;
                const MethodInfo* m = il2::Method(k, meth, 0);
                if (il2::ArrayLen(arr) > 0 && m) { bool ok; il2::Invoke(m, arr->items[0], nullptr, &ok); LOG("invoked %s.%s ok=%d", cls, meth, ok); }
                else LOG("invoke failed %s.%s", cls, meth);
            }
        } else if (c == "speedvars") {
            Il2CppObject* pl = il2::GcGet(g_playerH);
            for (const char* f : {"moveSpeed", "SpeedMove", "SpeedMoveCrouch", "FallSpeed", "SpeedFade", "cameraRotationSpeed"}) {
                int o = il2::FieldOffset(k_MobileFPS, f);
                if (pl && o >= 0) LOG("  %s = %.3f", f, il2::At<float>(pl, o));
            }
            int oc = il2::FieldOffset(k_MobileFPS, "IsCrouched"), om = il2::FieldOffset(k_MobileFPS, "isMoving");
            if (pl) LOG("  IsCrouched=%d isMoving=%d", oc >= 0 ? il2::At<bool>(pl, oc) : -1, om >= 0 ? il2::At<bool>(pl, om) : -1);
        } else if (c == "trace") { g_devTrace = 12;
        } else if (c == "playercomps") {
            // list every component under the player's root transform
            Il2CppObject* tr = U::TransformOf(il2::GcGet(g_playerH));
            for (Il2CppObject* p = U::Parent(tr); p; p = U::Parent(p)) tr = p;
            Il2CppArray* cs = U::GetComponentsInChildren(tr, U::C.Component);
            LOG("root '%s' has %d components", U::Name(tr).c_str(), il2::ArrayLen(cs));
            for (int i = 0; i < il2::ArrayLen(cs) && i < 400; ++i) {
                Il2CppObject* comp = (Il2CppObject*)cs->items[i];
                if (strcmp(il2::ObjClassName(comp), "Transform") == 0) continue;
                LOG("  comp %s on '%s'", il2::ObjClassName(comp), U::Name(U::GameObjectOf(comp)).c_str());
            }
        } else if (c == "dumpmethods") il2::DumpMethods(GAME, (g_baseDir + "dev\\methods.txt").c_str());
        else if (c == "quit") U::Quit();
        else if (c == "reload") ReloadMenuIfNeeded();
        else if (c == "dumpui") {
            std::lock_guard<std::mutex> lk(g_snap.mtx);
            for (size_t i = 0; i < g_snap.ui.size(); ++i)
                LOG("ui[%zu] kind=%d group='%s' label='%s' on=%d v=%.2f", i, (int)g_snap.ui[i].kind, g_snap.ui[i].group.c_str(), g_snap.ui[i].label.c_str(), g_snap.ui[i].on, g_snap.ui[i].value);
        } else if (sscanf(c.c_str(), "uiclick %f", &v) == 1) {
            UiElem e{};
            { std::lock_guard<std::mutex> lk(g_snap.mtx); if ((size_t)v < g_snap.ui.size()) e = g_snap.ui[(size_t)v]; }
            if (e.handle) ActionUiPress(e);
        } else if (c == "uiprimary") {
            UiElem e{};
            { std::lock_guard<std::mutex> lk(g_snap.mtx); for (auto& x : g_snap.ui) if (x.primary) { e = x; break; } }
            if (e.handle) { LOG("pressing primary '%s'", e.label.c_str()); ActionUiPress(e); }
        } else if (sscanf(c.c_str(), "coins %f", &v) == 1) ActionGiveCoins(v, false);
        else if (sscanf(c.c_str(), "setcoins %f", &v) == 1) ActionGiveCoins(v, true);
        else if (sscanf(c.c_str(), "pref %127s", a) == 1) LOG("pref %s int=%d float=%.2f has=%d", a, U::PrefGetInt(a, -1), U::PrefGetFloat(a, -1), U::PrefHasKey(a));
        else DevPostRender(c);   // menu/tab/cfg/shot/click are handled on the render thread
    }
}

// ---------------------------------------------------------------- per-frame tick
bool g_prevUnlock = false, g_unlockInit = false;

void MainTick() {
    int frame = U::FrameCount();
    if (frame == g_lastFrame) return;
    g_lastFrame = frame;
    float dt = std::min(U::DeltaTime(), 0.25f);

    RunMainQueue();

    g_sceneTimer -= dt;
    if (g_sceneTimer <= 0) { g_sceneTimer = 0.5f; TrackScene(); }

    g_godGroup.Set(g_cfg.god);
    g_invisGroup.Set(g_cfg.invisible);
    g_fallGroup.Set(g_cfg.god || g_cfg.fly || g_cfg.noclip);

    if (!g_unlockInit) {
        g_unlockInit = true;
        g_prevUnlock = g_cfg.unlockAll;
        bool has = BackupExists();
        if (g_cfg.unlockAll) ApplyUnlockAll();   // re-assert (idempotent; keeps the first backup)
        else if (has) RestoreUnlocks();
    } else if (g_cfg.unlockAll != g_prevUnlock) {
        g_prevUnlock = g_cfg.unlockAll;
        if (g_cfg.unlockAll) { ApplyUnlockAll(); Toast("Все предметы и режимы открыты", 1); }
        else { RestoreUnlocks(); Toast("Открытия возвращены как были", 0); }
        ReloadMenuIfNeeded();
    }

    UpdateCursor();
    UpdateEsp(dt);
    UpdateFreeze();
    UpdateGodUpkeep(dt);
    UpdateCustomUi(dt);
    UpdateView();

    g_coinTimer -= dt;
    if (g_coinTimer <= 0) {
        g_coinTimer = 0.3f;
        float coins = U::PrefGetFloat("Coins", 0);
        std::lock_guard<std::mutex> lk(g_snap.mtx);
        g_snap.coins = coins;
        g_snap.inMainMenu = g_inMenu;
        g_snap.inGame = g_inGame;
    }
    DevTick(dt);
}

// ---------------------------------------------------------------- movement (fly / noclip / speed)
void MovementPost(Il2CppObject* self, Il2CppObject* tr, V3 p0) {
    bool nc = g_cfg.noclip, fly = g_cfg.fly && !nc;
    if (!nc && !fly) return;
    Il2CppObject* cc = off_cc >= 0 ? il2::At<Il2CppObject*>(self, off_cc) : nullptr;
    if (!il2::Alive(cc)) cc = nullptr;
    float dt = std::min(U::DeltaTime(), 0.1f);
    if (nc || fly) {
        V3 in{0, 0, 0};
        if (g_devInputTime > 0) { in = g_devInput; g_devInputTime -= dt; }
        else if (GameFocused()) {
            if (Key('W')) in.z += 1;
            if (Key('S')) in.z -= 1;
            if (Key('D')) in.x += 1;
            if (Key('A')) in.x -= 1;
            if (Key(VK_SPACE)) in.y += 1;
            if (Key(VK_LCONTROL)) in.y -= 1;
        }
        Il2CppObject* cam = U::MainCamera();
        Il2CppObject* ct = cam ? U::TransformOf(cam) : tr;
        V3 f = U::Forward(ct), r = U::Right(ct);
        float speed = g_cfg.flySpeed * (Key(VK_LSHIFT) ? 2.5f : 1.f);
        V3 d = (f * in.z + r * in.x + V3{0, in.y, 0}) * (speed * dt);
        if (off_moveDir >= 0) il2::At<V3>(self, off_moveDir).y = 0;
        if (nc) {
            if (cc) U::SetColliderEnabled(cc, false);
            U::SetPos(tr, p0 + d);
            if (cc) U::SetColliderEnabled(cc, true);
        } else {
            if (cc) {
                U::SetColliderEnabled(cc, false);
                U::SetPos(tr, p0);
                U::SetColliderEnabled(cc, true);
                U::CCMove(cc, d);
            } else U::SetPos(tr, p0 + d);
        }
    }
}

// Speed boost. MobileFPS.Update assigns moveSpeed from constants, then reads Input.GetAxis, then
// multiplies the axes by moveSpeed. Input.GetAxis is hooked and, while it runs inside MobileFPS.Update,
// scales moveSpeed once - so the game itself moves faster (collisions, animations and crouch included).
Il2CppObject* g_fpsInUpdate = nullptr;
bool g_fpsSpeedScaled = false;
using AxisFn = float (*)(Il2CppString*, const MethodInfo*);
AxisFn o_GetAxis = nullptr, o_GetAxisRaw = nullptr;
void ScaleSpeedOnce() {
    if (!g_fpsInUpdate || g_fpsSpeedScaled || !g_cfg.speed || off_moveSpeed < 0) return;
    il2::At<float>(g_fpsInUpdate, off_moveSpeed) *= g_cfg.speedMul;
    g_fpsSpeedScaled = true;
}
float hk_GetAxis(Il2CppString* n, const MethodInfo* mi) { ScaleSpeedOnce(); return o_GetAxis(n, mi); }
float hk_GetAxisRaw(Il2CppString* n, const MethodInfo* mi) { ScaleSpeedOnce(); return o_GetAxisRaw(n, mi); }

// ---------------------------------------------------------------- detours
void hk_EventSystemUpdate(Il2CppObject* self, const MethodInfo* mi) {
    MainTick();
    o_EventSystemUpdate(self, mi);
}
void hk_MainMenuUpdate(Il2CppObject* self, const MethodInfo* mi) {
    MainTick();
    o_MainMenuUpdate(self, mi);
}
void hk_MobileFPSUpdate(Il2CppObject* self, const MethodInfo* mi) {
    MainTick();
    if (g_menuOpen || g_disclaimerOpen) return;  // freeze look/move while the ZenClient menu is open
    Il2CppObject* tr = U::TransformOf(self);
    V3 p0 = U::Pos(tr);
    g_fpsInUpdate = self;
    g_fpsSpeedScaled = false;
    o_MobileFPSUpdate(self, mi);
    g_fpsInUpdate = nullptr;
    MovementPost(self, tr, p0);
}
void hk_PickRayUpdate(Il2CppObject* self, const MethodInfo* mi) { if (!g_menuOpen) o_PickRayUpdate(self, mi); }
void hk_PausedUpdate(Il2CppObject* self, const MethodInfo* mi) { if (!g_menuOpen) o_PausedUpdate(self, mi); }
void hk_KeyBindUpdate(Il2CppObject* self, const MethodInfo* mi) { if (!g_menuOpen) o_KeyBindUpdate(self, mi); }

} // namespace

// ---------------------------------------------------------------- public API
void ActionGiveCoins(double amount, bool set) {
    PostMain([amount, set] {
        float cur = U::PrefGetFloat("Coins", 0);
        double nv = set ? amount : (double)cur + amount;
        if (nv < 0) nv = 0;
        if (nv > 999999999.0) nv = 999999999.0;
        nv = std::floor(nv);
        U::PrefSetFloat("Coins", (float)nv);
        U::PrefSave();
        LOG("coins %.0f -> %.0f", cur, nv);
        // Refresh the coin counter in the main menu if it is visible.
        Il2CppObject* mm = il2::GcGet(g_mainMenuH);
        if (mm) {
            int off = il2::FieldOffset(k_MainMenu, "CoinAmount");
            Il2CppObject* t = off >= 0 ? il2::At<Il2CppObject*>(mm, off) : nullptr;
            if (il2::Alive(t)) {
                std::string old = U::TextOf(t);
                char buf[64];
                snprintf(buf, sizeof buf, "%.0f", nv);
                size_t p = old.find_first_of("0123456789");
                std::string txt = (p == std::string::npos ? std::string() : old.substr(0, p)) + buf;
                U::SetTextOf(t, txt.c_str());
            }
        }
        g_coinTimer = 0;  // refresh the UI balance right away
        char msg[96];
        if (set) snprintf(msg, sizeof msg, "Баланс установлен: %.0f монет", nv);
        else snprintf(msg, sizeof msg, "Выдано %.0f монет · баланс %.0f", amount, nv);
        Toast(msg, 1);
    });
}

void ActionUiPress(const UiElem& e, int dir, float value) {
    UiElem c = e;
    if (c.kind != UiKind::Slider) LOG("ui press: kind=%d '%s'", (int)c.kind, c.label.c_str());
    PostMain([c, dir, value] {
        Il2CppObject* s = il2::GcGet(c.handle);
        if (!il2::Alive(s)) return;
        if (c.external && c.kind == UiKind::Button) {
            // show this screen in the game's original UI; the custom UI comes back when it is closed
            g_cuiRootIds = SnapshotIds();
            g_cuiSuspended = true;
            g_cuiSeenOther = false;
            g_cuiSuspendTime = 0.f;
            RestoreCanvases();
            { std::lock_guard<std::mutex> lk(g_snap.mtx); g_snap.uiSuspended = true; }
            LOG("custom ui suspended for '%s'", c.label.c_str());
        }
        switch (c.kind) {
        case UiKind::Button: U::ButtonClick(s); break;
        case UiKind::Toggle: U::ToggleSet(s, !U::ToggleIsOn(s)); break;
        case UiKind::Slider: U::SliderSet(s, value); break;
        case UiKind::Dropdown: {
            int n = U::DropdownCount(s);
            if (n > 0) U::DropdownSet(s, ((U::DropdownValue(s) + (dir == 0 ? 1 : dir)) % n + n) % n);
            break;
        }
        }
        g_uiTimer = 0;  // refresh immediately
    });
}

void DevPostRender(const std::string& cmd) { std::lock_guard<std::mutex> lk(g_devRenderMtx); g_devRender.push_back(cmd); }
bool DevPopRender(std::string& out) {
    std::lock_guard<std::mutex> lk(g_devRenderMtx);
    if (g_devRender.empty()) return false;
    out = g_devRender.front();
    g_devRender.pop_front();
    return true;
}

bool FeaturesInit() {
    if (!U::Init()) { LOG("Unity wrappers failed"); return false; }
    k_MainMenu = il2::Class(GAME, "", "MainMenu");
    k_MobileFPS = il2::Class(GAME, "", "MobileFPS");
    k_PlayerStatus = il2::Class(GAME, "", "PlayerStatus");
    off_cc = il2::FieldOffset(k_MobileFPS, "characterController");
    off_moveDir = il2::FieldOffset(k_MobileFPS, "moveDirection");
    off_killed = il2::FieldOffset(k_PlayerStatus, "Killed");
    off_moveSpeed = il2::FieldOffset(k_MobileFPS, "moveSpeed");
    off_speedMove = il2::FieldOffset(k_MobileFPS, "SpeedMove");
    off_speedCrouch = il2::FieldOffset(k_MobileFPS, "SpeedMoveCrouch");
    off_crouched = il2::FieldOffset(k_MobileFPS, "IsCrouched");
    LOG("offsets cc=%d moveDir=%d killed=%d", off_cc, off_moveDir, off_killed);
    g_devEnabled = GetFileAttributesW(Wide(g_baseDir + "dev").c_str()) != INVALID_FILE_ATTRIBUTES;

    struct { const char* cls; const char* label; } npcs[] = {
        {"AI_Granny", "Гренни"}, {"AI_Grandpa", "Дед"}, {"AI_Slendrina", "Слендрина"}, {"AI_MomSpider", "Паучиха"},
        {"AtticSpider", "Паук"}, {"RatEnemy", "Крыса"}, {"AI_RobotEnemy", "Робот"}, {"CrowManage", "Ворона"},
        {"LeprechaunAI", "Лепрекон"}, {"SnowManAI", "Снеговик"}, {"SpectralGranny", "Призрак Гренни"},
        {"UndeadBunny", "Кролик"}, {"LittleFucker", "Гоблин"}, {"SunnyGrannyEnemy", "Санни"}, {"SkeletonBarf", "Скелет"}};
    for (auto& n : npcs) if (void* k = il2::Class(GAME, "", n.cls)) g_npcClasses.push_back({k, n.label});

    // God mode: every path that hurts or kills the player.
    const struct { const char* c; const char* m; int n; } god[] = {
        {"PlayerStatus", "DamageFeet", 0}, {"PlayerStatus", "DamageSpider", 0}, {"PlayerStatus", "MomDeath", 0},
        {"PlayerStatus", "GrannyCaughtYou", 0}, {"PlayerStatus", "GrannyCaughtYouBed", 0},
        {"PlayerStatus", "GrandpaCaughtYou", 0}, {"PlayerStatus", "GrandpaCaughtYouBed", 0},
        {"PlayerStatus", "KnockDeath", 0}, {"PlayerStatus", "NormalDeath", 0},
        {"PickGranny", "OnTriggerStay", 1}, {"PickGrandpa", "OnTriggerStay", 1},
        {"AI_MomSpider", "MomCaughtPlayer", 0}, {"AtticSpider", "StingKill", 0},
        {"DeadlyDart", "OnTriggerEnter", 1}, {"RatEnemy", "OnTriggerEnter", 1},
        {"TriggerFallDeath", "OnTriggerStay", 1}, {"Poisoning", "OnTriggerEnter", 1},
        {"TrapPoison", "OnTriggerEnter", 1}, {"BearTrapLogic", "OnTriggerEnter", 1},
        {"RobotShoot", "Shoot", 0}, {"SaunaDeath", "OnTriggerStay", 1}};
    for (auto& g : god) g_godGroup.Add(g.c, g.m, g.n);
    const struct { const char* c; const char* m; int n; } invis[] = {
        {"AI_Granny", "SensorPlayer", 0}, {"AI_Grandpa", "SensorPlayer", 0}};
    for (auto& g : invis) g_invisGroup.Add(g.c, g.m, g.n);
    const struct { const char* c; const char* m; int n; } freeze[] = {
        {"AI_Granny", "FixedUpdate", 0}, {"AI_Grandpa", "FixedUpdate", 0}, {"AI_MomSpider", "Update", 0},
        {"AI_Slendrina", "Update", 0}, {"AtticSpider", "Update", 0}, {"RatEnemy", "Update", 0},
        {"AI_RobotEnemy", "Update", 0}, {"RobotShoot", "Update", 0}, {"ShootingProcessGranny", "Update", 0}};
    for (auto& g : freeze) g_freezeGroup.Add(g.c, g.m, g.n);
    g_fallGroup.Add("FallingHolder", "Update", 0);
    for (const char* cn : {"AI_Granny", "AI_Grandpa"}) {
        void* k = il2::Class(GAME, "", cn);
        if (k) g_catchers.push_back({k, il2::FieldOffset(k, "CaughtPlayer"), il2::FieldOffset(k, "Agent"), il2::FieldOffset(k, "EnemyVision")});
    }
    LOG("patch groups: god=%zu invis=%zu freeze=%zu fall=%zu", g_godGroup.targets.size(), g_invisGroup.targets.size(),
        g_freezeGroup.targets.size(), g_fallGroup.targets.size());

    // Cursor control (native icalls, so every caller is covered).
    void* lockSet = il2::ResolveICall("UnityEngine.Cursor::set_lockState(UnityEngine.CursorLockMode)");
    void* visSet = il2::ResolveICall("UnityEngine.Cursor::set_visible(System.Boolean)");
    f_GetLockState = reinterpret_cast<int (*)()>(il2::ResolveICall("UnityEngine.Cursor::get_lockState()"));
    if (f_GetLockState) g_wantLock = f_GetLockState();
    Hook(lockSet, reinterpret_cast<void*>(&hk_SetLockState), &o_SetLockState);
    Hook(visSet, reinterpret_cast<void*>(&hk_SetVisible), &o_SetVisible);

    // Per-frame drivers.
    bool any = false;
    any |= Hook(il2::Fn(U::C.EventSystem, "Update", 0), reinterpret_cast<void*>(&hk_EventSystemUpdate), &o_EventSystemUpdate);
    any |= Hook(il2::Fn(k_MainMenu, "Update", 0), reinterpret_cast<void*>(&hk_MainMenuUpdate), &o_MainMenuUpdate);
    any |= Hook(il2::Fn(k_MobileFPS, "Update", 0), reinterpret_cast<void*>(&hk_MobileFPSUpdate), &o_MobileFPSUpdate);
    {
        void* input = il2::Class("UnityEngine.InputLegacyModule.dll", "UnityEngine", "Input");
        Hook(il2::Fn(input, "GetAxis", 1), reinterpret_cast<void*>(&hk_GetAxis), &o_GetAxis);
        Hook(il2::Fn(input, "GetAxisRaw", 1), reinterpret_cast<void*>(&hk_GetAxisRaw), &o_GetAxisRaw);
    }
    Hook(il2::Fn(il2::Class(GAME, "", "PickRay"), "Update", 0), reinterpret_cast<void*>(&hk_PickRayUpdate), &o_PickRayUpdate);
    Hook(il2::Fn(il2::Class(GAME, "", "Paused"), "Update", 0), reinterpret_cast<void*>(&hk_PausedUpdate), &o_PausedUpdate);
    Hook(il2::Fn(il2::Class(GAME, "", "KeyBindManage"), "Update", 0), reinterpret_cast<void*>(&hk_KeyBindUpdate), &o_KeyBindUpdate);
    LOG("features init done (drivers ok=%d)", any ? 1 : 0);
    return any;
}

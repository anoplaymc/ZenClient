// ZenClient - shared state between the Unity main thread and the render thread.
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <atomic>

struct Config {
    // Client
    bool watermark = true;
    // Visuals
    bool hitboxes = false;
    bool hbNpc = true;
    bool hbItems = true;
    bool hbLabels = true;
    bool fullBright = false;       // no darkness / fog on any difficulty
    bool fbNoFog = true;
    float fbStrength = 1.f;
    // Player
    bool god = false;
    bool invisible = false;
    bool freezeEnemies = false;
    bool fly = false;
    bool noclip = false;
    bool speed = false;
    float speedMul = 2.0f;
    float flySpeed = 7.0f;
    // Unlocks
    bool unlockAll = false;
    // Interface
    bool customUi = false;
    // ---- custom main-menu UI (cui*) ----
    bool cuiEdit = false;          // editor mode (not saved)
    int cuiSubTab = 0;             // sub-section of the Interface tab
    // layout
    int cuiLayout = 0;             // 0 left panel, 1 right panel, 2 centre column, 3 floating card
    float cuiPanelW = 0.40f;       // panel width, fraction of the screen
    float cuiOffX = 0.f, cuiOffY = 0.f;  // shift in px (1080p units)
    float cuiScale = 1.f;          // element and text size
    int cuiColumns = 1;            // grid columns for buttons/toggles
    // backdrop
    int cuiBg = 0;                 // 0 gradient, 1 solid, 2 glass, 3 none
    float cuiDim = 0.30f;          // dim of the 3D scene
    float cuiPanelAlpha = 0.92f;
    float cuiPanelCol[3] = {0.031f, 0.035f, 0.047f};
    float cuiGlow = 0.06f;         // accent glow at the bottom
    bool cuiOwnAccent = false;
    float cuiAccent[3] = {0.27f, 0.87f, 0.55f};
    // header
    bool cuiHeader = true, cuiTag = true, cuiCoins = true, cuiHint = true, cuiBackBtn = true;
    int cuiHeaderAlign = 0;        // 0 left, 1 centre
    float cuiTitleScale = 1.f;
    std::string cuiTitle1 = "Granny", cuiTitle2 = "Legacy";
    std::string cuiTagText = "Z E N C L I E N T   ·   E D I T I O N";
    // elements
    int cuiBtnStyle = 0;           // 0 card, 1 flat, 2 outline, 3 pill, 4 minimal, 5 glass
    float cuiItemH = 46.f, cuiGap = 7.f, cuiRadius = 12.f, cuiItemAlpha = 0.85f;
    int cuiAlign = 0;              // text: 0 left, 1 centre
    bool cuiArrows = true, cuiBar = true, cuiHoverShift = true, cuiGroups = true;
    // primary (Play / Start)
    int cuiPrimaryPlace = 0;       // 0 pinned at the bottom, 1 top of the list, 2 in the game's order
    int cuiPrimaryStyle = 0;       // 0 fill, 1 gradient, 2 outline, 3 neon
    float cuiPrimaryH = 54.f;
    bool cuiPulse = false;
    // sub-screens & popups
    int cuiSubMode = 0;            // screens deeper than the first: 0 list, 1 popup, 2 bottom sheet, 3 side drawer
    int cuiPopupAnim = 0;          // 0 scale, 1 slide up, 2 spring, 3 fade, 4 from the side
    float cuiPopupW = 560.f, cuiPopupR = 24.f, cuiPopupDim = 0.55f;
    bool cuiPopupOutside = true;   // click outside = back / close
    bool cuiPopupPrimaryIn = true; // Play/Start inside the popup
    bool cuiAutoMore = false;      // settings next to a Play button go to the "More" popup
    std::string cuiMoreLabel = "Дополнительно";
    // animation
    int cuiPageAnim = 0;           // 0 slide, 1 rise, 2 fade, 3 none
    float cuiPageSpeed = 1.f;
    bool cuiStagger = true;
    float cuiStaggerDelay = 0.035f;
    // ---- ZenClient menu
    float menuScale = 1.f;         // whole ZenClient UI scale (fonts are rebuilt)
    float menuW = 800.f, menuH = 580.f, menuSide = 210.f, menuRadius = 16.f;
    float menuDim = 0.35f;         // backdrop dim while the menu is open
    float menuBg[3] = {0.043f, 0.047f, 0.063f};
    int menuAnim = 0;              // 0 rise, 1 slide, 2 fade, 3 none
    int menuKey = 0;               // index into kMenuKeys (ui.cpp)
    bool menuStatus = true;        // game state in the sidebar
    // ---- watermark
    int wmPos = 0;                 // 0 top centre, 1 top left, 2 top right, 3 bottom left, 4 bottom centre, 5 bottom right
    int wmStyle = 0;               // 0 pill, 1 rectangle, 2 text only
    bool wmName = true, wmFps = true, wmTime = false, wmCoins = false, wmStatus = false, wmDot = true;
    bool wmDay = true;             // current day while in a run
    std::string wmText = "ZenClient";
    float wmScale = 1.f, wmAlpha = 0.93f;
    // ---- hitboxes
    int espBox = 0;                // 0 3D box, 1 2D box, 2 corners
    float espNpcCol[3] = {1.f, 0.33f, 0.41f};
    bool espItemAccent = true;     // items use the accent colour
    float espItemCol[3] = {0.27f, 0.87f, 0.55f};
    float espThick = 1.f, espFill = 0.f, espLabelScale = 1.f, espMaxDist = 70.f;
    bool espDist = true, espTracers = false;
    // ---- notifications
    bool toastOn = true;
    int toastPos = 0;              // 0 bottom right, 1 bottom left, 2 top right, 3 top left
    float toastTime = 3.6f;
    // ---- player view / world
    bool fovOn = false;
    float fov = 75.f;
    bool timeOn = false;
    float timeScale = 1.f;
    // ---- loading
    bool skipIntro = false;        // no "DAY N" screen / waking up in bed when a run starts or restarts
    bool skipIntroNewDay = true;   // ...also when a new day starts after being caught
    bool skipLoading = true;       // no 3.5 s loading screen after "Play" in the main menu (while skipIntro is on)
    // Theme
    float accent[3] = {0.27f, 0.87f, 0.55f};
    float opacity = 0.95f;
    bool animations = true;
    float animSpeed = 1.0f;
    int tab = 0;
    std::string profile;           // active config name (configs\<name>.json)
};

extern Config g_cfg;
void ConfigLoad();
void ConfigSave();          // writes ZenClient/config.ini
void ConfigMarkDirty();     // debounced save (render thread calls ConfigTick)
void ConfigTick(float dt);

extern std::string g_baseDir;   // ...\ZenClient\  (with trailing backslash)

// Main-thread command queue (render/UI thread -> Unity thread).
void PostMain(std::function<void()> fn);
void RunMainQueue();

// Toast notifications (any thread -> UI).
void Toast(const std::string& text, int kind = 0);  // 0 info, 1 success, 2 warning

// ---------- snapshots produced by the main thread ----------
struct EspEntry {
    float pts[8][2];
    bool npc;
    std::string label;
    float dist;
};

enum class UiKind { Button, Toggle, Slider, Dropdown };
struct UiElem {
    UiKind kind;
    uint32_t handle;      // gchandle of the Selectable
    uintptr_t id = 0;     // stable identity (object address; IL2CPP's GC does not move objects)
    std::string label;
    std::string group;
    bool on = false;
    float value = 0, vmin = 0, vmax = 1;
    bool whole = false;
    int count = 0;
    float sx = 0, sy = 0;  // screen position (for ordering)
    bool primary = false;  // main call-to-action (Play)
    bool external = false; // opens a game screen shown without the custom UI (Game Jolt)
};

struct Snapshot {
    std::mutex mtx;
    std::vector<EspEntry> esp;
    int screenW = 0, screenH = 0;
    bool inMainMenu = false;
    bool inGame = false;
    float coins = 0;
    int day = 0;                        // current day of the run (0 = unknown / not in a run)
    int dayMax = 0;                     // last day (5, or 6 with the extra day)
    std::vector<UiElem> ui;
    std::vector<std::string> uiTexts;   // visible informational texts (custom UI)
    bool uiLoading = false;
    bool uiSuspended = false;           // custom UI temporarily hidden (e.g. Game Jolt panel)
};
extern Snapshot g_snap;

extern std::atomic<bool> g_menuOpen;
extern std::atomic<bool> g_ready;       // il2cpp + hooks ready
extern std::atomic<bool> g_splashSkipped;
extern std::atomic<bool> g_overlayReady;
extern std::atomic<bool> g_disclaimerOpen;
extern std::atomic<unsigned long long> g_escPassUntil;  // GetTickCount64 deadline: an Escape sent by the custom UI reaches the game
extern std::atomic<bool> g_escBackRequest;   // real Escape pressed under the custom UI -> UI goes back   // startup disclaimer is on screen
extern std::atomic<int> g_shotRequest;  // dev: screenshot request counter
extern std::string g_shotName;

bool ConfigSetByName(const std::string& key, const std::string& value);
void CuiResetSettings();        // custom UI settings back to defaults (rules are kept)

// ---------- custom UI rules (render thread only) ----------
enum CuiPlace { CP_Auto = 0, CP_List, CP_Pinned, CP_More, CP_Hidden };
struct CuiElemRule {
    int place = CP_Auto;
    int order = 0;                 // lower = higher in the list
    bool accent = false;           // highlighted
    bool wide = false;             // full row in grid layouts
    std::string orig;              // the game's label (for the rules list)
    std::string label;             // custom label (empty = original)
};
struct CuiPageRule {
    int mode = -1;                 // -1 = global setting, otherwise like Config::cuiSubMode
    std::string title;
};
#include <map>
extern std::map<std::string, CuiElemRule> g_cuiRules;   // key: hash of kind|group|label
extern std::map<std::string, CuiPageRule> g_cuiPages;   // key: hash of the screen's main group

// ---------- JSON configs (render thread) ----------
struct ProfileInfo { std::string name, modified; };
std::string ConfigToJson(const std::string& name);
bool ConfigFromJson(const std::string& text, std::string* name, std::string* err);  // applies all settings
void ConfigResetAll();
std::vector<ProfileInfo> ProfileList();
bool ProfileExists(const std::string& name);
bool ProfileSave(const std::string& name, std::string* err);
bool ProfileLoad(const std::string& name, std::string* err);
bool ProfileRename(const std::string& from, const std::string& to, std::string* err);
bool ProfileDelete(const std::string& name);
bool ProfileImport(const std::string& text, const std::string& fallbackName, std::string* outName, std::string* err);
std::string ProfilesFolder();
std::string ProfileSafeName(const std::string& n);
bool ReadFileU8(const std::string& path, std::string& out);
bool WriteFileUtf8(const std::string& path, const std::string& data);

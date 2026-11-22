#include "unity.hpp"
#include "log.hpp"

namespace U {

Classes C{};

namespace {
const char* CORE = "UnityEngine.CoreModule.dll";
const char* PHYS = "UnityEngine.PhysicsModule.dll";
const char* AI = "UnityEngine.AIModule.dll";
const char* ANIM = "UnityEngine.AnimationModule.dll";
const char* UI = "UnityEngine.UI.dll";
const char* UIM = "UnityEngine.UIModule.dll";
const char* TMP = "Unity.TextMeshPro.dll";

struct Scene { int handle; };

const MethodInfo *m_UnityEventInvoke, *m_ToggleSetIsOn, *m_SliderSetValue,
    *m_DropdownSetValue, *m_UIDropdownSetValue, *m_Quit;

Il2CppString* (*f_GetName)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_GetTransform)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_GetGameObject)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_GetComponent)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
int (*f_GetLayer)(Il2CppObject*, const MethodInfo*);
bool (*f_ActiveInHierarchy)(Il2CppObject*, const MethodInfo*);
V3 (*f_GetPosition)(Il2CppObject*, const MethodInfo*);
void (*f_SetPosition)(Il2CppObject*, V3, const MethodInfo*);
V3 (*f_GetForward)(Il2CppObject*, const MethodInfo*);
V3 (*f_GetRight)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_GetParent)(Il2CppObject*, const MethodInfo*);
bool (*f_BehGetEnabled)(Il2CppObject*, const MethodInfo*);
void (*f_BehSetEnabled)(Il2CppObject*, bool, const MethodInfo*);
bool (*f_ColGetEnabled)(Il2CppObject*, const MethodInfo*);
void (*f_ColSetEnabled)(Il2CppObject*, bool, const MethodInfo*);
Bounds (*f_ColBounds)(Il2CppObject*, const MethodInfo*);
Bounds (*f_RendBounds)(Il2CppObject*, const MethodInfo*);
int (*f_CCMove)(Il2CppObject*, V3, const MethodInfo*);
Il2CppObject* (*f_CamMain)(const MethodInfo*);
V3 (*f_WorldToScreen)(Il2CppObject*, V3, const MethodInfo*);
int (*f_ScreenW)(const MethodInfo*);
int (*f_ScreenH)(const MethodInfo*);
float (*f_DeltaTime)(const MethodInfo*);
float (*f_CamGetFov)(Il2CppObject*, const MethodInfo*);
void (*f_CamSetFov)(Il2CppObject*, float, const MethodInfo*);
float (*f_GetTimeScale)(const MethodInfo*);
void (*f_SetTimeScale)(float, const MethodInfo*);
float (*f_PrefGetFloat)(Il2CppString*, float, const MethodInfo*);
void (*f_PrefSetFloat)(Il2CppString*, float, const MethodInfo*);
int (*f_PrefGetInt)(Il2CppString*, int, const MethodInfo*);
void (*f_PrefSetInt)(Il2CppString*, int, const MethodInfo*);
bool (*f_PrefHasKey)(Il2CppString*, const MethodInfo*);
void (*f_PrefDeleteKey)(Il2CppString*, const MethodInfo*);
void (*f_PrefSave)(const MethodInfo*);
int (*f_NameToLayer)(Il2CppString*, const MethodInfo*);
void (*f_AgentSetStopped)(Il2CppObject*, bool, const MethodInfo*);
bool (*f_AgentWarp)(Il2CppObject*, V3, const MethodInfo*);
void (*f_AnimSetSpeed)(Il2CppObject*, float, const MethodInfo*);
bool (*f_IsInteractable)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_ButtonOnClick)(Il2CppObject*, const MethodInfo*);
bool (*f_ToggleIsOn)(Il2CppObject*, const MethodInfo*);
float (*f_SliderValue)(Il2CppObject*, const MethodInfo*);
float (*f_SliderMin)(Il2CppObject*, const MethodInfo*);
float (*f_SliderMax)(Il2CppObject*, const MethodInfo*);
bool (*f_SliderWhole)(Il2CppObject*, const MethodInfo*);
int (*f_DropdownValue)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_DropdownOptions)(Il2CppObject*, const MethodInfo*);
int (*f_UIDropdownValue)(Il2CppObject*, const MethodInfo*);
Il2CppObject* (*f_UIDropdownOptions)(Il2CppObject*, const MethodInfo*);
Il2CppString* (*f_TMPGetText)(Il2CppObject*, const MethodInfo*);
void (*f_TMPSetText)(Il2CppObject*, Il2CppString*, const MethodInfo*);
Il2CppString* (*f_UITextGetText)(Il2CppObject*, const MethodInfo*);
void (*f_UITextSetText)(Il2CppObject*, Il2CppString*, const MethodInfo*);
bool (*f_CanvasIsRoot)(Il2CppObject*, const MethodInfo*);

Il2CppArray* (*i_FindObjectsOfType)(Il2CppObject*, bool);
int (*i_FrameCount)();
bool (*i_IsTrigger)(Il2CppObject*);
void (*i_GetActiveScene)(Scene*);
int (*i_BuildIndex)(int);
float (*i_CamDepth)(Il2CppObject*);
Il2CppArray* (*i_GetComponentsInternal)(Il2CppObject*, Il2CppObject*, bool, bool, bool, bool, Il2CppObject*);
struct LoadSceneParameters { int mode; int physics; };
Il2CppObject* (*i_LoadSceneAsync)(Il2CppString*, int, LoadSceneParameters*, bool);
int g_missing = 0;
template <typename T> void R(T& out, void* klass, const char* name, int argc) {
    out = reinterpret_cast<T>(il2::Fn(klass, name, argc));
    if (!out) ++g_missing;
}
} // namespace

bool Init() {
    C.Object = il2::Class(CORE, "UnityEngine", "Object");
    C.Component = il2::Class(CORE, "UnityEngine", "Component");
    C.GameObject = il2::Class(CORE, "UnityEngine", "GameObject");
    C.Transform = il2::Class(CORE, "UnityEngine", "Transform");
    C.Behaviour = il2::Class(CORE, "UnityEngine", "Behaviour");
    C.Renderer = il2::Class(CORE, "UnityEngine", "Renderer");
    C.SkinnedMeshRenderer = il2::Class(CORE, "UnityEngine", "SkinnedMeshRenderer");
    C.Camera = il2::Class(CORE, "UnityEngine", "Camera");
    C.Collider = il2::Class(PHYS, "UnityEngine", "Collider");
    C.CharacterController = il2::Class(PHYS, "UnityEngine", "CharacterController");
    C.NavMeshAgent = il2::Class(AI, "UnityEngine.AI", "NavMeshAgent");
    C.Animator = il2::Class(ANIM, "UnityEngine", "Animator");
    C.Selectable = il2::Class(UI, "UnityEngine.UI", "Selectable");
    C.Button = il2::Class(UI, "UnityEngine.UI", "Button");
    C.Toggle = il2::Class(UI, "UnityEngine.UI", "Toggle");
    C.Slider = il2::Class(UI, "UnityEngine.UI", "Slider");
    C.UIText = il2::Class(UI, "UnityEngine.UI", "Text");
    C.UIDropdown = il2::Class(UI, "UnityEngine.UI", "Dropdown");
    C.EventSystem = il2::Class(UI, "UnityEngine.EventSystems", "EventSystem");
    C.TMPText = il2::Class(TMP, "TMPro", "TMP_Text");
    C.TMPDropdown = il2::Class(TMP, "TMPro", "TMP_Dropdown");
    C.Canvas = il2::Class(UIM, "UnityEngine", "Canvas");

    void* time = il2::Class(CORE, "UnityEngine", "Time");
    void* screen = il2::Class(CORE, "UnityEngine", "Screen");
    void* prefs = il2::Class(CORE, "UnityEngine", "PlayerPrefs");
    void* layerMask = il2::Class(CORE, "UnityEngine", "LayerMask");
    void* sceneMgr = il2::Class(CORE, "UnityEngine.SceneManagement", "SceneManager");
    void* scene = il2::Class(CORE, "UnityEngine.SceneManagement", "Scene");
    void* app = il2::Class(CORE, "UnityEngine", "Application");
    void* unityEvent = il2::Class(CORE, "UnityEngine.Events", "UnityEvent");

    // Methods stripped from the managed build are called through their engine icalls instead.
    i_FindObjectsOfType = reinterpret_cast<decltype(i_FindObjectsOfType)>(il2::ResolveICall("UnityEngine.Object::FindObjectsOfType"));
    i_FrameCount = reinterpret_cast<decltype(i_FrameCount)>(il2::ResolveICall("UnityEngine.Time::get_frameCount"));
    i_IsTrigger = reinterpret_cast<decltype(i_IsTrigger)>(il2::ResolveICall("UnityEngine.Collider::get_isTrigger"));
    i_GetActiveScene = reinterpret_cast<decltype(i_GetActiveScene)>(il2::ResolveICall("UnityEngine.SceneManagement.SceneManager::GetActiveScene_Injected"));
    i_BuildIndex = reinterpret_cast<decltype(i_BuildIndex)>(il2::ResolveICall("UnityEngine.SceneManagement.Scene::GetBuildIndexInternal"));
    i_CamDepth = reinterpret_cast<decltype(i_CamDepth)>(il2::ResolveICall("UnityEngine.Camera::get_depth"));
    i_GetComponentsInternal = reinterpret_cast<decltype(i_GetComponentsInternal)>(il2::ResolveICall("UnityEngine.GameObject::GetComponentsInternal"));
    i_LoadSceneAsync = reinterpret_cast<decltype(i_LoadSceneAsync)>(il2::ResolveICall("UnityEngine.SceneManagement.SceneManagerAPIInternal::LoadSceneAsyncNameIndexInternal_Injected"));
    m_UnityEventInvoke = il2::Method(unityEvent, "Invoke", 0);
    m_ToggleSetIsOn = il2::Method(C.Toggle, "set_isOn", 1);
    m_SliderSetValue = il2::Method(C.Slider, "set_value", 1);
    m_DropdownSetValue = il2::Method(C.TMPDropdown, "set_value", 1);
    m_UIDropdownSetValue = il2::Method(C.UIDropdown, "set_value", 1);
    m_Quit = il2::Method(app, "Quit", 0);

    R(f_GetName, C.Object, "get_name", 0);
    R(f_GetTransform, C.Component, "get_transform", 0);
    R(f_GetGameObject, C.Component, "get_gameObject", 0);
    {
        static const char* t1[] = {"System.Type"};
        const MethodInfo* m = il2::MethodSig(C.Component, "GetComponent", t1, 1);
        f_GetComponent = m ? reinterpret_cast<decltype(f_GetComponent)>(m->methodPointer) : nullptr;
        if (!f_GetComponent) ++g_missing;
    }
    R(f_GetLayer, C.GameObject, "get_layer", 0);
    R(f_ActiveInHierarchy, C.GameObject, "get_activeInHierarchy", 0);
    R(f_GetPosition, C.Transform, "get_position", 0);
    R(f_SetPosition, C.Transform, "set_position", 1);
    R(f_GetForward, C.Transform, "get_forward", 0);
    R(f_GetRight, C.Transform, "get_right", 0);
    R(f_GetParent, C.Transform, "get_parent", 0);
    R(f_BehGetEnabled, C.Behaviour, "get_enabled", 0);
    R(f_BehSetEnabled, C.Behaviour, "set_enabled", 1);
    R(f_ColGetEnabled, C.Collider, "get_enabled", 0);
    R(f_ColSetEnabled, C.Collider, "set_enabled", 1);
    R(f_ColBounds, C.Collider, "get_bounds", 0);
    R(f_RendBounds, C.Renderer, "get_bounds", 0);
    R(f_CCMove, C.CharacterController, "Move", 1);
    R(f_CamMain, C.Camera, "get_main", 0);
    R(f_WorldToScreen, C.Camera, "WorldToScreenPoint", 1);
    R(f_ScreenW, screen, "get_width", 0);
    R(f_ScreenH, screen, "get_height", 0);
    R(f_DeltaTime, time, "get_unscaledDeltaTime", 0);
    R(f_CamGetFov, C.Camera, "get_fieldOfView", 0);
    R(f_CamSetFov, C.Camera, "set_fieldOfView", 1);
    R(f_GetTimeScale, time, "get_timeScale", 0);
    R(f_SetTimeScale, time, "set_timeScale", 1);
    R(f_PrefGetFloat, prefs, "GetFloat", 2);
    R(f_PrefSetFloat, prefs, "SetFloat", 2);
    R(f_PrefGetInt, prefs, "GetInt", 2);
    R(f_PrefSetInt, prefs, "SetInt", 2);
    R(f_PrefHasKey, prefs, "HasKey", 1);
    R(f_PrefDeleteKey, prefs, "DeleteKey", 1);
    R(f_PrefSave, prefs, "Save", 0);
    R(f_NameToLayer, layerMask, "NameToLayer", 1);
    R(f_AgentSetStopped, C.NavMeshAgent, "set_isStopped", 1);
    R(f_AgentWarp, C.NavMeshAgent, "Warp", 1);
    R(f_AnimSetSpeed, C.Animator, "set_speed", 1);
    R(f_IsInteractable, C.Selectable, "IsInteractable", 0);
    R(f_ButtonOnClick, C.Button, "get_onClick", 0);
    R(f_ToggleIsOn, C.Toggle, "get_isOn", 0);
    R(f_SliderValue, C.Slider, "get_value", 0);
    R(f_SliderMin, C.Slider, "get_minValue", 0);
    R(f_SliderMax, C.Slider, "get_maxValue", 0);
    R(f_SliderWhole, C.Slider, "get_wholeNumbers", 0);
    R(f_DropdownValue, C.TMPDropdown, "get_value", 0);
    R(f_DropdownOptions, C.TMPDropdown, "get_options", 0);
    R(f_UIDropdownValue, C.UIDropdown, "get_value", 0);
    R(f_UIDropdownOptions, C.UIDropdown, "get_options", 0);
    R(f_TMPGetText, C.TMPText, "get_text", 0);
    R(f_TMPSetText, C.TMPText, "set_text", 1);
    R(f_UITextGetText, C.UIText, "get_text", 0);
    R(f_UITextSetText, C.UIText, "set_text", 1);
    R(f_CanvasIsRoot, C.Canvas, "get_isRootCanvas", 0);
    LOG("unity wrappers resolved, missing=%d", g_missing);
    (void)sceneMgr; (void)scene;
    bool ok = f_GetTransform && f_GetPosition && i_FrameCount && i_FindObjectsOfType && i_GetComponentsInternal && f_GetComponent;
    if (!ok) LOG("core wrappers missing: tr=%d pos=%d frame=%d find=%d gcic=%d gc=%d", !!f_GetTransform, !!f_GetPosition, !!i_FrameCount, !!i_FindObjectsOfType, !!i_GetComponentsInternal, !!f_GetComponent);
    return ok;
}

Il2CppArray* FindObjectsOfType(void* klass) {
    Il2CppObject* t = il2::TypeObject(klass);
    if (!t) return nullptr;
    return i_FindObjectsOfType(t, false);
}

std::string Name(Il2CppObject* o) { return il2::Alive(o) && f_GetName ? il2::ToUtf8(f_GetName(o, nullptr)) : std::string(); }
Il2CppObject* TransformOf(Il2CppObject* c) { return il2::Alive(c) ? f_GetTransform(c, nullptr) : nullptr; }
Il2CppObject* GameObjectOf(Il2CppObject* c) { return il2::Alive(c) ? f_GetGameObject(c, nullptr) : nullptr; }
Il2CppObject* GetComponent(Il2CppObject* c, void* k) {
    if (!il2::Alive(c) || !k) return nullptr;
    Il2CppObject* r = f_GetComponent(c, il2::TypeObject(k), nullptr);
    return il2::Alive(r) ? r : nullptr;
}
Il2CppObject* GetComponentInChildren(Il2CppObject* c, void* k) {
    if (!il2::Alive(c) || !k) return nullptr;
    Il2CppArray* a = GetComponentsInChildren(c, k);
    for (int i = 0; i < il2::ArrayLen(a); ++i)
        if (il2::Alive((Il2CppObject*)a->items[i])) return (Il2CppObject*)a->items[i];
    return nullptr;
}
Il2CppArray* GetComponentsInChildren(Il2CppObject* c, void* k) {
    if (!il2::Alive(c) || !k || !i_GetComponentsInternal) return nullptr;
    Il2CppObject* go = GameObjectOf(c);
    if (!il2::Alive(go)) return nullptr;
    // (type, useSearchTypeAsArrayReturnType, recursive, includeInactive, reverse, resultList)
    return i_GetComponentsInternal(go, il2::TypeObject(k), true, true, false, false, nullptr);
}
int Layer(Il2CppObject* go) { return il2::Alive(go) ? f_GetLayer(go, nullptr) : -1; }
bool ActiveInHierarchy(Il2CppObject* go) { return il2::Alive(go) && f_ActiveInHierarchy(go, nullptr); }

V3 Pos(Il2CppObject* t) { return il2::Alive(t) ? f_GetPosition(t, nullptr) : V3{0, 0, 0}; }
void SetPos(Il2CppObject* t, V3 p) { if (il2::Alive(t)) f_SetPosition(t, p, nullptr); }
V3 Forward(Il2CppObject* t) { return il2::Alive(t) ? f_GetForward(t, nullptr) : V3{0, 0, 1}; }
V3 Right(Il2CppObject* t) { return il2::Alive(t) ? f_GetRight(t, nullptr) : V3{1, 0, 0}; }
Il2CppObject* Parent(Il2CppObject* t) { return il2::Alive(t) ? f_GetParent(t, nullptr) : nullptr; }

bool Enabled(Il2CppObject* b) { return il2::Alive(b) && f_BehGetEnabled(b, nullptr); }
void SetEnabled(Il2CppObject* b, bool on) { if (il2::Alive(b)) f_BehSetEnabled(b, on, nullptr); }
bool ColliderEnabled(Il2CppObject* c) { return il2::Alive(c) && f_ColGetEnabled(c, nullptr); }
void SetColliderEnabled(Il2CppObject* c, bool on) { if (il2::Alive(c)) f_ColSetEnabled(c, on, nullptr); }
bool ColliderIsTrigger(Il2CppObject* c) { return il2::Alive(c) && i_IsTrigger && i_IsTrigger(c); }
Bounds ColliderBounds(Il2CppObject* c) { return il2::Alive(c) ? f_ColBounds(c, nullptr) : Bounds{}; }
Bounds RendererBounds(Il2CppObject* r) { return il2::Alive(r) ? f_RendBounds(r, nullptr) : Bounds{}; }
void CCMove(Il2CppObject* cc, V3 m) { if (il2::Alive(cc) && f_CCMove) f_CCMove(cc, m, nullptr); }

// The game's player camera is not tagged MainCamera, so fall back to the enabled camera with the highest depth.
static uint32_t g_camHandle = 0;
static int g_camFrame = -1000;
Il2CppObject* MainCamera() {
    Il2CppObject* c = f_CamMain ? f_CamMain(nullptr) : nullptr;
    if (il2::Alive(c)) return c;
    int frame = FrameCount();
    Il2CppObject* cached = il2::GcGet(g_camHandle);
    if (il2::Alive(cached) && Enabled(cached) && frame - g_camFrame < 30) return cached;
    g_camFrame = frame;
    il2::GcFree(g_camHandle);
    g_camHandle = 0;
    Il2CppArray* a = FindObjectsOfType(C.Camera);
    Il2CppObject* best = nullptr;
    float bestDepth = -1e9f;
    for (int i = 0; i < il2::ArrayLen(a); ++i) {
        Il2CppObject* cam = (Il2CppObject*)a->items[i];
        if (!il2::Alive(cam) || !Enabled(cam) || !ActiveInHierarchy(GameObjectOf(cam))) continue;
        float d = i_CamDepth ? i_CamDepth(cam) : 0.f;
        if (d > bestDepth) { bestDepth = d; best = cam; }
    }
    if (best) g_camHandle = il2::GcNew(best);
    return best;
}
V3 WorldToScreen(Il2CppObject* cam, V3 p) { return il2::Alive(cam) ? f_WorldToScreen(cam, p, nullptr) : V3{0, 0, -1}; }
int ScreenW() { return f_ScreenW(nullptr); }
int ScreenH() { return f_ScreenH(nullptr); }
float DeltaTime() { return f_DeltaTime(nullptr); }
int FrameCount() { return i_FrameCount(); }
float CamFov(Il2CppObject* c) { return il2::Alive(c) && f_CamGetFov ? f_CamGetFov(c, nullptr) : 0.f; }
void SetCamFov(Il2CppObject* c, float f) { if (il2::Alive(c) && f_CamSetFov) f_CamSetFov(c, f, nullptr); }
float TimeScale() { return f_GetTimeScale ? f_GetTimeScale(nullptr) : 1.f; }
void SetTimeScale(float s) { if (f_SetTimeScale) f_SetTimeScale(s, nullptr); }

float PrefGetFloat(const char* k, float d) { return f_PrefGetFloat(il2::Str(k), d, nullptr); }
void PrefSetFloat(const char* k, float v) { f_PrefSetFloat(il2::Str(k), v, nullptr); }
int PrefGetInt(const char* k, int d) { return f_PrefGetInt(il2::Str(k), d, nullptr); }
void PrefSetInt(const char* k, int v) { f_PrefSetInt(il2::Str(k), v, nullptr); }
bool PrefHasKey(const char* k) { return f_PrefHasKey(il2::Str(k), nullptr); }
void PrefDeleteKey(const char* k) { f_PrefDeleteKey(il2::Str(k), nullptr); }
void PrefSave() { f_PrefSave(nullptr); }

int NameToLayer(const char* n) { return f_NameToLayer(il2::Str(n), nullptr); }
int ActiveSceneIndex() {
    if (!i_GetActiveScene || !i_BuildIndex) return -1;
    Scene sc{0};
    i_GetActiveScene(&sc);
    return i_BuildIndex(sc.handle);
}
void LoadScene(int index) {
    if (!i_LoadSceneAsync) return;
    LoadSceneParameters p{0, 0};  // Single, no local physics
    i_LoadSceneAsync(nullptr, index, &p, true);
}
void Quit() { il2::Invoke(m_Quit, nullptr, nullptr); }

void AgentStop(Il2CppObject* a, bool s) { if (il2::Alive(a) && f_AgentSetStopped && U::Enabled(a)) f_AgentSetStopped(a, s, nullptr); }
void AgentWarp(Il2CppObject* a, V3 p) { if (il2::Alive(a) && f_AgentWarp) f_AgentWarp(a, p, nullptr); }
void AnimatorSpeed(Il2CppObject* a, float s) { if (il2::Alive(a) && f_AnimSetSpeed) f_AnimSetSpeed(a, s, nullptr); }

bool Interactable(Il2CppObject* s) { return il2::Alive(s) && f_IsInteractable(s, nullptr); }
void ButtonClick(Il2CppObject* b) {
    if (!il2::Alive(b)) return;
    Il2CppObject* ev = f_ButtonOnClick(b, nullptr);
    if (ev) il2::Invoke(m_UnityEventInvoke, ev, nullptr);
}
bool ToggleIsOn(Il2CppObject* t) { return il2::Alive(t) && f_ToggleIsOn(t, nullptr); }
void ToggleSet(Il2CppObject* t, bool on) { if (il2::Alive(t)) { void* a[1] = {&on}; il2::Invoke(m_ToggleSetIsOn, t, a); } }
float SliderValue(Il2CppObject* s) { return il2::Alive(s) ? f_SliderValue(s, nullptr) : 0.f; }
void SliderSet(Il2CppObject* s, float v) { if (il2::Alive(s)) { void* a[1] = {&v}; il2::Invoke(m_SliderSetValue, s, a); } }
float SliderMin(Il2CppObject* s) { return il2::Alive(s) ? f_SliderMin(s, nullptr) : 0.f; }
float SliderMax(Il2CppObject* s) { return il2::Alive(s) ? f_SliderMax(s, nullptr) : 1.f; }
bool SliderWhole(Il2CppObject* s) { return il2::Alive(s) && f_SliderWhole(s, nullptr); }

static bool IsTMPDropdown(Il2CppObject* d) {
    return C.TMPDropdown && il2::IsSubclass(*reinterpret_cast<void**>(d), C.TMPDropdown);
}
int DropdownValue(Il2CppObject* d) {
    if (!il2::Alive(d)) return 0;
    return IsTMPDropdown(d) ? f_DropdownValue(d, nullptr) : f_UIDropdownValue(d, nullptr);
}
void DropdownSet(Il2CppObject* d, int v) {
    if (!il2::Alive(d)) return;
    void* a[1] = {&v};
    il2::Invoke(IsTMPDropdown(d) ? m_DropdownSetValue : m_UIDropdownSetValue, d, a);
}
int DropdownCount(Il2CppObject* d) {
    if (!il2::Alive(d)) return 0;
    Il2CppObject* list = IsTMPDropdown(d) ? f_DropdownOptions(d, nullptr) : f_UIDropdownOptions(d, nullptr);
    return list ? il2::At<int>(list, 0x18) : 0;  // List<T>._size
}

std::string TextOf(Il2CppObject* t) {
    if (!il2::Alive(t)) return {};
    void* k = *reinterpret_cast<void**>(t);
    if (C.TMPText && il2::IsSubclass(k, C.TMPText)) return il2::ToUtf8(f_TMPGetText(t, nullptr));
    if (C.UIText && il2::IsSubclass(k, C.UIText)) return il2::ToUtf8(f_UITextGetText(t, nullptr));
    return {};
}
void SetTextOf(Il2CppObject* t, const char* s) {
    if (!il2::Alive(t)) return;
    void* k = *reinterpret_cast<void**>(t);
    if (C.TMPText && il2::IsSubclass(k, C.TMPText)) f_TMPSetText(t, il2::Str(s), nullptr);
    else if (C.UIText && il2::IsSubclass(k, C.UIText)) f_UITextSetText(t, il2::Str(s), nullptr);
}
std::string LabelFor(Il2CppObject* comp) {
    Il2CppObject* t = GetComponentInChildren(comp, C.TMPText);
    std::string s = t ? TextOf(t) : std::string();
    if (s.empty()) { t = GetComponentInChildren(comp, C.UIText); if (t) s = TextOf(t); }
    return s;
}
bool CanvasIsRoot(Il2CppObject* c) { return il2::Alive(c) && f_CanvasIsRoot && f_CanvasIsRoot(c, nullptr); }

} // namespace U

// ZenClient - thin wrappers over Unity engine methods (call only from the Unity main thread).
#pragma once
#include "il2cpp.hpp"
#include <string>
#include <cmath>

namespace U {

struct V3 { float x, y, z; };
struct Bounds { V3 center, extents; };

inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Len(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
inline V3 Norm(V3 a) { float l = Len(a); return l > 1e-5f ? a * (1.0f / l) : V3{0, 0, 0}; }

// Classes used across the client.
struct Classes {
    void *Object, *Component, *GameObject, *Transform, *Behaviour, *Collider, *CharacterController,
         *Renderer, *SkinnedMeshRenderer, *Camera, *NavMeshAgent, *Animator, *Selectable, *Button,
         *Toggle, *Slider, *UIText, *TMPText, *TMPDropdown, *UIDropdown, *Canvas, *EventSystem;
};
extern Classes C;

bool Init();

Il2CppArray* FindObjectsOfType(void* klass);
std::string Name(Il2CppObject* obj);

Il2CppObject* TransformOf(Il2CppObject* comp);
Il2CppObject* GameObjectOf(Il2CppObject* comp);
Il2CppObject* GetComponent(Il2CppObject* comp, void* klass);
Il2CppObject* GetComponentInChildren(Il2CppObject* comp, void* klass);
Il2CppArray* GetComponentsInChildren(Il2CppObject* comp, void* klass);

int Layer(Il2CppObject* go);
bool ActiveInHierarchy(Il2CppObject* go);

V3 Pos(Il2CppObject* tr);
void SetPos(Il2CppObject* tr, V3 p);
V3 Forward(Il2CppObject* tr);
V3 Right(Il2CppObject* tr);
Il2CppObject* Parent(Il2CppObject* tr);

bool Enabled(Il2CppObject* behaviour);
void SetEnabled(Il2CppObject* behaviour, bool on);
bool ColliderEnabled(Il2CppObject* col);
void SetColliderEnabled(Il2CppObject* col, bool on);
bool ColliderIsTrigger(Il2CppObject* col);
Bounds ColliderBounds(Il2CppObject* col);
Bounds RendererBounds(Il2CppObject* r);
void CCMove(Il2CppObject* cc, V3 motion);

Il2CppObject* MainCamera();
V3 WorldToScreen(Il2CppObject* cam, V3 p);
int ScreenW();
int ScreenH();
float DeltaTime();
int FrameCount();
float CamFov(Il2CppObject* cam);
void SetCamFov(Il2CppObject* cam, float fov);
float TimeScale();
void SetTimeScale(float s);

float PrefGetFloat(const char* k, float def);
void PrefSetFloat(const char* k, float v);
int PrefGetInt(const char* k, int def);
void PrefSetInt(const char* k, int v);
bool PrefHasKey(const char* k);
void PrefDeleteKey(const char* k);
void PrefSave();

int NameToLayer(const char* name);
int ActiveSceneIndex();
void LoadScene(int index);
void Quit();

void AgentStop(Il2CppObject* agent, bool stop);
void AgentWarp(Il2CppObject* agent, V3 p);
void AnimatorSpeed(Il2CppObject* anim, float s);

// UI
bool Interactable(Il2CppObject* selectable);
void ButtonClick(Il2CppObject* button);
bool ToggleIsOn(Il2CppObject* t);
void ToggleSet(Il2CppObject* t, bool on);
float SliderValue(Il2CppObject* s);
void SliderSet(Il2CppObject* s, float v);
float SliderMin(Il2CppObject* s);
float SliderMax(Il2CppObject* s);
bool SliderWhole(Il2CppObject* s);
int DropdownValue(Il2CppObject* d);
void DropdownSet(Il2CppObject* d, int v);
int DropdownCount(Il2CppObject* d);
std::string TextOf(Il2CppObject* textComponent);   // TMP_Text or UI.Text
void SetTextOf(Il2CppObject* textComponent, const char* s);
std::string LabelFor(Il2CppObject* comp);           // first text in children
bool CanvasIsRoot(Il2CppObject* canvas);

} // namespace U

// ZenClient - minimal IL2CPP runtime API layer.
// Everything is resolved by name at runtime, so no hardcoded addresses are used.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

struct Il2CppObject { void* klass; void* monitor; };
struct Il2CppString { Il2CppObject obj; int32_t length; char16_t chars[1]; };
struct Il2CppArray { Il2CppObject obj; void* bounds; uintptr_t max_length; void* items[1]; };
struct MethodInfo { void* methodPointer; void* virtualMethodPointer; void* invoker; const char* name; void* klass; };

namespace il2 {

bool Init();                 // waits for GameAssembly + domain, resolves API
void AttachThread();         // attach current thread to the IL2CPP domain
void DetachThread();

void* Class(const char* image, const char* ns, const char* name);
const MethodInfo* Method(void* klass, const char* name, int argc);
const MethodInfo* MethodSig(void* klass, const char* name, const char* const* params, int argc);  // exact parameter types
void* Fn(void* klass, const char* name, int argc);          // methodPointer or nullptr
int FieldOffset(void* klass, const char* name);              // -1 if missing
bool MethodReturnsVoid(const MethodInfo* m);
const char* ClassName(void* klass);
const char* ObjClassName(Il2CppObject* o);
bool IsSubclass(void* klass, void* parent);

Il2CppString* Str(const char* utf8);
std::string ToUtf8(Il2CppString* s);
Il2CppObject* TypeObject(void* klass);                       // System.Type for a class

// Safe managed call (exceptions are caught by the runtime).
Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args, bool* ok = nullptr);
void* Unbox(Il2CppObject* o);

uint32_t GcNew(Il2CppObject* o);
Il2CppObject* GcGet(uint32_t h);
void GcFree(uint32_t h);

void* ResolveICall(const char* name);
void DumpMethods(const char* image, const char* path);
void DumpFields(void* klass, const char* path);
void DumpObject(Il2CppObject* o, const char* path);

// UnityEngine.Object alive check (m_CachedPtr at 0x10).
inline bool Alive(Il2CppObject* o) {
    return o && *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(o) + 0x10) != nullptr;
}
template <typename T> inline T& At(void* obj, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + off);
}
inline int ArrayLen(Il2CppArray* a) { return a ? (int)a->max_length : 0; }

} // namespace il2

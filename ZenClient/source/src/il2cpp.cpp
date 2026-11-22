#include "il2cpp.hpp"
#include "log.hpp"
#include <cstring>
#include <unordered_map>
#include <mutex>

namespace il2 {

#define API(ret, name, args) using name##_t = ret(*) args; static name##_t name = nullptr;
API(void*, il2cpp_domain_get, ())
API(void**, il2cpp_domain_get_assemblies, (void* domain, size_t* size))
API(void*, il2cpp_assembly_get_image, (void* assembly))
API(const char*, il2cpp_image_get_name, (void* image))
API(void*, il2cpp_class_from_name, (void* image, const char* ns, const char* name))
API(const MethodInfo*, il2cpp_class_get_method_from_name, (void* klass, const char* name, int argc))
API(void*, il2cpp_class_get_field_from_name, (void* klass, const char* name))
API(size_t, il2cpp_field_get_offset, (void* field))
API(void*, il2cpp_thread_attach, (void* domain))
API(Il2CppString*, il2cpp_string_new, (const char* s))
API(Il2CppObject*, il2cpp_runtime_invoke, (const MethodInfo* m, void* obj, void** params, Il2CppObject** exc))
API(void*, il2cpp_class_get_type, (void* klass))
API(Il2CppObject*, il2cpp_type_get_object, (void* type))
API(const char*, il2cpp_class_get_name, (void* klass))
API(void*, il2cpp_object_get_class, (Il2CppObject* o))
API(void*, il2cpp_object_unbox, (Il2CppObject* o))
API(uint32_t, il2cpp_gchandle_new, (Il2CppObject* o, bool pinned))
API(Il2CppObject*, il2cpp_gchandle_get_target, (uint32_t h))
API(void, il2cpp_gchandle_free, (uint32_t h))
API(void*, il2cpp_resolve_icall, (const char* name))
API(void*, il2cpp_method_get_return_type, (const MethodInfo* m))
API(char*, il2cpp_type_get_name, (void* type))
API(void, il2cpp_free, (void* p))
API(bool, il2cpp_class_is_subclass_of, (void* klass, void* parent, bool checkInterfaces))
API(void*, il2cpp_thread_current, ())
API(void*, il2cpp_get_corlib, ())
API(const MethodInfo*, il2cpp_class_get_methods, (void* klass, void** iter))
API(const char*, il2cpp_method_get_name, (const MethodInfo* m))
API(uint32_t, il2cpp_method_get_param_count, (const MethodInfo* m))
API(void*, il2cpp_method_get_param, (const MethodInfo* m, uint32_t i))
API(bool, il2cpp_method_is_generic, (const MethodInfo* m))
API(void*, il2cpp_class_get_parent, (void* klass))
API(size_t, il2cpp_image_get_class_count, (void* image))
API(void*, il2cpp_class_get_fields, (void* klass, void** iter))
API(const char*, il2cpp_field_get_name, (void* field))
API(void*, il2cpp_image_get_class, (void* image, size_t i))
API(void, il2cpp_thread_detach, (void* thread))
API(void*, il2cpp_field_get_type, (void* field))
API(void*, il2cpp_class_get_nested_types, (void* klass, void** iter))
API(Il2CppObject*, il2cpp_object_new, (void* klass))
#undef API

static void* g_domain = nullptr;

template <typename T> static bool Load(HMODULE m, T& fn, const char* name) {
    fn = reinterpret_cast<T>(GetProcAddress(m, name));
    if (!fn) LOG("il2cpp: missing export %s", name);
    return fn != nullptr;
}

bool Init() {
    HMODULE ga = nullptr;
    for (int i = 0; i < 1200 && !(ga = GetModuleHandleW(L"GameAssembly.dll")); ++i) Sleep(100);
    if (!ga) { LOG("GameAssembly.dll never loaded"); return false; }
    bool ok = true;
#define L(n) ok &= Load(ga, n, #n)
    L(il2cpp_domain_get); L(il2cpp_domain_get_assemblies); L(il2cpp_assembly_get_image);
    L(il2cpp_image_get_name); L(il2cpp_class_from_name); L(il2cpp_class_get_method_from_name);
    L(il2cpp_class_get_field_from_name); L(il2cpp_field_get_offset); L(il2cpp_thread_attach);
    L(il2cpp_string_new); L(il2cpp_runtime_invoke); L(il2cpp_class_get_type); L(il2cpp_type_get_object);
    L(il2cpp_class_get_name); L(il2cpp_object_get_class); L(il2cpp_object_unbox); L(il2cpp_gchandle_new);
    L(il2cpp_gchandle_get_target); L(il2cpp_gchandle_free); L(il2cpp_resolve_icall);
    L(il2cpp_method_get_return_type); L(il2cpp_type_get_name); L(il2cpp_free); L(il2cpp_class_is_subclass_of);
    L(il2cpp_thread_current); L(il2cpp_thread_detach); L(il2cpp_get_corlib);
    L(il2cpp_class_get_methods); L(il2cpp_method_get_name); L(il2cpp_method_get_param_count);
    L(il2cpp_method_get_param); L(il2cpp_method_is_generic); L(il2cpp_class_get_parent);
    L(il2cpp_image_get_class_count); L(il2cpp_class_get_fields); L(il2cpp_field_get_name); L(il2cpp_image_get_class);
    L(il2cpp_field_get_type); L(il2cpp_class_get_nested_types); L(il2cpp_object_new);
#undef L
    if (!ok) return false;
    // il2cpp_domain_get() lazily creates the domain, so calling it before the runtime has started
    // corrupts initialisation. il2cpp_get_corlib() is a plain getter that becomes non-null during
    // il2cpp_init; wait for it, then give the runtime time to finish starting up.
    int waited = 0;
    while (!il2cpp_get_corlib()) { Sleep(50); if (++waited > 2400) { LOG("il2cpp runtime never started"); return false; } }
    Sleep(waited > 0 ? 2500 : 300);
    for (int i = 0; i < 1200; ++i) {
        g_domain = il2cpp_domain_get();
        if (g_domain) {
            size_t n = 0;
            void** as = il2cpp_domain_get_assemblies(g_domain, &n);
            for (size_t k = 0; as && k < n; ++k) {
                const char* nm = il2cpp_image_get_name(il2cpp_assembly_get_image(as[k]));
                if (nm && strcmp(nm, "Assembly-CSharp.dll") == 0) { LOG("il2cpp ready (%zu assemblies)", n); return true; }
            }
        }
        Sleep(100);
    }
    LOG("il2cpp domain not ready in time");
    return false;
}

void AttachThread() { if (g_domain) il2cpp_thread_attach(g_domain); }
void DetachThread() { if (void* t = il2cpp_thread_current()) il2cpp_thread_detach(t); }

static std::mutex g_cacheMtx;
static std::unordered_map<std::string, void*> g_images;

static void* Image(const char* name) {
    std::lock_guard<std::mutex> lk(g_cacheMtx);
    auto it = g_images.find(name);
    if (it != g_images.end()) return it->second;
    size_t n = 0;
    void** as = il2cpp_domain_get_assemblies(g_domain, &n);
    for (size_t k = 0; k < n; ++k) {
        void* img = il2cpp_assembly_get_image(as[k]);
        const char* nm = il2cpp_image_get_name(img);
        if (nm && strcmp(nm, name) == 0) { g_images[name] = img; return img; }
    }
    return nullptr;
}

void* Class(const char* image, const char* ns, const char* name) {
    void* img = Image(image);
    void* k = img ? il2cpp_class_from_name(img, ns, name) : nullptr;
    if (!k) LOG("class not found: %s %s.%s", image, ns, name);
    return k;
}

const MethodInfo* Method(void* klass, const char* name, int argc) {
    if (!klass) return nullptr;
    const MethodInfo* m = il2cpp_class_get_method_from_name(klass, name, argc);
    if (!m) LOG("method not found: %s.%s/%d", il2cpp_class_get_name(klass), name, argc);
    return m;
}

const MethodInfo* MethodSig(void* klass, const char* name, const char* const* params, int argc) {
    for (void* k = klass; k; k = il2cpp_class_get_parent(k)) {
        void* it = nullptr;
        while (const MethodInfo* m = il2cpp_class_get_methods(k, &it)) {
            if (strcmp(il2cpp_method_get_name(m), name) != 0 || (int)il2cpp_method_get_param_count(m) != argc || il2cpp_method_is_generic(m)) continue;
            bool match = true;
            for (int i = 0; i < argc && match; ++i) {
                char* tn = il2cpp_type_get_name(il2cpp_method_get_param(m, i));
                match = tn && strcmp(tn, params[i]) == 0;
                if (tn) il2cpp_free(tn);
            }
            if (match) return m;
        }
    }
    LOG("method not found: %s.%s(%s%s)", klass ? il2cpp_class_get_name(klass) : "?", name, argc ? params[0] : "", argc > 1 ? ",..." : "");
    return nullptr;
}

void* Fn(void* klass, const char* name, int argc) {
    const MethodInfo* m = Method(klass, name, argc);
    return m ? m->methodPointer : nullptr;
}

void* Nested(void* outer, const char* prefix) {
    if (!outer) return nullptr;
    size_t n = strlen(prefix);
    void* it = nullptr;
    while (void* k = il2cpp_class_get_nested_types(outer, &it)) {
        const char* nm = il2cpp_class_get_name(k);
        if (nm && strncmp(nm, prefix, n) == 0) return k;
    }
    LOG("nested class not found: %s/%s*", il2cpp_class_get_name(outer), prefix);
    return nullptr;
}

int FieldOffset(void* klass, const char* name) {
    if (!klass) return -1;
    void* f = il2cpp_class_get_field_from_name(klass, name);
    if (!f) { LOG("field not found: %s.%s", il2cpp_class_get_name(klass), name); return -1; }
    return (int)il2cpp_field_get_offset(f);
}

bool MethodReturnsVoid(const MethodInfo* m) {
    if (!m) return false;
    char* n = il2cpp_type_get_name(il2cpp_method_get_return_type(m));
    bool v = n && strcmp(n, "System.Void") == 0;
    if (n) il2cpp_free(n);
    return v;
}

const char* ClassName(void* klass) { return klass ? il2cpp_class_get_name(klass) : ""; }
const char* ObjClassName(Il2CppObject* o) { return o ? il2cpp_class_get_name(il2cpp_object_get_class(o)) : ""; }
bool IsSubclass(void* klass, void* parent) { return klass && parent && (klass == parent || il2cpp_class_is_subclass_of(klass, parent, false)); }

Il2CppString* Str(const char* s) { return il2cpp_string_new(s); }

std::string ToUtf8(Il2CppString* s) {
    if (!s || s->length <= 0) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(s->chars), s->length, nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(s->chars), s->length, out.data(), n, nullptr, nullptr);
    return out;
}

Il2CppObject* New(void* klass) { return klass ? il2cpp_object_new(klass) : nullptr; }

Il2CppObject* TypeObject(void* klass) { return klass ? il2cpp_type_get_object(il2cpp_class_get_type(klass)) : nullptr; }

Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args, bool* ok) {
    if (ok) *ok = false;
    if (!m) return nullptr;
    Il2CppObject* exc = nullptr;
    Il2CppObject* r = il2cpp_runtime_invoke(m, obj, args, &exc);
    if (exc) { LOG("managed exception in %s (%s)", m->name ? m->name : "?", ObjClassName(exc)); return nullptr; }
    if (ok) *ok = true;
    return r;
}

void* Unbox(Il2CppObject* o) { return o ? il2cpp_object_unbox(o) : nullptr; }

uint32_t GcNew(Il2CppObject* o) { return o ? il2cpp_gchandle_new(o, false) : 0; }
Il2CppObject* GcGet(uint32_t h) { return h ? il2cpp_gchandle_get_target(h) : nullptr; }
void GcFree(uint32_t h) { if (h) il2cpp_gchandle_free(h); }

void* ResolveICall(const char* name) {
    void* p = il2cpp_resolve_icall(name);
    if (!p) LOG("icall not found: %s", name);
    return p;
}

} // namespace il2

namespace il2 {
void DumpMethods(const char* image, const char* path) {
    void* img = Image(image);
    FILE* f = fopen(path, "w");
    if (!img || !f) { if (f) fclose(f); return; }
    uintptr_t base = (uintptr_t)GetModuleHandleW(L"GameAssembly.dll");
    size_t n = il2cpp_image_get_class_count(img);
    for (size_t i = 0; i < n; ++i) {
        void* k = il2cpp_image_get_class(img, i);
        void* it = nullptr;
        while (const MethodInfo* m = il2cpp_class_get_methods(k, &it))
            if (m->methodPointer) fprintf(f, "%llx %s.%s\n", (unsigned long long)((uintptr_t)m->methodPointer - base), il2cpp_class_get_name(k), il2cpp_method_get_name(m));
    }
    fclose(f);
}
void DumpFields(void* klass, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f || !klass) { if (f) fclose(f); return; }
    void* it = nullptr;
    while (void* fl = il2cpp_class_get_fields(klass, &it)) fprintf(f, "%zx %s\n", il2cpp_field_get_offset(fl), il2cpp_field_get_name(fl));
    fclose(f);
}

// Instance fields of an object with types and (for simple types) values.
void DumpObject(Il2CppObject* o, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f || !o) { if (f) fclose(f); return; }
    for (void* k = o->klass; k; k = il2cpp_class_get_parent(k)) {
        fprintf(f, "== %s\n", il2cpp_class_get_name(k));
        void* it = nullptr;
        while (void* fl = il2cpp_class_get_fields(k, &it)) {
            size_t off = il2cpp_field_get_offset(fl);
            char* tn = il2cpp_type_get_name(il2cpp_field_get_type(fl));
            std::string t = tn ? tn : "?";
            if (tn) il2cpp_free(tn);
            if (off == 0 || off > 0x4000) { fprintf(f, "  static %s %s\n", t.c_str(), il2cpp_field_get_name(fl)); continue; }
            char val[160] = "";
            uint8_t* p = reinterpret_cast<uint8_t*>(o) + off;
            if (t == "System.Single") snprintf(val, sizeof val, "%.3f", *reinterpret_cast<float*>(p));
            else if (t == "System.Int32") snprintf(val, sizeof val, "%d", *reinterpret_cast<int*>(p));
            else if (t == "System.Boolean") snprintf(val, sizeof val, "%d", *p);
            else if (t == "System.String") { Il2CppString* s = *reinterpret_cast<Il2CppString**>(p); snprintf(val, sizeof val, "\"%s\"", s ? ToUtf8(s).c_str() : "null"); }
            else if (t == "UnityEngine.Vector3") { float* v = reinterpret_cast<float*>(p); snprintf(val, sizeof val, "(%.2f %.2f %.2f)", v[0], v[1], v[2]); }
            else if (t.find('.') != std::string::npos && t.rfind("System.", 0) != 0) {
                Il2CppObject* r = *reinterpret_cast<Il2CppObject**>(p);
                if (t.find("[]") != std::string::npos) snprintf(val, sizeof val, "len=%d", r ? (int)reinterpret_cast<Il2CppArray*>(r)->max_length : -1);
                else snprintf(val, sizeof val, "%s", r ? (Alive(r) ? "obj" : "dead/plain") : "null");
            }
            fprintf(f, "  %4zx %-40s %-28s %s\n", off, t.c_str(), il2cpp_field_get_name(fl), val);
        }
    }
    fclose(f);
}
}

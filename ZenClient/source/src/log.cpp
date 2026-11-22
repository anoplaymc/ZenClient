#include "log.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>

namespace zlog {
static FILE* g_file = nullptr;
static std::mutex g_mtx;

void Init(const std::string& path) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_file) return;
    g_file = _wfopen(std::wstring(path.begin(), path.end()).c_str(), L"w");
}

void Write(const char* fmt, ...) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (!g_file) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_file, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    vfprintf(g_file, fmt, ap);
    va_end(ap);
    fputc('\n', g_file);
    fflush(g_file);
}
}

void ZenImAssert(const char* expr, const char* file, int line) {
    static int count = 0;
    if (++count <= 50) zlog::Write("IMGUI ASSERT %s (%s:%d)", expr, file, line);
}

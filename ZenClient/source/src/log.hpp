#pragma once
#include <string>

namespace zlog {
void Init(const std::string& path);
void Write(const char* fmt, ...);
}
#define LOG(...) zlog::Write(__VA_ARGS__)

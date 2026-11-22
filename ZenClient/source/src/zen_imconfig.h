// ImGui configuration for ZenClient: assertion failures are logged instead of aborting the game.
#pragma once
void ZenImAssert(const char* expr, const char* file, int line);
#define IM_ASSERT(_EXPR) do { if (!(_EXPR)) ZenImAssert(#_EXPR, __FILE__, __LINE__); } while (0)

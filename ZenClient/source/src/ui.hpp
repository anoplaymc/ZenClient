#pragma once

void UiInit(float width, float height);
void UiOnResize(float width, float height);
void UiPreFrame();            // before ImGui::NewFrame (font rebuilds)
void UiFrame(float dt);
int UiMenuKeyVk();
const char* UiMenuKeyName();
bool UiWantsInput();          // ZenClient menu open or the custom main-menu UI is shown
void UiSetMenuOpen(bool open);
bool UiCustomActive();        // the custom main-menu UI is on screen and interactive

#pragma once
#include <string>
#include "state.hpp"

bool FeaturesInit();            // call from a thread attached to il2cpp, after MinHook init

// UI-thread entry points (they post work to the Unity main thread)
void ActionGiveCoins(double amount, bool set);  // set=true: balance = amount
void ActionUiPress(const UiElem& e, int dir = 0, float value = 0.f);  // dir: dropdown step

// Render-thread hooks for the dev command channel
void DevPostRender(const std::string& cmd);
bool DevPopRender(std::string& out);

#pragma once

namespace ControlPanelAction {
bool Register();
void Unregister();
void OpenOrFocus(const char* tabName = nullptr);
}

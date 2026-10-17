// SPDX-License-Identifier: GPL-3.0-or-later
// ThemeChanger: automatic startup of the bundled ZiZc3 LiveShell engine.
// ELF remains a separate payload process, since it must inject into SceShellUI.
#pragma once
#include <string>
namespace engine {
void start_if_needed(const std::string& root, bool force=false);
bool busy();
std::string status();
}

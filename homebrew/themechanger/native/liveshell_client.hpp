// SPDX-License-Identifier: GPL-3.0-or-later
// LiveShell Engine (ZiZc3) HTTP API adapter for ThemeChanger.
// Deliberately does not inject or modify SceShellUI. Requires a running LiveShell payload.
#pragma once
#include <string>
namespace theme {
constexpr const char* kDataRoot="/data/ThemeChanger";
constexpr const char* kLiveRoot="/data/LiveShell/video";
constexpr int kCount=12;
extern const char* const kNames[kCount];
std::string theme_file(int index);
bool install_and_select(int index,const std::string& assets,std::string& result);
bool send_setting(const std::string& route,std::string& result);
struct PlaybackState {
 bool connected=false,enabled=false,mainVideo=true,gamePause=false,mainVideoSupported=false;
 std::string file, shellStatus, game, engineRevision;
};
PlaybackState playback_state();
bool probe();
bool activate_file(const std::string& filename,std::string& status);
int current_theme();
std::string current_file();
int load_selected();
void save_selected(int index);
}

// GPL-3.0-or-later. Host test against mocked LiveShell REST port 9050.
#include "liveshell_client.hpp"
#include <cassert>
#include <cstdio>
#include <string>
int main(){
 auto before=theme::playback_state();
 assert(before.connected);
 assert(before.engineRevision=="0.3.6");
 assert(!before.mainVideo);
 assert(before.file=="ThemeChanger-baseball.mp4");
 std::string message;
 assert(theme::activate_file("MotionBGS-neon.mp4",message));
 auto after=theme::playback_state();
 assert(after.connected&&after.enabled&&after.mainVideo&&after.engineRevision=="0.3.6");
 assert(after.file=="MotionBGS-neon.mp4");
 assert(after.shellStatus=="playing");
 std::printf("Mock LiveShell select and mainvideo verified: %s\n",message.c_str());
 return 0;
}

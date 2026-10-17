#!/usr/bin/env python3
"""Patch pinned ZiZc3 LiveShell source for standalone ThemeChanger use.

GPL-3.0 upstream remains attributed. Changes:
- PS5 Homebrew ThemeChanger isn't a game: don't pause wallpaper for PPSA99891
- No separate LiveShell Media tile: never install LVSH00001 on startup
- Keep wallpaper visible when PS Home opens while ThemeChanger is still running
No patch to ShellUI injection, filesystem mount, or playback pipeline.
"""
from pathlib import Path
import sys
if len(sys.argv)!=2:raise SystemExit("usage: patch_liveshell.py <LiveShell source dir>")
p=Path(sys.argv[1])/"src/liveshell_daemon.cpp"
s=p.read_text()
a='strcmp(id, "LVSH00001") != 0;'
b='strcmp(id, "LVSH00001") != 0 && strcmp(id, "PPSA99891") != 0;'
if s.count(a)!=1:raise SystemExit("Upstream LiveShell game detection has changed: refusing unsafe build")
s=s.replace(a,b,1)
a='start_thread(after_start_thread);'
b='log("ThemeChanger integration: LiveShell Media tile disabled (native app PPSA99891 used instead)");'
if s.count(a)!=1:raise SystemExit("Upstream LiveShell tile launch has changed: refusing unsafe build")
s=s.replace(a,b,1)
# Mirror the ThemeChanger running-app state to the injected ShellUI.
# A PS-button jump to Home often leaves the app alive and focus==Game;
# upstream ShellUI fades out its video in that state until the app exits.
marker='constexpr const char* kOurMarker      = "/system_tmp/liveshell/injected";'
if s.count(marker)!=1:raise SystemExit("LiveShell marker declaration changed")
s=s.replace(marker,marker+'\nconstexpr const char* kThemeChangerActive = "/system_tmp/liveshell/themechanger_active";',1)
marker='void* game_thread(void*) {\n\tbool last = false;'
if s.count(marker)!=1:raise SystemExit("LiveShell game_thread changed")
s=s.replace(marker,'void* game_thread(void*) {\n\tbool last = false;\n\tbool last_themechanger = false;',1)
marker='\t\tif (now == last) {\n\t\t\tcontinue;\n\t\t}'
if s.count(marker)!=1:raise SystemExit("LiveShell game state loop changed")
replacement='''\t\t// The home screen may be opened without closing ThemeChanger.
\t\t// Keep a ShellUI-readable marker until the app is really closed.
\t\tconst bool themechanger = strcmp(id, "PPSA99891") == 0;
\t\tif (themechanger != last_themechanger) {
\t\t\tlast_themechanger = themechanger;
\t\t\tif (themechanger) write_text(kThemeChangerActive, "1\\n");
\t\t\telse unlink(kThemeChangerActive);
\t\t\tlog("ThemeChanger running app %s (home focus exception)", themechanger ? "active" : "closed");
\t\t}
'''
s=s.replace(marker,replacement+marker,1)
marker='\tmkdir("/system_tmp/liveshell", 0777);'
if s.count(marker)!=1:raise SystemExit("LiveShell startup directory changed")
s=s.replace(marker,marker+'\n\tunlink(kThemeChangerActive); // discard a marker left by an older engine instance',1)
p.write_text(s)

shell=Path(sys.argv[1])/"src/liveshell_shellui.cpp"
ui=shell.read_text()
marker='\tconst bool game    = g_game_val >= 0 && focus == g_game_val;'
if ui.count(marker)!=1:raise SystemExit("LiveShell ShellUI focus logic changed")
replacement='''\t// ThemeChanger is still alive when the user presses PS to open Home.
\t// Sony can report FocusLayer.Game until the app is CLOSED; upstream's
\t// unconditional game fade hid our video even though the daemon no longer
\t// treats PPSA99891 as a game. Preserve the hide for every other title.
\tstatic int tc_marker_tick=-100;
\tstatic bool tc_app_active=false;
\tif (g_tick - tc_marker_tick >= 12) {
\t\ttc_marker_tick=g_tick;
\t\ttc_app_active=access("/system_tmp/liveshell/themechanger_active",F_OK)==0;
\t}
\tconst bool game = g_game_val >= 0 && focus == g_game_val && !tc_app_active;
'''
ui=ui.replace(marker,replacement,1)
shell.write_text(ui)
# Make the live API identify the precise integrated payload revision.
# /api/state is also available for older LiveShell, so API availability alone
# is not evidence that the current daemon contains the ThemeChanger fixes.
web=Path(sys.argv[1])/"src/liveshell_web.cpp"
ws=web.read_text()
old='std::string j  = std::string("{\\"enabled\\":")'
new='std::string j  = std::string("{\\"themechanger_engine\\":\\"0.3.6\\",\\"enabled\\":")'
if ws.count(old)!=1:raise SystemExit("LiveShell state JSON layout changed")
ws=ws.replace(old,new,1)
web.write_text(ws)
print("Verified: /api/state advertises ThemeChanger engine 0.3.6")

assert 'tc_app_active=access("/system_tmp/liveshell/themechanger_active",F_OK)==0;' in ui
assert 'strcmp(id, "PPSA99891") == 0' in s
print("Verified: ShellUI preserves wallpaper while ThemeChanger is backgrounded in PS5 Home")

assert "strcmp(id, \"PPSA99891\") != 0" in s
assert 'start_thread(after_start_thread);' not in s
print("Verified: ThemeChanger is exempt from game-pause detection")
print("Verified: LiveShell Engine Media tile will NOT auto-install")

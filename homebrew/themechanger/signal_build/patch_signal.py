#!/usr/bin/env python3
"""Patch only the pinned GPL Signal sources. Compatibility is explicitly opt-in."""
from pathlib import Path
import shutil,sys
root=Path(sys.argv[1]);here=Path(__file__).resolve().parent
def replace(s,old,new):
    if s.count(old)!=1:raise RuntimeError("Pinned Signal source changed: "+old[:100])
    return s.replace(old,new)
for name in ("tc_indicator_compat.c","tc_indicator_compat.h"):
    shutil.copyfile(here/name,root/"src"/name)
p=root/"Makefile";s=p.read_text();s=replace(s,"SOURCES := src/main.c","SOURCES := src/tc_indicator_compat.c src/main.c");p.write_text(s)
p=root/"src/indicator.c";s=p.read_text()
s=replace(s,'#include "log.h"','#include "log.h"\n#include "tc_indicator_compat.h"\n#include <stdio.h>')
s=replace(s,"    if (fd < 0) return false;",'''    if (fd < 0) {
        char detail[120];snprintf(detail,sizeof(detail),"open /dev/icc_indicator errno=%d",errno);
        tc_hw_log("DEVICE_ERROR",detail);return false;
    }''')
s=replace(s,"    close(fd);\n    if (result != 0)",'''    close(fd);
    char detail[160];snprintf(detail,sizeof(detail),"ioctl=0x%lx result=%d errno=%d",request,result,result?error:0);
    tc_hw_log("IOCTL",detail);
    errno=error;
    if (result != 0)''')
s=replace(s,"bool indicator_set_color(int blue, int white, int amber) {",
'''bool indicator_set_color(int blue, int white, int amber) {
    if(tc_compat_enabled())return tc_compat_set(blue,white,amber);''')
p.write_text(s)
p=root/"src/player.c";s=p.read_text()
s=replace(s,'#include "log.h"','#include "log.h"\n#include "tc_indicator_compat.h"')
s=replace(s,"    if (hold && (off ? after_sequence : !indicator_shows(b, w, a))) indicator_set_color(b, w, a);",
'''    if (hold && (off ? after_sequence : !indicator_shows(b, w, a))) {
        if(!indicator_set_color(b,w,a)){
            pthread_mutex_lock(&held_lock);held=false;pthread_mutex_unlock(&held_lock);
            tc_hw_log("HOLD_CANCELLED","hardware write failed; no automatic retries");return false;
        }
    }''')
s=replace(s,"        indicator_set_color(blue, white, amber);",
'''        if(!indicator_set_color(blue,white,amber)){
            pthread_mutex_lock(&held_lock);held=false;pthread_mutex_unlock(&held_lock);
            return PLAYER_UNAVAILABLE;
        }''')
s+='''\n// Stop competing LED writers before taking or restoring the compatibility snapshot.
bool player_compat_control(bool restore) {
    player_stop();
    for(int i=0;i<100&&player_lighting();i++)usleep(20000);
    if(player_lighting()){tc_hw_log("COMPAT_BUSY","light sequence did not stop");return false;}
    pthread_mutex_lock(&held_lock);held=false;pthread_mutex_unlock(&held_lock);
    usleep(100000);
    return restore?tc_compat_restore():tc_compat_enable();
}
'''
p.write_text(s)
p=root/"src/http.c";s=p.read_text()
s=replace(s,'#include "player.h"','#include "player.h"\n#include "tc_indicator_compat.h"')
s=replace(s,'#define VERSION "0.1.0"','#define VERSION "0.1.0-tc-compat1"')
at='    if (strcmp(path, "/status") == 0) {'
s=replace(s,at,'''    if (strcmp(path,"/compat/enable")==0||strcmp(path,"/compat/restore")==0) {
        if(!post){respond_error(fd,405,"Method Not Allowed","use POST");return;}
        const bool restore=strcmp(path,"/compat/restore")==0;
        if(player_compat_control(restore))
            respond(fd,200,"OK",restore?"{\\"restored\\":true}":"{\\"compat_enabled\\":true,\\"backup_saved\\":true}");
        else respond_error(fd,503,"Service Unavailable","Compatibility action failed; check /data/ThemeChanger/signal-hardware.log");
        return;
    }
'''+at)
p.write_text(s)
print("Patched Signal: opt-in direct SET, durable backup/restore, hardware log and write-error propagation")

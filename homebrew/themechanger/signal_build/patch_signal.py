#!/usr/bin/env python3
"""Patch only the pinned GPL Signal sources. Compatibility is explicitly opt-in."""
from pathlib import Path
import shutil,sys
root=Path(sys.argv[1]);here=Path(__file__).resolve().parent
def replace(s,old,new):
    if s.count(old)!=1:raise RuntimeError("Pinned Signal source changed: "+old[:100])
    return s.replace(old,new)
for name in ("tc_indicator_compat.c","tc_indicator_compat.h","tc_effect.c","tc_effect.h","tc_effect_pattern.c"):
    shutil.copyfile(here/name,root/"src"/name)
p=root/"Makefile";s=p.read_text();s=replace(s,"SOURCES := src/main.c","SOURCES := src/tc_effect.c src/tc_effect_pattern.c src/tc_indicator_compat.c src/main.c");p.write_text(s)
p=root/"src/indicator.c";s=p.read_text()
s=replace(s,'#include "log.h"','#include "log.h"\n#include "tc_indicator_compat.h"\n#include <stdio.h>')
s=replace(s,"    if (fd < 0) return false;",'''    if (fd < 0) {
        char detail[120];snprintf(detail,sizeof(detail),"open /dev/icc_indicator errno=%d",errno);
        tc_hw_log("DEVICE_ERROR",detail);return false;
    }''')
s=replace(s,"    close(fd);\n    if (result != 0)",'''    close(fd);
    char detail[160];snprintf(detail,sizeof(detail),"ioctl=0x%lx result=%d errno=%d",request,result,result?error:0);
    if(result!=0)tc_hw_log("IOCTL",detail);
    errno=error;
    if (result != 0)''')
s=replace(s,"bool indicator_set_color(int blue, int white, int amber) {",
'''bool indicator_set_color(int blue, int white, int amber) {
    if(tc_compat_automatic()&&!tc_compat_enabled()&&!tc_compat_enable())return false;
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
s=replace(s,"        indicator_enable(true);\n        indicator_write(system_state);","        if(tc_compat_enabled())tc_compat_restore();\n        else {if(!tc_compat_automatic())indicator_enable(true);indicator_write(system_state);}")
s=replace(s,"        indicator_enable(true);\n        indicator_write(before_state);","        if(!tc_compat_automatic())indicator_enable(true);\n        indicator_write(before_state);")
s+='''\n// Stop competing LED writers before taking or restoring the compatibility snapshot.
bool player_compat_control(bool restore) {
    atomic_store(&light.cancel, true);
    for(int i=0;i<100&&player_lighting();i++)usleep(20000);
    if(player_lighting()){tc_hw_log("COMPAT_BUSY","light sequence did not stop");return false;}
    pthread_mutex_lock(&held_lock);held=false;pthread_mutex_unlock(&held_lock);
    usleep(100000);
    return restore?tc_compat_restore():tc_compat_enable();
}
'''
p.write_text(s)
p=root/"src/main.c";s=p.read_text()
s=replace(s,'#include "player.h"','#include "player.h"\n#include "tc_indicator_compat.h"\n#include "tc_effect.h"\n#include <ps5/kernel.h>')
s=replace(s,"    player_start();","    tc_compat_set_automatic(kernel_get_fw_version()==0x05100023u);\n    if(tc_compat_automatic())tc_compat_enable();\n    player_start();")
s=replace(s,"    apply_config();","    apply_config();\n    tc_effect_init(kernel_get_fw_version()==0x05100023u);")
p.write_text(s)
p=root/"src/http.c";s=p.read_text()
s=replace(s,'#include "player.h"','#include "player.h"\n#include "tc_indicator_compat.h"\n#include "tc_effect.h"')
s=replace(s,'#define VERSION "0.1.0"','#define VERSION "0.1.0-tc-led510"')
at='    if (strcmp(path, "/status") == 0) {'
s=replace(s,at,'''    if(strcmp(path,"/effect")==0){
        if(get){char status[160];tc_effect_status(status,sizeof(status));respond(fd,200,"OK",status);return;}
        if(!post){respond_error(fd,405,"Method Not Allowed","use GET or POST");return;}
        if(!json_parse(&doc,body,(size_t)content_length)||doc.nodes[0].type!=JSON_OBJECT){respond_error(fd,400,"Bad Request","Expected effect object");return;}
        const char* names[]={"mode","period_ms","blue","white","amber"};int values[5];
        for(int i=0;i<5;i++){const json_node* n=json_member(&doc,&doc.nodes[0],names[i]);if(!n||n->type!=JSON_NUMBER||n->number<0||n->number>20000){respond_error(fd,400,"Bad Request","Invalid effect values");return;}values[i]=(int)n->number;}
        const json_node* save=json_member(&doc,&doc.nodes[0],"save");
        if(save&&save->type!=JSON_BOOL){respond_error(fd,400,"Bad Request","save must be boolean");return;}
        if(tc_effect_apply(values[0],(unsigned)values[1],values[2],values[3],values[4],save&&save->number))respond(fd,200,"OK","{\\"effect_applied\\":true}");
        else respond_error(fd,503,"Service Unavailable","5.10 LED effect unavailable or invalid settings");
        return;
    }
    if(post&&(strcmp(path,"/color")==0||strcmp(path,"/led")==0||strcmp(path,"/stop")==0||strcmp(path,"/compat/restore")==0))tc_effect_stop(true);
    if(post&&strcmp(path,"/quit")==0){tc_effect_stop(false);player_compat_control(true);}
    if (strcmp(path,"/compat/enable")==0||strcmp(path,"/compat/restore")==0) {
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

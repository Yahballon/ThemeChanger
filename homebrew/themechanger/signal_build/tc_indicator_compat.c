// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental opt-in GameOS LED path. No new ioctl numbers or kernel offsets.
#include "tc_indicator_compat.h"
#include "indicator.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifndef TC_BACKUP_PATH
#define TC_BACKUP_PATH "/data/ThemeChanger/signal-led.before.bin"
#endif
#ifndef TC_HW_LOG_PATH
#define TC_HW_LOG_PATH "/data/ThemeChanger/signal-hardware.log"
#endif
static pthread_mutex_t compat_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_lock=PTHREAD_MUTEX_INITIALIZER;
static bool selected,enabled,have_backup;
static uint8_t before[130];
void tc_hw_log(const char* event,const char* detail){
 pthread_mutex_lock(&log_lock);
 mkdir("/data/ThemeChanger",0755);
 struct stat st;
 if(stat(TC_HW_LOG_PATH,&st)==0&&st.st_size>2*1024*1024)
  rename(TC_HW_LOG_PATH,TC_HW_LOG_PATH ".old");
 FILE* f=fopen(TC_HW_LOG_PATH,"ab");
 if(f){fprintf(f,"[%lld] %s %s\n",(long long)time(NULL),event,detail);fflush(f);fclose(f);}
 pthread_mutex_unlock(&log_lock);
}
static void dump(const char* event,const uint8_t* state){
 char hex[391];for(int i=0;i<130;i++)snprintf(hex+i*3,4,"%02x ",state[i]);
 tc_hw_log(event,hex);
}
bool tc_compat_patch(uint8_t state[130],int blue,int white,int amber){
 if(blue<0||blue>255||white<0||white>255||amber<0||amber>255)return false;
 // An all-zero SET is rejected on some controllers. Do not guess an off command.
 if(!(blue||white||amber))return false;
 const int count=state[2]|((int)state[3]<<8);
 if(state[0]!=0||state[1]!=0||count!=3)return false;
 const int ids[]={1,17,33},levels[]={blue,white,amber};
 int positions[3]={-1,-1,-1};
 // Validate the whole layout first; reject ambiguous/missing channels without mutation.
 for(int n=0;n<count;n++){
  int at=4+n*42,steps=state[at+1],channel=-1;
  if(steps<1||2+steps*5>42)return false;
  for(int c=0;c<3;c++)if(state[at]==ids[c])channel=c;
  if(channel<0||positions[channel]!=-1)return false;
  positions[channel]=at;
 }
 for(int c=0;c<3;c++)if(positions[c]<0)return false;
 // Preserve every program, flag and timing byte; replace only its level bytes.
 for(int c=0;c<3;c++){
  int at=positions[c];
  for(int n=0;n<state[at+1];n++)state[at+3+n*5]=(uint8_t)levels[c];
 }
 return true;
}
static bool save_backup(void){
 FILE* f=fopen(TC_BACKUP_PATH ".part","wb");if(!f)return false;
 bool ok=fwrite(before,1,sizeof(before),f)==sizeof(before);
 if(fflush(f)!=0||fsync(fileno(f))!=0)ok=false;
 if(fclose(f)!=0)ok=false;
 if(!ok||rename(TC_BACKUP_PATH ".part",TC_BACKUP_PATH)!=0){
  unlink(TC_BACKUP_PATH ".part");return false;
 }
 return true;
}
bool tc_compat_enabled(void){
 pthread_mutex_lock(&compat_lock);bool value=selected;pthread_mutex_unlock(&compat_lock);return value;
}
bool tc_compat_enable(void){
 pthread_mutex_lock(&compat_lock);
 if(enabled){pthread_mutex_unlock(&compat_lock);return true;}
 uint8_t state[130],check[130];
 bool ok=indicator_read(state);
 if(ok){
  memcpy(check,state,130);
  // Validating a copy does not change the console.
  ok=tc_compat_patch(check,1,1,1);
  dump("BACKUP_READ",state);
 }
 if(ok){
  memcpy(before,state,130);have_backup=save_backup();ok=have_backup;
 }
 enabled=ok;if(ok)selected=true;
 tc_hw_log("COMPAT_ENABLE",ok?"ready; no hardware write; next colour uses direct SET":"refused: read, layout or durable backup failed");
 pthread_mutex_unlock(&compat_lock);return ok;
}
bool tc_compat_restore(void){
 pthread_mutex_lock(&compat_lock);
 if(!have_backup){
  FILE* f=fopen(TC_BACKUP_PATH,"rb");
  if(f){uint8_t check[130];bool valid=fread(before,1,130,f)==130&&fgetc(f)==EOF;fclose(f);
   memcpy(check,before,130);have_backup=valid&&tc_compat_patch(check,1,1,1);}
 }
 bool ok=have_backup;
 if(ok){
  ok=indicator_write(before);
  if(ok){uint8_t actual[130];ok=indicator_read(actual)&&memcmp(actual,before,130)==0;
   if(ok)dump("RESTORED",actual);}
 }
 // Never leave an automatic compatibility writer active after restore.
 enabled=false;selected=false;
 tc_hw_log("COMPAT_RESTORE",ok?"original state restored and verified":"restore unconfirmed; compatibility disabled; backup retained");
 pthread_mutex_unlock(&compat_lock);return ok;
}
bool tc_compat_set(int blue,int white,int amber){
 pthread_mutex_lock(&compat_lock);
 uint8_t state[130],original[130];char info[160];
 snprintf(info,sizeof(info),"blue=%d white=%d amber=%d",blue,white,amber);
 tc_hw_log("COMPAT_REQUEST",info);
 bool ok=enabled&&have_backup&&indicator_read(state);
 if(ok){memcpy(original,state,130);dump("BEFORE_SET",state);ok=tc_compat_patch(state,blue,white,amber);}
 if(ok){
  dump("REQUESTED_SET",state);
  // Deliberately no indicator_enable(): this is the hypothesis under test.
  ok=indicator_write(state);
  if(ok){
   uint8_t actual[130];int b,w,a;
   usleep(50000);
   ok=indicator_read(actual);
   if(ok)dump("AFTER_SET",actual);
   indicator_levels(&b,&w,&a);
   snprintf(info,sizeof(info),"reported=%d/%d/%d requested=%d/%d/%d",b,w,a,blue,white,amber);
   tc_hw_log("READBACK",info);
   ok=ok&&b==blue&&w==white&&a==amber;
  }
  if(!ok){
   const bool rolled_back=indicator_write(original);
   tc_hw_log("ROLLBACK",rolled_back?"previous state write accepted":"previous state write failed; original backup retained");
  }
 }
 if(!ok)enabled=false; // One failure stops keeper retries; explicit enable is required again.
 tc_hw_log("COMPAT_RESULT",ok?"reported values match; physical light requires observation":"failed; compatibility writer disabled");
 pthread_mutex_unlock(&compat_lock);return ok;
}

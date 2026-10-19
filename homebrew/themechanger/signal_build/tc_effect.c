// SPDX-License-Identifier: GPL-3.0-or-later
#include "tc_effect.h"
#include "tc_indicator_compat.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#ifndef TC_EFFECT_PATH
#define TC_EFFECT_PATH "/data/ThemeChanger/signal-effect.txt"
#endif
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static bool ready,started;
static int active,base[3]={255,0,0};
static unsigned cycle=4000;
static unsigned long long began;
static unsigned long long millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (unsigned long long)t.tv_sec*1000+t.tv_nsec/1000000;}
void tc_effect_stop(bool forget){pthread_mutex_lock(&lock);active=0;if(forget)unlink(TC_EFFECT_PATH);pthread_mutex_unlock(&lock);}
static void* worker(void* ignored){
 (void)ignored;
 for(;;){
  pthread_mutex_lock(&lock);
  if(active){int rgb[3];if(!tc_effect_frame(active,(unsigned)(millis()-began),cycle,base,rgb)||!tc_compat_set_quiet(rgb[0],rgb[1],rgb[2])){active=0;tc_hw_log("EFFECT_STOP","device error; no retries");}}
  pthread_mutex_unlock(&lock);
  usleep(100000);
 }
 return NULL;
}
bool tc_effect_apply(int mode,unsigned period,int blue,int white,int amber,bool save){
 if(!ready||mode<0||mode>=TC_EFFECT_COUNT||period<500||period>20000||blue<0||blue>255||white<0||white>255||amber<0||amber>255)return false;
 tc_effect_stop(false);
 if(!player_compat_control(mode==TC_EFFECT_OFF))return false;
 pthread_mutex_lock(&lock);
 if(!started){pthread_t thread;if(pthread_create(&thread,NULL,worker,NULL)!=0){pthread_mutex_unlock(&lock);return false;}pthread_detach(thread);started=true;}
 bool ok=true;
 if(save){
  FILE* f=fopen(TC_EFFECT_PATH ".part","wb");ok=f!=NULL;
  if(f){ok=fprintf(f,"%d %u %d %d %d\n",mode,period,blue,white,amber)>0;if(fflush(f)||fsync(fileno(f)))ok=false;if(fclose(f))ok=false;if(ok)ok=rename(TC_EFFECT_PATH ".part",TC_EFFECT_PATH)==0;if(!ok)unlink(TC_EFFECT_PATH ".part");}
 }
 if(ok){base[0]=blue;base[1]=white;base[2]=amber;cycle=period;began=millis();active=mode;}
 pthread_mutex_unlock(&lock);
 tc_hw_log("EFFECT",ok?"mode applied":"save failed; previous settings retained");return ok;
}
void tc_effect_init(bool supported){
 ready=supported;if(!ready)return;
 int mode,b,w,a;unsigned period;char extra;
 FILE* f=fopen(TC_EFFECT_PATH,"rb");if(!f)return;
 int count=fscanf(f,"%d %u %d %d %d %c",&mode,&period,&b,&w,&a,&extra);fclose(f);
 if(count==5&&!tc_effect_apply(mode,period,b,w,a,false))tc_hw_log("EFFECT_CONFIG","saved effect rejected");
}
void tc_effect_status(char* output,size_t cap){
 pthread_mutex_lock(&lock);snprintf(output,cap,"{\"available\":%s,\"mode\":%d,\"period_ms\":%u}",ready?"true":"false",active,cycle);pthread_mutex_unlock(&lock);
}

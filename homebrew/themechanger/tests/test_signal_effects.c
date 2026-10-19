// SPDX-License-Identifier: GPL-3.0-or-later
#include "tc_effect.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
static pthread_mutex_t guard=PTHREAD_MUTEX_INITIALIZER;
static int writes,restores;static bool fail;
bool tc_compat_set_quiet(int b,int w,int a){assert(b>=0&&b<=255&&w>=0&&w<=255&&a>=0&&a<=255);pthread_mutex_lock(&guard);writes++;bool ok=!fail;pthread_mutex_unlock(&guard);return ok;}
bool player_compat_control(bool restore){if(restore)restores++;return true;}
void tc_hw_log(const char* a,const char* b){(void)a;(void)b;}
static int count(void){pthread_mutex_lock(&guard);int n=writes;pthread_mutex_unlock(&guard);return n;}
int main(void){
 int base[]={255,120,80},out[3];
 for(int mode=1;mode<TC_EFFECT_COUNT;mode++)for(unsigned t=0;t<40000;t+=37){assert(tc_effect_frame(mode,t,4000,base,out));for(int c=0;c<3;c++)assert(out[c]>=0&&out[c]<=255);}
 assert(tc_effect_frame(TC_EFFECT_FOUR,3000,4000,base,out)&&out[0]==32&&out[1]==0&&out[2]==255);
 assert(!tc_effect_frame(99,0,4000,base,out));assert(!tc_effect_frame(1,0,0,base,out));
 unlink(TC_EFFECT_PATH);tc_effect_init(true);
 assert(tc_effect_apply(TC_EFFECT_PULSE,1000,255,0,0,true));usleep(250000);assert(count()>0);
 char status[160];tc_effect_status(status,sizeof(status));assert(strstr(status,"\"mode\":1"));
 assert(!tc_effect_apply(99,1000,255,0,0,true));tc_effect_status(status,sizeof(status));assert(strstr(status,"\"mode\":1"));
 tc_effect_stop(false);int stopped=count();usleep(250000);assert(stopped==count());
 tc_effect_init(true);usleep(250000);assert(count()>stopped); // Saved mode resumes without the UI.
 pthread_mutex_lock(&guard);fail=true;pthread_mutex_unlock(&guard);usleep(250000);tc_effect_status(status,sizeof(status));assert(strstr(status,"\"mode\":0"));
 stopped=count();usleep(250000);assert(count()==stopped); // A failed device write latches the effect off.
 assert(tc_effect_apply(0,4000,255,0,0,true));assert(restores==1);
 tc_effect_stop(true);assert(access(TC_EFFECT_PATH,F_OK)!=0);
 puts("PASS: effect bounds, saved restart, stop serialization, restore, failure latch");
}

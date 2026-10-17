// SPDX-License-Identifier: GPL-3.0-or-later
// Fake device: prove bounded edits, no ENABLE dependency, rollback and restoration.
#include "tc_indicator_compat.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static uint8_t device[130];static int writes;static bool fail_next,read_ok=true;
bool indicator_read(uint8_t* out){if(!read_ok)return false;memcpy(out,device,130);return true;}
bool indicator_write(const uint8_t* state){
 writes++;if(fail_next){fail_next=false;return false;}memcpy(device,state,130);return true;
}
void indicator_levels(int* b,int* w,int* a){
 *b=device[12];*w=device[54];*a=device[96]; // last level in two-step blocks
}
static void fixture(void){
 memset(device,0,130);device[2]=3;
 for(int i=0;i<3;i++){int at=4+42*i;device[at]=1+16*i;device[at+1]=2;
  for(int j=0;j<2;j++){device[at+2+j*5]=2;device[at+3+j*5]=i==1?255:0;
   device[at+4+j*5]=5;device[at+5+j*5]=2;}
 }
}
int main(void){
 fixture();uint8_t original[130],changed[130];memcpy(original,device,130);memcpy(changed,device,130);
 assert(tc_compat_patch(changed,64,0,32));
 for(int i=0;i<130;i++){
  bool level=false;for(int c=0;c<3;c++)for(int n=0;n<2;n++)if(i==4+c*42+3+n*5)level=true;
  if(!level)assert(changed[i]==original[i]); // Every flag/timing/header byte is preserved.
 }
 changed[2]=4;uint8_t bad[130];memcpy(bad,changed,130);
 assert(!tc_compat_patch(changed,1,2,3)&&memcmp(changed,bad,130)==0);
 assert(!tc_compat_patch(device,0,0,0)&&!tc_compat_patch(device,256,0,0));
 read_ok=false;assert(!tc_compat_enable()&&writes==0);read_ok=true;
 assert(tc_compat_enable()&&tc_compat_enabled()&&writes==0); // Enabling is read-only.
 FILE* f=fopen(TC_BACKUP_PATH,"rb");assert(f);assert(fread(changed,1,130,f)==130);fclose(f);assert(memcmp(changed,original,130)==0);
 assert(tc_compat_set(64,0,32)&&writes==1); // No indicator_enable implementation exists in this test.
 assert(device[7]==64&&device[49]==0&&device[91]==32);
 fail_next=true;assert(!tc_compat_set(1,2,3));assert(device[7]==64); // previous state rolled back
 int before=writes;assert(!tc_compat_set(1,2,3)&&writes==before); // no retry after failure
 assert(tc_compat_restore());assert(!tc_compat_enabled()&&memcmp(device,original,130)==0);
 unlink(TC_BACKUP_PATH);unlink(TC_HW_LOG_PATH);unlink(TC_HW_LOG_PATH ".old");
 puts("LED compatibility: validated layout, level-only edits, snapshot, direct SET, failure latch and restore passed");
}

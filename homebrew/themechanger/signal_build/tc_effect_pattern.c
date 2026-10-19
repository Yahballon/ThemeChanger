// SPDX-License-Identifier: GPL-3.0-or-later
#include "tc_effect.h"
bool tc_effect_frame(int mode,unsigned elapsed,unsigned period,const int base[3],int out[3]){
 if(mode<=TC_EFFECT_OFF||mode>=TC_EFFECT_COUNT||period<500||period>20000)return false;
 for(int c=0;c<3;c++)if(base[c]<0||base[c]>255)return false;
 unsigned phase=elapsed%period;
 if(mode==TC_EFFECT_PULSE||mode==TC_EFFECT_BREATHE){
  unsigned level=phase<period/2?phase*510/period:(period-phase)*510/period;
  if(mode==TC_EFFECT_BREATHE)level=level*level/255;
  for(int c=0;c<3;c++)out[c]=base[c]*(int)level/255;
 }else if(mode==TC_EFFECT_BLINK){for(int c=0;c<3;c++)out[c]=phase<period/2?base[c]:0;}
 else{
  // Four console-tested colours; this is a B/W/A wave, not a true RGB rainbow.
  static const int palette[4][3]={{255,0,0},{0,255,0},{0,0,255},{32,0,255}};
  unsigned step=phase*4/period,next=(step+1)%4;
  unsigned blend=(phase*4%period)*255/period;
  for(int c=0;c<3;c++)out[c]=mode==TC_EFFECT_FOUR?palette[step][c]:(palette[step][c]*(255-(int)blend)+palette[next][c]*(int)blend)/255;
 }
 return true;
}

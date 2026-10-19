// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { TC_EFFECT_OFF, TC_EFFECT_PULSE, TC_EFFECT_BLINK, TC_EFFECT_FOUR, TC_EFFECT_WAVE, TC_EFFECT_BREATHE, TC_EFFECT_COUNT };
bool tc_effect_frame(int mode,unsigned elapsed,unsigned period,const int base[3],int out[3]);
void tc_effect_init(bool supported);
void tc_effect_stop(bool forget);
bool tc_effect_apply(int mode,unsigned period,int blue,int white,int amber,bool save);
void tc_effect_status(char* output,size_t cap);

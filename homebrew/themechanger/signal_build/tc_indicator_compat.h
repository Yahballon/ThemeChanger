// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool tc_compat_enabled(void);
bool tc_compat_enable(void);
bool tc_compat_restore(void);
bool tc_compat_set(int blue,int white,int amber);
bool tc_compat_patch(uint8_t state[130],int blue,int white,int amber);
void tc_hw_log(const char* event,const char* detail);

bool player_compat_control(bool restore);
void tc_compat_set_automatic(bool value);
bool tc_compat_automatic(void);

bool tc_compat_set_quiet(int blue,int white,int amber);

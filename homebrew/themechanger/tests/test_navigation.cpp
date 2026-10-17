// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation.hpp"
#include <cassert>
#include <cstdio>
int main(){
 int page=0,sideRow=0;bool sidebar=false;
 navigation::back(page,sidebar,sideRow);assert(sidebar&&sideRow==0);
 navigation::open(2,page,sidebar,sideRow);assert(page==4&&!sidebar);
 assert(navigation::signal_tabs(page,sidebar));
 navigation::back(page,sidebar,sideRow);assert(sidebar&&sideRow==2);
 assert(!navigation::signal_tabs(page,sidebar));
 navigation::open(3,page,sidebar,sideRow);assert(page==1);
 navigation::back(page,sidebar,sideRow);assert(sidebar&&sideRow==3);
 navigation::open(4,page,sidebar,sideRow);assert(page==2);
 navigation::cycle(1,page,sidebar,sideRow);assert(page==0);
 navigation::cycle(-1,page,sidebar,sideRow);assert(page==2);
 for(int i=0;i<5;i++)navigation::cycle(1,page,sidebar,sideRow);
 assert(page==2&&!sidebar);
 navigation::back(page,sidebar,sideRow);navigation::back(page,sidebar,sideRow);
 assert(page==2&&!sidebar); // Back retains page instead of quitting or jumping to Backgrounds.
 puts("Navigation: sidebar round trips, page wrap and Signal tab ownership passed");
}

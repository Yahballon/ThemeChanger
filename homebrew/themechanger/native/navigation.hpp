// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace navigation {
constexpr int pages[]={0,3,4,1,2};
constexpr int row(int page){for(int i=0;i<5;i++)if(pages[i]==page)return i;return 0;}
inline void open(int selected,int& page,bool& sidebar,int& sideRow){
 sideRow=selected<0?0:selected>4?4:selected;page=pages[sideRow];sidebar=false;
}
inline void back(int page,bool& sidebar,int& sideRow){
 sidebar=!sidebar;if(sidebar)sideRow=row(page);
}
// Inside Signal, shoulder buttons belong to its three local tabs.
constexpr bool signal_tabs(int page,bool sidebar){return page==4&&!sidebar;}
inline void cycle(int direction,int& page,bool& sidebar,int& sideRow){
 open((row(page)+direction+5)%5,page,sidebar,sideRow);
}
}

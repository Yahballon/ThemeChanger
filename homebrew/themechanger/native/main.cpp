// SPDX-License-Identifier: GPL-3.0-or-later
// ThemeChanger v0.3.22 - native PS5 Homebrew UI / LiveShell bridge.
#include "gfx/renderer.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include "liveshell_client.hpp"
#include "liveshell_loader.hpp"
#include "motion_catalog.hpp"
#include "theme_log.hpp"
#include "wallpaper_storage.hpp"
#include "signal_panel.hpp"
#include "navigation.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <deque>
#include <cctype>
#include <ctime>
#include <GL/glcorearb.h>
#include <sys/stat.h>
#include <string>
using namespace hui::gfx;
namespace {
const Color white=Color::rgb(0xf7f9ff), muted=Color::rgb(0xaebbd2);
std::string read(const std::string& path) {
 FILE* f=fopen(path.c_str(),"rb");if(!f)return {};
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return {};}
 long n=ftell(f);rewind(f);if(n<1||n>12000000){fclose(f);return {};}
 std::string s((size_t)n,0);bool ok=fread(s.data(),1,s.size(),f)==s.size();fclose(f);return ok?s:std::string{};
}
unsigned texture(Renderer& r,const std::string& path){
 auto d=read(path);if(d.empty())return 0;
 int w=0,h=0,ch=0;
 unsigned char* p=stbi_load_from_memory((const unsigned char*)d.data(),(int)d.size(),&w,&h,&ch,4);
 if(!p)return 0;unsigned t=w<=2048&&h<=2048?r.batch().create_texture(w,h,p):0;
 stbi_image_free(p);return t;
}
void txt(DrawList& d,const Font& f,unsigned a,const std::string& s,float x,float y,float sz=24,Color c=white,Align align=Align::left){d.text(f,a,s,x,y,sz,c,align);}
void panel(DrawList& d,Rect r,float rad=13,Color fill=Color::rgb(0x040d20),Color border=Color::rgb(0x365074)){d.bordered_rect(r,rad,fill,1.5f,border);}
void symbol(DrawList& d,float x,float y,float size=58){
 d.chamfer_rect({x,y,size,size},size*.23f,Color::rgb(0x069cfb),2,Color::rgb(0x16bafa));
 d.chamfer_rect({x+size*.18f,y+size*.19f,size*.64f,size*.62f},size*.15f,Color::rgb(0x071931),0,{});
 d.rounded_rect({x+size*.48f,y+size*.4f,size*.47f,size*.14f},2,Color::rgb(0x00b9ff));
}
std::string lower(std::string s){
 for(char& c:s)c=(char)std::tolower((unsigned char)c);
 return s;
}
constexpr const char* kKeyChars="1234567890qwertyuiopasdfghjklzxcvbnm -";
enum class Overlay {none,search,tags,cleanup};
void modal(DrawList& d,const Font& font,unsigned atlas,Overlay overlay,
           const std::string& query,const std::string& tag,
           const std::vector<std::pair<std::string,int>>& tags,
           int key,int tagFocus){
 if(overlay==Overlay::none)return;
 if(overlay==Overlay::cleanup){
  panel(d,{445,420,1350,300},16,Color::rgb(0x07162b),Color::rgb(0x1baaff));
  txt(d,font,atlas,"Delete all downloaded wallpapers?",490,484,34);
  txt(d,font,atlas,"Removes downloaded videos, backup copies and unfinished downloads.",490,548,24,muted);
  txt(d,font,atlas,"This cannot be undone. LiveShell wallpaper playback will be turned off.",490,593,24,muted);
  txt(d,font,atlas,"X: DELETE ALL     O: CANCEL",490,669,27,Color::rgb(0xffce83));
  return;
 }
 d.rounded_rect({371,402,1538,583},18,Color::rgb(0x010916,.98f));
 d.bordered_rect({385,412,1513,565},16,Color::rgb(0x07162b),
                 2.5f,Color::rgb(0x1baaff));
 if(overlay==Overlay::search){
   txt(d,font,atlas,"SEARCH WALLPAPERS",445,465,35);
   panel(d,{445,489,1352,65},9,Color::rgb(0x020c1e),Color::rgb(0x148bea));
   txt(d,font,atlas,query.empty()?"Type to search all indexed wallpapers...":query,
       468,532,27,query.empty()?muted:white);
   const std::string caps=kKeyChars;
   for(int n=0;n<(int)caps.size();n++){
     const int col=n%10,row=n/10;
     const float x=447+col*135.f,y=581+row*89.f;
     panel(d,{x,y,122,70},9,Color::rgb(n==key?0x0758be:0x0b2444),
           Color::rgb(n==key?0x52cfff:0x294f7d));
     txt(d,font,atlas,caps[n]==' '?"SPACE":std::string(1,caps[n]),x+61,y+46,
         caps[n]==' '?17:31,white,Align::center);
   }
   txt(d,font,atlas,"X: TYPE     SQUARE: DELETE     R2: CLEAR     TRIANGLE: DONE     O: CLOSE",
       457,953,21,muted);
 }else{
   txt(d,font,atlas,"MOTIONBGS TAGS",446,468,35);
   txt(d,font,atlas,"Selected: "+(tag.empty()?"ALL TAGS":tag)+
       "     L2/R2: PAGE     SQUARE: CLEAR     X: SELECT",447,507,21,muted);
   const int start=(tagFocus/18)*18;
   for(int n=start;n<(int)tags.size()&&n<start+18;n++){
     const int slot=n-start,col=slot%3,row=slot/3;
     const float x=444+col*460.f,y=530+row*64.f;
     panel(d,{x,y,444,55},8,Color::rgb(n==tagFocus?0x0a66ba:0x0a2345),
           Color::rgb(n==tagFocus?0x50ccff:0x2b4b70));
     std::string label=tags[(size_t)n].first;
     if(label.size()>23)label=label.substr(0,21)+"...";
     txt(d,font,atlas,label,x+13,y+34,22);
     txt(d,font,atlas,std::to_string(tags[(size_t)n].second),x+420,y+34,19,muted,Align::right);
   }
   txt(d,font,atlas,"Tag "+std::to_string(tags.empty()?0:tagFocus+1)+
     " / "+std::to_string(tags.size()),1770,957,20,muted,Align::right);
 }
}
void signalScreen(DrawList& d,const Font& f,unsigned a,const companion::Panel& signal){
 panel(d,{398,18,1458,951},13,Color::rgb(0x030d20),Color::rgb(0x405370));
 txt(d,f,a,"Signal Companion",432,69,36);
 txt(d,f,a,"Console light and startup tune | powered by Marice / ps5-signal",434,106,22,muted);
 const char* tabs[]={"Status","Colour","Startup","Effects"};
 for(int i=0;i<4;i++){
  const float x=432+i*350.f;
  panel(d,{x,130,340,58},10,Color::rgb(i==signal.tab()?0x085ef3:0x071b34),Color::rgb(0x1f3857));
  txt(d,f,a,tabs[i],x+170,169,26,white,Align::center);
 }
 panel(d,{432,199,1390,61},9,Color::rgb(0x292014),Color::rgb(0x80643b));
 txt(d,f,a,"LED colours: tested on firmware 5.10 | Original Signal support: 9.00+",450,223,21,Color::rgb(0xffce83));
 txt(d,f,a,"Blue / White / Amber mixing; not full RGB. Effects continue after closing ThemeChanger.",450,247,19,muted);
 txt(d,f,a,f.fit(signal.detail(),22,1380),434,291,22,signal.online()?Color::rgb(0x4affbc):muted);
 if(signal.tab()==2){
  txt(d,f,a,signal.light_column()?"Startup light sequence":"Startup beep sequence",434,327,25);
  txt(d,f,a,"Options: Add   R3: Delete step   Touchpad: Preview   Square: Save",1818,326,18,muted,Align::right);
 }
 const auto rows=signal.rows();
 const int first=(signal.row()/10)*10;
 for(int i=first;i<(int)rows.size()&&i<first+10;i++){
  const float y=345+(i-first)*47.f;
  panel(d,{432,y,1390,42},8,Color::rgb(i==signal.row()?0x0b397a:0x071a34),Color::rgb(i==signal.row()?0x10a6ff:0x2b4667));
  txt(d,f,a,rows[(size_t)i].first,452,y+30,23);
  txt(d,f,a,f.fit(rows[(size_t)i].second,22,830),1798,y+30,22,muted,Align::right);
 }
 txt(d,f,a,f.fit(signal.message(),23,1380),434,868,23,Color::rgb(0xffce83));
 txt(d,f,a,f.fit(signal.help(),20,1380),434,912,20,muted);
 if(signal.tab()==2)txt(d,f,a,"L1/R1: Tabs   |   O: Sidebar   |   24 steps per sequence, 10 seconds maximum",434,946,18,muted);
 else txt(d,f,a,"Blue/White/Amber LEDs | Diagnostics: /data/ThemeChanger/signal.log",434,946,18,muted);
}
void draw(DrawList& d,const Font& f,unsigned a,int selected,int page,int opt,const std::string& status,bool live,bool loading,bool sidebar,const motion::Catalog& catalog,
          const std::vector<int>& filtered,const std::vector<unsigned>& thumbnails,
          int onlineSelected,int onlinePage,int filterIndex,int sideRow,
          const std::vector<int>& installedIds,const std::string& activeFile,const std::string& shellStatus,
          Overlay overlay,const std::string& search,const std::string& tag,
          const std::vector<std::pair<std::string,int>>& tags,int key,int tagFocus,const companion::Panel& signal){
 // All positions are in 1920x1080 virtual pixels, derived from the reference's 1671x941 grid.
 // The third row ends at y=985; the footer begins at y=994 (no clipping).
 d.gradient_rect({0,0,1920,1080},0,Color::rgb(0x020a16),Color::rgb(0x010615));
 d.gradient_rect({0,0,375,994},0,Color::rgb(0x061b35),Color::rgb(0x030a18));
 // Measure both title segments so they cannot overlap or cross the sidebar padding.
 constexpr float titleX=56.f,titleRight=350.f,titleGap=2.f;
 float titleSize=32.f;
 const float titleWidth=f.measure("Theme",titleSize)+f.measure("Changer",titleSize);
 if(titleWidth>titleRight-titleX-titleGap)
   titleSize*=(titleRight-titleX-titleGap)/titleWidth;
 txt(d,f,a,"Theme",titleX,73,titleSize);
 txt(d,f,a,"Changer",titleX+f.measure("Theme",titleSize)+titleGap,73,titleSize,Color::rgb(0x0da4ff));
 const std::string credit="by YAHBALLON  v0.3.22";
 const float creditSize=std::min(18.f,18.f*(titleRight-titleX)/f.measure(credit,18.f));
 txt(d,f,a,credit,titleX,109,creditSize,muted);
 d.line(2,125,375,125,1,Color::rgb(0x233a57));
 bool side=sidebar&&sideRow==0;
 panel(d,{25,148,325,71},10,Color::rgb(page==0?0x085ef3:0x071b34),Color::rgb(side?0x39c9ff:0x095fba));
 d.bordered_rect({53,170,32,27},5,white,2,white);
 d.circle(76,178,3,Color::rgb(0x075fff));
 d.line(60,190,67,182,3,Color::rgb(0x075fff));d.line(67,182,79,194,3,Color::rgb(0x075fff));
 txt(d,f,a,"Backgrounds",119,194,25);
 bool motionFocus=sidebar&&sideRow==1;
 panel(d,{25,235,325,71},10,Color::rgb(page==3?0x085ef3:0x071b34),
       Color::rgb(motionFocus?0x39c9ff:0x1f3857));
 d.bordered_rect({53,254,32,29},5,Color::rgb(0x08254e),2,white);
 d.triangle({63,261,16,17},white,0,1.5708f);
 txt(d,f,a,"Online Wallpapers",119,280,23);
 const bool signalFocus=sidebar&&sideRow==2;
 panel(d,{25,322,325,71},10,Color::rgb(page==4?0x085ef3:0x071b34),Color::rgb(signalFocus?0x39c9ff:0x1f3857));
 d.circle(69,357,16,Color::rgb(0x0da4ff));
 d.circle(69,357,7,white);
 txt(d,f,a,"Signal Companion",119,367,23);
 d.line(29,806,346,806,1,Color::rgb(0x20354d));
 for(int i=1;i<3;i++){
   float y=i==1?833.f:907.f;
   if(page==i||(sidebar&&sideRow==i+2))panel(d,{24,y,328,69},10,Color::rgb(0x0a3470),Color::rgb(sidebar&&sideRow==i+2?0x39c9ff:0x13a0ff));
   d.circle(69,y+33,18,white);txt(d,f,a,i==1?"i":"*",69,y+42,26,Color::rgb(0x041631),Align::center);
   txt(d,f,a,i==1?"About":"Settings",121,y+43,25);
 }
 if(page==4)signalScreen(d,f,a,signal);
 else{
 // Align the preview with the wallpaper grid edges (398..1856).
 panel(d,{398,18,1458,375},13,Color::rgb(0x010819),Color::rgb(0x405370));
 unsigned picture=0;
 if(page==0&&!installedIds.empty()&&selected>=0&&selected<(int)installedIds.size()){
   const int id=installedIds[(size_t)selected];
   if(id>=0&&id<(int)thumbnails.size())picture=thumbnails[(size_t)id];
 }
 if(page==3&&!filtered.empty()&&onlineSelected>=0&&onlineSelected<(int)filtered.size()){
   const auto id=filtered[(size_t)onlineSelected];
   if(id>=0&&id<(int)thumbnails.size()&&thumbnails[(size_t)id])picture=thumbnails[(size_t)id];
 }
 if(picture)d.image(picture,{400,119,1454,271},kFullUv,white,10);
 else {
   d.gradient_rect({401,119,1453,270},0,Color::rgb(0x061c42),Color::rgb(0x13447a));
   txt(d,f,a,"Loading wallpaper preview...",1127,343,23,muted,Align::center);
 }
 d.gradient_rect({400,120,1454,218},0,Color::rgb(0x03091c,.68f),Color::rgb(0x01091c,.10f));
 d.gradient_rect({401,20,1451,103},0,Color::rgb(0x010918),Color::rgb(0x020817,.90f));
 txt(d,f,a,(page==0||page==3)?"Preview":page==1?"About":"Settings",432,65,34);
 txt(d,f,a,page==0?"This is how your PS5 menu will look with this theme.":page==3?"MotionBGS: select HD wallpaper and download to LiveShell.":page==1?"ThemeChanger information":"Configure the integrated LiveShell engine",434,99,21,muted);
 txt(d,f,a,"16:42",1823,78,27,white,Align::right);
 // Faux shell tiles are decorative inside the preview and do not pretend to launch installed games.
 for(int i=0;i<9;i++){
  float x=440+i*104.f,y=153;
  if(i==1)d.glow({x,y,92,94},10,13,Color::rgb(0x007fff,.38f));
  panel(d,{x,y,92,91},9,Color::rgb(0x08172e,.95f),Color::rgb(i==1?0x05afff:0x21395b));
  if(i==1)symbol(d,x+17,y+17,57);
  else {
   int id=-1;
   if(i>=2&&i<6){
      const int slot=i-2;
      if(page==0&&slot<(int)installedIds.size())id=installedIds[(size_t)slot];
      else if(page==3&&slot<(int)filtered.size())id=filtered[(size_t)slot];
   }
   if(id>=0&&id<(int)thumbnails.size()&&thumbnails[(size_t)id])
      d.image(thumbnails[(size_t)id],{x+3,y+4,86,83},kFullUv,white,8);
   else txt(d,f,a,i==0?"*":(i==6?"[]":(i==7?"O":"#")),x+46,y+60,35,Color::rgb(0xdbeeff),Align::center);
  }
 }
 txt(d,f,a,"Open Theme",544,282,20);
 if(page==0){
  const int total=(int)installedIds.size();
  const int start=total>0?(std::clamp(selected,0,total-1)/12)*12:0;
  txt(d,f,a,"Downloaded Backgrounds",397,435,30);
  txt(d,f,a,std::to_string(total)+" installed   |   Page "+
    std::to_string(start/12+1)+" / "+std::to_string(std::max(1,(total+11)/12)),
    1878,432,18,muted,Align::right);
  if(total==0){
    txt(d,f,a,"No downloaded wallpapers yet",1105,610,35,white,Align::center);
    txt(d,f,a,"Press R1 to browse MotionBGS and download your first background",
        1105,662,23,muted,Align::center);
  }
  for(int pos=0;pos<12&&start+pos<total;pos++){
   const int selectedIndex=start+pos;
   const int id=installedIds[(size_t)selectedIndex];
   if(id<0||id>=(int)catalog.entries().size())continue;
   const auto& item=catalog.entries()[(size_t)id];
   std::string name=item.title;
   unsigned tex=id<(int)thumbnails.size()?thumbnails[(size_t)id]:0;
   const float x=398+(pos%4)*370.f,y=455+(pos/4)*176.f;
   const float w=348,h=157;
   const bool focus=selected==selectedIndex&&!sidebar;
   if(focus)d.glow({x,y,w,h},13,13,Color::rgb(0x006eff,.40f));
   panel(d,{x,y,w,h},9,Color::rgb(0x071329),Color::rgb(focus?0x0d9fff:0x324359));
   if(tex)d.image(tex,{x+2,y+2,w-4,h-4},kFullUv,white,7);
   else {
     d.gradient_rect({x+2,y+2,w-4,h-4},8,Color::rgb(0x051c44),Color::rgb(0x134070));
     txt(d,f,a,"Downloaded MP4",x+174,y+77,20,muted,Align::center);
   }
   d.gradient_rect({x+2,y+h-52,w-4,50},4,Color::rgb(0x000000,.01f),Color::rgb(0x000000,.88f));
   if(name.size()>28)name=name.substr(0,25)+"...";
   txt(d,f,a,name,x+15,y+h-17,21,white);
   const bool applied=activeFile=="MotionBGS-"+item.slug+".mp4";
   if(applied){
     d.circle(x+w-33,y+29,20,Color::rgb(0x0876ff));
     txt(d,f,a,"X",x+w-33,y+37,23,white,Align::center);
   }
   if(focus)d.bordered_rect({x-2,y-2,w+4,h+4},10,Color::rgb(0x000000,0),3,Color::rgb(0x04a7ff));
  }
 }else if(page==3){
  const char* categories[]={"All","Featured","Anime","Games","Nature","Cars","Space","Technology","Fantasy","Superhero","TV & Movie","Horror","Animal","Japan","Football","Other"};
  txt(d,f,a,"MotionBGS Online Wallpapers",398,435,30);
  txt(d,f,a,std::to_string(filtered.size())+" results | Page "+std::to_string(onlinePage+1)+
      "/"+std::to_string(std::max(1,(int)((filtered.size()+11)/12)))+
      " | "+categories[std::clamp(filterIndex,0,15)],1880,431,19,muted,Align::right);
  txt(d,f,a,"TRIANGLE Search: "+(search.empty()?std::string("All"):search.substr(0,20))+
      "   OPTIONS Tags: "+(tag.empty()?std::string("All"):tag.substr(0,16))+
      "   SQUARE Previous page",402,1006,18,muted);
  const int begin=onlinePage*12;
  if(filtered.empty())txt(d,f,a,"No matching wallpapers. Try another search or tag.",1102,644,29,white,Align::center);
  for(int pos=0;pos<12&&begin+pos<(int)filtered.size();pos++){
   int idx=filtered[(size_t)(begin+pos)];
   const auto& item=catalog.entries()[(size_t)idx];
   float x=398+(pos%4)*370.f,y=455+(pos/4)*176.f;
   bool focus=onlineSelected==begin+pos&&!sidebar;
   if(focus)d.glow({x,y,348,159},12,15,Color::rgb(0x006dff,.43f));
   panel(d,{x,y,348,159},10,Color::rgb(0x071b37),Color::rgb(focus?0x00afff:0x334e71));
   if(idx<(int)thumbnails.size()&&thumbnails[(size_t)idx]){
     d.image(thumbnails[(size_t)idx],{x+2,y+2,344,155},kFullUv,white,7);
   }else{
     d.gradient_rect({x+2,y+2,344,155},8,Color::rgb(0x05204a),Color::rgb(0x0e3676));
     d.circle(x+174,y+74,23,Color::rgb(0x1369b0));
     d.triangle({x+169,y+57,27,33},white,0,1.5708f);
   }
   d.gradient_rect({x+2,y+104,344,53},5,Color::rgb(0x000000,0.05f),Color::rgb(0x000000,.87f));
   std::string name=item.title;
   if(name.size()>27)name=name.substr(0,24)+"...";
   txt(d,f,a,name,x+13,y+143,19);
   if(catalog.installed(idx)){
     d.circle(x+324,y+23,16,Color::rgb(0x0bcb8e));
     txt(d,f,a,"X",x+324,y+30,18,white,Align::center);
   }
   if(focus)d.bordered_rect({x-2,y-2,352,163},10,Color::rgb(0x000000,0),3,Color::rgb(0x05a5ff));
  }
  if(catalog.downloading()){
    d.bordered_rect({398,969,1470,13},6,Color::rgb(0x112646),1,Color::rgb(0x2d4669));
    d.rounded_rect({400,972,1466*(catalog.progress()/100.f),7},3,Color::rgb(0x00aaff));
  }
 }else if(page==1){
  panel(d,{437,437,1420,470},13,Color::rgb(0x05132c),Color::rgb(0x254970));
  txt(d,f,a,"ThemeChanger  v0.3.22",490,515,38);
  txt(d,f,a,"by YAHBALLON",490,565,28,Color::rgb(0x0da4ff));
  txt(d,f,a,"Native UI: ps5-homebrew-ui  |  Wallpaper engine: ZiZc3 LiveShell",490,610,24,muted);
  txt(d,f,a,"Includes LiveShell engine v1.1. Starts automatically via elfldr.",490,650,24);
  txt(d,f,a,"Autostart needs jailbreak elfldr on 127.0.0.1:9021.",490,708,24,muted);
  txt(d,f,a,"App log: /data/ThemeChanger/themechanger.log",490,775,24,muted);
  txt(d,f,a,"LEFT: Open menu   |   O: Back to Backgrounds",490,848,23,muted);
 }else{
  panel(d,{437,437,1420,475},13,Color::rgb(0x05132c),Color::rgb(0x264a77));
  const char* fields[]={"Wallpaper engine","Darkness","Pause during games","Hide in system settings","Restart LiveShell engine","Clean all downloaded wallpapers"};
  for(int i=0;i<6;i++){
   float y=454+i*74;
   panel(d,{466,y,1358,66},10,Color::rgb(i==opt?0x0b397a:0x071a34),Color::rgb(i==opt?0x10a6ff:0x2b4667));
   txt(d,f,a,fields[i],499,y+47,26);
   txt(d,f,a,i==0?"X  Toggle":i==1?"LEFT / RIGHT":i==4?"X  Retry":i==5?"X  Clean all":"X  Toggle",1796,y+47,22,muted,Align::right);
  }
 }
 }
 d.gradient_rect({0,993,1920,87},0,Color::rgb(0x051221,.92f),Color::rgb(0x020816));
 d.line(0,993,1920,993,1,Color::rgb(0x1c3859));
 d.circle(76,1034,16,white);txt(d,f,a,"X",76,1043,24,Color::rgb(0x021329),Align::center);
 txt(d,f,a,"Select",105,1044,22,muted);
 d.circle(225,1034,16,white);txt(d,f,a,"O",225,1043,24,Color::rgb(0x021329),Align::center);
 txt(d,f,a,"Back",256,1044,22,muted);
 txt(d,f,a,page==4?"L1/R1: Signal tabs   O: Sidebar":page==3?"L1/R1: Pages   Triangle: Search   Options: Tags":
                    "L1/R1: Pages   O: Sidebar   Square: Retry Engine",403,1045,19,muted);
 txt(d,f,a,page==4?(signal.online()?"Signal connected":"Signal starting / offline"):loading?"LiveShell starting...":live?("LiveShell API | ShellUI: "+(shellStatus.empty()?std::string("no status"):shellStatus.substr(0,38))):"LiveShell offline",1856,1028,18,live?Color::rgb(0x4affbc):Color::rgb(0xffce83),Align::right);
 txt(d,f,a,(page==4?signal.message():(page==3||(page==2&&opt==5))?catalog.status():status).substr(0,99),1856,1057,17,muted,Align::right);
 modal(d,f,a,overlay,search,tag,tags,key,tagFocus);
}
}
int main(){
 hui::ps5::Display display;if(!display.open())hui::sys::park();
 Renderer renderer;if(!renderer.init())hui::sys::park();
 const std::string title="/data/homebrew/PPSA99891/assets/";
 auto fonts=read("/app0/assets/fonts/inter-semibold.huifont");
 if(fonts.empty())fonts=read(title+"fonts/inter-semibold.huifont");
 Font font;if(!font.load(fonts))hui::sys::park();
 unsigned atlas=renderer.batch().create_font_texture(font);
 std::string asset="/app0/assets";
 struct stat catalogStat{};
 if(stat((asset+"/motionbgs/catalog.tsv").c_str(),&catalogStat)!=0)
   asset="/data/homebrew/PPSA99891/assets";
 hui::signal::set_paths(asset,"/data/ThemeChanger/signal");
 companion::Panel signal;
 tc::log("BOOT","ThemeChanger v0.3.22 starting");
 const auto storage=wallpaper_storage::recover_indexed();
 tc::log("LIBRARY_RECOVERY",("restored="+std::to_string(storage.restored)+
  " existing="+std::to_string(storage.live)+
  " backups="+std::to_string(storage.backups)+
  " errors="+std::to_string(storage.errors)).c_str());
 motion::Catalog online;
 online.load(asset);
 std::vector<unsigned> onlineTextures(online.entries().size(),0);
 std::deque<int> textureAge;
 std::vector<unsigned char> attemptedTexture(online.entries().size(),0);
 auto loadTexture=[&](int id,bool force=false){
   if(id<0||id>=(int)onlineTextures.size()||onlineTextures[(size_t)id])return;
   if(attemptedTexture[(size_t)id]&&!force)return;
   attemptedTexture[(size_t)id]=1;
   const unsigned tex=texture(renderer,online.thumbnail_path(id));
   if(!tex)return;
   onlineTextures[(size_t)id]=tex;textureAge.push_back(id);
   // Thumbnail JPEGs remain on disk; only ~72 uncompressed RGBA textures
   // remain in GPU RAM, even after browsing thousands of results.
   while(textureAge.size()>72){
     const int old=textureAge.front();textureAge.pop_front();
     GLuint handle=(GLuint)onlineTextures[(size_t)old];
     if(handle)glDeleteTextures(1,&handle);
     onlineTextures[(size_t)old]=0;
     attemptedTexture[(size_t)old]=0;
   }
 };
 std::vector<int> installedIds;
 auto refreshInstalled=[&](){
   std::vector<std::pair<int,std::time_t>> installed;
   // Only downloaded MotionBGS-*.mp4 files are listed; newest first.
   for(int i=0;i<(int)online.entries().size();i++){
     if(!online.installed(i))continue;
     struct stat info{};
     const std::string filename=online.wallpaper_path(i);
     if(stat(filename.c_str(),&info)!=0||info.st_size<1024)continue;
     installed.emplace_back(i,info.st_mtime);
   }
   std::sort(installed.begin(),installed.end(),[](const auto& a,const auto& b){
     if(a.second!=b.second)return a.second>b.second;
     return a.first>b.first;
   });
   installedIds.clear();
   for(const auto& pair:installed)installedIds.push_back(pair.first);
 };
 refreshInstalled();
 tc::log("WALLPAPERS_FOUND",std::to_string(installedIds.size()).c_str());
 std::vector<int> filtered;
 int filterIndex=0, onlineSelected=0, onlinePage=0,sideRow=0;
 const char* categoryKeys[]={"","Featured","Anime","Games","Nature","Cars","Space",
 "Technology","Fantasy","Superhero","TV & Movie","Horror","Animal","Japan","Football","Other"};
 std::unordered_map<std::string,int> tagFrequency;
 for(const auto& entry:online.entries()){
   size_t begin=0;
   while(begin<entry.tags.size()){
     size_t end=entry.tags.find(',',begin);
     const std::string value=entry.tags.substr(begin,end==std::string::npos?std::string::npos:end-begin);
     if(!value.empty())tagFrequency[value]++;
     if(end==std::string::npos)break;
     begin=end+1;
   }
 }
 std::vector<std::pair<std::string,int>> availableTags(tagFrequency.begin(),tagFrequency.end());
 std::sort(availableTags.begin(),availableTags.end(),[](const auto& a,const auto& b){
   if(a.second!=b.second)return a.second>b.second;
   return a.first<b.first;
 });
 Overlay overlay=Overlay::none;
 std::string searchText,selectedTag;
 int keyboardKey=0,tagFocus=0;
 auto rebuildFilter=[&](){
   filtered.clear();
   const std::string term=lower(searchText),wanted=lower(selectedTag);
   for(int i=0;i<(int)online.entries().size();i++){
     const auto& entry=online.entries()[(size_t)i];
     if(filterIndex!=0&&entry.category!=categoryKeys[filterIndex])continue;
     const std::string tags=lower(entry.tags);
     if(!wanted.empty()&&(","+tags+",").find(","+wanted+",")==std::string::npos)continue;
     if(!term.empty()&&lower(entry.title+" "+entry.slug+" "+tags+" "+entry.category).find(term)==std::string::npos)continue;
     filtered.push_back(i);
   }
   onlineSelected=0;onlinePage=0;
 };
 rebuildFilter();
 hui::ps5::Pad pad;pad.open();
 int selected=0,active=-1,page=0,opt=0,dim=25,frame=0;
 bool sidebar=false;bool pendingRetry=false;
 const std::string appRoot=asset.substr(0,asset.size()-7);
 engine::start_if_needed(appRoot);
 auto state=theme::playback_state();
 bool live=state.connected;
 if(live)active=theme::current_theme();
 std::string activeFile=state.file;
 std::string shellStatus=state.shellStatus;
 std::string lastStateLog;
 tc::log("LIVESHELL_START",(live?std::string("Engine revision=")+state.engineRevision+
 " game="+state.game+" ShellUI="+state.shellStatus:"Waiting for bundled engine").c_str());
 if(live&&state.engineRevision!="0.3.6")
   tc::log("ENGINE_LEGACY","LiveShell without bundled fixes detected. Automatic handover requested");
 std::string status=(live&&state.engineRevision!="0.3.6")
   ?"Upgrading outdated LiveShell engine. Please wait..."
   :"Choose a background and press X to apply";
 unsigned previous=0,previousDirections=0;int repeats=0;
 constexpr unsigned arrows=hui::pad_bits::kUp|hui::pad_bits::kDown|
                            hui::pad_bits::kLeft|hui::pad_bits::kRight;
 for(;;){
   std::array<hui::PadSample,64> samples{};const size_t n=pad.read(samples);
   // scePadRead returns a history of samples. OR-ing every sample made a
   // released Cross button look held again, producing repeated APPLY events.
   // Keep the newest connected sample, and preserve state on empty reads.
   unsigned held=previous;
   if(n>0){
     for(size_t i=0;i<n;i++){
       if(!samples[i].connected)continue;
       held=samples[i].buttons;
       if(samples[i].left_x<60)held|=hui::pad_bits::kLeft;
       if(samples[i].left_x>195)held|=hui::pad_bits::kRight;
       if(samples[i].left_y<60)held|=hui::pad_bits::kUp;
       if(samples[i].left_y>195)held|=hui::pad_bits::kDown;
     }
   }
   unsigned pressed=held&~previous;
   const unsigned directions=held&arrows;
   if(directions!=previousDirections){repeats=0;}
   else if(directions){
     ++repeats;
     if(repeats>=20&&((repeats-20)%7)==0)pressed|=directions;
   }else repeats=0;
   previousDirections=directions;
   previous=held;
   static auto signalTime=hui::sys::monotonic_us();
   const auto signalNow=hui::sys::monotonic_us();
   signal.tick(std::clamp((signalNow-signalTime)/1000000.f,0.f,1.f),page==4);signalTime=signalNow;
   if(overlay!=Overlay::none){
     if(overlay==Overlay::cleanup){
       if(pressed&hui::pad_bits::kCircle)overlay=Overlay::none;
       else if(pressed&hui::pad_bits::kCross){
         if(online.clear_downloads())status="Cleaning downloaded wallpapers...";
         else status="Wait for the current download or cleanup to finish";
         overlay=Overlay::none;
       }
     }else if(overlay==Overlay::search){
       const int total=(int)std::string(kKeyChars).size();
       if(pressed&hui::pad_bits::kLeft)keyboardKey=std::max(0,keyboardKey-1);
       if(pressed&hui::pad_bits::kRight)keyboardKey=std::min(total-1,keyboardKey+1);
       if(pressed&hui::pad_bits::kUp)keyboardKey=std::max(0,keyboardKey-10);
       if(pressed&hui::pad_bits::kDown)keyboardKey=std::min(total-1,keyboardKey+10);
       if(pressed&hui::pad_bits::kCross && searchText.size()<48){
         searchText+=kKeyChars[keyboardKey];rebuildFilter();
       }
       if((pressed&hui::pad_bits::kSquare)&&!searchText.empty()){searchText.pop_back();rebuildFilter();}
       if(pressed&hui::pad_bits::kR2){searchText.clear();rebuildFilter();}
       if(pressed&(hui::pad_bits::kTriangle|hui::pad_bits::kCircle))overlay=Overlay::none;
     }else{
       if(!availableTags.empty()){
         if(pressed&hui::pad_bits::kLeft)tagFocus=std::max(0,tagFocus-1);
         if(pressed&hui::pad_bits::kRight)tagFocus=std::min((int)availableTags.size()-1,tagFocus+1);
         if(pressed&hui::pad_bits::kUp)tagFocus=std::max(0,tagFocus-3);
         if(pressed&hui::pad_bits::kDown)tagFocus=std::min((int)availableTags.size()-1,tagFocus+3);
         if(pressed&hui::pad_bits::kL2)tagFocus=std::max(0,tagFocus-18);
         if(pressed&hui::pad_bits::kR2)tagFocus=std::min((int)availableTags.size()-1,tagFocus+18);
         if(pressed&hui::pad_bits::kCross){
           selectedTag=availableTags[(size_t)tagFocus].first;rebuildFilter();overlay=Overlay::none;
         }
       }
       if(pressed&hui::pad_bits::kSquare){selectedTag.clear();rebuildFilter();overlay=Overlay::none;}
       if(pressed&(hui::pad_bits::kCircle|hui::pad_bits::kOptions))overlay=Overlay::none;
     }
   }else{
   if(page==3&&(pressed&hui::pad_bits::kTriangle)){overlay=Overlay::search;keyboardKey=0;}
   else if(page==3&&(pressed&hui::pad_bits::kOptions)){overlay=Overlay::tags;tagFocus=0;}
   else if(navigation::signal_tabs(page,sidebar)){
     if(pressed&hui::pad_bits::kCircle){sidebar=true;sideRow=2;}
     else{
       unsigned keys=0;
       if(pressed&hui::pad_bits::kUp)keys|=companion::Up;
       if(pressed&hui::pad_bits::kDown)keys|=companion::Down;
       if(pressed&hui::pad_bits::kLeft)keys|=companion::Left;
       if(pressed&hui::pad_bits::kRight)keys|=companion::Right;
       if(pressed&hui::pad_bits::kCross)keys|=companion::Confirm;
       if(pressed&hui::pad_bits::kSquare)keys|=companion::Save;
       if(pressed&hui::pad_bits::kTriangle)keys|=companion::Column;
       if(pressed&hui::pad_bits::kOptions)keys|=companion::Add;
       if(pressed&(hui::pad_bits::kL1|hui::pad_bits::kL2))keys|=companion::Prev;
       if(pressed&(hui::pad_bits::kR1|hui::pad_bits::kR2))keys|=companion::Next;
       if(pressed&hui::pad_bits::kR3)keys|=companion::Remove;
       if(pressed&hui::pad_bits::kTouchpad)keys|=companion::Play;
       signal.input(keys);
     }
   }
   else {
   if(pressed&hui::pad_bits::kCircle){
     navigation::back(page,sidebar,sideRow);
     pressed=0; // Back never also activates or exits the current screen.
   }
   if(page!=3&&page!=4&&!sidebar&&(pressed&hui::pad_bits::kSquare)){engine::start_if_needed(appRoot,true);status="Restart requested: "+engine::status();}
   if(pressed&(hui::pad_bits::kL1|hui::pad_bits::kR1)){
     navigation::cycle((pressed&hui::pad_bits::kR1)?1:-1,page,sidebar,sideRow);
     tc::log("TAB",page==4?"Signal Companion":page==3?"Online Wallpapers":page==0?"Backgrounds":page==1?"About":"Settings");
     pressed=0;
   }
   if(page==3&&(pressed&hui::pad_bits::kL2)){filterIndex=(filterIndex+15)%16;rebuildFilter();}
   if(page==3&&(pressed&hui::pad_bits::kR2)){filterIndex=(filterIndex+1)%16;rebuildFilter();}
   if(page==3&&(pressed&hui::pad_bits::kSquare)&&!filtered.empty()){
     onlinePage=std::max(0,onlinePage-1);
     onlineSelected=onlinePage*12;
   }
   if(!sidebar&&page==1&&(pressed&hui::pad_bits::kLeft)){sidebar=true;sideRow=3;}
   if(!sidebar&&page==2&&opt!=1&&(pressed&hui::pad_bits::kLeft)){sidebar=true;sideRow=4;}
   if(sidebar){
       if(pressed&hui::pad_bits::kUp)sideRow=std::max(0,sideRow-1);
       if(pressed&hui::pad_bits::kDown)sideRow=std::min(4,sideRow+1);
       if(pressed&(hui::pad_bits::kCross|hui::pad_bits::kRight)){
         navigation::open(sideRow,page,sidebar,sideRow);
         tc::log("TAB",(page==3?"Online Wallpapers":page==0?"Backgrounds":page==1?"About":page==4?"Signal Companion":"Settings"));
       }
   }else if(page==0||page==3){
     if(page==3){
       if(pressed&hui::pad_bits::kLeft){
         if(onlineSelected%4==0){sidebar=true;sideRow=1;}
         else onlineSelected=std::max(0,onlineSelected-1);
       }
       if(pressed&hui::pad_bits::kRight)onlineSelected=std::min((int)filtered.size()-1,onlineSelected+1);
       if(pressed&hui::pad_bits::kUp)onlineSelected=std::max(0,onlineSelected-4);
       if(pressed&hui::pad_bits::kDown)onlineSelected=std::min((int)filtered.size()-1,onlineSelected+4);
       if(!filtered.empty())onlinePage=onlineSelected/12;
       if((pressed&hui::pad_bits::kCross)&&!filtered.empty()){
         if(!online.download(filtered[(size_t)onlineSelected]))status="Download already running";
       }
     }else{
     const int total=(int)installedIds.size();
     if(pressed&hui::pad_bits::kLeft){
       if(selected%4==0){sidebar=true;sideRow=0;}
       else selected=std::max(0,selected-1);
     }
     if(total>0){
       if(pressed&hui::pad_bits::kRight)selected=std::min(total-1,selected+1);
       if(pressed&hui::pad_bits::kUp)selected=std::max(0,selected-4);
       if(pressed&hui::pad_bits::kDown)selected=std::min(total-1,selected+4);
       if((pressed&hui::pad_bits::kCross)&&selected>=0&&selected<total){
         const auto id=installedIds[(size_t)selected];
         const auto name="MotionBGS-"+online.entries()[(size_t)id].slug+".mp4";
         if(online.cleaning()){status="Wait for wallpaper cleanup to finish";}
         else if(theme::activate_file(name,status)){
           activeFile=name;active=-1;
           tc::log("SELECT_DOWNLOADED",name.c_str());
         }else tc::log("SELECT_FAILED",(name+" "+status).c_str());
       }
     }
     }
   }else if(page==2){
     if(pressed&hui::pad_bits::kUp)opt=std::max(0,opt-1);
     if(pressed&hui::pad_bits::kDown)opt=std::min(5,opt+1);
     if(opt==1&&(pressed&(hui::pad_bits::kLeft|hui::pad_bits::kRight))){
       dim=std::clamp(dim+((pressed&hui::pad_bits::kRight)?5:-5),0,80);
       theme::send_setting("/api/dim?v="+std::to_string(dim),status);
     }
     if(pressed&hui::pad_bits::kCross){
       const char* routes[]={"/api/on","", "/api/set?k=gamepause&v=1","/api/set?k=menuhide&v=1"};
       if(opt==5){
         if(online.downloading()||engine::busy())status="Wait for the current download or engine startup to finish";
         else overlay=Overlay::cleanup;
       }else if(opt==4){engine::start_if_needed(appRoot,true);status="Restart requested: "+engine::status();}
       else if(opt!=1){
         if(opt==0)theme::send_setting(theme::playback_state().enabled?"/api/off":"/api/on",status);
         else if(opt==2)theme::send_setting(std::string("/api/set?k=gamepause&v=")+(theme::playback_state().gamePause?"0":"1"),status);
         else theme::send_setting(routes[opt],status);
       }
       live=theme::probe();
     }
   }
   }
   }
   static bool wasCleaning=false;
   const bool cleaning=online.cleaning();
   if(wasCleaning&&!cleaning){refreshInstalled();selected=0;activeFile.clear();active=-1;status=online.status();}
   wasCleaning=cleaning;
   if(++frame%120==0){
     refreshInstalled();
     selected=std::clamp(selected,0,std::max(0,(int)installedIds.size()-1));
     state=theme::playback_state();
     live=state.connected;
     if(live){
      activeFile=state.file;
      shellStatus=state.shellStatus;
      active=-1;
      for(int k=0;k<theme::kCount;k++)if(activeFile==theme::theme_file(k))active=k;
      const std::string stateText=std::string("api_v11=")+(state.mainVideoSupported?"1":"0")+" file="+activeFile+
        " shellui="+shellStatus+" enabled="+(state.enabled?"1":"0")+
        " mainvideo="+(state.mainVideo?"1":"0")+
        " gamepause="+(state.gamePause?"1":"0")+" game="+state.game;
      if(stateText!=lastStateLog){
        tc::log("SHELLUI_MONITOR",(stateText+" engine="+state.engineRevision).c_str());
        lastStateLog=stateText;
        if(state.engineRevision=="0.3.6"&&engine::busy()==false&&status.find("Upgrading")!=std::string::npos)status="LiveShell engine updated. Select a wallpaper";
      }
     }else{
       shellStatus.clear();
       if(!engine::busy())status=engine::status();
     }
   }
   // Download-complete event is emitted immediately by the worker and is
   // independent of whether LiveShell could activate the wallpaper.
   if(frame%3==0)online.discover_legacy(12);
   for(int idx:online.consume_installed()){
     refreshInstalled();
     if(page==0)selected=0;
     tc::log("LIBRARY_UPDATED",("Downloaded "+online.entries()[(size_t)idx].slug+
       " | total="+std::to_string(installedIds.size())).c_str());
   }
   for(int idx:online.consume_ready()){
     if(idx>=0&&idx<(int)onlineTextures.size()&&!onlineTextures[(size_t)idx]){
       loadTexture(idx,true);
       if(!onlineTextures[(size_t)idx])
         tc::log("PREVIEW_DECODE_ERROR",online.entries()[(size_t)idx].slug.c_str());
     }
   }
   if(page==3&&!filtered.empty()){
     const int begin=onlinePage*12;
     for(int i=begin;i<(int)filtered.size()&&i<begin+12;i++)
       loadTexture(filtered[(size_t)i]);
     if(onlineSelected>=0&&onlineSelected<(int)filtered.size())
       loadTexture(filtered[(size_t)onlineSelected]);
   }else if(page==0){
     const int begin=(selected/12)*12;
     for(int q=std::max(0,begin-12);q<(int)installedIds.size()&&q<begin+12;q++)
       loadTexture(installedIds[(size_t)q]);
   }
   if(!online.thumbnail_busy()){
     // Prioritize the focused wallpaper, then fill the visible rows.
     if(page==3&&!filtered.empty()&&onlineSelected<(int)filtered.size()){
       online.request_thumbnail(filtered[(size_t)onlineSelected]);
     }
     if(!online.thumbnail_busy()){
       if(page==3){
         const int first=onlinePage*12;
         for(int q=first;q<(int)filtered.size()&&q<first+12;q++){
           const int id=filtered[(size_t)q];
           if(id>=0&&id<(int)onlineTextures.size()&&!onlineTextures[(size_t)id]&&
               online.request_thumbnail(id))break;
         }
       }else if(page==0){
         const int first=(selected/12)*12;
         for(int q=std::max(0,first-12);q<(int)installedIds.size()&&q<first+12;q++){
           const int id=installedIds[(size_t)q];
           if(id>=0&&id<(int)onlineTextures.size()&&!onlineTextures[(size_t)id]&&
               online.request_thumbnail(id))break;
         }
       }
     }
   }
   DrawList ui;draw(ui,font,atlas,selected,page,opt,status,live,engine::busy(),sidebar,
                     online,filtered,onlineTextures,onlineSelected,onlinePage,filterIndex,sideRow,installedIds,activeFile,shellStatus,
                    overlay,searchText,selectedTag,availableTags,keyboardKey,tagFocus,signal);
   renderer.begin();renderer.draw(ui);renderer.present(0,display.width(),display.height());display.swap();hui::sys::hide_splash_screen();hui::sys::sleep_us(16000);
 }
}

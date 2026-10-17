// SPDX-License-Identifier: GPL-3.0-or-later
// ThemeChanger presentation/controller for Marice's GPL Signal Companion backend.
#include "signal_panel.hpp"
#include <algorithm>
#include "theme_log.hpp"
namespace companion {
using namespace hui::signal;
namespace {
constexpr Mix presets[]={{255,0,0},{0,255,0},{0,0,255},{255,128,0},{255,0,128},{0,128,255},{0,0,0}};
constexpr const char* presetNames[]={"Blue","White","Amber","Ice","Violet mix","Warm white","Off"};
std::string channels(const Mix& m){return std::to_string(m.blue)+" / "+std::to_string(m.white)+" / "+std::to_string(m.amber);}
}
int Panel::count() const {
 if(tab_==0)return autoload_.found?8:7;
 if(tab_==1)return 10;
 return (int)(lights_?draft_.leds.size():draft_.beeps.size())+4;
}
void Panel::tick(float dt,bool visible){
 if(!link_){if(!visible)return;link_=&link();}
 if(!visible&&preview_){preview_=false;previewDirty_=false;if(online())send(Call::stop);}
 if(!loaded_){
  palette_=load_palette();autoload_=autoload_state();loaded_=true;
  tc::log("SIGNAL_SESSION","LED diagnostics enabled; log=/data/ThemeChanger/signal.log");
 }
 link_->update(dt);
 if(visible&&!attemptedStart_&&link_->state()==LinkState::offline){attemptedStart_=true;start();}

 if(link_->config_serial()!=configSeen_){
  configSeen_=link_->config_serial();
  tc::log("SIGNAL_CONFIG",("hold="+std::to_string(link_->config().hold)+" held="+channels(link_->config().held)+" beeps="+std::to_string(link_->config().beeps.size())+" lights="+std::to_string(link_->config().leds.size())).c_str());
  if(!startupDirty_){draft_.beeps=link_->config().beeps;draft_.leds=link_->config().leds;}
  if(!mixDirty_)mix_=link_->config().hold?link_->config().held:Mix{255,0,0};
 }
 row_=std::clamp(row_,0,count()-1);
 if(link_->starting())detail_="Starting ps5-signal via the console's ELF loader...";
 else if(!link_->start_error().empty())detail_=link_->start_error();
 else if(link_->online()){
  const auto& s=link_->status();
  detail_="ps5-signal "+s.version+" | LEDs: "+(s.color_available?channels(s.color):std::string("unavailable"))+
          " | Beeper: "+(s.muted==1?"muted":"ready");
 }else detail_=link_->state()==LinkState::checking?"Checking ps5-signal...":"Offline | Start ps5-signal below (requires jailbreak ELF loader)";
 for(size_t i=2;i<seen_.size();i++){
  const auto call=(Call)i;const auto outcome=link_->service().outcome(call);
  if(outcome.serial==seen_[i])continue;seen_[i]=outcome.serial;
  if(call==Call::launch||call==Call::quit)continue;
  if(outcome.reply.status>=200&&outcome.reply.status<300){
   if(call==Call::compat_enable){compatActive_=true;message_="LED test enabled. Adjust Colour; use Restore LED state to undo.";}
   else if(call==Call::compat_restore){compatActive_=false;message_="Original LED state restored; test mode disabled";}
   else message_=call==Call::save_config?"Saved to ps5-signal. Applies on its next start.":"Signal command accepted";
  }else{
   const auto error=error_text(outcome.reply.body);
   message_=error.empty()?"Command failed: ps5-signal unavailable or request rejected":error;
  }
 }
 previewAge_+=dt;
 if(visible&&preview_&&previewDirty_&&previewAge_>=.5f&&online()){
  send(Call::color,color_request(mix_));previewDirty_=false;previewAge_=0;
 }
}
void Panel::send(Call call,std::string body){
 if(!online()){tc::log("SIGNAL_BLOCKED",("call="+std::to_string((int)call)+" offline desired="+channels(mix_)).c_str());message_="Start ps5-signal on the Status tab first";return;}
 link_->service().send(call,std::move(body));message_="Sending...";
}
void Panel::save(bool startup,bool system){
 if(compatActive_){message_="Test mode: preview only. Restore LED state before saving configuration.";return;}
 if(!online()||!link_->has_config()){message_="Wait for the service configuration before saving";return;}
 if(link_->service().pending(Call::save_config)){message_="A save is already in progress";return;}
 // Keep the other section exactly as last fetched from the service.
 Config config=link_->config();
 if(startup){config.beeps=draft_.beeps;config.leds=draft_.leds;}
 else{config.hold=!system;config.held=mix_;}
 Config checked;std::string error;const auto json=config_json(config);
 if(!parse_config(json,checked,error)){message_=error;return;}
 send(Call::save_config,json);
 if(!startup)send(Call::color,system?system_color_request():color_request(mix_));
}
void Panel::edit(int direction){
 if(tab_==1){
  if(row_<3){
   int* value=row_==0?&mix_.blue:row_==1?&mix_.white:&mix_.amber;
   *value=std::clamp(*value+direction*5,0,255);mixDirty_=true;previewDirty_=true;preview_=true;
  }else if(row_==6){preset_=(preset_+direction+7)%7;mix_=presets[preset_];mixDirty_=true;previewDirty_=true;preview_=true;}
  else if(row_==7)slot_=(slot_+direction+kPaletteSlots)%kPaletteSlots;
  tc::log("SIGNAL_COLOUR_EDIT",("row="+std::to_string(row_)+" blue/white/amber="+channels(mix_)+" preview="+std::to_string(preview_)+" online="+std::to_string(online())).c_str());
 }else if(tab_==2){
  if(row_>=(int)(lights_?draft_.leds.size():draft_.beeps.size()))return;
  Config candidate=draft_;
  if(lights_&&!candidate.leds.empty()){
   auto& s=candidate.leds[(size_t)row_];
   if(field_==0)s.kind=(LedKind)(((int)s.kind+direction+3)%3);
   else if(field_==1){
    if(s.kind==LedKind::named)s.name=(LedName)(((int)s.name+direction+4)%4);
    else if(s.kind==LedKind::level)s.level=(s.level+direction+3)%3;
    else s.mix.blue=std::clamp(s.mix.blue+direction*5,0,255);
   }else if(field_==2)s.ms=std::clamp(s.ms+direction*50,kMinStepMs,kMaxStepMs);
   else if(s.kind==LedKind::mix){
    int& value=field_==3?s.mix.white:s.mix.amber;
    value=std::clamp(value+direction*5,0,255);
   }
  }else if(!lights_&&!candidate.beeps.empty()){
   auto& s=candidate.beeps[(size_t)row_];
   if(field_==0)s.tone=(s.tone+direction+kTones)%kTones;
   else s.pause_ms=std::clamp(s.pause_ms+direction*50,kMinPauseMs,kMaxPauseMs);
  }
  if(tune_ms(candidate.beeps)>kMaxTotalMs||light_ms(candidate.leds)>kMaxTotalMs){
   message_="Each startup sequence must stay within 10 seconds";return;
  }
  draft_=std::move(candidate);startupDirty_=true;
 }
}
void Panel::input(unsigned keys){
 if(!link_)return;
 if(keys&(Prev|Next)){if(preview_){preview_=false;previewDirty_=false;if(online())send(Call::stop);}tab_=(tab_+((keys&Next)?1:2))%3;row_=field_=0;return;}
 if(keys&Up){row_=std::max(0,row_-1);field_=0;}
 if(keys&Down){row_=std::min(count()-1,row_+1);field_=0;}
 if(tab_==0){
  if(!(keys&Confirm))return;
  switch(row_){
   case 0:if(online())link_->restart_service();else link_->start_service();break;
   case 1:link_->refresh();message_="Refreshing status";break;
   case 2:send(Call::beep,beep_request({{1,200},{3,200}}));break;
   case 3:{LedStep step;step.name=LedName::blue;step.ms=500;send(Call::led,led_request({step}));break;}
   case 4:preview_=false;send(Call::stop);break;
   case 5:
    if(link_->status().version.find("tc-compat1")==std::string::npos){message_="Choose Restart ps5-signal to load the test service first";break;}
    preview_=false;send(Call::compat_enable);break;
   case 6:
    if(link_->status().version.find("tc-compat1")==std::string::npos){message_="Restart ps5-signal first";break;}
    preview_=false;previewDirty_=false;send(Call::compat_restore);break;
   case 7:
    if(!autoload_.found){message_="ps5-autoloader list unavailable. Allow /data in the sandbox.";break;}
    {const auto error=autoload_.listed?autoload_remove():autoload_add();
     message_=error.empty()?"Autoloader updated":error;autoload_=autoload_state();}break;
  }
 }else if(tab_==1){
  if(keys&Left)edit(-1);if(keys&Right)edit(1);
  if(keys&Save)save(false);
  if(keys&Confirm){
   if(row_<3)edit(1);
   else if(row_==3){
    preview_=!preview_;previewDirty_=preview_;
    if(!preview_)send(Call::stop);
   }else if(row_==4)save(false);
   else if(row_==5){preview_=false;save(false,true);}
   else if(row_==6)edit(1);
   else if(row_==8){
    if(palette_.slots[(size_t)slot_]){mix_=*palette_.slots[(size_t)slot_];mixDirty_=true;previewDirty_=true;preview_=true;}
    else message_="This colour slot is empty";
   }else if(row_==9){
    palette_.slots[(size_t)slot_]=mix_;const auto error=save_palette(palette_);
    message_=error.empty()?"Colour saved in ThemeChanger":error;
   }
  }
 }else{
  if(!link_->has_config()){message_="Connecting: wait for the startup configuration";return;}
  if(keys&Column){lights_=!lights_;row_=field_=0;}
  const int steps=(int)(lights_?draft_.leds.size():draft_.beeps.size());
  if((keys&Confirm)&&row_>=steps){
   const int action=row_-steps;keys&=~Confirm;
   if(action==0)keys|=Add;
   else if(action==1)keys|=Play;
   else if(action==2)keys|=Save;
   else{if(lights_)draft_.leds.clear();else draft_.beeps.clear();startupDirty_=true;row_=0;return;}
  }
  if(keys&Confirm)field_=(field_+1)%(lights_?(!draft_.leds.empty()&&draft_.leds[(size_t)row_].kind==LedKind::mix?5:3):2);
  if(keys&Left)edit(-1);if(keys&Right)edit(1);
  if(keys&Add){
   if(lights_){
    if(draft_.leds.size()<kMaxLedSteps&&light_ms(draft_.leds)+400<=kMaxTotalMs){draft_.leds.push_back({});row_=(int)draft_.leds.size()-1;}
    else{message_="Maximum 24 light steps / 10 seconds";return;}
   }else{
    auto next=draft_.beeps;next.push_back({});
    if(next.size()<=kMaxConfigBeeps&&tune_ms(next)<=kMaxTotalMs){draft_.beeps=std::move(next);row_=(int)draft_.beeps.size()-1;}
    else{message_="Maximum 24 beeps / 10 seconds";return;}
   }
   startupDirty_=true;
  }
  if(keys&Remove){
   if(lights_&&row_<(int)draft_.leds.size())draft_.leds.erase(draft_.leds.begin()+row_);
   else if(!lights_&&row_<(int)draft_.beeps.size())draft_.beeps.erase(draft_.beeps.begin()+row_);
   row_=std::clamp(row_,0,std::max(0,(int)(lights_?draft_.leds.size():draft_.beeps.size())-1));startupDirty_=true;
  }
  if(keys&Save)save(true);
  if(keys&Play){
   if(lights_&&!draft_.leds.empty())send(Call::led,led_request(draft_.leds));
   else if(!lights_&&!draft_.beeps.empty())send(Call::beep,beep_request(draft_.beeps));
   else message_="Add at least one step to preview";
  }
 }
}
std::vector<std::pair<std::string,std::string>> Panel::rows() const {
 if(tab_==0){
  std::vector<std::pair<std::string,std::string>> out={{online()?"Restart ps5-signal":"Start ps5-signal",link_&&link_->starting()?"Starting...":"X"},
   {"Refresh status","X"},{"Test beeper","X"},{"Test blue light","X"},{"Stop previews","X"},
   {"Enable 5.10 LED test (experimental)",compatActive_?"Enabled":"X"},{"Restore LED state","X"}};
  if(autoload_.found)out.push_back({"Start with ps5-autoloader",autoload_.listed?"On":"Off"});
  return out;
 }
 if(tab_==1)return {{"Blue",std::to_string(mix_.blue)},{"White",std::to_string(mix_.white)},{"Amber",std::to_string(mix_.amber)},
  {"Show colour on console",preview_?"On":"Off"},{"Keep this colour","X / Square"},{"Use system colour","X"},
  {"Colour preset",presetNames[preset_]},{"My colour slot",std::to_string(slot_+1)+" / 6"},{"Load my colour","X"},{"Save to my colour slot","X"}};
 std::vector<std::pair<std::string,std::string>> out;
 if(lights_)for(size_t i=0;i<draft_.leds.size();i++){
  const auto& s=draft_.leds[i];out.push_back({"Light "+std::to_string(i+1),led_step_text(s)+" | "+std::to_string(s.ms)+" ms"});
 }else for(size_t i=0;i<draft_.beeps.size();i++){
  const auto& s=draft_.beeps[i];out.push_back({"Beep "+std::to_string(i+1),std::string(tone_name(s.tone))+" | "+std::to_string(s.pause_ms)+" ms"});
 }
 out.push_back({"Add step","X / Options"});
 out.push_back({"Preview sequence","X / Touchpad"});
 out.push_back({"Save startup","X / Square"});
 out.push_back({"Clear sequence (save to apply)","X"});
 return out;
}
std::string Panel::help() const {
 if(tab_==0)return "X: Select   |   L1/R1: Tabs   |   O: Sidebar";
 if(tab_==1)return "Left/Right: Adjust   X: Select   Square: Keep colour   L1/R1: Tabs   O: Sidebar";
 const char* field=lights_?(field_==0?"Kind":field_==1?"Colour / Blue / Level":field_==2?"Duration":field_==3?"White":"Amber"):(field_==0?"Tone":"Pause");
 return std::string("Editing: ")+field+" | X: Field  Left/Right: Adjust  Triangle: Beeps/Lights";
}
}

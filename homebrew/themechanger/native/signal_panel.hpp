// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "signal/service.hpp"
#include "signal/palette.hpp"
#include "signal/autoload.hpp"
#include <array>
#include <utility>
namespace companion {
enum Key : unsigned { Up=1,Down=2,Left=4,Right=8,Confirm=16,Save=32,Column=64,Add=128,Prev=256,Next=512,Remove=1024,Play=2048 };
class Panel {
public:
 explicit Panel(hui::signal::Link* link=nullptr):link_(link){}
 void tick(float dt,bool visible);
 void input(unsigned keys);
 void start(){if(link_&&!link_->online()&&!link_->starting())link_->start_service();}
 std::vector<std::pair<std::string,std::string>> rows() const;
 int tab() const{return tab_;}
 int row() const{return row_;}
 bool light_column() const{return lights_;}
 const std::string& message() const{return message_;}
 const std::string& detail() const{return detail_;}
 bool online() const{return link_&&link_->online();}
 const hui::signal::Mix& mix() const{return mix_;}
 const hui::signal::Config& draft() const{return draft_;}
 std::string help() const;
private:
 void send(hui::signal::Call call,std::string body={});
 void save(bool startup,bool system=false);
 void edit(int direction);
 int count() const;
 hui::signal::Link* link_=nullptr;
 hui::signal::Config draft_;
 hui::signal::Mix mix_{255,0,0};
 hui::signal::Palette palette_;
 hui::signal::AutoloadState autoload_;
 std::array<unsigned,(size_t)hui::signal::Call::count> seen_{};
 unsigned configSeen_=0;
 int tab_=0,row_=0,field_=0,preset_=0,slot_=0;
 bool compatActive_=false;
 bool attemptedStart_=false,loaded_=false,preview_=false,mixDirty_=false,startupDirty_=false,lights_=false;
 float previewAge_=0;
 bool previewDirty_=false;
 std::string message_="After updating ThemeChanger, choose Restart ps5-signal once";
 std::string detail_="Checking ps5-signal...";
};
}

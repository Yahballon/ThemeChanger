// SPDX-License-Identifier: GPL-3.0-or-later
#include "signal_panel.hpp"
#include "fake_signal.hpp"
#include "core/save_file.hpp"
#include <cassert>
#include <cstdio>
#include <unistd.h>
using namespace companion;
using namespace hui::signal;
void settle(Link& link,Panel& panel){
 for(int i=0;i<6;i++){link.service().wait_idle();panel.tick(.01f,true);}
}
int main(){
 const std::string dir="/tmp/themechanger-signal-test-"+std::to_string(getpid());
 assert(hui::save::ensure_directory(dir));set_paths(dir,dir);
 auto transport=std::make_unique<hui::testing::FakeSignal>();auto* fake=transport.get();
 Link link(std::move(transport));Panel panel(&link);
 panel.tick(.01f,true);settle(link,panel);assert(panel.online());
 const Config initial=link.config();
 assert(!initial.beeps.empty()&&!initial.leds.empty());
 panel.input(Next);assert(panel.tab()==1);
 panel.input(Left);assert(panel.mix().blue==250);
 panel.input(Save);settle(link,panel);
 Config saved;std::string error;
 assert(parse_config(fake->saved(),saved,error));
 assert(saved.beeps==initial.beeps&&saved.leds==initial.leds); // Colour saves preserve startup.
 assert(saved.hold&&saved.held.blue==250);
 panel.input(Next);assert(panel.tab()==2);
 panel.input(Right); // Change first beep, then save without touching the held colour.
 panel.input(Save);settle(link,panel);
 assert(parse_config(fake->saved(),saved,error));
 assert(saved.beeps[0].tone!=initial.beeps[0].tone);
 assert(saved.hold&&saved.held.blue==250);
 panel.input(Column);assert(panel.light_column());
 for(int i=0;i<80;i++)panel.input(Add);
 assert(panel.draft().leds.size()<=24&&light_ms(panel.draft().leds)<=10000);
 panel.input(Confirm);panel.input(Confirm); // Duration field
 for(int i=0;i<250;i++)panel.input(Right);
 assert(light_ms(panel.draft().leds)<=10000);
 while(!panel.draft().leds.empty())panel.input(Remove);
 panel.input(Save);settle(link,panel);
 assert(parse_config(fake->saved(),saved,error));assert(saved.leds.empty());
 // Background effects have independent apply/save/restore actions.
 panel.input(Next);assert(panel.tab()==3);
 panel.input(Right);assert(panel.rows()[0].second=="Pulse");
 panel.input(Down);panel.input(Right);assert(panel.rows()[1].second=="4500 ms");
 panel.input(Down);panel.input(Confirm);settle(link,panel);
 auto effectRequests=fake->requests();bool applied=false;
 for(const auto& request:effectRequests)if(request.find("POST /effect")!=std::string::npos&&request.find("\"mode\":1")!=std::string::npos&&request.find("\"save\":false")!=std::string::npos)applied=true;
 assert(applied);
 panel.input(Down);panel.input(Confirm);settle(link,panel);
 effectRequests=fake->requests();assert(effectRequests.back().find("\"save\":true")!=std::string::npos);
 panel.input(Down);panel.input(Confirm);settle(link,panel);
 effectRequests=fake->requests();assert(effectRequests.back().find("\"mode\":0")!=std::string::npos);
 // No service: do not send default configuration over a previously saved one.
 fake->set_running(false);link.refresh();settle(link,panel);assert(!panel.online());
 const auto before=fake->saved();panel.input(Save);settle(link,panel);
 assert(fake->saved()==before);
 // Disabling auto-start preserves the other payloads and their order.
 const std::string list="one.elf\n!1200\ntwo.elf\n";
 const auto enabled=autoload_with(list);
 assert(enabled==list+"!3000\nps5-signal.elf\n");
 assert(autoload_with(enabled)==enabled);
 assert(autoload_without(enabled)==list);
 Palette palette;palette.slots[3]=Mix{0,128,255};
 assert(save_palette(palette).empty());assert(load_palette()==palette);
 std::remove((dir+"/colours.json").c_str());rmdir(dir.c_str());
 puts("Signal integration: section-preserving saves, editor limits, offline protection, palette and autoloader passed");
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "signal/service.hpp"
namespace hui::signal {
inline std::string colour_diagnostic(std::string_view requested,const Reply& reply){
 Status state;
 if(reply.status!=200||!parse_status(reply.body,state))
  return "readback_failed http="+std::to_string(reply.status)+" body="+reply.body.substr(0,8192);
 JsonValue desired;
 const bool parsed=parse_json(requested,desired);
 const bool system=parsed&&desired["system"].as_bool();
 const bool matches=parsed&&!system&&state.color_available&&
   state.color.blue==desired["blue"].as_number()&&state.color.white==desired["white"].as_number()&&
   state.color.amber==desired["amber"].as_number();
 return std::string("requested=")+std::string(requested)+" reported="+
   std::to_string(state.color.blue)+"/"+std::to_string(state.color.white)+"/"+std::to_string(state.color.amber)+
   " led_available="+std::to_string(state.led_available)+" color_available="+std::to_string(state.color_available)+
   " level="+std::to_string(state.level)+" lighting="+std::to_string(state.lighting)+
   " hold="+std::to_string(state.hold)+" service="+state.version+
   " result="+(system?"system_colour_requested":!state.color_available?"colour_control_unavailable":matches?"reported_values_match":"reported_values_differ")+
   " physical_LED_not_verified raw="+reply.body.substr(0,8192);
}
}

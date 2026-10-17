// SPDX-License-Identifier: GPL-3.0-or-later
#include "signal/service.hpp"
#include "signal/diagnostics.hpp"
#include "fake_signal.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <thread>
#include <string>
#include <cstdio>
int listener(int& port){
 int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(fd,(sockaddr*)&addr,sizeof(addr))==0);assert(listen(fd,2)==0);
 socklen_t len=sizeof(addr);assert(getsockname(fd,(sockaddr*)&addr,&len)==0);port=ntohs(addr.sin_port);return fd;
}
int main(){
 const auto body=hui::testing::FakeSignal::status_json(true,{12,34,56});
 assert(hui::signal::colour_diagnostic("{\"blue\":12,\"white\":34,\"amber\":56}",{200,body}).find("reported_values_match")!=std::string::npos);
 assert(hui::signal::colour_diagnostic("{\"blue\":255,\"white\":0,\"amber\":0}",{200,body}).find("reported_values_differ")!=std::string::npos);
 assert(hui::signal::colour_diagnostic("{}",{-1,{}}).find("readback_failed")!=std::string::npos);
 auto unavailable=body;const auto at=unavailable.find("\"color\":{\"available\":true");assert(at!=std::string::npos);
 unavailable.replace(at,25,"\"color\":{\"available\":false");
 assert(hui::signal::colour_diagnostic("{}",{200,unavailable}).find("colour_control_unavailable")!=std::string::npos);

 int port;int server=listener(port);
 std::thread http([&]{
  const char* codes[]={"200 OK","202 Accepted","400 Bad Request"};
  for(int i=0;i<3;i++){
   int fd=accept(server,nullptr,nullptr);assert(fd>=0);std::string request;char buf[1024];
   while(request.find("\r\n\r\n")==std::string::npos){auto n=recv(fd,buf,sizeof(buf),0);assert(n>0);request.append(buf,n);}
   assert(request.rfind(i==0?"GET /status ":"POST /color ",0)==0);
   const std::string body=i==2?"{\"error\":\"invalid colour\"}":"{\"ok\":true}";
   const std::string response="HTTP/1.1 "+std::string(codes[i])+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
   // Fragmented replies must parse identically.
   for(char c:response)assert(send(fd,&c,1,0)==1);
   close(fd);
  }
 });
 auto transport=hui::signal::make_http_transport(port);
 assert(transport->send("GET","/status",{}).status==200);
 assert(transport->send("POST","/color","{}").status==202);
 const auto rejected=transport->send("POST","/color","{}");assert(rejected.status==400);
 assert(hui::signal::error_text(rejected.body)=="invalid colour");
 http.join();close(server);
 server=listener(port);std::string received;
 std::thread loader([&]{int fd=accept(server,nullptr,nullptr);assert(fd>=0);char buf[64];ssize_t n;
  while((n=recv(fd,buf,sizeof(buf),0))>0)received.append(buf,n);
  assert(send(fd,"OK",2,0)==2);close(fd);
 });
 const std::string elf="fake-elf-loader-test";
 assert(transport->push(port,elf).status==200);
 loader.join();close(server);assert(received==elf);
 puts("Signal native transport: fragmented HTTP 200/202/400 and loader EOF handoff passed");
}

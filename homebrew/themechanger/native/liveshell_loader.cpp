// SPDX-License-Identifier: GPL-3.0-or-later
// Bundled engine launcher - sends the upstream LiveShell ELF to local elfldr.
// The upstream GPL-3.0 payload is attributed and verified during the build.
#include "liveshell_loader.hpp"
#include "liveshell_client.hpp"
#include "theme_log.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
namespace engine {
namespace {
std::mutex g_lock;
std::string g_status="Bundled LiveShell engine ready";
std::atomic<bool> g_busy{false};
std::atomic<bool> g_attempted{false};
struct Job{std::string root;};
void set_status(const std::string& s){std::lock_guard<std::mutex> lock(g_lock);g_status=s;}
void* run(void* raw) {
 Job job=*static_cast<Job*>(raw);delete static_cast<Job*>(raw);
 // The runtime's default mount is /app0, ShadowMount may map /data/homebrew.
 std::string path=job.root+"/liveshell.elf";
 FILE* in=fopen(path.c_str(),"rb");
 if(!in){set_status("Bundled liveshell.elf missing from app install");g_busy=false;return nullptr;}
 if(fseek(in,0,SEEK_END)!=0){fclose(in);set_status("Cannot read LiveShell payload");g_busy=false;return nullptr;}
 long size=ftell(in);rewind(in);
 if(size<4096||size>4*1024*1024){fclose(in);set_status("Unexpected LiveShell payload size");g_busy=false;return nullptr;}
 unsigned char hdr[5]{};
 if(fread(hdr,1,5,in)!=5||memcmp(hdr,"\x7f""ELF",4)!=0||hdr[4]!=2){
  fclose(in);set_status("Bundled payload is not a 64-bit ELF");g_busy=false;return nullptr;
 }
 rewind(in);
 auto existing=theme::playback_state();
 if(existing.connected&&existing.engineRevision=="0.3.6"){
  fclose(in);set_status("Integrated LiveShell 0.3.6 already running");g_busy=false;return nullptr;
 }
 if(existing.connected){
  set_status("Updating old LiveShell engine via safe handover...");
  tc::log("ENGINE_HANDOVER",(std::string("old revision=")+existing.engineRevision+
     " game="+existing.game).c_str());
 }
 int fd=socket(AF_INET,SOCK_STREAM,0);
 if(fd<0){fclose(in);set_status("Cannot open ELF loader socket");g_busy=false;return nullptr;}
 timeval timeout{5,0};
 setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
 setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(9021);
 addr.sin_addr.s_addr=htonl(0x7f000001);
 if(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0){
  close(fd);fclose(in);
  set_status("Autostart needs jailbreak elfldr at 127.0.0.1:9021");
  g_busy=false;return nullptr;
 }
 set_status("Starting integrated LiveShell engine...");
 unsigned char buf[16384];long sent=0;bool ok=true;
 while(sent<size){
  size_t n=fread(buf,1,sizeof(buf),in);
  if(n==0){ok=false;break;}
  size_t off=0;
  while(off<n){
   ssize_t k=send(fd,buf+off,n-off,0);
   if(k<=0){ok=false;break;}
   off+=static_cast<size_t>(k);
  }
  if(!ok)break;
  sent+=static_cast<long>(n);
 }
 fclose(in);
 shutdown(fd,SHUT_WR);
 char reply[1024];(void)recv(fd,reply,sizeof(reply),0);
 close(fd);
 if(!ok||sent!=size){set_status("LiveShell ELF transfer failed");g_busy=false;return nullptr;}
 set_status("Waiting for LiveShell to initialize ShellUI...");
 for(int retry=0;retry<45;retry++){
  auto state=theme::playback_state();
  if(state.connected&&state.engineRevision=="0.3.6"){
    set_status("Integrated LiveShell 0.3.6 running");
    tc::log("ENGINE_HANDOVER_OK","Integrated patched engine is active");
    g_busy=false;return nullptr;
  }
  sleep(1);
 }
 tc::log("ENGINE_HANDOVER_FAIL","Engine ID unchanged; check /data/LiveShell/liveshell.log");
 set_status("Engine handover unconfirmed. Reboot PS5 if selection remains stopped");
 g_busy=false;return nullptr;
}
}
void start_if_needed(const std::string& root,bool force){
 if(theme::playback_state().engineRevision=="0.3.6"){
  set_status("Integrated LiveShell 0.3.6 already running");return;
 }
 if(g_busy.load())return;
 if(!force&&g_attempted.exchange(true))return;
 g_attempted=true;
 bool expected=false;if(!g_busy.compare_exchange_strong(expected,true))return;
 pthread_t thread{};Job* job=new Job{root};
 if(pthread_create(&thread,nullptr,run,job)!=0){
  delete job;g_busy=false;set_status("Could not start engine loader thread");return;
 }
 pthread_detach(thread);
}
bool busy(){return g_busy.load();}
std::string status(){std::lock_guard<std::mutex> lock(g_lock);return g_status;}
}

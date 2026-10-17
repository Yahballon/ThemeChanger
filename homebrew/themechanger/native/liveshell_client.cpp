// SPDX-License-Identifier: GPL-3.0-or-later
// Bridge to ZiZc3/LiveShell v1.1.  LiveShell and this app are GPL-3.0 licensed.
#include "liveshell_client.hpp"
#include "theme_log.hpp"
#include "mp4_compat.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <initializer_list>
#include <string>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
namespace theme {
const char* const kNames[kCount]={"Baseball","Anime","Gaming","Cyber City","Racing","Space","Samurai","Retro Neon","Mountains","Forest","Beach","Snow"};
namespace {
const char* const kFiles[kCount]={"baseball","anime","gaming","cyber-city","racing","space","samurai","retro-neon","mountains","forest","beach","snow"};
bool ensure_dir(const std::string& path){return mkdir(path.c_str(),0755)==0||errno==EEXIST;}
std::string http_get(const std::string& route,int timeout_seconds=3){
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return {};
 timeval tv{};tv.tv_sec=timeout_seconds;setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(9050);addr.sin_addr.s_addr=htonl(0x7f000001);
 if(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0){close(fd);return {};}
 const std::string req="GET "+route+" HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
 size_t sent=0;
 while(sent<req.size()){ssize_t n=send(fd,req.data()+sent,req.size()-sent,0);if(n<=0){close(fd);return {};}sent+=static_cast<size_t>(n);}
 std::string output;char chunk[4096];ssize_t got=0;
 while((got=recv(fd,chunk,sizeof(chunk),0))>0&&output.size()<262144)
   output.append(chunk,static_cast<size_t>(got));
 close(fd);
 if(output.rfind("HTTP/1.1 200",0)!=0)return {};
 const auto at=output.find("\r\n\r\n");
 return at==std::string::npos?std::string{}:output.substr(at+4);
}
bool copy_clip(const std::string& source,const std::string& dest,std::string& error){
 FILE* in=fopen(source.c_str(),"rb");
 if(!in){error="Wallpaper clip missing from application assets";return false;}
 if(!ensure_dir("/data/LiveShell")||!ensure_dir(kLiveRoot)){fclose(in);error="Cannot create LiveShell video folder";return false;}
 // Never overwrite another app's clip; all our files live under ThemeChanger-.
 const std::string temp=dest+".part";
 FILE* out=fopen(temp.c_str(),"wb");
 if(!out){fclose(in);error="Cannot write LiveShell video folder";return false;}
 char buf[65536];bool ok=true;size_t n;
 while((n=fread(buf,1,sizeof(buf),in))>0)
   if(fwrite(buf,1,n,out)!=n){ok=false;break;}
 if(ferror(in))ok=false;
 if(fflush(out)!=0)ok=false;
 if(fclose(out)!=0)ok=false;
 fclose(in);
 if(!ok||rename(temp.c_str(),dest.c_str())!=0){unlink(temp.c_str());error="Cannot finish wallpaper copy";return false;}
 return true;
}
int find_index(const std::string& json,const std::string& name){
 const auto videos=json.find("\"videos\":[");
 if(videos==std::string::npos)return -1;
 auto pos=videos+10;int idx=0;
 while((pos=json.find("\"name\":\"",pos))!=std::string::npos){
   pos+=8;const auto end=json.find('"',pos);
   if(end==std::string::npos)return -1;
   if(json.substr(pos,end-pos)==name)return idx;
   ++idx;pos=end+1;
 }
 return -1;
}

// The LiveShell REST JSON has stable flat fields; parse only the known keys.
std::string json_field(const std::string& json,const std::string& key){
 const auto at=json.find("\""+key+"\":");
 if(at==std::string::npos)return {};
 auto pos=at+key.size()+3;
 if(pos>=json.size())return {};
 if(json[pos]!='"'){
  const auto end=json.find_first_of(",}",pos);
  return json.substr(pos,end==std::string::npos?std::string::npos:end-pos);
 }
 ++pos;
 std::string value;
 while(pos<json.size()){
  const char ch=json[pos++];
  if(ch=='"')break;
  if(ch=='\\'&&pos<json.size()){value.push_back(json[pos++]);continue;}
  value.push_back(ch);
 }
 return value;
}
bool field_bool(const std::string& json,const std::string& key,bool fallback=false){
 const std::string s=json_field(json,key);
 return s.empty()?fallback:s=="true";
}
std::string display_file(const std::string& path){
 const auto slash=path.find_last_of('/');
 return slash==std::string::npos?path:path.substr(slash+1);
}
PlaybackState decode_playback(const std::string& json){
 PlaybackState out;
 if(json.empty())return out;
 out.connected=true;
 out.enabled=field_bool(json,"enabled");
 out.mainVideoSupported=json.find("\"mainvideo\":")!=std::string::npos;
 out.mainVideo=field_bool(json,"mainvideo",true);
 out.gamePause=field_bool(json,"gamepause",false);
 out.file=display_file(json_field(json,"current"));
 out.shellStatus=json_field(json,"status");
 out.game=json_field(json,"game");
 out.engineRevision=json_field(json,"themechanger_engine");
 return out;
}
std::string summary(const PlaybackState& p){
 return std::string("selected=")+p.file+" shellui="+p.shellStatus+
  " enabled="+(p.enabled?"1":"0")+" mainvideo="+(p.mainVideo?"1":"0")+
  " gamepause="+(p.gamePause?"1":"0")+" game="+p.game;
}
std::string codec_hint(const std::string& filename){
 // A lightweight MP4 signature probe for the diagnostic log. ffprobe is not
 // available on the console, so read only the first 512 KB of the container.
 const std::string path=std::string(kLiveRoot)+"/"+filename;
 struct stat info{};
 if(stat(path.c_str(),&info)!=0)return "file missing";
 FILE* f=fopen(path.c_str(),"rb");
 if(!f)return "file unreadable";
 std::string header(512*1024,'\0');
 const size_t n=fread(header.data(),1,header.size(),f);fclose(f);
 header.resize(n);
 std::string codec="unknown";
 for(const char* type:{"avc1","avc3","hvc1","hev1","av01","mp4v"}){
   if(header.find(type)!=std::string::npos){codec=type;break;}
 }
 return "bytes="+std::to_string((long long)info.st_size)+" codec_hint="+codec;
}
std::mutex g_select_lock;
bool select_clip(const std::string& filename,std::string& result){
 std::lock_guard<std::mutex> guard(g_select_lock);
 const auto fix=mp4compat::normalize(std::string(kLiveRoot)+"/"+filename);
 tc::log(fix.ok?"MP4_COMPAT":"MP4_INCOMPATIBLE",
    (filename+" "+fix.message).c_str());
 if(!fix.ok){
   result="This wallpaper is not PS5-compatible: "+fix.message+
     ". The previous background has been kept.";
   return false;
 }
 const auto state=http_get("/api/state");
 if(state.empty()){result="LiveShell offline, clip saved";tc::log("SELECT_OFFLINE",filename.c_str());return false;}
 const auto current=decode_playback(state);
 if(current.engineRevision!="0.3.6"){
   result="Outdated LiveShell engine. Please wait for bundled engine handover";
   tc::log("ENGINE_OUTDATED",(std::string("revision=")+current.engineRevision+
     " game="+current.game+" status="+current.shellStatus).c_str());
   return false;
 }
 const int idx=find_index(state,filename);
 if(idx<0){result="Video not yet listed by LiveShell";tc::log("SELECT_NOT_FOUND",filename.c_str());return false;}
 // Selecting a global wallpaper must also enable the global video option.
 // v1.0 has no mainvideo setting; v1.1 supports this.
 if(json_field(state,"mainvideo")=="false"){
   const auto setting=http_get("/api/set?k=mainvideo&v=1",12);
   tc::log("MAINVIDEO_ENABLE",setting.c_str());
   if(setting.find("\"ok\":true")==std::string::npos){
      result="Unable to enable main background in LiveShell";return false;
   }
 }
 tc::log("VIDEO_INFO",(filename+" "+codec_hint(filename)).c_str());
 tc::log("SELECT_REQUEST",(filename+" index="+std::to_string(idx)).c_str());
 // LiveShell can take >8s waiting for ShellUI to release the previous video.
 // Original 2s socket timeout truncated slow selections.
 const auto response=http_get("/api/select?i="+std::to_string(idx),20);
 tc::log("SELECT_RESPONSE",(filename+" "+response).c_str());
 if(response.find("\"ok\":true")==std::string::npos){
   result=response.empty()?"LiveShell timed out after 20s":"LiveShell rejected video: "+response.substr(0,110);
   tc::log("SELECT_FAILURE",result.c_str());return false;
 }
 const auto after=decode_playback(http_get("/api/state",4));
 tc::log("PLAYBACK_STATE",summary(after).c_str());
 if(!after.connected||after.file!=filename||!after.enabled){
   result="LiveShell did not confirm selected wallpaper";
   tc::log("SELECT_UNCONFIRMED",result.c_str());return false;
 }
 if(!after.mainVideo){
   result="Selected, but Main Video is disabled in LiveShell";
 }else if(!after.game.empty()&&after.gamePause){
   result="Selected. Return to PS5 Home (paused while game runs)";
 }else if(after.shellStatus.rfind("playing",0)==0){
   result="Selected; ShellUI reports playing";
 }else{
   result="Selected; ShellUI: "+(after.shellStatus.empty()?std::string("not reporting"):after.shellStatus.substr(0,70));
 }
 tc::log("SELECT_CONFIRMED",result.c_str());
 return true;
}
}
std::string theme_file(int index){
 if(index<0||index>=kCount)return {};
 return std::string("ThemeChanger-")+kFiles[index]+".mp4";
}
PlaybackState playback_state(){return decode_playback(http_get("/api/state"));}
bool probe(){return playback_state().connected;}
std::string current_file(){return playback_state().file;}
int current_theme(){
 const std::string selected=current_file();
 for(int i=0;i<kCount;i++)if(selected==theme_file(i))return i;
 return -1;
}
bool install_and_select(int index,const std::string& assets,std::string& result){
 if(index<0||index>=kCount){result="Invalid theme";return false;}
 const std::string filename=theme_file(index);
 const std::string output=std::string(kLiveRoot)+"/"+filename;
 struct stat st{};
 if(stat(output.c_str(),&st)!=0||st.st_size<1024){
   if(!copy_clip(assets+"/wallpapers/"+filename,output,result))return false;
 }
 const bool ok=select_clip(filename,result);
 if(ok){save_selected(index);tc::log("SELECT_BUILTIN",filename.c_str());}
 return ok;
}
bool activate_file(const std::string& filename,std::string& result){
 if(filename.empty()||filename.size()>120||filename.find('/')!=std::string::npos||
   filename.find('\\')!=std::string::npos||filename.find("..")!=std::string::npos){
   result="Invalid wallpaper filename";return false;
 }
 const bool ok=select_clip(filename,result);
 if(ok)tc::log("SELECT_ONLINE",filename.c_str());
 return ok;
}
bool send_setting(const std::string& route,std::string& result){
 if(route.empty()||route[0]!='/'||route.find("..")!=std::string::npos){result="Invalid API route";return false;}
 const auto body=http_get(route);
 if(body.empty()){result="LiveShell not running (load liveshell.elf)";return false;}
 if(body.find("\"ok\":true")==std::string::npos){result="LiveShell rejected setting";return false;}
 result="LiveShell setting updated";return true;
}
int load_selected(){
 FILE* f=fopen("/data/ThemeChanger/selected.txt","rb");if(!f)return 1;
 int i=1;if(fscanf(f,"%d",&i)!=1)i=1;fclose(f);return i>=0&&i<kCount?i:1;
}
void save_selected(int index){
 if(index<0||index>=kCount||!ensure_dir(kDataRoot))return;
 FILE* f=fopen("/data/ThemeChanger/selected.txt","wb");if(!f)return;
 fprintf(f,"%d\n",index);fclose(f);
}
}

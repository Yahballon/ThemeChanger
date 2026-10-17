// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Persistent ThemeChanger-owned archive; LiveShell's video folder is a
// deployment/cache location, never the sole copy of a user's downloads.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <vector>
#include <unordered_set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
namespace wallpaper_storage {
static constexpr const char* kArchive="/data/ThemeChanger/wallpapers";
static constexpr const char* kLive="/data/LiveShell/video";
struct Report { int archived=0,restored=0,errors=0,live=0,backups=0; };
inline bool filename_ok(const std::string& name){
 if(name.size()<15||name.size()>125||name.rfind("MotionBGS-",0)!=0 ||
    name.substr(name.size()-4)!=".mp4")return false;
 for(size_t i=10;i<name.size()-4;i++){
  const char c=name[i];
  if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
 }
 return true;
}
inline bool valid_file(const std::string& name){
 struct stat st{};
 if(stat(name.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<1024)return false;
 FILE* f=fopen(name.c_str(),"rb");if(!f)return false;
 unsigned char h[12]{};
 const bool ok=fread(h,1,12,f)==12&&memcmp(h+4,"ftyp",4)==0;
 fclose(f);return ok;
}
inline bool folders(){
 if(mkdir("/data/ThemeChanger",0755)!=0&&errno!=EEXIST)return false;
 if(mkdir(kArchive,0755)!=0&&errno!=EEXIST)return false;
 if(mkdir("/data/LiveShell",0755)!=0&&errno!=EEXIST)return false;
 return mkdir(kLive,0755)==0||errno==EEXIST;
}
inline bool durable_copy(const std::string& source,const std::string& dst){
 if(!valid_file(source))return false;
 // Never leave incomplete MP4 files that are interpreted as installed.
 const std::string temp=dst+".part";
 unlink(temp.c_str());
 FILE* in=fopen(source.c_str(),"rb");
 if(!in)return false;
 FILE* out=fopen(temp.c_str(),"wb");
 if(!out){fclose(in);return false;}
 // Keep data buffers small: PS5 homebrew worker stacks can be below 128 KB.
 char chunk[8192];size_t n;
 bool ok=true;
 while((n=fread(chunk,1,sizeof(chunk),in))!=0){
  if(fwrite(chunk,1,n,out)!=n){ok=false;break;}
 }
 if(ferror(in))ok=false;
 if(fflush(out)!=0||fsync(fileno(out))!=0)ok=false;
 if(fclose(out)!=0)ok=false;
 fclose(in);
 if(!ok||!valid_file(temp)||rename(temp.c_str(),dst.c_str())!=0){
  unlink(temp.c_str());return false;
 }
 return true;
}

inline constexpr const char* kIndex="/data/ThemeChanger/wallpapers.index";
inline std::vector<std::string> saved_names(){
 std::vector<std::string> names;
 FILE* f=fopen(kIndex,"rb");if(!f)return names;
 char line[160];
 std::unordered_set<std::string> seen;
 while(fgets(line,sizeof(line),f) && names.size()<25000){
  const size_t len=strlen(line);
  if(!len)continue;
  if(line[len-1]!='\n'){
   int c;while((c=fgetc(f))!=EOF&&c!='\n'){}
   continue;
  }
  line[len-1]='\0';
  const std::string name(line);
  if(filename_ok(name)&&seen.insert(name).second)names.push_back(name);
 }
 fclose(f);return names;
}
inline bool remember(const std::string& name){
 if(!filename_ok(name))return false;
 // Avoid growing the index after reselecting an existing background.
 const auto names=saved_names();
 for(const auto& n:names)if(n==name)return true;
 FILE* f=fopen(kIndex,"ab");if(!f)return false;
 bool ok=fwrite(name.data(),1,name.size(),f)==name.size();
 ok=ok&&fputc('\n',f)!=EOF;
 if(fflush(f)!=0||fsync(fileno(f))!=0)ok=false;
 if(fclose(f)!=0)ok=false;
 return ok;
}
inline bool preserve(const std::string& name,bool replace_existing=false){
 if(!filename_ok(name)||!folders())return false;
 const std::string dst=std::string(kArchive)+"/"+name;
 if((!replace_existing&&valid_file(dst)) ||
    durable_copy(std::string(kLive)+"/"+name,dst)){
   // Keep an explicit filename index; PS5 native opendir/readdir may not work.
   return remember(name);
 }
 return false;
}
inline Report recover(const std::vector<std::string>& catalogueNames={}){
 Report r{};
 if(!folders()){r.errors++;return r;}
 auto names=saved_names();
 std::unordered_set<std::string> visited(names.begin(),names.end());
 for(const auto& name:catalogueNames)
  if(filename_ok(name)&&visited.insert(name).second)names.push_back(name);
 // Stat filenames by their full path, never rely on broken dirent emulation.
 for(const auto& name:names){
  const std::string live=std::string(kLive)+"/"+name;
  const std::string backup=std::string(kArchive)+"/"+name;
  const bool hasLive=valid_file(live);
  const bool hasBackup=valid_file(backup);
  if(hasBackup)r.backups++;
  if(hasLive)r.live++;
  if(hasBackup&&!hasLive){
   if(durable_copy(backup,live))r.restored++;
   else r.errors++;
  }else if(hasLive&&!hasBackup){
   if(durable_copy(live,backup)){
    r.archived++;r.backups++;
    if(!remember(name))r.errors++;
   }else r.errors++;
  }else if(hasLive&&hasBackup){
   if(!remember(name))r.errors++;
  }
 }
 return r;
}
// Delete only validated ThemeChanger download names, never whole directories.
struct ClearReport { int files=0,errors=0; };
inline ClearReport clear_downloads(const std::vector<std::string>& catalogueNames){
 ClearReport r;
 auto names=saved_names();
 names.insert(names.end(),catalogueNames.begin(),catalogueNames.end());
 std::unordered_set<std::string> seen;
 for(const auto& name:names){
  if(!filename_ok(name)||!seen.insert(name).second)continue;
  for(const auto* root:{kArchive,kLive}){
   const std::string path=std::string(root)+"/"+name;
   for(const auto* suffix:{"",".part",".smooth.part"}){
    if(unlink((path+suffix).c_str())==0)r.files++;
    else if(errno!=ENOENT)r.errors++;
   }
  }
 }
 // Retain the index on partial failure so remaining backups remain discoverable.
 if(r.errors==0&&unlink(kIndex)!=0&&errno!=ENOENT)r.errors++;
 return r;
}
// Recover only explicitly indexed downloads. Never stat the entire 9K+
// catalog on the UI thread (previous v0.3.11 startup regression).
inline Report recover_indexed(){
 return recover({});
}
} // namespace wallpaper_storage

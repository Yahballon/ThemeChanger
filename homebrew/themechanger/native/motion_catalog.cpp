// SPDX-License-Identifier: GPL-3.0-or-later
// Native catalog and background workers for user's personal MotionBGS downloads.
#include "motion_catalog.hpp"
#include "motion_download.hpp"
#include "liveshell_client.hpp"
#include "theme_log.hpp"
#include "wallpaper_storage.hpp"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <unordered_set>
#include <memory>
#include <string_view>
namespace motion {
namespace {
bool valid_slug(const std::string& slug) {
 if(slug.size()<3||slug.size()>100)return false;
 for(unsigned char c:slug)if(!(std::isalnum(c)||c=='-'))return false;
 return true;
}
bool make_directory(const char* p){return mkdir(p,0755)==0||errno==EEXIST;}
bool exists(const std::string& p,long minimum=500){
 struct stat st{};return stat(p.c_str(),&st)==0&&st.st_size>minimum;
}
bool is_mp4(const std::string& path){
 FILE* f=fopen(path.c_str(),"rb");if(!f)return false;
 unsigned char b[12]{};bool ok=fread(b,1,sizeof(b),f)==sizeof(b);
 fclose(f);return ok&&b[4]=='f'&&b[5]=='t'&&b[6]=='y'&&b[7]=='p';
}
bool is_image(const std::string& path){
 FILE* f=fopen(path.c_str(),"rb");if(!f)return false;
 unsigned char b[12]{};size_t n=fread(b,1,sizeof(b),f);fclose(f);
 return (n>=4&&b[0]==0xff&&b[1]==0xd8)||(n>=8&&b[0]==137&&b[1]=='P'&&b[2]=='N'&&b[3]=='G');
}
std::string read_small(const std::string& path){
 FILE* f=fopen(path.c_str(),"rb");if(!f)return {};
 if(fseek(f,0,SEEK_END)){fclose(f);return {};}
 const long n=ftell(f);rewind(f);
 if(n<100||n>600000){fclose(f);return {};}
 std::string out((size_t)n,'\0');
 const bool ok=fread(out.data(),1,out.size(),f)==out.size();
 fclose(f);return ok?out:std::string{};
}
std::string attr(std::string_view tag,std::string_view key){
 const size_t p=tag.find(key);
 if(p==std::string_view::npos)return {};
 const size_t eq=p+key.size();
 if(eq>=tag.size()||tag[eq]!='=')return {};
 size_t i=eq+1;char quote=0;
 if(i<tag.size()&&(tag[i]=='"'||tag[i]=='\''))quote=tag[i++];
 const size_t begin=i;
 while(i<tag.size() && (quote?tag[i]!=quote:tag[i]!=' '&&tag[i]!='>'&&tag[i]!='\n'))i++;
 return std::string(tag.substr(begin,i-begin));
}
bool resolve(const Entry& entry,std::string& video,std::string& poster,std::string& error){
 video=entry.url;poster=entry.preview;
 if(!video.empty()&&!poster.empty())return true;
 if(!valid_slug(entry.slug)){error="Invalid page slug";return false;}
 make_directory("/data/ThemeChanger");make_directory("/data/ThemeChanger/meta");
 const std::string path="/data/ThemeChanger/meta/"+entry.slug+".html";
 std::string raw=read_small(path);
 if(raw.empty()){
   std::atomic<int> progress{0};
   if(!fetch_file("https://motionbgs.com/"+entry.slug,path,progress,error))return false;
   raw=read_small(path);
 }
 if(raw.empty()){error="Wallpaper metadata is missing";return false;}
 if(video.empty()){
   size_t i=raw.find("/dl/hd/");
   if(i!=std::string::npos){
     i+=7;const size_t start=i;
     while(i<raw.size()&&raw[i]>='0'&&raw[i]<='9'&&i-start<12)i++;
     if(i>start&&i-start<=11)video="https://motionbgs.com/dl/hd/"+raw.substr(start,i-start)+"/";
   }
 }
 if(poster.empty()){
   size_t i=raw.find("<video");
   if(i!=std::string::npos){
     const size_t end=raw.find('>',i);
     if(end!=std::string::npos&&end-i<4000)
       poster=attr(std::string_view(raw.data()+i,end-i),"poster");
   }
   if(poster.empty()){
     i=raw.find("rel=preload");
     if(i!=std::string::npos){
       const size_t begin=raw.rfind("<link",i),end=raw.find('>',i);
       if(begin!=std::string::npos&&end!=std::string::npos&&end-begin<4000)
         poster=attr(std::string_view(raw.data()+begin,end-begin),"href");
     }
   }
   if(poster.rfind("/i/",0)==0)poster="https://motionbgs.com"+poster;
   if(poster.rfind("https://motionbgs.com/",0)!=0)poster.clear();
 }
 if(video.empty()){error="No HD MP4 link on wallpaper page";return false;}
 return true;
}
std::string clean_label(std::string s){
 for(auto& c:s)if(c=='\r'||c=='\n'||c=='\t')c=' ';
 return s;
}
}
bool Catalog::load(const std::string& assets){
 std::string path=assets+"/motionbgs/catalog.tsv";
 FILE* f=fopen(path.c_str(),"rb");if(!f){report("MotionBGS catalog missing");tc::log("CATALOG_ERROR",path.c_str());return false;}
 char line[2048];std::vector<Entry> next;
 while(fgets(line,sizeof(line),f)&&next.size()<20000){
  std::string s(line);if(s.empty()||s[0]=='#')continue;
  if(!s.empty()&&s.back()=='\n')s.pop_back();
  size_t first=s.find('\t'),second=first==std::string::npos?first:s.find('\t',first+1),
   third=second==std::string::npos?second:s.find('\t',second+1);
  if(first==std::string::npos||second==std::string::npos||third==std::string::npos)continue;
  const size_t fourth=s.find('\t',third+1);
  const size_t fifth=fourth==std::string::npos?std::string::npos:s.find('\t',fourth+1);
  Entry item{s.substr(0,first),clean_label(s.substr(first+1,second-first-1)),
    clean_label(s.substr(second+1,third-second-1)),
    s.substr(third+1,fourth==std::string::npos?std::string::npos:fourth-third-1),
    fourth==std::string::npos?"":s.substr(fourth+1,fifth==std::string::npos?std::string::npos:fifth-fourth-1),
    fifth==std::string::npos?"":clean_label(s.substr(fifth+1))};
  if(!valid_slug(item.slug)||item.title.size()>120||item.category.size()>40)continue;
  if(!item.url.empty()&&item.url.rfind("https://motionbgs.com/dl/hd/",0)!=0)continue;
  if(item.preview.rfind("https://motionbgs.com/",0)!=0 && item.preview.rfind("https://",0)==0) item.preview="";
  next.push_back(std::move(item));
 }
 fclose(f);
 // Older downloaded videos must remain visible even if a future online catalog
 // no longer includes their source page. Only read MP4 files owned by ThemeChanger.
 if(DIR* dir=opendir("/data/LiveShell/video")){
   int recovered=0;
   while(dirent* de=readdir(dir)){
     const std::string filename=de->d_name;
     if(filename.rfind("MotionBGS-",0)!=0||filename.size()<15||
        filename.substr(filename.size()-4)!=".mp4")continue;
     const std::string slug=filename.substr(10,filename.size()-14);
     if(!valid_slug(slug))continue;
     const bool present=std::any_of(next.begin(),next.end(),
       [&](const Entry& e){return e.slug==slug;});
     if(present)continue;
     if(!exists(std::string("/data/LiveShell/video/")+filename,1024))continue;
     std::string title=slug;
     std::replace(title.begin(),title.end(),'-',' ');
     next.push_back(Entry{slug,title,"Downloaded","","",""});
     recovered++;
   }
   closedir(dir);
   if(recovered)tc::log("RESTORED_DOWNLOADS",std::to_string(recovered).c_str());
 }
 records_=std::move(next);
 preview_requested_.assign(records_.size(),false);
 installed_cache_.assign(records_.size(),false);
 // Restore only the user's indexed wallpapers; no unbounded directory
 // enumeration or per-record filesystem scan during boot.
 int recovered=0;
 const auto names=wallpaper_storage::saved_names();
 std::unordered_set<std::string> saved(names.begin(),names.end());
 for(size_t i=0;i<records_.size();i++){
   const std::string name="MotionBGS-"+records_[i].slug+".mp4";
   if(!saved.count(name))continue;
   const std::string live=wallpaper_path((int)i);
   if(wallpaper_storage::valid_file(live)){
     installed_cache_[i]=true;recovered++;
   }
 }
 tc::log("LIBRARY_INDEX_LOADED",(std::to_string(names.size())+
      " names; "+std::to_string(recovered)+" active files").c_str());
 if(records_.empty()){report("Online catalog empty. Check MotionBGS availability.");return false;}
 report("MotionBGS: "+std::to_string(records_.size())+" indexed (download on demand)");
 tc::log("CATALOG_LOADED",std::to_string(records_.size()).c_str());
 return true;
}
int Catalog::discover_legacy(int budget){
 if(downloading_)return 0;
 int imported=0;
 while(budget-->0 && discovery_cursor_<records_.size()){
  const int i=(int)discovery_cursor_++;
  if(installed(i))continue;
  const std::string path=wallpaper_path(i);
  const std::string name="MotionBGS-"+records_[(size_t)i].slug+".mp4";
  const std::string archive=std::string(wallpaper_storage::kArchive)+"/"+name;
  if(!wallpaper_storage::valid_file(path)){
    // Legacy v0.3.10/11 archives may predate the filename index.
    if(!wallpaper_storage::valid_file(archive)||
       !wallpaper_storage::durable_copy(archive,path))continue;
    tc::log("LEGACY_ARCHIVE_RESTORED",name.c_str());
  }
  // Existing v0.3.12 MP4s may not be present in the durable index.
  if(wallpaper_storage::preserve(name)){
   std::lock_guard<std::mutex> lock(mutex_);
   installed_cache_[(size_t)i]=true;
   newly_installed_.push_back(i);
   imported++;
   tc::log("LEGACY_WALLPAPER_IMPORTED",name.c_str());
  }else tc::log("LEGACY_IMPORT_FAILED",name.c_str());
 }
 return imported;
}
std::string Catalog::wallpaper_path(int i)const{
 if(i<0||i>=(int)records_.size())return {};
 return "/data/LiveShell/video/MotionBGS-"+records_[(size_t)i].slug+".mp4";
}
std::string Catalog::thumbnail_path(int i)const{
 if(i<0||i>=(int)records_.size())return {};
 return "/data/ThemeChanger/cache/MotionBGS-"+records_[(size_t)i].slug+".img";
}
bool Catalog::installed(int i)const{
 if(i<0||i>=(int)records_.size())return false;
 std::lock_guard<std::mutex> lock(mutex_);
 return installed_cache_[(size_t)i];
}
void Catalog::report(std::string message){std::lock_guard<std::mutex> g(mutex_);status_=std::move(message);}
std::string Catalog::status()const{std::lock_guard<std::mutex> g(mutex_);return status_;}
bool Catalog::start(int i,bool preview){
 if(i<0||i>=(int)records_.size())return false;
 auto& active=preview?preview_busy_:downloading_;
 if(active.exchange(true))return false;
 if(preview){
  std::lock_guard<std::mutex> g(mutex_);
  if(preview_requested_[(size_t)i]){active=false;return false;}
  preview_requested_[(size_t)i]=true;
 }else{percent_=0;report("Connecting to MotionBGS...");tc::log("DOWNLOAD_REQUEST",records_[(size_t)i].slug.c_str());}
 auto* w=new Worker{this,i,preview};pthread_t thread{};
 if(pthread_create(&thread,nullptr,Catalog::run,w)!=0){
  delete w;active=false;report("Unable to start network thread");return false;
 }
 pthread_detach(thread);return true;
}
bool Catalog::download(int i){return start(i,false);}
bool Catalog::clear_downloads(){
 if(downloading_.exchange(true))return false;
 cleaning_=true;percent_=0;report("Removing downloaded wallpapers and backups...");
 auto* w=new Worker{this,-1,false};pthread_t thread{};
 if(pthread_create(&thread,nullptr,Catalog::run,w)!=0){
  delete w;cleaning_=false;downloading_=false;report("Unable to start cleanup");return false;
 }
 pthread_detach(thread);return true;
}
bool Catalog::request_thumbnail(int i){return start(i,true);}
std::vector<int> Catalog::consume_ready(){
 std::lock_guard<std::mutex> g(mutex_);
 auto items=ready_;ready_.clear();return items;
}
std::vector<int> Catalog::consume_installed(){
 std::lock_guard<std::mutex> g(mutex_);
 auto ids=newly_installed_;newly_installed_.clear();return ids;
}
void* Catalog::run(void* p){
 Worker job=*static_cast<Worker*>(p);delete static_cast<Worker*>(p);
 Catalog& self=*job.self;
 if(job.index==-1){
  // The download reservation also blocks new MP4s and incremental recovery.
  const auto state=theme::playback_state();std::string result;
  if(state.connected&&!theme::send_setting("/api/off",result)){
   self.report("Cleanup cancelled: could not stop LiveShell playback");
   self.cleaning_=false;self.downloading_=false;return nullptr;
  }
  std::vector<std::string> names;
  for(const auto& entry:self.records_)names.push_back("MotionBGS-"+entry.slug+".mp4");
  const auto cleared=wallpaper_storage::clear_downloads(names);
  std::vector<bool> remaining(self.records_.size(),false);
  if(cleared.errors)for(size_t i=0;i<remaining.size();i++)
    remaining[i]=wallpaper_storage::valid_file(self.wallpaper_path((int)i));
  {
   std::lock_guard<std::mutex> lock(self.mutex_);
   self.installed_cache_=std::move(remaining);self.newly_installed_.clear();
  }
  self.report(cleared.errors?"Cleanup incomplete: "+std::to_string(cleared.errors)+" file errors. Retry Clean all wallpapers.":
                            "All downloaded wallpapers and backups deleted");
  tc::log("WALLPAPER_CLEANUP",("files="+std::to_string(cleared.files)+" errors="+std::to_string(cleared.errors)).c_str());
  self.percent_=100;self.cleaning_=false;self.downloading_=false;return nullptr;
 }
 const auto& item=self.records_[(size_t)job.index];
 if(job.preview){
  make_directory("/data/ThemeChanger");make_directory("/data/ThemeChanger/cache");
  const std::string path=self.thumbnail_path(job.index);
  bool ok=is_image(path);
  if(!ok){
   std::atomic<int> prog{0};std::string err,video,poster;
   bool metadata=resolve(item,video,poster,err);
   if(metadata&&!poster.empty()){
     tc::log("PREVIEW_REQUEST",(item.slug+" "+poster).c_str());
     ok=fetch_file(poster,path,prog,err);
     if(ok&&!is_image(path)){unlink(path.c_str());ok=false;err="Invalid JPEG/PNG format";}
   }else if(metadata)err="Source page has no JPEG poster";
   tc::log(ok?"PREVIEW_SAVED":"PREVIEW_ERROR",(item.slug+" "+err).c_str());
  }
  if(ok){std::lock_guard<std::mutex> g(self.mutex_);self.ready_.push_back(job.index);}
  self.preview_busy_=false;return nullptr;
 }
 const std::string name="MotionBGS-"+item.slug+".mp4";
 const std::string archive=std::string(wallpaper_storage::kArchive)+"/"+name;
 const std::string path=self.wallpaper_path(job.index);
 std::string err;
 bool ok=wallpaper_storage::folders();
 if(!ok)err="Cannot create permanent wallpaper storage";
 if(ok){
   // Older downloads may still be in LiveShell's working folder.
   if(wallpaper_storage::valid_file(path) &&
      !wallpaper_storage::valid_file(archive)){
     ok=wallpaper_storage::preserve(name);
     if(!ok)err="Unable to preserve existing wallpaper";
   }
 }
 if(ok&&!wallpaper_storage::valid_file(archive)){
   self.report("Downloading "+item.title+" to permanent library...");
   std::string url,preview;
   if(resolve(item,url,preview,err)){
     tc::log("MP4_URL_RESOLVED",(item.slug+" "+url).c_str());
     ok=fetch_file(url,archive,self.percent_,err);
     if(ok&&!wallpaper_storage::valid_file(archive)){
       unlink(archive.c_str());ok=false;err="Invalid MP4 from server";
     }
   }else ok=false;
 }
 if(ok){
   tc::log("WALLPAPER_ARCHIVED",archive.c_str());
   if(!wallpaper_storage::valid_file(path)){
     self.report("Installing downloaded wallpaper to LiveShell...");
     ok=wallpaper_storage::durable_copy(archive,path);
     if(!ok)err="LiveShell video installation failed; permanent copy was kept";
   }
 }
 if(ok&&!wallpaper_storage::remember(name)){
   tc::log("WALLPAPER_INDEX_WARNING",(name+" index write failed, archive kept").c_str());
   // The permanent MP4 is still present; never discard it for an index error.
 }
 if(ok){
  {std::lock_guard<std::mutex> lock(self.mutex_);
   self.installed_cache_[(size_t)job.index]=true;
   self.newly_installed_.push_back(job.index);
  }
  self.percent_=100;
  tc::log("DOWNLOAD_COMPLETE",(path+" archive="+archive).c_str());
  std::string applied;
  if(theme::activate_file("MotionBGS-"+item.slug+".mp4",applied)){
   tc::log("LIVESHELL_SELECTED",(item.slug+" "+applied).c_str());
   self.report(item.title+": "+applied);
  }else{
   tc::log("LIVESHELL_PENDING",(item.slug+" "+applied).c_str());
   self.report(item.title+": installed. "+applied);
  }
 }else{tc::log("DOWNLOAD_ERROR",(item.slug+" "+err).c_str());self.report("Download failed: "+err);}
 self.downloading_=false;return nullptr;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Host-side contract test: the same native OpenSSL transport used on PS5.
// Downloads a small public HD MP4 to /tmp; never redistributes the clip.
#include "motion_download.hpp"
#include <atomic>
#include <cstdio>
#include <string>
int main(int argc,char** argv){
 const char* url="https://motionbgs.com/dl/hd/737/";
 const std::string path="/tmp/themechanger-motionbgs-probe.mp4";
 std::atomic<int> percent{0};std::string error;
 const bool ok=motion::fetch_file(url,path,percent,error);
 if(!ok){std::fprintf(stderr,"HTTPS HD download failed: %s\n",error.c_str());return 1;}
 FILE* f=std::fopen(path.c_str(),"rb");if(!f)return 2;
 unsigned char header[8]{};bool good=std::fread(header,1,8,f)==8;
 std::fclose(f);std::remove(path.c_str());
 good=good&&header[4]=='f'&&header[5]=='t'&&header[6]=='y'&&header[7]=='p';
 std::printf("MotionBGS native HTTPS: %s, progress=%d\n",good?"MP4 OK":"invalid",percent.load());
 if(!good||percent!=100)return 3;
 if(argc>1){
   percent=0;error.clear();
   const std::string image="/tmp/themechanger-motionbgs-preview.img";
   if(!motion::fetch_file(argv[1],image,percent,error)){
     std::fprintf(stderr,"HTTPS preview download failed: %s\n",error.c_str());return 4;
   }
   FILE* g=std::fopen(image.c_str(),"rb");if(!g)return 5;
   unsigned char h[8]{};const bool imageRead=std::fread(h,1,8,g)==8;
   std::fclose(g);std::remove(image.c_str());
   const bool jpeg=imageRead&&h[0]==0xff&&h[1]==0xd8;
   std::printf("MotionBGS native preview: %s, progress=%d\n",jpeg?"JPEG OK":"INVALID",percent.load());
   if(!jpeg||percent!=100)return 6;
 }
 // New sitemap entries resolve their preview/HD IDs lazily from item HTML.
 // Verify the native PS5 HTTPS implementation can fetch regular HTML as well.
 const std::string htmlPath="/tmp/themechanger-motionbgs-detail.html";
 percent=0;error.clear();
 const bool page=motion::fetch_file("https://motionbgs.com/a-galaxy-within",htmlPath,percent,error);
 if(!page){
   std::fprintf(stderr,"MotionBGS detail HTML failed: %s\n",error.c_str());
   return 7;
 }
 FILE* item=std::fopen(htmlPath.c_str(),"rb");
 if(!item)return 8;
 char head[1024]{};
 const size_t count=std::fread(head,1,sizeof(head)-1,item);
 std::fclose(item);std::remove(htmlPath.c_str());
 if(count<100||std::string(head).find("<html")==std::string::npos)return 9;
 std::printf("MotionBGS detail HTML: OK\n");
 return 0;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "mp4_compat.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
static void box(std::vector<unsigned char>& v,const char* type,std::vector<unsigned char> body){
 size_t size=body.size()+8;
 v.push_back((unsigned char)(size>>24));
 v.push_back((unsigned char)(size>>16));
 v.push_back((unsigned char)(size>>8));
 v.push_back((unsigned char)size);
 for(int j=0;j<4;j++)v.push_back(type[j]);
 v.insert(v.end(),body.begin(),body.end());
}
static void run(unsigned w,unsigned h,bool expect){
 std::vector<unsigned char> bytes;
 box(bytes,"ftyp",std::vector<unsigned char>(64,0));
 std::vector<unsigned char> avc1(90,0);
 avc1[24]=(unsigned char)(w>>8);avc1[25]=(unsigned char)w;
 avc1[26]=(unsigned char)(h>>8);avc1[27]=(unsigned char)h;
 box(bytes,"avc1",avc1);
 const size_t avcC=bytes.size();
 const std::vector<unsigned char> record={1,100,0,50,255,225,0,4,0x67,100,0,50,1,0,2,0x68,0};
 box(bytes,"avcC",record);
 box(bytes,"free",std::vector<unsigned char>(512,0));
 const std::string name="/tmp/test-themechanger-compat.mp4";
 FILE* f=std::fopen(name.c_str(),"wb");assert(f);
 assert(std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());std::fclose(f);
 auto out=mp4compat::normalize(name);
 assert(out.ok==expect);
 assert(out.changed==expect);
 if(expect){
  assert(out.original_level==50&&out.new_level==42);
  auto second=mp4compat::normalize(name);
  assert(second.ok&&!second.changed&&second.original_level==42);
 }
 f=std::fopen(name.c_str(),"rb");assert(f);
 std::vector<unsigned char> after(bytes.size());
 assert(fread(after.data(),1,after.size(),f)==after.size());fclose(f);
 assert(after[avcC+11]==(expect?42:50));
 assert(after[avcC+19]==(expect?42:50));
 for(size_t j=0;j<bytes.size();j++){
  if(expect&&(j==avcC+11||j==avcC+19))continue;
  assert(bytes[j]==after[j]);
 }
 std::remove(name.c_str());
 std::printf("MP4 compatibility test: %ux%u %s\n",w,h,out.message.c_str());
}
int main(){
 run(1920,1080,true);
 run(3840,2160,false);
}

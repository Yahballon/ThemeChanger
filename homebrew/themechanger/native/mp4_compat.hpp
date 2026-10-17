// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Normalize wrongly tagged 1080p H.264 streams from MotionBGS for the Sony
// ShellUI FullHD decoder. This does not re-encode or alter image pixels.
// H.264 level_idc is stored both in MP4 avcC and the sequence parameter set.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
namespace mp4compat {
struct Result {
 bool ok=false, changed=false;
 unsigned width=0,height=0,original_level=0,new_level=0;
 std::string message;
};
inline unsigned be16(const unsigned char* b){return ((unsigned)b[0]<<8)|b[1];}
inline unsigned be32(const unsigned char* b){return ((unsigned)b[0]<<24)|((unsigned)b[1]<<16)|((unsigned)b[2]<<8)|b[3];}
inline Result normalize(const std::string& path){
 Result out{};
 struct stat st{};
 if(stat(path.c_str(),&st)!=0||st.st_size<128||st.st_size>200LL*1024*1024){
  out.message="MP4 file missing or invalid size";return out;
 }
 FILE* f=fopen(path.c_str(),"r+b");
 if(!f){out.message="Cannot inspect MP4 for compatibility";return out;}
 // Search ISO-BMFF box identifiers. moov can be at either end of an MP4.
 // Validate candidate boxes before using any offsets (never blindly patch mdat).
 std::vector<unsigned char> block(65536+64);
 size_t carry=0;
 long long offset=0,config=-1;
 unsigned width=0,height=0;
 while(offset<st.st_size){
  size_t n=fread(block.data()+carry,1,65536,f);
  if(!n)break;
  const size_t count=carry+n;
  const long long base=offset-(long long)carry;
  for(size_t i=4;i+40<count;i++){
   if(!memcmp(block.data()+i,"avc1",4)){
    const unsigned len=be32(block.data()+i-4);
    if(len>=86&&len<0x4000000){
     unsigned w=be16(block.data()+i+28),h=be16(block.data()+i+30);
     if(w>=64&&h>=64&&w<=8192&&h<=4320){width=w;height=h;}
    }
   }else if(config<0&&!memcmp(block.data()+i,"avcC",4)){
    const unsigned len=be32(block.data()+i-4);
    if(len>=23&&len<=4096&&base+(long long)i+len-4<=st.st_size &&
       block[i+4]==1&&(block[i+5]==66||block[i+5]==77||block[i+5]==100)){
       config=base+(long long)i-4;
    }
   }
  }
  offset+=n;
  if(config>=0&&width&&height)break;
  carry=count<64?count:64;
  memmove(block.data(),block.data()+count-carry,carry);
  // fread's file position already advanced; do not fseek here.
 }
 out.width=width;out.height=height;
 if(config<0){out.message="No H.264 avcC track (unsupported video codec?)";fclose(f);return out;}
 unsigned char sizebuf[4]{};
 if(fseek(f,(long)config,SEEK_SET)!=0||fread(sizebuf,1,4,f)!=4){
  out.message="Unreadable avcC atom";fclose(f);return out;
 }
 const unsigned boxlen=be32(sizebuf);
 if(boxlen<23||boxlen>4096||config+boxlen>st.st_size){
  out.message="Corrupt avcC atom";fclose(f);return out;
 }
 std::vector<unsigned char> data(boxlen);
 memcpy(data.data(),sizebuf,4);
 if(fread(data.data()+4,1,boxlen-4,f)!=boxlen-4 ||
    memcmp(data.data()+4,"avcC",4)!=0||data[8]!=1){
  out.message="Invalid H.264 configuration";fclose(f);return out;
 }
 out.original_level=data[11];
 out.new_level=data[11];
 if(out.original_level<=42){
  out.ok=true;out.message="H.264 already compatible (level "+std::to_string(out.original_level)+")";
  fclose(f);return out;
 }
 if(width==0||height==0||width>1920||height>1080){
  out.message="Level "+std::to_string(out.original_level)+
     " needs video conversion (not a supported FullHD stream)";
  fclose(f);return out;
 }
 // MotionBGS FullHD clips are usually 1080p30 but many contain the
 // unnecessarily high level_idc=50. A declared 5.0 can make ShellUI black.
 // 4.2 accepts 1080p60, and also 1080p30, without modifying encoded frames.
 if(out.original_level!=50){
  out.message="Unverified H.264 level "+std::to_string(out.original_level);
  fclose(f);return out;
 }
 size_t pos=13;
 const unsigned sps_count=data[pos++]&31;
 if(!sps_count||sps_count>10){out.message="Missing H.264 SPS";fclose(f);return out;}
 std::vector<size_t> level_offsets;
 for(unsigned i=0;i<sps_count;i++){
  if(pos+2>=data.size()){out.message="Truncated H.264 SPS";fclose(f);return out;}
  unsigned size=be16(data.data()+pos);pos+=2;
  if(size<4||pos+size>data.size()||(data[pos]&31)!=7){
   out.message="Malformed H.264 SPS";fclose(f);return out;
  }
  if(data[pos+3]!=out.original_level){
   out.message="avcC/SPS level disagreement";fclose(f);return out;
  }
  level_offsets.push_back(pos+3);pos+=size;
 }
 // Update a single on-disk atom with checks and a best-effort rollback.
 const auto original=data;
 data[11]=42;
 for(auto p:level_offsets)data[p]=42;
 if(fseek(f,(long)config,SEEK_SET)!=0||
    fwrite(data.data(),1,data.size(),f)!=data.size()||
    fflush(f)!=0||fsync(fileno(f))!=0){
  if(fseek(f,(long)config,SEEK_SET)==0){
   fwrite(original.data(),1,original.size(),f);fflush(f);fsync(fileno(f));
  }
  out.message="Failed to update H.264 headers";fclose(f);return out;
 }
 fclose(f);
 out.ok=true;out.changed=true;out.new_level=42;
 out.message="H.264 Level 5.0 -> 4.2 metadata fix ("+
    std::to_string(width)+"x"+std::to_string(height)+")";
 return out;
}
} // namespace mp4compat

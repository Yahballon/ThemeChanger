// SPDX-License-Identifier: GPL-3.0-or-later
#include "motion_download.hpp"
#include "smooth_video.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
int main(){
 // MotionBGS 5411 is known 1080p H.264 High L5 / SPS max_num_ref_frames=8.
 const char* url="https://motionbgs.com/dl/hd/5411/";
 const std::string path="/tmp/themechanger-ps5-smooth-test.mp4";
 std::atomic<int> progress{0};
 std::string err;
 assert(motion::fetch_file(url,path,progress,err));
 assert(smooth_video::needs_conversion(path));
 progress=0;
 if(!smooth_video::convert(path,progress,err)){
  fprintf(stderr,"Optimization failed: %s\n",err.c_str());return 1;
 }
 assert(!smooth_video::needs_conversion(path));
 assert(progress==100);
 struct stat st{};
 assert(stat(path.c_str(),&st)==0&&st.st_size>10000);
 printf("H.264 High 5.0 8 refs converted to smooth playback stream: %ld bytes\n",(long)st.st_size);
 unlink(path.c_str());
 return 0;
}

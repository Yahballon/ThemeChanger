// SPDX-License-Identifier: GPL-3.0-or-later
// Native PS5-side FFmpeg decode + x264 encode for 1080p H.264 videos whose
// SPS requires >4 reference frames. This fixes actual decoder constraints;
// rewriting level_idc alone does not make that bitstream conform to 4.1.
#include "smooth_video.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
namespace smooth_video {
namespace {
struct Bits {
 std::vector<unsigned char> bytes;
 size_t bit=0;
 bool good=true;
 explicit Bits(const unsigned char* nal,size_t len){
  // Strip Annex B emulation-prevention bytes; first byte is SPS NAL header.
  if(len<5||len>4096||(nal[0]&31)!=7){good=false;return;}
  bytes.reserve(len);
  int zeros=0;
  for(size_t i=1;i<len;i++){
   unsigned char c=nal[i];
   if(zeros>=2&&c==3){zeros=0;continue;}
   bytes.push_back(c);
   zeros=(c==0)?zeros+1:0;
  }
 }
 unsigned b(unsigned n){
  if(!good||n>32||bit+n>bytes.size()*8){good=false;return 0;}
  unsigned value=0;
  for(unsigned i=0;i<n;i++){
   const size_t pos=bit++;
   value=(value<<1)|((bytes[pos/8]>>(7-(pos%8)))&1);
  }
  return value;
 }
 unsigned ue(){
  unsigned zeros=0;
  while(good&&b(1)==0){if(++zeros>25){good=false;return 0;}}
  if(!good)return 0;
  return ((1u<<zeros)-1u)+b(zeros);
 }
 int se(){const auto v=ue();return v&1?(int)((v+1)/2):-(int)(v/2);}
 void scaling(unsigned n){
  int last=8,next=8;
  for(unsigned i=0;i<n&&good;i++){
   if(next!=0)next=(last+se()+256)%256;
   last=next?next:last;
  }
 }
};
int refs_from_sps(const unsigned char* nal,size_t n){
 Bits v(nal,n);
 if(!v.good)return -1;
 const unsigned profile=v.b(8);
 v.b(8);v.b(8);
 v.ue(); // seq_parameter_set_id
 if(profile==100||profile==110||profile==122||profile==244||profile==44||
    profile==83||profile==86||profile==118||profile==128||profile==138||profile==139){
  unsigned chroma=v.ue();
  if(chroma>3)return -1;
  if(chroma==3)v.b(1);
  v.ue();v.ue();v.b(1);
  if(v.b(1)){
   const unsigned count=chroma==3?12:8;
   for(unsigned i=0;i<count;i++)if(v.b(1))v.scaling(i<6?16:64);
  }
 }
 v.ue(); // log2_max_frame_num_minus4
 const unsigned poc=v.ue();
 if(poc==0)v.ue();
 else if(poc==1){
  v.b(1);v.se();v.se();
  const unsigned count=v.ue();
  if(count>255)return -1;
  for(unsigned i=0;i<count;i++)v.se();
 }else if(poc>2)return -1;
 const unsigned refs=v.ue();
 return v.good&&refs<=32?(int)refs:-1;
}
int reference_frames(const std::string& path){
 FILE* f=fopen(path.c_str(),"rb");
 if(!f)return -1;
 // MP4 avcC may appear after mdat. Scan in fixed memory.
 // Native homebrew worker threads may have much smaller stacks than Linux CI.
 // The old 65 KB stack buffer could overflow before VIDEO_OPTIMIZE_START.
 auto backing=std::make_unique<unsigned char[]>(65536+48);
 unsigned char* buf=backing.get();
 std::memset(buf,0,65536+48);
 size_t carry=0;
 int refs=-1;
 while(true){
  const size_t n=fread(buf+carry,1,65536,f);
  if(n==0)break;
  const size_t count=carry+n;
  for(size_t i=4;i+20<count;i++){
   if(memcmp(buf+i,"avcC",4)!=0)continue;
   const unsigned atom=(unsigned(buf[i-4])<<24)|(unsigned(buf[i-3])<<16)|
                       (unsigned(buf[i-2])<<8)|buf[i-1];
   if(atom<23||atom>4096||i+atom-4>count||buf[i+4]!=1)continue;
   size_t j=i+9;
   const unsigned count_sps=buf[j++]&31;
   if(count_sps==0||count_sps>8)continue;
   if(j+2>=count)continue;
   const unsigned size=(unsigned(buf[j])<<8)|buf[j+1];j+=2;
   if(size<5||j+size>count)continue;
   refs=refs_from_sps(buf+j,size);
   if(refs>=0)break;
  }
  if(refs>=0)break;
  carry=std::min<size_t>(count,48);
  memmove(buf,buf+count-carry,carry);
 }
 fclose(f);
 return refs;
}
std::string ff_error(int code){
 char msg[AV_ERROR_MAX_STRING_SIZE]{};
 av_strerror(code,msg,sizeof(msg));
 return msg;
}
}
bool needs_conversion(const std::string& path){
 const int refs=reference_frames(path);
 return refs>4;
}
bool convert(const std::string& filename,std::atomic<int>& progress,std::string& error){
 // Output in the same LiveShell directory for an atomic final rename.
 const std::string output=filename+".smooth.part"; // LiveShell must never enumerate half-written videos.
 unlink(output.c_str());
 AVFormatContext* input=nullptr;
 AVFormatContext* dst=nullptr;
 AVCodecContext *dec=nullptr,*enc=nullptr;
 AVFrame *raw=nullptr,*scaled=nullptr;
 AVPacket* packet=nullptr;
 SwsContext* sws=nullptr;
 bool success=false;
 int ret=0;
 do{
  ret=avformat_open_input(&input,filename.c_str(),nullptr,nullptr);
  if(ret<0){error="Cannot open downloaded video: "+ff_error(ret);break;}
  ret=avformat_find_stream_info(input,nullptr);
  if(ret<0){error="Cannot read video metadata: "+ff_error(ret);break;}
  const int vi=av_find_best_stream(input,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
  if(vi<0){error="MP4 has no supported video stream";break;}
  AVStream* src=input->streams[vi];
  const AVCodec* decoder=avcodec_find_decoder(src->codecpar->codec_id);
  if(!decoder){error="No decoder for video codec";break;}
  dec=avcodec_alloc_context3(decoder);
  if(!dec||avcodec_parameters_to_context(dec,src->codecpar)<0){
   error="Cannot configure source decoder";break;
  }
  dec->thread_count=2;
  ret=avcodec_open2(dec,decoder,nullptr);
  if(ret<0){error="Video decoder failed: "+ff_error(ret);break;}
  // Keep Full HD while reducing DPB to one reference, without B frames.
  const AVCodec* encoder=avcodec_find_encoder_by_name("libx264");
  if(!encoder){error="PS5 x264 encoder unavailable";break;}
  enc=avcodec_alloc_context3(encoder);
  if(!enc){error="Cannot allocate encoder";break;}
  enc->width=dec->width;
  enc->height=dec->height;
  if(enc->width<=0||enc->height<=0||enc->width>1920||enc->height>1080){
   error="Only FullHD H.264 can be optimized";break;
  }
  enc->width&=~1;enc->height&=~1;
  enc->time_base=AVRational{1,30};
  enc->framerate=AVRational{30,1};
  enc->pix_fmt=AV_PIX_FMT_YUV420P;
  enc->gop_size=60;
  enc->max_b_frames=0;
  enc->refs=1;
  enc->thread_count=3;
  enc->flags|=AV_CODEC_FLAG_GLOBAL_HEADER;
  av_opt_set(enc->priv_data,"preset","ultrafast",0);
  av_opt_set(enc->priv_data,"tune","zerolatency",0);
  av_opt_set(enc->priv_data,"profile","baseline",0);
  av_opt_set(enc->priv_data,"level","4.1",0);
  av_opt_set(enc->priv_data,"crf","25",0);
  av_opt_set(enc->priv_data,"x264-params","ref=1:bframes=0:level=4.1:keyint=60:scenecut=0",0);
  ret=avcodec_open2(enc,encoder,nullptr);
  if(ret<0){error="x264 encoder failed: "+ff_error(ret);break;}
  ret=avformat_alloc_output_context2(&dst,nullptr,"mp4",output.c_str());
  if(ret<0||!dst){error="Cannot create MP4 output container";break;}
  AVStream* out=avformat_new_stream(dst,nullptr);
  if(!out){error="Cannot create video stream";break;}
  out->time_base=enc->time_base;
  ret=avcodec_parameters_from_context(out->codecpar,enc);
  if(ret<0){error="Cannot write encoder settings";break;}
  ret=avio_open(&dst->pb,output.c_str(),AVIO_FLAG_WRITE);
  if(ret<0){error="Cannot write optimized video: "+ff_error(ret);break;}
  ret=avformat_write_header(dst,nullptr);
  if(ret<0){error="Cannot write MP4 header: "+ff_error(ret);break;}
  raw=av_frame_alloc();scaled=av_frame_alloc();packet=av_packet_alloc();
  if(!raw||!scaled||!packet){error="Video optimization out of memory";break;}
  scaled->format=enc->pix_fmt;scaled->width=enc->width;scaled->height=enc->height;
  ret=av_frame_get_buffer(scaled,32);
  if(ret<0){error="No converted video frame memory";break;}
  sws=sws_getContext(dec->width,dec->height,dec->pix_fmt,enc->width,enc->height,
     enc->pix_fmt,SWS_FAST_BILINEAR,nullptr,nullptr,nullptr);
  if(!sws){error="Unsupported PS5 video pixel format";break;}
  const int64_t estimated=src->nb_frames>0?src->nb_frames:
    (input->duration>0?input->duration*30/AV_TIME_BASE:900);
  int64_t frames=0;
  auto drain_encoder=[&]()->bool{
   AVPacket* pkt=av_packet_alloc();
   if(!pkt){error="Packet allocation failed";return false;}
   bool ok=true;
   for(;;){
    int result=avcodec_receive_packet(enc,pkt);
    if(result==AVERROR(EAGAIN)||result==AVERROR_EOF)break;
    if(result<0){error="Encoding output failed: "+ff_error(result);ok=false;break;}
    av_packet_rescale_ts(pkt,enc->time_base,out->time_base);
    pkt->stream_index=out->index;
    result=av_interleaved_write_frame(dst,pkt);
    av_packet_unref(pkt);
    if(result<0){error="Writing MP4 frames failed: "+ff_error(result);ok=false;break;}
   }
   av_packet_free(&pkt);return ok;
  };
  auto drain_decoder=[&]()->bool{
   for(;;){
    int result=avcodec_receive_frame(dec,raw);
    if(result==AVERROR(EAGAIN)||result==AVERROR_EOF)return true;
    if(result<0){error="Decoding motion video failed: "+ff_error(result);return false;}
    result=av_frame_make_writable(scaled);
    if(result<0){error="Cannot access converted frame";return false;}
    sws_scale(sws,raw->data,raw->linesize,0,dec->height,scaled->data,scaled->linesize);
    scaled->pts=frames++;
    // Limit to ~97% until the MP4 trailer has been flushed to disk.
    progress.store((int)std::min<int64_t>(97,frames*96/std::max<int64_t>(1,estimated)));
    result=avcodec_send_frame(enc,scaled);
    av_frame_unref(raw);
    if(result<0){error="Encoding frame rejected: "+ff_error(result);return false;}
    if(!drain_encoder())return false;
   }
  };
  bool passed=true;
  while((ret=av_read_frame(input,packet))>=0){
   if(packet->stream_index==vi){
    ret=avcodec_send_packet(dec,packet);
    if(ret<0){error="Bad video packet: "+ff_error(ret);passed=false;}
    if(passed&&!drain_decoder())passed=false;
   }
   av_packet_unref(packet);
   if(!passed)break;
  }
  if(!passed)break;
  avcodec_send_packet(dec,nullptr);
  if(!drain_decoder())break;
  ret=avcodec_send_frame(enc,nullptr);
  if(ret<0||!drain_encoder()){if(error.empty())error="Failed finalizing video encoder";break;}
  ret=av_write_trailer(dst);
  if(ret<0){error="Failed finalizing MP4: "+ff_error(ret);break;}
  avio_closep(&dst->pb);
  struct stat st{};
  if(stat(output.c_str(),&st)!=0||st.st_size<2048){
   error="Optimized MP4 output is empty";break;
  }
  if(rename(output.c_str(),filename.c_str())!=0){
   error="Cannot replace incompatible wallpaper with optimized version";break;
  }
  progress=100;success=true;
 }while(false);
 if(dst){
  if(dst->pb)avio_closep(&dst->pb);
  avformat_free_context(dst);
 }
 sws_freeContext(sws);
 av_packet_free(&packet);
 av_frame_free(&raw);av_frame_free(&scaled);
 avcodec_free_context(&dec);avcodec_free_context(&enc);
 if(input)avformat_close_input(&input);
 if(!success)unlink(output.c_str());
 return success;
}
} // namespace smooth_video

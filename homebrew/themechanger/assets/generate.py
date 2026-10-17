#!/usr/bin/env python3
"""Build original illustrative wallpaper images and short H.264 LiveShell clips.
No Sony assets. The PNG images are also the grid and preview textures."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from pathlib import Path
import math,random,subprocess
root=Path(__file__).resolve().parents[1]/'generated/wallpapers';root.mkdir(parents=True,exist_ok=True)
names=['baseball','anime','gaming','cyber-city','racing','space','samurai','retro-neon','mountains','forest','beach','snow']
palettes=[('#081b34','#ff6c2b'),('#14133d','#e45bd8'),('#060e35','#595eff'),('#07142d','#fa2add'),('#050c2d','#ff328e'),('#061331','#79a6f4'),('#301015','#fe381b'),('#230e45','#fe53b9'),('#172b61','#fbb492'),('#081e1a','#70a959'),('#037ca7','#f3d173'),('#21365d','#b6d7fa')]
def rgb(s):return tuple(bytes.fromhex(s[1:]))
for index,(name,(a,b)) in enumerate(zip(names,palettes)):
 im=Image.new('RGB',(1280,720));p=im.load();top=rgb(a);bot=rgb(b)
 for y in range(720):
  t=y/720
  c=tuple(int(top[k]*(1-t)+bot[k]*t) for k in range(3))
  for x in range(1280):p[x,y]=c
 d=ImageDraw.Draw(im,'RGBA')
 random.seed(index+47)
 for j in range(30):
  x=random.randint(-100,1280);y=random.randint(15,720);rr=random.randint(4,86)
  d.ellipse((x-rr,y-rr,x+rr,y+rr),outline=(180,225,255,random.randint(28,100)),width=random.randint(1,4))
 if index in (3,7):
  for j in range(46):
   x=j*32;h=random.randint(100,400)
   d.rectangle((x,720-h,x+26,720),fill=(2,7,30,210),outline=(58,171,254,135))
   for yy in range(720-h+9,715,17):d.line((x+7,yy,x+18,yy),fill=(249,48,200,175),width=2)
 elif index in (8,10,11):
  for layer in range(3):
   points=[(0,720)]
   for j in range(13):points.append((j*110,random.randrange(360+layer*100,510+layer*85)))
   points.append((1280,720))
   d.polygon(points,fill=(5+layer*9,26+layer*13,48+layer*20,235))
 elif index==5:
  d.ellipse((500,40,1120,660),fill=(40,83,158,225),outline=(189,217,250,255),width=8)
  d.arc((420,-40,1190,730),50,310,fill=(247,230,180,190),width=22)
 elif index==9:
  for x in range(0,1280,85):
   d.polygon(((x-60,720),(x+25,720),(x+7,random.randint(90,230))),fill=(4,34,26,255))
 elif index==0:
  d.ellipse((460,280,800,620),fill=(247,244,224,245),outline=(147,58,40,230),width=8)
  for j in range(8):
   xx=493+j*39;d.arc((xx,295,xx+120,608),80,160,fill=(175,47,42,240),width=6)
 elif index in (2,4):
  d.rounded_rectangle((410,380,1010,550),42,fill=(5,14,34,235),outline=(58,186,255,220),width=10)
  for j in range(5):
   d.line((540+j*44,440,540+j*44,510),fill=(244,71,245,240),width=8)
 elif index==6:
  d.ellipse((785,85,1120,420),fill=(255,71,30,210))
  d.polygon(((610,690),(695,270),(750,330),(821,690)),fill=(13,5,16,240))
 else:
  d.ellipse((785,55,1160,430),fill=(235,167,252,105),outline=(226,219,255,235),width=6)
 im.save(root/('ThemeChanger-'+name+'.png'),optimize=True)
 # No false promise: real LiveShell requires playable H.264 .mp4 data.
 subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-loop','1','-framerate','25','-i',str(root/('ThemeChanger-'+name+'.png')),'-t','2','-vf','scale=1280:720,format=yuv420p','-c:v','libx264','-preset','ultrafast','-crf','28','-movflags','+faststart',str(root/('ThemeChanger-'+name+'.mp4'))],check=True)
print('Generated',len(names),'original PNG previews and LiveShell H.264 clips')

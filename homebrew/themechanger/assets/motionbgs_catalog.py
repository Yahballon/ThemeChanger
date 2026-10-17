#!/usr/bin/env python3
"""Discover MotionBGS public 1080p MP4 links for ThemeChanger.

The catalog contains only metadata + original source links. Video and artwork
are NOT mirrored or bundled. Users download selected assets privately onto PS5.
"""
from html.parser import HTMLParser
from urllib.parse import urljoin,urlparse
import xml.etree.ElementTree as ET
from collections import defaultdict
from threading import Lock
from urllib.request import Request,urlopen
from concurrent.futures import ThreadPoolExecutor,as_completed
import html, re, pathlib, time, os

ORIGIN="https://motionbgs.com"
UA="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/125 Safari/537.36"
CATEGORIES=[
  ("Featured","/"),("Anime","/tag:anime/"),("Games","/tag:games/"),
  ("Nature","/tag:nature/"),("Cars","/tag:car/"),("Space","/tag:space/"),
  ("Technology","/tag:technology/"),("Fantasy","/tag:fantasy/"),
]
SKIP={"about","privacy-policy","dmca","contact","windows-11","mac","mobile","guides"}
class Anchors(HTMLParser):
    def __init__(self): super().__init__(convert_charrefs=True);self.found=[];self.link=None
    def handle_starttag(self,tag,attrs):
        attrs=dict(attrs)
        if tag=="a":
            self.link=[attrs.get("href") or "",attrs.get("title") or "",""]
        if tag=="img" and self.link:
            self.link[2]+=(attrs.get("alt") or "")+" "
    def handle_data(self,data):
        if self.link:self.link[2]+=data
    def handle_endtag(self,tag):
        if tag=="a" and self.link:
            self.found.append(self.link)
            self.link=None

class Artwork(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.poster=""
        self.preload=""
        self.og_image=""
    def handle_starttag(self,tag,attrs):
        values=dict(attrs)
        if tag=="video" and values.get("poster"):
            self.poster=values["poster"]
        if tag=="link" and values.get("as")=="image" and values.get("href"):
            self.preload=values["href"]
        if tag=="meta" and values.get("property")=="og:image":
            self.og_image=values.get("content") or ""

def request(url,limit=3*1024*1024):
    req=Request(url,headers={"User-Agent":UA,"Accept":"text/html,*/*;q=0.8","Accept-Encoding":"identity"})
    with urlopen(req,timeout=18) as res:
        if urlparse(res.geturl()).hostname!="motionbgs.com":raise RuntimeError("redirect outside source")
        return res.read(limit+1)[:limit].decode("utf-8","replace")

def clean(s):
    return re.sub(r"\s+"," ",html.unescape(re.sub(r"<[^>]*>"," ",s))).strip()

def links(category,path):
    h=request(ORIGIN+path)
    p=Anchors();p.feed(h)
    found=[]
    for href,title,text in p.found:
        url=urljoin(ORIGIN+"/",href)
        parsed=urlparse(url)
        slug=parsed.path.strip("/")
        if parsed.hostname!="motionbgs.com" or not re.fullmatch(r"[a-z0-9][a-z0-9-]{2,100}",slug) or slug in SKIP:continue
        name=clean(title or text)
        if not 3<=len(name)<=100:continue
        if name.lower().startswith(("more ","next","previous","how to")):continue
        found.append((slug,name.replace(" Live Wallpaper","").replace(" 4K",""),category))
    return found

def page(item):
    slug,name,category=item
    try:
        raw=request(f"{ORIGIN}/{slug}",limit=600000)
        hit=re.search(r'(?:href|data-href)=["\'](?:https?://motionbgs\.com)?(/dl/hd/(\d+))["\']',raw,re.I)
        if not hit:
            hit=re.search(r'(/dl/hd/(\d+))',raw,re.I)
        if not hit:return None
        fid=hit.group(2)
        title=re.search(r'<h1[^>]*>(.*?)</h1>',raw,re.I|re.S)
        if title:
            better=clean(title.group(1)).removesuffix(" Live Wallpaper")
            if 3<len(better)<90:name=better
        art=Artwork()
        art.feed(raw)
        image=""
        # Prefer the site's 960x540 JPEG video poster, not the original 4K OG image.
        for candidate in (art.poster,art.preload,art.og_image):
            if not candidate:continue
            full=urljoin(ORIGIN+"/",html.unescape(candidate))
            host=urlparse(full).hostname or ""
            if host=="motionbgs.com" or host.endswith(".motionbgs.com"):
                if re.search(r'\.(?:jpg|jpeg|png)$',urlparse(full).path,re.I):
                    image=full
                    break
        return (slug,name[:90],category,f"{ORIGIN}/dl/hd/{fid}/",image)
    except Exception as e:
        print("Skip:",slug,type(e).__name__,str(e)[:100],flush=True)
        return None

# Crawl the sitemap once for the complete, deduplicated 4K desktop list.
# Only listing pages are fetched in bulk. Individual detail pages are fetched
# lazily by the PS5 app when a thumbnail or MP4 is actually requested.
POPULAR_TAGS=[
 "naruto","demon-slayer","dragon-ball","one-piece","goku","samurai","pokemon",
 "minecraft","cyberpunk","genshin-impact","fortnite","spiderman","batman",
 "rain","sunset","forest","night","mountain","anime-girl","space","black-hole",
 "bmw","nissan","jdm","porsche","audi","japan","horror","fantasy","knight",
 "nature","technology","cars","games","anime","red","dark","blue","purple",
 "white","cat","dragon","snow","winter","water","star-wars","warcraft",
 "wuthering-waves","honkai-star-rail","valorant","bleach","solo-leveling"
]
CATEGORY_ROUTES=[
 ("Anime","/tag:anime/"),("Games","/tag:games/"),("Nature","/tag:nature/"),
 ("Cars","/tag:car/"),("Space","/tag:space/"),
 ("Technology","/tag:technology/"),("Fantasy","/tag:fantasy/"),
 ("Superhero","/tag:superhero/"),("TV & Movie","/tag:tv-and-movie/"),
 ("Horror","/tag:horror/"),("Animal","/tag:animal/"),
 ("Japan","/tag:japan/"),("Football","/tag:football/")
]
class PageLinkParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.listing=[];self.active=None;self.next_url=None
    def handle_starttag(self,tag,attrs):
        v=dict(attrs)
        if tag=="link" and v.get("rel")=="next":
            self.next_url=v.get("href")
        if tag=="a":self.active={"href":v.get("href") or "","title":v.get("title") or "","text":""}
        if tag=="img" and self.active:
            self.active["text"]+=" "+(v.get("alt") or "")
    def handle_data(self,data):
        if self.active:self.active["text"]+=data
    def handle_endtag(self,tag):
        if tag=="a" and self.active:
            self.listing.append(self.active);self.active=None

def sitemap():
    raw=request(ORIGIN+"/sitemap.xml",limit=6*1024*1024)
    tree=ET.fromstring(raw)
    slugs=[]
    tags=set()
    special={"4k","gifs","mobile","about","guides","search","latest","privacy-policy","dmca","contact"}
    for loc in tree.iter():
        if not loc.tag.endswith("loc") or not loc.text:continue
        u=urlparse(loc.text.strip())
        if u.hostname!="motionbgs.com":continue
        part=u.path.strip("/")
        if re.fullmatch(r"tag:[a-z0-9-]{2,80}",part):
            tags.add(part[4:]);continue
        if re.fullmatch(r"[a-z0-9][a-z0-9-]{2,100}",part) and part not in special:
            slugs.append(part)
    slugs=list(dict.fromkeys(slugs))
    if len(slugs)<3000:raise RuntimeError(f"Sitemap unexpectedly small: {len(slugs)}")
    print("MotionBGS sitemap: desktop candidates",len(slugs),"known tags",len(tags),flush=True)
    return slugs,tags

def capture_listing(category,route,known,maxpages=115):
    found={}
    pageurl=urljoin(ORIGIN+"/",route)
    pages=0
    while pageurl and pages<maxpages:
        try:
            source=request(pageurl,limit=550000)
            parser=PageLinkParser();parser.feed(source)
        except Exception as e:
            print("Listing stopped",category,pages,repr(e)[:100],flush=True);break
        pages+=1
        for a in parser.listing:
            u=urlparse(urljoin(ORIGIN+"/",a["href"]))
            slug=u.path.strip("/")
            if u.hostname=="motionbgs.com" and slug in known:
                found[slug]=clean(a["title"] or a["text"]).replace(" Live Wallpaper","").removesuffix(" 4K")
        nxt=urljoin(ORIGIN+"/",parser.next_url) if parser.next_url else ""
        if nxt==pageurl or (nxt and urlparse(nxt).hostname!="motionbgs.com"):break
        pageurl=nxt
        if pages%30==0:print("  ",category,pages,"pages",len(found),"items",flush=True)
        time.sleep(0.10)
    print("Listed",category,pages,"pages",len(found),"items",flush=True)
    return category,found

def prettify(slug):
    value=slug.replace("-"," ").strip().title()
    return value[:90]

def main():
    slugs,site_tags=sitemap()
    known=set(slugs)
    titles={slug:prettify(slug) for slug in slugs}
    category={slug:"Other" for slug in slugs}
    tagset=defaultdict(set)
    grouped=CATEGORY_ROUTES+[
       (tag.title().replace("-"," "),"/tag:"+tag+"/")
       for tag in POPULAR_TAGS if tag in site_tags and tag not in
       {"anime","games","nature","car","space","technology","fantasy","horror"}
    ]
    # Avoid thousands of unnecessary detail page requests. Listing pages supply
    # accurate categories and popular subtag memberships in ~hundreds of GETs.
    with ThreadPoolExecutor(max_workers=7) as pool:
        futures={pool.submit(capture_listing,label,route,known,
                             120 if (label,route) in CATEGORY_ROUTES else 18):
                 (label,route) for label,route in grouped}
        for future in as_completed(futures):
            label,route=futures[future]
            try:_,found=future.result()
            except Exception as e:
                print("Tag listing unavailable",label,repr(e),flush=True);continue
            is_category=(label,route) in CATEGORY_ROUTES
            for slug,name in found.items():
                if name and 3<=len(name)<=100:titles[slug]=name[:90]
                if is_category and category[slug]=="Other":category[slug]=label
                tagset[slug].add(route.split("tag:",1)[-1].strip("/"))
    # Known-good page metadata from the previous implementation improves the
    # first screens and supplies early download tests while the remaining ~9K
    # items are resolved lazily on the PS5.
    featured=[]
    for name,route in CATEGORIES:
        try:featured.extend(links(name,route)[:15])
        except Exception:pass
    unique={}
    for entry in featured:
        if entry[0] in known:unique[entry[0]]=entry
    verified={}
    with ThreadPoolExecutor(max_workers=6) as pool:
        futures=[pool.submit(page,item) for item in list(unique.values())[:90]]
        for future in as_completed(futures):
            item=future.result()
            if item:verified[item[0]]=item
    # Keep stable sitemap order; downloaded MP4 and JPEG requests remain
    # strictly on-demand to respect the original site's bandwidth.
    ordered=list(dict.fromkeys(list(verified)+slugs))
    output=pathlib.Path(__file__).resolve().parents[1]/"generated/motionbgs/catalog.tsv"
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("w",encoding="utf8") as f:
        f.write("# slug\ttitle\tcategory\tmp4_url\tpreview_url\ttags\n")
        for slug in ordered:
            hit=verified.get(slug)
            title=(hit[1] if hit else titles[slug]).replace("\t"," ").replace("\n"," ")
            cat=category[slug] if category[slug]!="Other" else (hit[2] if hit else "Other")
            url=hit[3] if hit else ""
            preview=hit[4] if hit else ""
            tags=",".join(sorted(tagset[slug]))
            f.write("\t".join((slug,title,cat,url,preview,tags))+"\n")
    print("FULL desktop catalog:",len(ordered),"items; pre-verified:",
          len(verified),"; tagged:",sum(bool(v) for v in tagset.values()),flush=True)
    for slug in ordered[:5]:print(" ",slug,verified.get(slug,"lazy detail"),flush=True)

if __name__=="__main__":main()

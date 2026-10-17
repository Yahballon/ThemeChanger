#!/usr/bin/env python3
"""Mock the LiveShell v1.1 REST API for CI integration tests."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlparse,parse_qs
import json
state=dict(themechanger_engine="0.3.6",enabled=True,mainvideo=False,gamepause=False,game="",
           current="/data/LiveShell/video/ThemeChanger-baseball.mp4",status="stopped",
           videos=[
             dict(name="ThemeChanger-baseball.mp4",where="Internal",path="/data/LiveShell/video/ThemeChanger-baseball.mp4"),
             dict(name="MotionBGS-neon.mp4",where="Internal",path="/data/LiveShell/video/MotionBGS-neon.mp4")
           ])
class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def do_GET(self):
        path=urlparse(self.path)
        q=parse_qs(path.query)
        response=dict(ok=True)
        if path.path=="/api/state":
            response=state
        elif path.path=="/api/set":
            if q.get("k")==["mainvideo"] and q.get("v")==["1"]:state["mainvideo"]=True
            else: response=dict(ok=False,error="bad option")
        elif path.path=="/api/select":
            i=int(q.get("i",["-1"])[0])
            if 0<=i<len(state["videos"]):
                state["current"]=state["videos"][i]["path"]
                state["status"]="playing"
            else:response=dict(ok=False,error="No such video")
        else:response=dict(ok=False,error="no handler")
        b=json.dumps(response,separators=(",",":")).encode()
        self.send_response(200);self.send_header("Content-Type","application/json")
        self.send_header("Content-Length",str(len(b)));self.end_headers();self.wfile.write(b)
    def log_message(self,*args): pass
ThreadingHTTPServer(("127.0.0.1",9050),Handler).serve_forever()

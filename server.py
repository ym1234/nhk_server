import http.server
from http import HTTPStatus
from pprint import pprint
import subprocess
import socketserver
from urllib.parse import urlparse
from urllib.parse import parse_qs
import os
import json

DICT_PATH = "/home/ym/Drive3/JPEPWINGS/shoui EPWINGs Pack (Standard Edition)/Misc/NHKAccent/"
SUBBOOK_IDX = 1
NHK_BIN = "./nhk_server"
SPORT = 8808
FSPORT = 8809

# Idk how to do this, I just copied it from the original frovo yomichan server
class MyHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-type", "application/json")
        self.end_headers()

        query_components = parse_qs(urlparse(self.path).query)
        expression = query_components["expression"][0] if "expression" in query_components else ""
        reading = query_components["reading"][0] if "reading" in query_components else ""
        params = [NHK_BIN, DICT_PATH, str(SUBBOOK_IDX)]
        print(params)
        stdout = subprocess.run(params + [expression], encoding='utf8', capture_output=True).stdout + '\n' + subprocess.run(params + [reading], encoding='utf8', capture_output=True).stdout
        y = set(stdout.strip().split("\n"))
        audio_sources = [{"name": f"http://127.0.0.1:{FSPORT}/{i}", "url": f"http://127.0.0.1:{FSPORT}/{i}"} for i in y if i != ""]
        print(audio_sources)
        resp = {
            "type": "audioSourceList",
            "audioSources": audio_sources
        }
        pprint(resp)
        self.wfile.write(bytes(json.dumps(resp), "utf8"))
        return

x = os.fork()
if x == 0:
    with socketserver.TCPServer(('localhost', SPORT), MyHandler, bind_and_activate=True) as httpd:
        print("Serving yomichan server at", SPORT)
        httpd.serve_forever()
else:
    # os.chdir("audio")
    with socketserver.TCPServer(("localhost", FSPORT), http.server.SimpleHTTPRequestHandler) as httpd:
        print("Serving file server at", FSPORT)
        httpd.serve_forever()


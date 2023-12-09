#!/usr/bin/python3

from http.server import HTTPServer, BaseHTTPRequestHandler
from subprocess import run
import re

fmt = 'pi5_{}{{name="{}",id="{}"}} {}\n'

class MyHandler(BaseHTTPRequestHandler):
  def do_GET(self):
    self.send_response(200)
    self.send_header("Content-type", "text/html")
    self.end_headers()
    res = run(["vcgencmd","pmic_read_adc"], capture_output=True)
    lines = res.stdout.decode("utf-8").splitlines()
    for line in lines:
      res = re.search('([A-Z_0-9]+)_[VA] (current|volt)\(([0-9]+)\)=([0-9.]+)', line)
      self.wfile.write(fmt.format(res.group(2), res.group(1), res.group(3), res.group(4)).encode("utf-8"))

httpd = HTTPServer(('', 9101), MyHandler)
httpd.serve_forever()

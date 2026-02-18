#!/usr/bin/env python3
"""Serve HamClock-Next WASM with COOP/COEP headers and an integrated CORS proxy.

Usage:  python3 serve.py <build-dir> [port]

The CORS proxy accepts requests at:
  GET /proxy/<url>
  e.g. /proxy/https://services.swpc.noaa.gov/json/solar-geophysical-data.json

This lets the WASM app reach external APIs without a separate proxy server.
"""
import http.server
import socketserver
import sys
import os
import urllib.request
import urllib.error

PROXY_PREFIX = "/proxy/"
# Only http/https targets are allowed.
ALLOWED_SCHEMES = {"http", "https"}


class HamClockHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

    def do_GET(self):
        if self.path.startswith(PROXY_PREFIX):
            target = self.path[len(PROXY_PREFIX) :]
            self._proxy(target)
        else:
            super().do_GET()

    def do_OPTIONS(self):
        """Pre-flight CORS requests from the browser."""
        if self.path.startswith(PROXY_PREFIX):
            self.send_response(204)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "*")
            self.end_headers()
        else:
            self.send_error(405)

    def _proxy(self, target_url: str):
        from urllib.parse import urlparse

        parsed = urlparse(target_url)
        if parsed.scheme not in ALLOWED_SCHEMES:
            self.send_error(400, "Only http/https targets allowed")
            return
        try:
            req = urllib.request.Request(
                target_url,
                headers={"User-Agent": "HamClock-Next/WASM-proxy"},
            )
            with urllib.request.urlopen(req, timeout=15) as resp:
                body = resp.read()
                content_type = resp.headers.get(
                    "Content-Type", "application/octet-stream"
                )
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
                self.end_headers()
                self.wfile.write(body)
        except urllib.error.HTTPError as e:
            self.send_error(e.code, str(e.reason))
        except Exception as e:
            self.send_error(502, f"Proxy error: {e}")

    def log_message(self, fmt, *args):
        super().log_message(fmt, *args)


directory = sys.argv[1] if len(sys.argv) > 1 else "."
try:
    os.chdir(directory)
except FileNotFoundError:
    print(f"Error: Directory '{directory}' not found.")
    sys.exit(1)

port = int(sys.argv[2]) if len(sys.argv) > 2 else 8090

print(f"Serving {directory} at http://localhost:{port}/")
print("COOP/COEP headers: enabled")
print(f"CORS proxy: http://localhost:{port}{PROXY_PREFIX}<url>")

with socketserver.ThreadingTCPServer(("", port), HamClockHandler) as httpd:
    httpd.serve_forever()

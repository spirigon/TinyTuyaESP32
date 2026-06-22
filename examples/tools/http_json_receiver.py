#!/usr/bin/env python3
"""Generic local JSON receiver for HTTP forwarding examples."""

from __future__ import annotations

import argparse
import json
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class JsonReceiverHandler(BaseHTTPRequestHandler):
    server_version = "JsonReceiver/1.0"

    def _send_json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path.split("?", 1)[0] == "/health":
            self._send_json(200, {"ok": True})
            return
        self._send_json(404, {"ok": False, "error": "not_found"})

    def do_POST(self) -> None:
        content_length = int(self.headers.get("Content-Length", "0") or "0")
        raw = self.rfile.read(content_length)
        text = raw.decode("utf-8", errors="replace")
        timestamp = datetime.now().isoformat(timespec="seconds")

        print(f"\n[{timestamp}] POST {self.path} from {self.client_address[0]}:{self.client_address[1]}")
        try:
            payload = json.loads(text)
        except json.JSONDecodeError as exc:
            print(text)
            print(f"JSON parse error: {exc}")
            self._send_json(400, {"ok": False, "error": "bad_json"})
            return

        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
        self._send_json(200, {"ok": True})

    def log_message(self, fmt: str, *args: object) -> None:
        return


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Receive and print HTTP JSON POST payloads.")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address, default: 0.0.0.0")
    parser.add_argument("--port", type=int, default=28080, help="Bind port, default: 28080")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    server = ThreadingHTTPServer((args.host, args.port), JsonReceiverHandler)
    print(f"Listening on http://{args.host}:{args.port}")
    print("Health check: GET /health")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()

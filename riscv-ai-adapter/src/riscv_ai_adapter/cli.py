from __future__ import annotations

import argparse
import json
import os
import time
import urllib.error
import urllib.request


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Inspect a RISC-V AI Adapter")
    parser.add_argument(
        "command",
        choices=[
            "health",
            "models",
            "metrics",
            "system",
            "backends",
            "doctor",
            "wait-ready",
        ],
    )
    parser.add_argument("--url", default="http://127.0.0.1:18080")
    parser.add_argument("--token-env", default="RVAI_ADAPTER_TOKEN")
    parser.add_argument("--wait-seconds", type=float, default=90)
    args = parser.parse_args(argv)

    paths = {
        "health": "/health",
        "doctor": "/health",
        "wait-ready": "/health",
        "models": "/v1/models",
        "metrics": "/metrics",
        "system": "/v1/system/info",
        "backends": "/admin/backends",
    }
    headers: dict[str, str] = {}
    token = os.environ.get(args.token_env, "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    endpoint = args.url.rstrip("/") + paths[args.command]
    deadline = time.monotonic() + args.wait_seconds
    while True:
        request = urllib.request.Request(endpoint, headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                body = response.read().decode(errors="replace")
            if args.command != "wait-ready":
                break
            value = json.loads(body)
            if value.get("status") == "ready":
                print("adapter is ready")
                return 0
        except urllib.error.HTTPError as error:
            body = error.read().decode(errors="replace")
            if args.command != "wait-ready":
                print(body)
                return 1
        except OSError as error:
            body = str(error)
            if args.command != "wait-ready":
                print(f"adapter request failed: {error}")
                return 1
        if time.monotonic() >= deadline:
            print(f"adapter did not become ready: {body}")
            return 1
        time.sleep(1)

    if args.command == "metrics":
        print(body, end="")
    else:
        value = json.loads(body)
        print(json.dumps(value, ensure_ascii=False, indent=2))
        if args.command == "doctor" and value.get("status") != "ready":
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
